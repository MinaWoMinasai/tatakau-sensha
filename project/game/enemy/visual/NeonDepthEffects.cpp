#include "NeonDepthEffects.h"
#include <stdexcept>

bool NeonDepthEffects::Initialize(cg2::DirectXCommon* dx,const std::string& texture) {
    if(renderer_) return dx_==dx;
    if(!dx) return false;
    static_assert(neondepth::kMaximumEffectVertices<cg2::NeonGridRenderer::kMaxVertices);
    try {
        auto renderer=std::make_unique<cg2::NeonGridRenderer>();
        renderer->Initialize(dx,texture);
        renderer->SetLineStyle(.35f,1.0f);
        renderer_=std::move(renderer); dx_=dx;
        ++stats_.resourceCreates;
        status_="Depth FX / one owned immutable frame buffer";
        ResetEncounter(); return true;
    } catch(const std::exception& error) {
        status_=std::string(error.what()).substr(0,256);
        ++stats_.resourceFailures;
        return false;
    }
}
void NeonDepthEffects::Release() {
    if(renderer_) { renderer_.reset(); ++stats_.resourceReleases; }
    dx_=nullptr;
    ResetEncounter();
    status_="Depth FX / released";
}
void NeonDepthEffects::ResetEncounter() {
    builder_.Reset(); framePrepared_=false; lastFrameId_=0;
    airStart_=airCount_=floorStart_=floorCount_=0;
    stats_.vertices=stats_.airVertices=stats_.floorVertices=stats_.lineCommands=stats_.fillCommands=stats_.launchLatches=0;
}
bool NeonDepthEffects::Update(const neondepth::EffectsInput& input) {
    if(!renderer_) return false;
    if(framePrepared_ && input.frameId==lastFrameId_) { ++stats_.duplicateUpdates; return false; }
    // Exactly one BeginFrame and one population of this separate upload buffer.
    // Draw never clears, repopulates or grows it, including two render passes.
    renderer_->BeginFrame(); lastFrameId_=input.frameId; framePrepared_=true;
    airStart_=airCount_=floorStart_=floorCount_=0;
    stats_.vertices=stats_.airVertices=stats_.floorVertices=stats_.lineCommands=stats_.fillCommands=0;
    ++stats_.updates;
    // Always replace the public CPU frame, even when VP is rejected, so an
    // invalid frame cannot expose the previous frame's diagnostic geometry.
    const bool geometryValid=builder_.Build(input);
    stats_.launchLatches=builder_.GetLaunchCount();
    if(!neondepth::Finite(input.viewProjection) || !geometryValid) {
        ++stats_.invalidFrames;
        if(builder_.GetFrame().capacityReached) ++stats_.capacityRejected;
        status_="Depth FX invalid/capacity input; frame geometry suppressed.";
        return false;
    }
    viewProjection_=input.viewProjection;
    const auto& geometry=builder_.GetFrame();
    stats_.lineCommands=geometry.airCount+geometry.floorCount;
    stats_.fillCommands=geometry.fillCount;
    stats_.launchLatches=builder_.GetLaunchCount();
    airStart_=renderer_->GetVertexCount();
    for(unsigned index=0;index<geometry.airCount;++index) {
        const auto& line=geometry.air[index];
        renderer_->QueueCameraFacingLine(line.start,line.end,line.width,line.color,input.cameraForward);
    }
    airCount_=renderer_->GetVertexCount()-airStart_;
    floorStart_=renderer_->GetVertexCount();
    // Weak fill first, then fixed shape edges and core target, in the same range.
    for(unsigned index=0;index<geometry.fillCount;++index) {
        const auto& fill=geometry.fills[index];
        if(fill.rectangle) {
            constexpr float inverseRoot2=.70710678118f;
            renderer_->QueueBillboardRegularPolygonFill(fill.center,4,1,cg2::pi*.25f,
                {fill.size.x*inverseRoot2,fill.size.y*inverseRoot2},fill.color,fill.right,fill.up);
        } else renderer_->QueueBillboardDisc(fill.center,fill.radius,fill.color,fill.right,fill.up,neondepth::kEffectCircleSteps);
    }
    // Core is queued after warning/active lines to remain readable on overlaps.
    for(unsigned pass=0;pass<2;++pass) for(unsigned index=0;index<geometry.floorCount;++index) {
        const auto& line=geometry.floor[index];
        if((line.cue==neondepth::Cue::Core)!=(pass==1)) continue;
        renderer_->QueueLine(line.start,line.end,line.width,line.color);
    }
    floorCount_=renderer_->GetVertexCount()-floorStart_;
    stats_.vertices=renderer_->GetVertexCount(); stats_.airVertices=airCount_; stats_.floorVertices=floorCount_;
    if(stats_.vertices>neondepth::kMaximumEffectVertices) {
        ++stats_.capacityRejected;
        airCount_=floorCount_=0;
        status_="Depth FX vertex budget exceeded; frame suppressed."; return false;
    }
    status_="Depth FX / frozen air and floor ranges ready";
    return true;
}
void NeonDepthEffects::DrawAir() {
    if(!renderer_ || !framePrepared_ || !airCount_) return;
    if(!dx_->HasCurrentDSV()) { ++stats_.depthBindingRejected; return; }
    renderer_->DrawRange(airStart_,airCount_,viewProjection_); ++stats_.airDraws;
}
void NeonDepthEffects::DrawFloor() {
    if(!renderer_ || !framePrepared_ || !floorCount_) return;
    if(dx_->HasCurrentDSV()) { ++stats_.depthBindingRejected; return; }
    renderer_->DrawRange(floorStart_,floorCount_,viewProjection_,true); ++stats_.floorDraws;
}
