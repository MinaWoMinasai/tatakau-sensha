#pragma once
// Scene-owned immutable Depth effect ranges using the existing renderer.
#include "NeonDepthEffectGeometry.h"
#include "NeonGridRenderer.h"
#include <memory>
#include <string>

namespace neondepth {
struct EffectsStats {
    unsigned resourceCreates=0,resourceReleases=0,resourceFailures=0;
    uint64_t updates=0,duplicateUpdates=0,invalidFrames=0,capacityRejected=0;
    uint64_t airDraws=0,floorDraws=0,depthBindingRejected=0;
    unsigned vertices=0,airVertices=0,floorVertices=0,lineCommands=0,fillCommands=0,launchLatches=0;
};
}

// Scene-owned presentation resources. Snapshot and sockets are copied values.
// Initialize/Release/Reset/Update obey the existing NeonGridRenderer frame fence
// contract: previous submitted use has completed before mapped buffers change.
class NeonDepthEffects {
public:
    ~NeonDepthEffects() { Release(); }
    bool Initialize(cg2::DirectXCommon* dx,const std::string& texture="resources/white512x512.png");
    void Release();
    bool HasResources() const { return renderer_ != nullptr; }
    void ResetEncounter();
    // Call once per frame after Visual/model Update, using that same final camera.
    // frameId is a monotonically changing Scene frame identity, not a motion clock.
    bool Update(const neondepth::EffectsInput& input);
    // Scene HDR + current Scene DSV. Depth read, no write; preserves occlusion.
    void DrawAir();
    // Scene-compatible HDR NoDepth target owned/bound by root's local overlay or
    // existing ObjectPost capture. Same frozen VP. No DrawRangeSolid/HUD format.
    void DrawFloor();
    const neondepth::EffectsStats& GetStats() const { return stats_; }
    const neondepth::EffectGeometryFrame& GetGeometry() const { return builder_.GetFrame(); }
    const std::string& GetStatus() const { return status_; }
private:
    cg2::DirectXCommon* dx_=nullptr;
    std::unique_ptr<cg2::NeonGridRenderer> renderer_;
    neondepth::EffectGeometryBuilder builder_;
    neondepth::EffectsStats stats_{};
    cg2::Matrix4x4 viewProjection_{};
    uint64_t lastFrameId_=0;
    uint32_t airStart_=0,airCount_=0,floorStart_=0,floorCount_=0;
    bool framePrepared_=false;
    std::string status_;
};
