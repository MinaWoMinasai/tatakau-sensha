// Pure placement/camera/picking regression tests using the actual math API.
#include "game/enemy/visual/NeonDepthPlacement.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool Near(float a, float b, float tolerance = 1.0e-4f) { return std::abs(a-b) <= tolerance; }
bool Near(const cg2::Vector3& a, const cg2::Vector3& b, float tolerance = 1.0e-4f) {
    return Near(a.x,b.x,tolerance) && Near(a.y,b.y,tolerance) && Near(a.z,b.z,tolerance);
}
bool Same(const cg2::Matrix4x4& a, const cg2::Matrix4x4& b) {
    return std::memcmp(&a,&b,sizeof(a)) == 0;
}

void Placement() {
    neondepth::FloorBasis floor;
    neondepth::PlacementConfig config;
    config.stableFootLocal = {0.013f, 0.004f, 0.023f};
    neondepth::PlacementInput input;
    input.floorAnchor = {40,25,0};
    input.floorOffset = {2,4};
    input.hoverHeight = 3;
    for (float heading : {-3.14159f,-1.5708f,0.0f,1.5708f,3.14159f}) {
        input.bodyHeading = heading;
        neondepth::PlacementResult result;
        Require(neondepth::TryPlacement(floor,config,input,result), "Valid placement rejected");
        Require(Near(cg2::TransformMatrix(config.stableFootLocal,result.world), {42,29,-3}),
            "Stable foot must map to anchor+floor offset+normal height");
        Require(Near(result.localUpWorld,neondepth::FloorBasis::normal), "Local up must map to floor normal");
        const auto forward = cg2::TransformNormal({0,0,-1},result.world);
        const float inverseScale = 1.0f/result.uniformScale;
        Require(Near(forward.x*inverseScale,std::cos(heading)) &&
            Near(forward.y*inverseScale,std::sin(heading)) && Near(forward.z,0),
            "Local forward and body heading disagree");
        const auto right = cg2::TransformNormal({1,0,0},result.world);
        const auto up = cg2::TransformNormal({0,1,0},result.world);
        const auto back = cg2::TransformNormal({0,0,1},result.world);
        Require(Near(cg2::Length(right),result.uniformScale) &&
            Near(cg2::Length(up),result.uniformScale) && Near(cg2::Length(back),result.uniformScale),
            "Placement must use uniform model scale");
        Require(cg2::Dot(cg2::Cross(right,up),back) > 0, "Model matrix must not reflect handedness");
        for (unsigned repeat=0; repeat<1000; ++repeat) {
            neondepth::PlacementResult repeated;
            Require(neondepth::TryPlacement(floor,config,input,repeated) && Same(result.world,repeated.world),
                "Placement must be a pure same-input result without accumulation");
        }
        input.localLean = {.18f,-.22f,.07f};
        neondepth::PlacementResult leaned;
        Require(neondepth::TryPlacement(floor,config,input,leaned) &&
            Near(cg2::TransformMatrix(config.stableFootLocal,leaned.world),result.footWorld),
            "Local lean must rotate about the stable foot");
        input.localLean = {};
    }
    neondepth::PlacementResult prior;
    Require(neondepth::TryPlacement(floor,config,input,prior), "Placement prior missing");
    const auto before = prior.world;
    config.boundsHeight = 0;
    Require(!neondepth::TryPlacement(floor,config,input,prior) && Same(before,prior.world),
        "Invalid model bounds must preserve prior output");
    config.boundsHeight = 1.5674443f;
    input.floorAnchor.z = 1;
    Require(!neondepth::TryPlacement(floor,config,input,prior), "A lifted collider is not the XY floor anchor");
    input.floorAnchor.z = 0;
    input.localLean.x = std::numeric_limits<float>::quiet_NaN();
    Require(!neondepth::TryPlacement(floor,config,input,prior) && Same(before,prior.world),
        "Nonfinite lean must not poison world state");
}

void CameraAndAim() {
    for (float angle : {15.0f,20.0f,25.0f}) {
        for (cg2::Vector2 viewport : {cg2::Vector2{1280,720},{1024,768},{1680,720}}) {
            neondepth::CameraProfile profile;
            profile.tiltDegrees = angle;
            profile.aspect = viewport.x/viewport.y;
            neondepth::CameraFrame camera;
            Require(neondepth::TryCamera(profile,{40,25,0},camera), "Valid camera rejected");
            cg2::Vector2 focusPixel{}, footPixel{}, topPixel{};
            Require(neondepth::TryProject({40,25,0},camera.viewProjection,viewport,focusPixel) &&
                Near(focusPixel.x,viewport.x/2,.005f) && Near(focusPixel.y,viewport.y/2,.005f),
                "Camera distance/rotation must look at the stated focus");
            Require(neondepth::TryProject({40,25,-3},camera.viewProjection,viewport,footPixel) &&
                neondepth::TryProject({40,25,-21},camera.viewProjection,viewport,topPixel) && topPixel.y<footPixel.y,
                "Floor normal height must project above the stable foot");
            for (float u : {.05f,.5f,.95f}) for(float v : {.05f,.5f,.95f}) {
                const cg2::Vector2 pixel{viewport.x*u,viewport.y*v};
                cg2::Vector3 world{};
                cg2::Vector2 restored{};
                Require(neondepth::TryScreenToFloorFromViewProjection(pixel,viewport,camera.viewProjection,0,world) && world.z==0,
                    "Screen aim must intersect the actual XY floor");
                Require(neondepth::TryProject(world,camera.viewProjection,viewport,restored) &&
                    Near(pixel.x,restored.x,.01f) && Near(pixel.y,restored.y,.01f),
                    "Screen-floor-screen mismatch at aspect/angle/edge");
                cg2::Vector3 inverseWorld{};
                cg2::Vector2 inverseRestored{};
                Require(neondepth::TryScreenToFloor(pixel,viewport,camera.inverseViewProjection,0,inverseWorld) &&
                    neondepth::TryProject(inverseWorld,camera.viewProjection,viewport,inverseRestored) &&
                    Near(pixel.x,inverseRestored.x,.125f) && Near(pixel.y,inverseRestored.y,.125f),
                    "Float inverse fallback exceeds its measured subpixel precision budget");
            }
            for (cg2::Vector2 client : {cg2::Vector2{1280,720},{1920,1080},{800,600}}) {
                cg2::Vector2 normalized{};
                Require(neondepth::TryClientToViewport({client.x*.73f,client.y*.29f},client,viewport,normalized) &&
                    Near(normalized.x,viewport.x*.73f,.001f) && Near(normalized.y,viewport.y*.29f,.001f),
                    "Physical client coordinate must normalize before unprojection");
            }
            // A same-tick camera displacement is also part of the displayed inverse.
            neondepth::CameraFrame shaken;
            Require(neondepth::TryCamera(profile,{40.35f,24.8f,0},shaken), "Shake camera rejected");
            cg2::Vector2 targetPixel{};
            cg2::Vector3 restored{};
            Require(neondepth::TryProject({34,28,0},shaken.viewProjection,viewport,targetPixel) &&
                neondepth::TryScreenToFloorFromViewProjection(targetPixel,viewport,shaken.viewProjection,0,restored) &&
                Near(restored,{34,28,0},.001f), "Shake must use the same displayed inverse for aim");
        }
    }
}

void SafeInverse() {
    auto output=cg2::MakeIdentity4x4(); const auto previous=output;
    cg2::Matrix4x4 zero{};
    Require(!neondepth::TryInverseViewProjection(zero,output) && Same(output,previous),"zero inverse did not preserve output");
    auto dependent=previous;
    for(int column=0;column<4;++column) dependent.m[1][column]=dependent.m[0][column];
    Require(!neondepth::TryInverseViewProjection(dependent,output) && Same(output,previous),"dependent row inverse accepted");
    auto poisoned=previous; poisoned.m[2][2]=std::numeric_limits<float>::quiet_NaN();
    Require(!neondepth::TryInverseViewProjection(poisoned,output) && Same(output,previous),"nonfinite inverse accepted");
    neondepth::CameraFrame frame;
    Require(neondepth::TryCamera({}, {40,25,0},frame) &&
        neondepth::TryInverseViewProjection(frame.viewProjection,output),"actual camera VP inverse rejected");
    cg2::Vector3 floor;
    Require(neondepth::TryScreenToFloor({640,360},{1280,720},output,0,floor) &&
        Near(floor,{40,25,0},.002f),"safe inverse did not pick displayed floor");
}
void InvalidMatrices() {
    cg2::Vector3 prior{2,3,4};
    const auto zero = cg2::Matrix4x4{};
    Require(!neondepth::TryScreenToFloor({10,10},{1280,720},zero,0,prior) && Near(prior,{2,3,4}),
        "Zero-w inverse must reject before homogeneous division");
    Require(!neondepth::TryScreenToFloorFromViewProjection({10,10},{1280,720},zero,0,prior) && Near(prior,{2,3,4}),
        "Singular displayed VP must preserve prior floor aim");
    const auto identity = cg2::MakeIdentity4x4();
    Require(!neondepth::TryScreenToFloor({10,10},{1280,720},identity,-1,prior), "Floor behind ray must reject");
    auto parallel = identity;
    parallel.m[2][2] = 0;
    Require(!neondepth::TryScreenToFloor({10,10},{1280,720},parallel,0,prior), "Parallel/collapsed ray must reject");
    Require(!neondepth::TryScreenToFloor({10,10},{0,720},identity,0,prior), "Zero viewport must reject");
    auto poisoned = identity;
    poisoned.m[2][1] = std::numeric_limits<float>::infinity();
    Require(!neondepth::TryScreenToFloor({10,10},{1280,720},poisoned,0,prior), "Nonfinite inverse must reject");
    Require(!neondepth::TryScreenToFloorFromViewProjection({10,10},{1280,720},poisoned,0,prior) && Near(prior,{2,3,4}),
        "Nonfinite displayed VP must preserve prior floor aim");
    cg2::Vector2 pixelPrior{5,6};
    Require(!neondepth::TryProject({1,2,0},zero,{1280,720},pixelPrior) &&
        Near(pixelPrior.x,5) && Near(pixelPrior.y,6), "Invalid projection must preserve prior output");
    neondepth::CameraProfile profile;
    profile.tiltDegrees = std::numeric_limits<float>::quiet_NaN();
    neondepth::CameraFrame frame;
    Require(!neondepth::TryCamera(profile,{40,25,0},frame), "Nonfinite camera settings must reject");
    profile.tiltDegrees = 20;
    profile.distance = 64;
    profile.farClip = 20;
    Require(neondepth::TryCamera(profile,{40,25,0},frame), "Finite short far clip camera rejected");
    Require(!neondepth::TryScreenToFloor({640,360},{1280,720},frame.inverseViewProjection,0,prior) &&
        Near(prior,{2,3,4}), "Invisible floor beyond far clip must preserve prior aim");
    Require(!neondepth::TryScreenToFloorFromViewProjection({640,360},{1280,720},frame.viewProjection,0,prior) &&
        Near(prior,{2,3,4}), "Displayed VP floor beyond far clip must preserve prior aim");
    Require(!neondepth::TryProject({40,25,0},frame.viewProjection,{1280,720},pixelPrior),
        "The same far-clipped floor must also reject projection");
    profile.farClip = 100;
    Require(neondepth::TryCamera(profile,{40,25,0},frame) &&
        neondepth::TryScreenToFloorFromViewProjection({640,360},{1280,720},frame.viewProjection,0,prior) &&
        Near(prior,{40,25,0},.001f) &&
        neondepth::TryProject(prior,frame.viewProjection,{1280,720},pixelPrior) &&
        Near(pixelPrior.x,640,.01f) && Near(pixelPrior.y,360,.01f),
        "Visible floor inside far clip must agree between picking and projection");
    Require(neondepth::TryScreenToFloor({640,360},{1280,720},frame.inverseViewProjection,0,prior) &&
        neondepth::TryProject(prior,frame.viewProjection,{1280,720},pixelPrior) &&
        Near(pixelPrior.x,640,.125f) && Near(pixelPrior.y,360,.125f),
        "Short-far float inverse fallback exceeds measured subpixel budget");
    for(float farClip:{63.9f,64.0f,64.1f}) {
        profile.farClip=farClip;
        Require(neondepth::TryCamera(profile,{40,25,0},frame),"clip-edge camera rejected");
        cg2::Vector3 picked{2,3,4};cg2::Vector2 projected{};
        const bool visible=neondepth::TryProject({40,25,0},frame.viewProjection,{1280,720},projected);
        const bool aimed=neondepth::TryScreenToFloorFromViewProjection({640,360},{1280,720},frame.viewProjection,0,picked);
        Require(visible==aimed,"unrounded far-clip edge diverged between displayed projection and direct floor input");
        Require(aimed?Near(picked,{40,25,0},.001f):Near(picked,{2,3,4}),"clip-edge aim changed rejected output or visible floor");
    }
    profile.farClip=100;
    profile.nearClip = 10;
    profile.distance = 5;
    Require(neondepth::TryCamera(profile,{40,25,0},frame) &&
        !neondepth::TryScreenToFloor({640,360},{1280,720},frame.inverseViewProjection,0,prior),
        "Floor before the near clip must reject picking");
    Require(!neondepth::TryScreenToFloorFromViewProjection({640,360},{1280,720},frame.viewProjection,0,prior),
        "Direct displayed VP floor solve accepted floor before near clip");
}
}

int main() {
    try {
        Placement(); CameraAndAim(); SafeInverse(); InvalidMatrices();
        std::cout << "Neon depth placement/camera contracts passed. Actual rendering/input still require runtime.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
