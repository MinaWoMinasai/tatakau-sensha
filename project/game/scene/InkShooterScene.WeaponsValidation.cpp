#include "InkShooterScene.h"
#include <cmath>

ink::Controls InkShooterScene::ReadWeaponsValidationControls(float dt) {
    replayTime_ += dt;
    const float t = replayTime_;
    const float previous = t - dt;
    const int section = t < 3 ? 0 : t < 7 ? 1 : t < 11 ? 2 : t < 16 ? 3 :
        t < 20 ? 4 : t < 24 ? 5 : t < 30 ? 6 : t < 35 ? 7 : 8;

    if (section != replaySection_) {
        if (section < 8) {
            // Equip asks for trigger release; Reset then starts each independent
            // capture scenario without carrying that latch or previous arrows.
            EquipCatalogWeapon(section == 0 ? "splattershot" : "tri_stringer");
            Reset();
            if (section == 7) {
                // The wall is z=14. Move into range, beyond the metal panel at
                // z=4, so the explicit scene aim target reaches an inkable wall.
                simulation_.SetPlayerPosition({0, 0, 7});
            }
        }
        cameraReady_ = false;
        replaySection_ = section;
    }

    ink::Controls c{};
    yaw_ = 0;
    pitch_ = section == 7 ? -0.06f : 0.19f;
    auto crossed = [&](float time) { return t >= time && previous < time; };
    if (section == 0) {
        c.fire = true;
        c.jump = crossed(1.4f);
    } else if (section == 1) {
        // Release before the native minimum charge; the simulation queues a tap.
        c.fire = std::fmod(t - 3.0f, 0.6f) < 0.12f;
    } else if (section == 2) {
        c.fire = std::fmod(t - 7.0f, 1.0f) < 0.6f;
    } else if (section == 3) {
        c.fire = std::fmod(t - 11.0f, 2.1f) < 1.3f;
    } else if (section == 4) {
        c.fire = t < 17.25f;
        // MovePlayer runs before the release, producing the airborne vertical fan.
        c.jump = crossed(17.25f);
    } else if (section == 5) {
        // Cancel while the trigger is still held, then release it fully before
        // the next charge. Both cancellations must spend no ink and emit no arrows.
        c.fire = t < 21.1f || (t >= 22.0f && t < 23.3f);
        c.swim = (t >= 20.8f && t < 21.5f) || t >= 23.0f;
    } else if (section == 6) {
        // Scene.cpp overrides the target with Dummy().position after UpdateCamera.
        c.fire = std::fmod(t - 24.0f, 2.1f) < 1.3f;
    } else if (section == 7) {
        // Scene.cpp similarly supplies the inkable wall target {0,2,14}.
        c.fire = std::fmod(t - 30.0f, 2.1f) < 1.3f;
    }

    auto captureAt = [&](float time, const char* name) {
        if (crossed(time)) RequestCapture(name);
    };
    captureAt(0.8f, "01_shooter_ground_reticle");
    captureAt(1.6f, "02_shooter_jump_reticle");
    captureAt(3.38f, "03_stringer_tap");
    captureAt(7.55f, "04_stringer_first_charge");
    captureAt(8.0f, "05_stringer_embedded_floor");
    captureAt(8.6f, "06_stringer_floor_explosion");
    captureAt(12.25f, "07_stringer_full_charge");
    captureAt(12.4f, "08_stringer_full_flight");
    captureAt(17.32f, "09_stringer_vertical_flight");
    captureAt(17.6f, "10_stringer_vertical_paint");
    captureAt(20.65f, "11_stringer_before_cancel");
    captureAt(21.0f, "12_stringer_squid_cancel");
    captureAt(23.2f, "13_stringer_second_cancel");
    captureAt(25.5f, "14_stringer_dummy_full_hit");
    captureAt(31.6f, "15_stringer_wall_embedded");
    captureAt(32.15f, "16_stringer_wall_explosion");

    if (section == 8) {
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
