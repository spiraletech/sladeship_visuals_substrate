#pragma once
// Original procedural geometry: no third-party console or game assets.
inline constexpr char kStudioShader[] = R"HLSL(
cbuffer State : register(b0) {
    float4 view;  // viewport width, height, audio clock, scene index
    float4 audio; // rms, bass, beat, stereo width
    float4 mood;  // flux, normalized spectral centroid, pressure, glassiness
};
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut VSMain(uint id : SV_VertexID) {
    float2 p = float2((id<<1)&2,id&2);
    VSOut o; o.pos = float4(2*p.x-1,1-2*p.y,0,1); o.uv=p; return o;
}
float edge(float d,float w) {return 1-smoothstep(w,w+.018,abs(d));}
float star(float2 p) {
    float2 c=floor(p),f=frac(p)-.5;
    float n=frac(sin(dot(c,float2(127.1,311.7)))*43758.5453);
    return step(.995,n)*exp(-75*dot(f,f));
}
float4 PSMain(VSOut i) : SV_Target {
    float t=view.z,bass=saturate(audio.y),beat=saturate(audio.z);
    float2 p=(i.uv-.5)*float2(view.x/max(view.y,1),1);
    float d=length(p);
    float3 color=float3(.006,.008,.033)+star(i.uv*float2(100,60))*float3(.08,.08,.2);
    if(view.w<.5) {
        // ORBITAL STUDIO: low frequencies deform an iridescent sphere.
        float radius=.17+.065*bass+.035*beat;
        float sphere=1-smoothstep(radius-.024,radius+.012,d);
        float3 normal=normalize(float3(p/max(.001,radius),sqrt(saturate(1-dot(p,p)/(radius*radius)))));
        float spec=pow(saturate(dot(normal,normalize(float3(-.5,.7,1)))),22+24*mood.w);
        color+=sphere*(float3(.042,.019,.13)+float3(.14,.07,.29)*(1-d/radius));
        color+=sphere*spec*float3(.42,.44,.9);
        color+=exp(-13*max(0,d-radius))*(.02+.08*beat)*float3(.3,.1,.67);
        float a=t*.12,c=cos(a),s=sin(a);
        float2 q=float2(c*p.x+s*p.y,-s*p.x+c*p.y);
        float ring=length(float2(q.x/.40,q.y/(.09+.07*audio.w)))-1;
        float ring2=length(float2((c*p.y-s*p.x)/.32,(s*p.y+c*p.x)/.21))-1;
        color+=edge(ring,.02)*float3(.35,.1,.73)+edge(ring2,.014)*float3(.09,.3,.66);
    }else if(view.w<1.5) {
        // FREQUENCY CATHEDRAL: spectrum/width modulate geometric perspective.
        float z=1/max(.10,abs(p.y+.32));
        float lines=1-smoothstep(.015,.050,abs(frac(p.x*z*(1+audio.w))-.5));
        float cross=1-smoothstep(.02,.07,abs(frac(z*.13-t*(.01+.02*bass))-.5));
        color+=(lines+cross)*float3(.1,.034,.25)*exp(-abs(p.x)*1.3)*(1+.5*beat);
        float arch=length(float2(p.x/(.22+.10*audio.w),(p.y-.01)/(.4+.09*bass)))-1;
        color+=edge(arch,.020)*float3(.2,.21,.67);
    }else{
        // LIQUID MEMORY: flux excites spatial ripples.
        float r=length(p*float2(1,1.5));
        float ripple=sin((18+7*mood.y)*r-t*(.3+3*mood.x));
        float surface=exp(-5*abs(p.y+.08));
        color+=surface*(.04+.22*bass)*(.5+.5*ripple)*float3(.26,.12,.69);
        color+=(1-smoothstep(.01,.07,abs(sin(29*r-t*2*mood.x))))*(.02+.13*mood.x)*float3(.12,.26,.73);
        color+=exp(-8*r)*(.04+.13*mood.z)*float3(.44,.1,.69);
    }
    return float4(pow(saturate(color),.88),1);
}
)HLSL";
