#pragma once
#include "NeonGridRenderer.h"
#include <algorithm>
#include <cmath>

// The game world and reward preview both call these geometry routines. They
// only append vertices: no actors, global random source or camera is consulted.
namespace tankneon {
inline void QueueBodyOutline(NeonGridRenderer& renderer,const Vector3& center,
    float radius,float lineWidth,int segments,float rotation,const Vector2& scale,
    const Vector4& color,const Vector3& cameraRight,const Vector3& cameraUp) {
    constexpr float tau=6.28318530718f;
    segments=(std::clamp)(segments,3,48);Vector3 previous{};
    for(int i=0;i<=segments;++i) {
        const float angle=rotation+static_cast<float>(i%segments)*tau/static_cast<float>(segments);
        const Vector3 current=center+cameraRight*(std::cos(angle)*radius*scale.x)+cameraUp*(std::sin(angle)*radius*scale.y);
        if(i>0)renderer.QueueLine(previous,current,lineWidth,color);previous=current;
    }
}
inline void QueueBarrelOutline(NeonGridRenderer& renderer,const Vector3& base,const Vector3& tip,
    const Vector3& right,float halfWidth,float lineWidth,const Vector4& color,bool trapezoid) {
    const float baseHalf=halfWidth*(trapezoid?1.28f:1.0f),tipHalf=halfWidth*(trapezoid?0.72f:1.0f);
    renderer.QueueLine(base-right*baseHalf,tip-right*tipHalf,lineWidth,color);
    renderer.QueueLine(tip-right*tipHalf,tip+right*tipHalf,lineWidth,color);
    renderer.QueueLine(tip+right*tipHalf,base+right*baseHalf,lineWidth,color);
    renderer.QueueLine(base+right*baseHalf,base-right*baseHalf,lineWidth,color);
}
struct BladeStyle {float outerWidth=2.8f,haloWidth=1.0f,coreWidth=0.24f;};
inline void QueueMeleeBlade(NeonGridRenderer& renderer,const Vector3& hilt,const Vector3& direction,
    float length,float width,const Vector4& color,float coreAlpha,const BladeStyle& style) {
    const Vector3 tip=hilt+direction*length;
    Vector4 outer{color.x*1.20f,color.y*1.20f,color.z*1.20f,color.w*0.30f};
    renderer.QueueLine(hilt,tip,width*style.outerWidth,outer);
    Vector4 halo{color.x*1.35f,color.y*1.35f,color.z*1.35f,color.w*0.78f};
    renderer.QueueLine(hilt+direction*(length*0.03f),tip,width*style.haloWidth,halo);
    renderer.QueueLine(hilt+direction*(length*0.08f),tip,width*style.coreWidth,{1,1,1,color.w*coreAlpha});
    const Vector3 right{-direction.y,direction.x,0};Vector4 guard=color;guard.w*=0.62f;
    renderer.QueueLine(hilt-right*(width*1.9f),hilt+right*(width*1.9f),width*0.42f,guard);
    Vector4 tipColor=color;tipColor.w*=0.58f;
    renderer.QueueLine(tip-right*(width*0.90f),tip+right*(width*0.90f),width*0.34f,tipColor);
}
} // namespace tankneon
