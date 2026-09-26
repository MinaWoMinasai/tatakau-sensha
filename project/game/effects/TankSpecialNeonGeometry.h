#pragma once
#include "NeonGridRenderer.h"
#include "Calculation.h"
#include <cmath>

// Shared live/preview geometry. Only appends bounded vertices to the existing
// neon batch; there are no assets, sprites, PSOs or persistent effect instances.
namespace tankspecialfx {
inline Vector3 Direction(float angle) {return {std::cos(angle),std::sin(angle),0};}
inline void Ring(NeonGridRenderer& renderer,Vector3 at,float radius,float width,Vector4 color) {
    auto previous=at+Vector3{radius,0,0};
    for(int i=1;i<=24;++i) {const auto p=at+Direction(i*6.2831853f/24)*radius;renderer.QueueLine(previous,p,width,color);previous=p;}
}
inline void Charge(NeonGridRenderer& renderer,Vector3 at,float charge,float age,float scale=1) {
    if(charge<=0)return;
    const bool full=charge>=.999f;
    const Vector4 color=full?Vector4{1.55f,.65f,2.3f,.8f}:Vector4{.18f,1.0f,1.8f,.35f+charge*.4f};
    Ring(renderer,at,(.16f+charge*.27f)*scale,.12f*scale,color);
    Ring(renderer,at,(.12f+charge*.16f)*scale,.05f*scale,{1.1f,1.8f,2.0f,.6f});
    if(full)Ring(renderer,at,(.62f+.035f*std::sin(age*14))*scale,.045f*scale,color);
    for(int i=0;i<8;++i) {
        const float phase=std::fmod(age*1.7f+i*.137f,1.0f);
        const auto dir=Direction(i*2.399963f+age*.3f);
        const auto p=at+dir*((.28f+(1-phase)*.95f)*scale);
        renderer.QueueLine(p,p-dir*.11f*scale,.04f*scale,{color.x,color.y,color.z,phase*.55f*charge});
    }
}
inline void Link(NeonGridRenderer& renderer,Vector3 a,Vector3 b,float scale=1) {
    renderer.QueueLine(a,b,.09f*scale,{.15f,1.1f,1.45f,.19f});
    renderer.QueueLine(a,b,.025f*scale,{.5f,1.45f,1.8f,.38f});
}
inline void Crescent(NeonGridRenderer& renderer,Vector3 at,Vector3 direction,float radius,float alpha=1) {
    const float angle=std::atan2(direction.y,direction.x);
    for(int band=0;band<3;++band) {
        const auto center=at-direction*(radius*.35f+band*.16f);
        const float r=radius*(1-band*.10f);
        auto prev=center+Direction(angle-1.3f)*r;
        for(int n=1;n<=18;++n) {
            const float p=n/18.0f;
            const auto next=center+Direction(angle-1.3f+p*2.6f)*r;
            const float taper=.25f+.75f*std::sin(p*3.14159265f);
            renderer.QueueLine(prev,next,(band==0?.13f:.065f)*taper,
                {band==0?1.25f:.45f,.85f,2.1f,alpha*taper*(band==0?.8f:.25f)});prev=next;
        }
    }
}
inline void Contact(NeonGridRenderer& renderer,Vector3 at,float age,float scale=1,bool perfect=false) {
    const float fade=(std::max)(0.0f,1-age/.24f);
    const Vector4 color=perfect?Vector4{.45f,1.8f,2.0f,fade}:Vector4{1.7f,1.15f,.35f,fade};
    Ring(renderer,at,(.22f+age*3)*scale,.045f*scale,color);
    for(int i=0;i<6;++i) {
        const auto dir=Direction(i*1.04719755f+.3f);
        renderer.QueueLine(at+dir*(.18f+age*2)*scale,at+dir*(.48f+age*3)*scale,.045f*scale,color);
    }
}
}
