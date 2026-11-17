#include "dsp/PrismWav.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;
struct Layer { const char* id; const char* name; const char* kind; double x,y; bool interactive; };
constexpr std::array<Layer,8> layers{{
  {"L00","background","svg",.5,.5,false},
  {"L01","ground_reflection","svg",.5,.91,false},
  {"L02","core_orb","svg",.5,.29,false},
  {"L03","orbital_rings","svg",.5,.29,false},
  {"L04","core_glint","svg",.5,.29,false},
  {"L05","wordmark","live-text",.5,.60,false},
  {"L06","subtitle","live-text",.5,.67,false},
  {"L07","enter_studio","html-button",.5,.80,true}
}};

std::string svg(int layer) {
  static const std::array<const char*,5> bodies{{
R"SVG(<rect width="960" height="540" fill="#000"/>)SVG",
R"SVG(<defs><radialGradient id="h"><stop stop-color="#A44CFF" stop-opacity=".35"/><stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient></defs><ellipse cx="480" cy="492" rx="245" ry="27" fill="url(#h)"/><g fill="none" stroke="#A44CFF" stroke-width="1.1" opacity=".9"><ellipse cx="480" cy="492" rx="204" ry="20"/><ellipse cx="480" cy="492" rx="157" ry="13"/><ellipse cx="480" cy="492" rx="92" ry="7"/></g><path d="M480 467V516M457 492H503" stroke="#D891FF" opacity=".75"/>)SVG",
R"SVG(<defs><radialGradient id="orb" cx=".7" cy=".8" r=".95"><stop stop-color="#9E54FF"/><stop offset=".22" stop-color="#382D9E"/><stop offset=".52" stop-color="#07102B"/><stop offset=".83" stop-color="#7430CE"/><stop offset="1" stop-color="#E394FF"/></radialGradient><filter id="glow" x="-40%" y="-40%" width="180%" height="180%"><feGaussianBlur stdDeviation="8"/></filter></defs><circle cx="480" cy="157" r="71" fill="#A44CFF" opacity=".4" filter="url(#glow)"/><circle cx="480" cy="157" r="65" fill="url(#orb)" stroke="#CC78FF" stroke-width="2"/><path d="M425 137C450 90 507 89 537 129" fill="none" stroke="#B5B8FF" stroke-width="2.5" opacity=".65"/><path d="M501 208Q540 187 544 163" fill="none" stroke="#68C8FF" stroke-width="3" opacity=".65"/>)SVG",
R"SVG(<defs><linearGradient id="o"><stop stop-color="#6822EE"/><stop offset=".4" stop-color="#E7B6FF"/><stop offset=".68" stop-color="#68C8FF"/><stop offset="1" stop-color="#BA46FF"/></linearGradient></defs><g transform="translate(480 157)" fill="none"><ellipse rx="182" ry="53" transform="rotate(-24)" stroke="url(#o)" stroke-width="2.4"/><ellipse rx="138" ry="46" transform="rotate(-67)" stroke="url(#o)" stroke-width="1.5"/><ellipse rx="151" ry="114" transform="rotate(16)" stroke="#8D47CA" stroke-width="1" opacity=".72"/></g>)SVG",
R"SVG(<defs><radialGradient id="s"><stop stop-color="#FFF"/><stop offset=".16" stop-color="#E9B5FF" stop-opacity=".85"/><stop offset="1" stop-color="#A44CFF" stop-opacity="0"/></radialGradient></defs><g transform="translate(480 157)"><ellipse rx="125" ry="3" fill="url(#s)"/><ellipse rx="3" ry="110" fill="url(#s)"/><path d="M0 -20Q4 -4 20 0Q4 4 0 20Q-4 4 -20 0Q-4 -4 0 -20" fill="#F5D8FF"/><circle r="5" fill="white"/></g>)SVG"
  }};
  if(layer<0 || layer>=static_cast<int>(bodies.size())) return {};
  return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 960 540\">\n" + std::string(bodies[static_cast<std::size_t>(layer)]) + "\n</svg>\n";
}

std::string html() { return R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>SLADESHIP — EtherTech Visual System</title>
<style>
*{box-sizing:border-box}[hidden]{display:none!important}
body{margin:0;min-height:100dvh;display:grid;place-items:center;background:#000;color:#F3EAFF}
.stage{position:relative;width:min(100vw,177.7778dvh);aspect-ratio:16/9;overflow:hidden;container-type:inline-size;background:#000}
.visual{position:absolute;inset:0;width:100%;height:100%;pointer-events:none;object-fit:contain}
.screen{position:absolute;inset:0}.wordmark,.subtitle,.enter{position:absolute;left:50%;transform:translate(-50%,-50%);text-align:center;white-space:nowrap}
.wordmark{top:60%;font:400 5.1cqw Georgia,serif;letter-spacing:.22em;text-shadow:0 0 1.1cqw #A44CFF}
.subtitle{top:67%;font:400 1.3cqw Georgia,serif;letter-spacing:.42em}
.enter{top:80%;width:38%;height:8%;font:400 1.8cqw Georgia,serif;letter-spacing:.24em;color:#F3EAFF;cursor:pointer;border:1px solid #C578FF;background:linear-gradient(90deg,#250D39,#09041A,#250D39);box-shadow:0 0 1.6cqw #A44CFF88;clip-path:polygon(7% 0,93% 0,100% 50%,93% 100%,7% 100%,0 50%)}
.enter:hover{filter:brightness(1.3)}button:focus-visible{outline:3px solid #F3EAFF;outline-offset:5px}
.studio{position:absolute;inset:0;display:grid;place-content:center;text-align:center;gap:1cqw;background:#070312;font-family:system-ui,sans-serif}
.studio h2{font:400 5cqw Georgia,serif;letter-spacing:.2em}.back{padding:1cqw 2cqw;border:1px solid #A44CFF;background:#210D34;color:#F3EAFF;cursor:pointer}
@media(prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head><body><main class="stage" aria-label="SLADESHIP prototype"><section class="screen" id="title">
<img class="visual" data-layer="L00" src="layers/L00_background.svg" alt="" aria-hidden="true">
<img class="visual" data-layer="L01" src="layers/L01_ground_reflection.svg" alt="" aria-hidden="true">
<img class="visual" data-layer="L02" src="layers/L02_core_orb.svg" alt="" aria-hidden="true">
<img class="visual" data-layer="L03" src="layers/L03_orbital_rings.svg" alt="" aria-hidden="true">
<img class="visual" data-layer="L04" src="layers/L04_core_glint.svg" alt="" aria-hidden="true">
<h1 class="wordmark" data-layer="L05">SLADESHIP</h1><div class="subtitle" data-layer="L06">ETHERTECH VISUAL SYSTEM</div>
<button class="enter" id="enter" type="button" data-layer="L07">ENTER STUDIO</button></section>
<section class="studio" id="studio" hidden><h2>SLADESHIP</h2><p>Studio navigation proof — no audio engine connected</p><button class="back" id="back" type="button">BACK TO TITLE</button></section></main>
<script>const title=document.getElementById('title'),studio=document.getElementById('studio'),enter=document.getElementById('enter'),back=document.getElementById('back');enter.addEventListener('click',()=>{title.hidden=true;studio.hidden=false;back.focus()});back.addEventListener('click',()=>{studio.hidden=true;title.hidden=false;enter.focus()});</script></body></html>)HTML"; }

void write(const fs::path& path,const std::string& body) {std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("Could not open: "+path.string());out<<body;if(!out)throw std::runtime_error("Could not write: "+path.string());}
void generate(const fs::path& root){fs::create_directories(root/"layers");for(int i=0;i<5;i++)write(root/"layers"/(std::string(layers[static_cast<std::size_t>(i)].id)+"_"+layers[static_cast<std::size_t>(i)].name+".svg"),svg(i));write(root/"index.html",html());}
int main(int argc,char**argv){try{fs::path output="preview";for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--analyze-wav" && i+1<argc){const auto result=sladeship::prism::analyzeWaveFile(argv[++i]);std::cout<<"PRISM WAV analysis\n"<<"sample_rate="<<result.sampleRate<<"\n"<<"dominant_hz="<<result.dominantHz<<"\n"<<"centroid_hz="<<result.centroidHz<<"\n"<<"rms_dbfs="<<result.rmsDb<<"\n"<<"peak_dbfs="<<result.peakDb<<"\n"<<"stereo_correlation="<<result.stereoCorrelation<<"\n"<<"spectral_flux="<<result.spectralFlux<<"\n"<<"somatic_pressure="<<result.somatic.pressure<<"\n";return 0;}if(arg=="--output" && i+1<argc){output=argv[++i];continue;}if(arg=="--list-layers"){for(const auto&l:layers)std::cout<<l.id<<"  "<<l.name<<"  ("<<l.kind<<")\n";return 0;}if(arg=="--verify"){const auto tmp=fs::temp_directory_path()/"sladeship-onion-test";fs::remove_all(tmp);generate(tmp);for(int j=0;j<5;++j){const auto&l=layers[static_cast<std::size_t>(j)];if(!fs::exists(tmp/"layers"/(std::string(l.id)+"_"+l.name+".svg")))throw std::runtime_error("missing layer");}for(const auto&l:layers){if(html().find(std::string("data-layer=\"")+l.id+"\"")==std::string::npos)throw std::runtime_error("unbound layer");}if(html().find("enter.addEventListener")==std::string::npos)throw std::runtime_error("unbound button");fs::remove_all(tmp);std::cout<<"PASS: 8 contracts; 5 SVGs; 2 text objects; 1 interactive button\n";return 0;}if(arg=="--help"){std::cout<<"sladeship [--output DIRECTORY] [--list-layers] [--verify] [--analyze-wav FILE.wav]\n";return 0;}throw std::runtime_error("invalid argument: "+arg);}generate(output);std::cout<<"Generated "<<(output/"index.html")<<"\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
