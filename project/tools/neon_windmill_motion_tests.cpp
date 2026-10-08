#include "game/scene/NeonWindmillMotion.h"
#include <iostream>
#include <stdexcept>
#include <limits>

/// @brief 動きの不変条件が崩れた場合にテストを失敗させる。
void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main()
{
    using namespace neonwindmill;
    try {
        for (int sample = 0; sample <= 3200; ++sample) {
            const auto p = Evaluate(sample * .005);
            Check(std::isfinite(p.angle) && std::isfinite(p.center.z), "Nonfinite generated pose");
            Check(p.visibility >= 0 && p.visibility <= 1 && p.recognition >= 0 && p.recognition <= 1, "Unbounded transition");
            for (std::size_t i = 0; i < 4; ++i) {
                const float x = p.hands[i].x - p.center.x, y = p.hands[i].y - p.center.y;
                Check(std::abs(x * x + y * y - 1.65f * 1.65f) < 1e-4f, "Circular orbit radius changed");
                Check(p.hands[i].z == p.center.z, "Plate left fixed XY plane");
                const auto& opposite = p.hands[(i + 2) % 4];
                Check(std::abs(p.hands[i].x + opposite.x - 2 * p.center.x) < 1e-4f &&
                          std::abs(p.hands[i].y + opposite.y - 2 * p.center.y) < 1e-4f,
                      "Opposite arms lost symmetry");
            }
        }
        const auto locked = Evaluate(13.3), recognized = Evaluate(13.95);
        Check(recognized.phase == Phase::Recognize && recognized.recognition == 1, "Recognition glyph switch missed");
        Check(std::abs(std::sin(Evaluate(13.4).angle)) < 1e-4f, "Stopped rotor is not cardinal");
        Check(std::abs(locked.angle - recognized.angle) < .003f, "Rotation continued after lock");
        Check(Evaluate(15.7).visibility == 0, "Reset exposed the trajectory discontinuity");
        Check(std::abs(Evaluate(-.25).seconds - 15.75f) < 1e-5f, "Negative replay time did not wrap");
        Check(Evaluate(std::numeric_limits<double>::infinity()).seconds == 0, "Invalid time was not sanitized");
        for (double boundary : {7., 12., 13.4, 14.2, 14.9}) {
            const auto a = Evaluate(boundary - .0001), b = Evaluate(boundary + .0001);
            Check(std::abs(a.angle - b.angle) < .002f && std::abs(a.center.z - b.center.z) < .001f, "Phase boundary jumps");
        }
        std::cout << "PASS: orbit, fixed planes, cardinal stop, recognition, reset, time wrapping and continuity\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
