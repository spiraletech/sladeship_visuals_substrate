#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <mfapi.h>
#include <mfplay.h>
#include <propvarutil.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "dsp/PrismTimeline.h"
#include "windows/StudioShader.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;
using sladeship::prism::PrismTimeline;
using sladeship::prism::SpectrumFrame;

namespace {
void ensure(HRESULT hr, const char* message) {
    if (FAILED(hr)) throw std::runtime_error(message);
}
std::string utf8(const std::wstring& wide) {
    const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),
        static_cast<int>(wide.size()),nullptr,0,nullptr,nullptr);
    if(n<=0)throw std::runtime_error("invalid Unicode WAV path");
    std::string result(static_cast<std::size_t>(n),0);
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),static_cast<int>(wide.size()),
        result.data(),n,nullptr,nullptr);
    return result;
}
struct GpuState {
    float view[4]{};
    float audio[4]{};
    float mood[4]{};
};
static_assert(sizeof(GpuState)%16==0,"GPU constant buffer alignment");

class Studio {
public:
    HWND window{};
    UINT width=1280,height=720;
    int scene=0;
    bool playing=false;
    std::wstring sourcePath;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> pixel;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<IMFPMediaPlayer> player;
    ComPtr<IMFPMediaItem> media;
    std::future<PrismTimeline> worker;
    std::unique_ptr<PrismTimeline> timeline;

    void initializeGpu() {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width=width;desc.BufferDesc.Height=height;
        desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount=2;desc.OutputWindow=window;desc.Windowed=TRUE;
        desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        D3D_FEATURE_LEVEL feature{};
        auto hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
            nullptr,0,D3D11_SDK_VERSION,&desc,swap.GetAddressOf(),device.GetAddressOf(),
            &feature,context.GetAddressOf());
        if(FAILED(hr)) {
            hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
                nullptr,0,D3D11_SDK_VERSION,&desc,swap.ReleaseAndGetAddressOf(),
                device.ReleaseAndGetAddressOf(),&feature,context.ReleaseAndGetAddressOf());
        }
        ensure(hr,"Cannot create Direct3D11 device (hardware and WARP unavailable)");
        ComPtr<ID3DBlob> vs,ps,error;
        ensure(D3DCompile(kStudioShader,std::strlen(kStudioShader),nullptr,nullptr,nullptr,
            "VSMain","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,vs.GetAddressOf(),error.GetAddressOf()),
            "Cannot compile visualizer vertex shader");
        error.Reset();
        ensure(D3DCompile(kStudioShader,std::strlen(kStudioShader),nullptr,nullptr,nullptr,
            "PSMain","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,ps.GetAddressOf(),error.GetAddressOf()),
            "Cannot compile visualizer pixel shader");
        ensure(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,
            vertex.GetAddressOf()),"Cannot create vertex shader");
        ensure(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,
            pixel.GetAddressOf()),"Cannot create pixel shader");
        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth=sizeof(GpuState);buffer.Usage=D3D11_USAGE_DYNAMIC;
        buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        ensure(device->CreateBuffer(&buffer,nullptr,constants.GetAddressOf()),
            "Cannot create GPU control buffer");
        resize(width,height);
    }

    void resize(UINT w, UINT h) {
        if(!swap || w==0 || h==0)return;
        width=w;height=h;
        context->OMSetRenderTargets(0,nullptr,nullptr);
        target.Reset();
        if(FAILED(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0)))return;
        ComPtr<ID3D11Texture2D> image;
        if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(image.GetAddressOf()))))return;
        device->CreateRenderTargetView(image.Get(),nullptr,target.GetAddressOf());
    }

    void closeAudio() {
        if(player){player->Stop();player->Shutdown();}
        player.Reset();media.Reset();playing=false;
    }
    ~Studio(){closeAudio();}

    void import(const std::wstring& path) {
        if(worker.valid())return; // one loading task at a time; never race source paths
        closeAudio();timeline.reset();sourcePath=path;
        SetWindowTextW(window,L"SLADESHIP // ANALYZING IMPORT...");
        const auto utf8Path=utf8(path);
        worker=std::async(std::launch::async,[utf8Path]{
            return PrismTimeline::fromWaveFile(utf8Path,1024);
        });
    }
    void filePicker() {
        if(worker.valid())return;
        wchar_t file[32768]{};
        OPENFILENAMEW dialog{};
        dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;
        dialog.lpstrFile=file;dialog.nMaxFile=32768;
        dialog.lpstrFilter=L"WAV files (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
        dialog.Flags=OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
        if(GetOpenFileNameW(&dialog))import(file);
    }
    void beginAudio() {
        ensure(MFPCreateMediaPlayer(nullptr,FALSE,0,nullptr,window,player.GetAddressOf()),
            "Cannot open Media Foundation player");
        ensure(player->CreateMediaItemFromURL(sourcePath.c_str(),TRUE,0,media.GetAddressOf()),
            "Cannot decode WAV through Media Foundation");
        ensure(player->SetMediaItem(media.Get()),"Cannot set playback media");
        ensure(player->Play(),"Cannot start audio");
        playing=true;
    }
    double playbackTime() const {
        if(!player || !timeline)return 0.0;
        PROPVARIANT pos;PropVariantInit(&pos);
        double seconds=0.0;
        if(SUCCEEDED(player->GetPosition(MFP_POSITIONTYPE_100NS,&pos))) {
            if(pos.vt==VT_I8)seconds=static_cast<double>(pos.hVal.QuadPart)/1.0e7;
            else if(pos.vt==VT_UI8)seconds=static_cast<double>(pos.uhVal.QuadPart)/1.0e7;
        }
        PropVariantClear(&pos);
        return std::clamp(seconds,0.0,timeline->durationSeconds());
    }
    void seek(double offset) {
        if(!player || !timeline)return;
        PROPVARIANT position;PropVariantInit(&position);
        position.vt=VT_I8;
        position.hVal.QuadPart=static_cast<LONGLONG>(std::clamp(playbackTime()+offset,
            0.0,timeline->durationSeconds())*1.0e7);
        player->SetPosition(MFP_POSITIONTYPE_100NS,&position);
        PropVariantClear(&position);
    }
    void togglePlayback() {
        if(!player)return;
        if(playing){if(SUCCEEDED(player->Pause()))playing=false;}
        else if(SUCCEEDED(player->Play()))playing=true;
    }

    void tick() {
        if(worker.valid() &&
            worker.wait_for(std::chrono::milliseconds(0))==std::future_status::ready) {
            try {
                timeline=std::make_unique<PrismTimeline>(worker.get());
                beginAudio();
            }catch(const std::exception&) {
                worker=std::future<PrismTimeline>{};
                timeline.reset();closeAudio();
                MessageBoxW(window,L"WAV analysis or playback failed. Use a valid PCM16/24/32 or float32 WAV.",
                    L"SLADESHIP import",MB_ICONWARNING);
            }
        }
        const wchar_t* sceneNames[]={L"ORBITAL STUDIO",L"FREQUENCY CATHEDRAL",L"LIQUID MEMORY"};
        const std::wstring name=std::wstring(L"SLADESHIP // ")+sceneNames[scene]+
            L" | O: Open WAV | Space: Play/Pause | Arrows: Seek | 1-3: Scenes";
        SetWindowTextW(window,name.c_str());
        render();
    }
    void render() {
        if(!target)return;
        const auto seconds=playbackTime();
        const SpectrumFrame f=timeline?timeline->sampleAt(seconds):SpectrumFrame{};
        GpuState state{};
        state.view[0]=static_cast<float>(width);
        state.view[1]=static_cast<float>(height);
        state.view[2]=static_cast<float>(seconds);
        state.view[3]=static_cast<float>(scene);
        state.audio[0]=std::clamp(f.rms,0.f,1.f);
        state.audio[1]=std::clamp(f.bassBody,0.f,1.f);
        state.audio[2]=std::clamp(f.beat,0.f,1.f);
        state.audio[3]=std::clamp(f.stereoWidth,0.f,1.f);
        state.mood[0]=std::clamp(f.spectralFlux,0.f,1.f);
        state.mood[1]=std::clamp(f.centroidHz/6000.f,0.f,1.f);
        state.mood[2]=std::clamp(f.somatic.pressure,0.f,1.f);
        state.mood[3]=std::clamp(f.somatic.glassy,0.f,1.f);
        D3D11_MAPPED_SUBRESOURCE map{};
        if(FAILED(context->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map)))return;
        std::memcpy(map.pData,&state,sizeof(state));
        context->Unmap(constants.Get(),0);
        const float clear[]{.004f,.004f,.025f,1.f};
        context->ClearRenderTargetView(target.Get(),clear);
        D3D11_VIEWPORT port{};
        port.Width=static_cast<float>(width);port.Height=static_cast<float>(height);
        port.MaxDepth=1.f;
        context->RSSetViewports(1,&port);
        auto* renderTarget=target.Get();
        context->OMSetRenderTargets(1,&renderTarget,nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex.Get(),nullptr,0);
        context->PSSetShader(pixel.Get(),nullptr,0);
        auto* uniform=constants.Get();
        context->PSSetConstantBuffers(0,1,&uniform);
        context->Draw(3,0);
        swap->Present(1,0);
    }
};
Studio* active=nullptr;
LRESULT CALLBACK wndProc(HWND window,UINT message,WPARAM key,LPARAM data){
    switch(message){
        case WM_SIZE:
            if(active && active->device && key!=SIZE_MINIMIZED)
                active->resize(LOWORD(data),HIWORD(data));
            return 0;
        case WM_TIMER:
            if(active)active->tick();
            return 0;
        case WM_KEYDOWN:
            if(!active)return 0;
            if(key=='O')active->filePicker();
            else if(key==VK_SPACE)active->togglePlayback();
            else if(key==VK_LEFT)active->seek(-5.);
            else if(key==VK_RIGHT)active->seek(5.);
            else if(key>='1' && key<='3')active->scene=static_cast<int>(key-'1');
            return 0;
        case WM_DESTROY:
            KillTimer(window,1);PostQuitMessage(0);return 0;
        default:return DefWindowProcW(window,message,key,data);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)))return 1;
    if(FAILED(MFStartup(MF_VERSION))){CoUninitialize();return 1;}
    int exitCode=0;
    try{
        WNDCLASSW wc{};
        wc.hInstance=instance;wc.lpfnWndProc=wndProc;
        wc.lpszClassName=L"SLADESHIP_NATIVE_STUDIO";
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        wc.style=CS_HREDRAW|CS_VREDRAW;
        if(!RegisterClassW(&wc))throw std::runtime_error("Cannot register studio window");
        {
            Studio studio;
            studio.window=CreateWindowExW(0,wc.lpszClassName,L"SLADESHIP // O: OPEN WAV",
                WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,760,
                nullptr,nullptr,instance,nullptr);
            if(!studio.window)throw std::runtime_error("Cannot create studio window");
            active=&studio;
            studio.initializeGpu();
            ShowWindow(studio.window,show);
            UpdateWindow(studio.window);
            SetTimer(studio.window,1,16,nullptr);
            int argc=0;
            LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
            if(argv){if(argc>1)studio.import(argv[1]);LocalFree(argv);}
            MSG msg{};
            while(GetMessageW(&msg,nullptr,0,0)>0){
                TranslateMessage(&msg);DispatchMessageW(&msg);
            }
            exitCode=static_cast<int>(msg.wParam);
            active=nullptr;
        }
    }catch(const std::exception&) {
        MessageBoxW(nullptr,L"Studio initialization failed. Verify Direct3D11 and Media Foundation support.",
            L"SLADESHIP",MB_ICONERROR);
        exitCode=1;
    }
    MFShutdown();CoUninitialize();
    return exitCode;
}
