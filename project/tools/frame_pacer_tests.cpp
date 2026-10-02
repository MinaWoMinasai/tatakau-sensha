#include "../DirectX/engine/commom/FramePacer.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>

using cg2::FramePacer;

namespace {
double ThreadCpuMs() {
    FILETIME created{}, exited{}, kernel{}, user{};
    assert(GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user));
    const auto ticks = [](const FILETIME& value) {
        return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    };
    return static_cast<double>(ticks(kernel) + ticks(user)) / 10'000.0;
}

void CheckCappedStats(const FramePacer::Stats& stats) {
    // Each interval must be a full 1/60 second, even in the old 60--65 FPS gap.
    assert(stats.frameMs >= 16.6665f);
    assert(stats.preLimitMs >= 0.0f && stats.waitMs >= 0.0f);
    assert(std::abs(stats.frameMs - stats.preLimitMs - stats.waitMs) < 0.001f);
}
}

int main() {
    FramePacer pacer;
    assert(pacer.IsEnabled());
    pacer.Initialize();
    std::cout << std::fixed << std::setprecision(4)
        << "highResolutionTimer=" << pacer.IsHighResolutionTimer() << '\n';

    constexpr int samples = 120;
    double total = 0.0;
    double minimum = 1e9;
    double maximum = 0.0;
    const double cpuStart = ThreadCpuMs();
    pacer.Reset();
    for (int index = 0; index < samples; ++index) {
        pacer.Wait();
        const auto stats = pacer.GetStats();
        CheckCappedStats(stats);
        total += stats.frameMs;
        if (stats.frameMs < minimum) { minimum = stats.frameMs; }
        if (stats.frameMs > maximum) { maximum = stats.frameMs; }
    }
    const double cpuMs = ThreadCpuMs() - cpuStart;
    std::cout << "idle: samples=" << samples << " meanMs=" << total / samples
        << " fps=" << samples * 1000.0 / total << " minMs=" << minimum
        << " maxMs=" << maximum << " threadCpuMs=" << cpuMs
        << " cpuPercentOfOneCore=" << cpuMs / total * 100.0 << '\n';

    int waitedGapSamples = 0;
    for (int index = 0; index < 8; ++index) {
        pacer.Reset();
        const auto workEnd = FramePacer::Clock::now() + std::chrono::milliseconds(16);
        while (FramePacer::Clock::now() < workEnd) { YieldProcessor(); }
        pacer.Wait();
        const auto stats = pacer.GetStats();
        CheckCappedStats(stats);
        if (stats.preLimitMs < 16.5f && stats.waitMs > 0.15f) { ++waitedGapSamples; }
    }
    assert(waitedGapSamples > 0);
    std::cout << "oldBypassGap: waitedSamples=" << waitedGapSamples << "/8\n";

    // Slow frames cannot be accelerated, and must not permit a following burst.
    pacer.Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(24));
    pacer.Wait();
    assert(pacer.GetStats().preLimitMs >= 23.0f);
    CheckCappedStats(pacer.GetStats());
    pacer.Wait();
    CheckCappedStats(pacer.GetStats());
    assert(pacer.GetStats().waitMs > 1.0f);

    pacer.SetEnabled(false);
    assert(!pacer.IsEnabled());
    for (int index = 0; index < 8; ++index) {
        pacer.Wait();
        assert(pacer.GetStats().waitMs == 0.0f);
        assert(pacer.GetStats().overshootMs == 0.0f);
    }
    pacer.SetEnabled(true);
    pacer.Wait();
    CheckCappedStats(pacer.GetStats());
    pacer.Initialize();
    pacer.Wait();
    CheckCappedStats(pacer.GetStats());

    std::cout << "Frame pacer tests passed: 60 FPS ceiling, old-gap wait, slow-frame recovery, toggles, reinitialize.\n";
}
