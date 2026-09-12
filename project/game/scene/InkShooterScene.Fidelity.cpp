#include "InkShooterScene.h"
#include <cmath>

ink::Controls InkShooterScene::ReadFidelityControls(float dt) {
    replayTime_ += dt;
    const float t = replayTime_;
    const float previousTime = t - dt;
    const int section = t < 1.5f ? 0 : t < 3.0f ? 1 : t < 14.1f ? 2 :
        t < 15.0f ? 3 : t < 18.0f ? 4 : t < 21.0f ? 5 : t < 27.0f ? 6 : 7;

    if (section != replaySection_) {
        // Reset only independent scenarios. In particular, the exhausted tank
        // survives the dry-squid and own-ink refill scenarios at 14.1 and 15 s.
        if (section == 0 || section == 2 || section == 5 || section == 6) Reset();
        if (section == 3) {
            const auto& floor = simulation_.Surfaces().at(0);
            // The ellipse contains all four corners of this finite floor tile.
            // Team 0 is the existing shared CPU/GPU erase operation.
            simulation_.Paint({0, floor.width * 0.5f, floor.height * 0.5f,
                floor.width, floor.height, 0, 0});
            simulation_.SetPlayerPosition({0, 0, -8});
        }
        if (section == 4) {
            const auto& floor = simulation_.Surfaces().at(0);
            const ink::Vec3 center = {0, 0, 0};
            const auto local = center - floor.origin;
            simulation_.Paint({0, ink::Dot(local, floor.u), ink::Dot(local, floor.v),
                3.5f, 15.0f, 0, 1});
            simulation_.SetPlayerPosition({0, 0, -8});
        }
        if (section == 5) {
            const auto& floor = simulation_.Surfaces().at(0);
            const ink::Vec3 center = {0, 0, -6};
            const auto local = center - floor.origin;
            simulation_.Paint({0, ink::Dot(local, floor.u), ink::Dot(local, floor.v),
                2.8f, 8.0f, 0, 2});
        }
        cameraReady_ = false;
        replaySection_ = section;
    }

    ink::Controls c{};
    yaw_ = 0;
    pitch_ = 0.19f;
    if (section == 0) {
        c.swim = true;
        c.moveZ = 1;
    } else if (section == 1) {
        c.swim = true;
        c.moveZ = 1;
        c.jump = t >= 1.5f && previousTime < 1.5f;
    } else if (section == 2) {
        // A genuine single-shot footprint is photographed before continuous
        // fire resumes. The remaining interval still empties the 108-shot tank.
        c.fire = t < 3.075f || t >= 3.36f;
    } else if (section == 3) {
        c.swim = true;
    } else if (section == 4) {
        c.swim = true;
        c.moveZ = 1;
        c.jump = t >= 16.2f && previousTime < 16.2f;
    } else if (section == 5) {
        c.moveZ = 1;
        c.jump = t >= 19.0f && previousTime < 19.0f;
    } else if (section == 6) {
        // A press every 0.18 seconds with 0.09 seconds held tests that the
        // emission pattern continues across trigger releases instead of resetting.
        c.fire = std::fmod(t - 21.0f, 0.18f) < 0.09f;
    }

    auto captureAt = [&](float time, const char* name) {
        if (t >= time && previousTime < time) RequestCapture(name);
    };
    captureAt(0.8f, "01_dry_squid");
    captureAt(1.8f, "02_air_squid");
    captureAt(3.3f, "03_single_shot");
    captureAt(4.1f, "04_burst_fire");
    captureAt(14.0f, "05_empty_fire");
    captureAt(14.25f, "06_empty_dry_squid");
    captureAt(16.4f, "07_swim_jump");
    captureAt(19.1f, "08_enemy_ink");
    captureAt(26.5f, "09_tap_fire");

    if (section == 7) {
        replay_ = false;
        replayLog_.flush();
        WriteGpuDiagnostics();
        debug_ = true;
        SetCaptured(false);
    }
    c.yaw = yaw_;
    c.pitch = pitch_;
    c.aimPoint = aimPoint_;
    return c;
}
