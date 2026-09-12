#pragma once
#include <cmath>
#include <cstdint>

namespace ink {
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
    Vec3 operator/(float s) const { return {x/s,y/s,z/s}; }
    Vec3& operator+=(Vec3 b) { *this=*this+b; return *this; }
};
inline float Dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a,a)); }
inline Vec3 Normalize(Vec3 a) { float n=Length(a); return n>0.00001f?a/n:Vec3{}; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }

// Rectangular, finite plane: origin is the lower-left corner; u/v are unit axes.
// Normal is explicit, faces the playable side. IDs equal vector indices.
struct Surface {
    Vec3 origin, u, v, normal;
    float width=1, height=1;
    bool inkable=true;
    Vec3 color={0.38f,0.43f,0.48f};
};
// Local surface coordinates in WORLD units. Both masks consume identical stamps.
// Union of rotated ellipses builds an irregular splash without visual/query disagreement.
struct PaintStamp {
    uint32_t surface=0;
    float u=0, v=0, radiusU=0.4f, radiusV=0.4f, angle=0;
    uint32_t team=1;
};
struct RayHit {
    bool hit=false;
    uint32_t surface=0;
    float distance=0;
    Vec3 position, normal;
};
struct Controls {
    float moveX=0, moveZ=0, yaw=0, pitch=0;
    bool fire=false, swim=false, jump=false;
    Vec3 aimPoint={0,1,10};
};
enum class PlayerState { Human, Swim, WallSwim };
struct PlayerStatus {
    Vec3 position={0,0,-8}, velocity;
    PlayerState state=PlayerState::Human;
    bool grounded=true, onOwnInk=false;
    float ink=1, speed=0, accuracy=0, formBlend=0;
    Vec3 wallNormal={0,0,-1};
};
struct Projectile {
    Vec3 position, velocity;
    float age=0, distance=0, nextDroplet=0;
    int droplets=0;
};
} // namespace ink
