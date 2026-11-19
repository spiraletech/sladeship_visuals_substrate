#include "windows/StudioShader.h"
#include <d3dcompiler.h>
#include <cstring>
#include <iostream>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
int main(){
    for(const char* entry : {"VSMain","PSMain"}){
        ComPtr<ID3DBlob> code,error;
        const char* target=std::strcmp(entry,"VSMain")==0?"vs_5_0":"ps_5_0";
        const auto hr=D3DCompile(kStudioShader,std::strlen(kStudioShader),nullptr,nullptr,nullptr,
            entry,target,D3DCOMPILE_ENABLE_STRICTNESS,0,code.GetAddressOf(),error.GetAddressOf());
        if(FAILED(hr)){
            std::cerr<<entry<<" compile failed: "<<(error?static_cast<const char*>(error->GetBufferPointer()):"no diagnostics")<<"\n";
            return 1;
        }
    }
    std::cout<<"PASS: PS2 studio vertex/pixel shaders compile for D3D11\n";
    return 0;
}
