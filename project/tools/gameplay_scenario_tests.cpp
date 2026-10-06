#include "game/debug/GameplayScenarioSession.h"
#include <Windows.h>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <limits>

namespace {
void SettingsAndInput() {
    gameplaytest::Settings settings;
    std::string error;
    assert(gameplaytest::ValidateSettings(settings, error));
    settings.upgrades = {"shooter_damage_1", "shared_speed_1"};
    settings.enemyWave = {{"tutorial_target", {3,4,0}, 100}, {"special_target", {5,6,0}, 200}};
    settings.input = {{3,7,{{-1,1},{9,8,0},false,true}}};
    assert(gameplaytest::ValidateSettings(settings, error));
    assert(gameplaytest::InputAtFrame(settings, 3).dash && !gameplaytest::InputAtFrame(settings, 6).shoot);
    assert(gameplaytest::InputAtFrame(settings, 7).shoot); // Segment end is excluded.
    assert(!gameplaytest::InputAtFrame(settings, 2).dash);
    auto reject = [&](auto mutate) {
        auto invalid = settings; mutate(invalid);
        assert(!gameplaytest::ValidateSettings(invalid, error) && !error.empty());
    };
    reject([](auto& s) { s.fixedDeltaTime = std::numeric_limits<float>::quiet_NaN(); });
    reject([](auto& s) { s.frames = 119; });
    reject([](auto& s) { s.playerHp = 1000001; });
    reject([](auto& s) { s.playerHp = 0; });
    reject([](auto& s) { s.bossHp = 0; });
    reject([](auto& s) { s.room = "../arena"; });
    reject([](auto& s) { s.upgrades.push_back(s.upgrades.front()); });
    reject([](auto& s) { s.enemyWave.front().hp = 0; });
    reject([](auto& s) { s.enemyWave.front().position[0] = std::numeric_limits<float>::infinity(); });
    reject([](auto& s) { s.enemyWave.resize(129); });
    reject([](auto& s) { s.input.front().input.movement[0] = 1.01f; });
    reject([](auto& s) { s.input.front().input.aim[0] = 10001; });
    reject([](auto& s) { s.input.push_back(s.input.front()); });
    reject([](auto& s) { s.captureFrames = {120,120}; });
    reject([](auto& s) { s.captureFrames = {481}; });
    for (int value = 0; value <= static_cast<int>(gameplaytest::Scenario::PreviewLifecycle); ++value) {
        const auto scenario = static_cast<gameplaytest::Scenario>(value);
        gameplaytest::Scenario parsed{};
        assert(gameplaytest::ParseScenario(gameplaytest::Name(scenario), parsed) && parsed == scenario);
    }
}
gameplaytest::Snapshot Healthy(unsigned frame = 1) {
    gameplaytest::Snapshot snapshot;
    snapshot.frame = frame; snapshot.sceneEpoch = 1;
    snapshot.playerHp = snapshot.playerMaxHp = 10;
    snapshot.bossActive = true; snapshot.bossHp = 10; snapshot.bossEncounterGeneration = 1;
    return snapshot;
}
void FixedTimePresentationPolicy() {
    constexpr float normal = 1.0f / 60.0f;
    const float boundary = 0.95f / 60.0f;
    const float samples[] = {0.0f, normal * 0.2f, normal * 0.94f,
        std::nextafter(boundary, 0.0f), boundary, std::nextafter(boundary, normal), normal, normal * 1.1f};
    for (float combat : samples) {
        assert(gameplaytest::ScenarioShouldSnapTimeScale(combat, normal) == (combat * 60.0f > 0.95f));
        assert(gameplaytest::ScenarioGrayscaleEnabled(combat, normal, false, false) == (combat < normal * 0.98f));
    }
    for (float base : {1.0f / 240.0f, 1.0f / 15.0f}) {
        assert(gameplaytest::ScenarioShouldSnapTimeScale(base, base));
        assert(!gameplaytest::ScenarioShouldSnapTimeScale(base * 0.2f, base));
        assert(!gameplaytest::ScenarioShouldSnapTimeScale(base * 0.94f, base));
        assert(gameplaytest::ScenarioShouldSnapTimeScale(base * 0.96f, base));
        assert(!gameplaytest::ScenarioGrayscaleEnabled(base, base, false, false));
        assert(gameplaytest::ScenarioGrayscaleEnabled(base * 0.2f, base, false, false));
        // Freeze/Showcase getters deliberately retain 1/60 for existing Bloom.
        assert(!gameplaytest::ScenarioGrayscaleEnabled(normal, base, true, false));
        assert(!gameplaytest::ScenarioGrayscaleEnabled(normal, base, false, true));
    }
    // These inputs expose the old absolute-60-Hz comparisons' two failures.
    assert((1.0f / 240.0f) < normal * 0.98f);
    assert(((1.0f / 15.0f) * 0.3f) * 60.0f > 0.95f);
    assert(!gameplaytest::ScenarioShouldSnapTimeScale((1.0f / 15.0f) * 0.3f, 1.0f / 15.0f));
}
void RecordingPolicy() {
    auto continuous = [] {
        gameplaytest::Settings settings;
        settings.frames = 2520; settings.captureFrames.clear();
        settings.recording = {true, 121, 2400, 60, 12ull * 1024 * 1024 * 1024};
        return settings;
    };
    std::string error;
    auto settings = continuous();
    assert(gameplaytest::ValidateSettings(settings, error));
    assert(!gameplaytest::ShouldRecordCompletedFrame(settings, 120));
    assert(gameplaytest::ShouldRecordCompletedFrame(settings, 121));
    assert(gameplaytest::ShouldRecordCompletedFrame(settings, 2520));
    assert(!gameplaytest::ShouldRecordCompletedFrame(settings, 2521));
    auto reject = [&](auto mutate) {
        auto invalid = continuous(); mutate(invalid);
        assert(!gameplaytest::ValidateSettings(invalid, error) && !error.empty());
    };
    for (unsigned count : {0u, 2701u, (std::numeric_limits<unsigned>::max)()})
        reject([&](auto& s) { s.recording.frameCount = count; });
    for (unsigned first : {0u, 122u, (std::numeric_limits<unsigned>::max)()})
        reject([&](auto& s) { s.recording.firstFrame = first; });
    reject([](auto& s) { s.captureFrames = {121}; });
    reject([](auto& s) { s.captureFrames = {2520}; });
    reject([](auto& s) { s.recording.enabled = false; });
    settings.captureFrames = {0,120};
    assert(gameplaytest::ValidateSettings(settings, error));
    settings = continuous(); settings.frames = 3600;
    settings.recording.firstFrame = 901; settings.recording.frameCount = 2700;
    assert(gameplaytest::ValidateSettings(settings, error));
    settings = continuous(); settings.frames = 120;
    settings.recording.firstFrame = 120; settings.recording.frameCount = 1;
    assert(gameplaytest::ValidateSettings(settings, error));
    for (unsigned fps : {30u,60u,120u,240u}) {
        settings = continuous(); settings.fixedDeltaTime = 1.0f / static_cast<float>(fps);
        settings.recording.encodedFps = fps;
        assert(gameplaytest::ValidateSettings(settings, error));
    }
    for (unsigned fps : {0u,15u,59u,61u,241u,(std::numeric_limits<unsigned>::max)()})
        reject([&](auto& s) { s.recording.encodedFps = fps; });
    for (uint64_t bytes : {uint64_t(0),uint64_t(12ull*1024*1024*1024+1),(std::numeric_limits<uint64_t>::max)()})
        reject([&](auto& s) { s.recording.maxOutputBytes = bytes; });
    settings = continuous(); settings.recording.maxOutputBytes = 1;
    assert(gameplaytest::ValidateSettings(settings, error)); // Native preflight owns the actual size check.
    settings = {}; settings.captureFrames.clear();
    for (unsigned frame = 0; frame < 32; ++frame) settings.captureFrames.push_back(frame);
    assert(gameplaytest::ValidateSettings(settings, error));
    settings.captureFrames.push_back(32);
    assert(!gameplaytest::ValidateSettings(settings, error));
    for (int value = 0; value <= static_cast<int>(gameplaytest::Scenario::PreviewLifecycle); ++value) {
        settings = {}; settings.scenario = static_cast<gameplaytest::Scenario>(value);
        assert(gameplaytest::ValidateSettings(settings, error));
        assert(!gameplaytest::ShouldRecordCompletedFrame(settings, 120));
    }
}
void DeathAndEncounterInvariants() {
    gameplaytest::InvariantValidator legal;
    auto snapshot = Healthy(); legal.Observe(snapshot);
    snapshot.frame = 2; snapshot.bossHp = 0; snapshot.bossDead = true;
    legal.Observe(snapshot);
    snapshot.frame = 3; legal.Observe(snapshot);
    snapshot.frame = 4; snapshot.bossEncounterGeneration = 2; snapshot.bossDead = false;
    snapshot.bossHp = 10; snapshot.bossPosition[0] = 9;
    legal.Observe(snapshot); // A real new encounter may revive/reposition the actor.
    snapshot.frame = 5; snapshot.playerHp = 0; snapshot.playerDead = true;
    legal.Observe(snapshot);
    snapshot.frame = 6; legal.Observe(snapshot);
    snapshot.frame = 7; snapshot.sceneEpoch = 2; snapshot.playerHp = 10; snapshot.playerDead = false;
    snapshot.primaryAttacks = 0; snapshot.playerPosition[0] = 10;
    legal.Observe(snapshot); // Scene restart preserves frame order but permits new Player.
    assert(legal.GetErrors().empty() && legal.SawPlayerDeath());

    auto rejectAfter = [](gameplaytest::Snapshot before, gameplaytest::Snapshot after) {
        gameplaytest::InvariantValidator validator;
        validator.Observe(before); validator.Observe(after);
        assert(!validator.GetErrors().empty());
    };
    auto dead = Healthy(); dead.bossHp = 0; dead.bossDead = true;
    auto after = dead; after.frame = 2; after.bossShots = 1;
    rejectAfter(dead, after);
    after = dead; after.frame = 2; after.bossDead = false; after.bossHp = 10;
    rejectAfter(dead, after);
    dead = Healthy(); dead.playerDead = true; dead.playerHp = 0;
    after = dead; after.frame = 2; after.playerPosition[0] = 1;
    rejectAfter(dead, after);
    after = dead; after.frame = 2; after.primaryAttacks = 1;
    rejectAfter(dead, after);
    after = Healthy(2); after.playerPosition[1] = std::numeric_limits<float>::quiet_NaN();
    rejectAfter(Healthy(), after);
    after = Healthy(2); after.dissolveProgress = 1; after.visualHasResources = true;
    rejectAfter(Healthy(), after);
    after = Healthy(3); rejectAfter(Healthy(), after);
}
void RecordingSession(GameplayScenarioSession& session, const std::string& mode) {
    assert(session.IsActive() && session.GetSettings().recording.enabled);
    assert(session.GetSettings().frames == 120);
    const bool successful = mode == "recording_valid" || mode == "recording_late";
    if (mode == "recording_dimensions") assert(!session.BeginRecording(17,16));
    else if (mode == "recording_budget") assert(!session.BeginRecording(16,16));
    else {
        assert(session.BeginRecording(16,16));
        assert(session.BeginRecording(16,16)); // Rechecking the same native size must not allocate again.
        if (mode == "recording_resolution") assert(!session.BeginRecording(32,16));
    }
    session.BeginScene();
    assert(!session.ShouldRecordCompletedFrame());
    const float base = session.GetSettings().fixedDeltaTime;
    double gameplayElapsed = 0, presentationElapsed = 0;
    for (unsigned frame = 1; frame <= 120; ++frame) {
        auto snapshot = Healthy(frame);
        const float gameplayDt = frame % 2 ? base : base * .5f;
        const float presentationDt = frame % 3 ? base : 0;
        if (mode == "recording_clock" && frame == 1) {
            session.Record(snapshot, -base, base, 1);
            session.Record(snapshot, base, std::numeric_limits<float>::quiet_NaN(), 1);
            session.Record(snapshot, base * 2, base, 1);
            session.Record(snapshot, base, base, 2);
            assert(session.GetFrame() == 0 && session.GetErrors().size() == 1);
        }
        session.Record(snapshot, gameplayDt, presentationDt, gameplayDt / base);
        gameplayElapsed += gameplayDt; presentationElapsed += presentationDt;
        assert(session.GetFrame() == frame);
        snapshot.sceneEpoch = session.GetSceneEpoch();
        if (!session.ShouldRecordCompletedFrame()) continue;
        const unsigned sequence = session.GetRecordingSequenceFrame();
        assert(sequence == frame - session.GetSettings().recording.firstFrame);
        const auto metadata = session.MakeRecordingMetadata();
        assert(metadata.at("sequenceFrame") == sequence && metadata.at("sequenceRate") == 60);
        assert(!metadata.at("comparisonFreeze").get<bool>() && metadata.at("heldDrawCount") == 0);
        const auto& clocks = metadata.at("recordingClocks");
        assert(std::abs(clocks.at("baseElapsed").get<double>() - double(frame) * base) < 1e-6);
        assert(std::abs(clocks.at("gameplayElapsed").get<double>() - gameplayElapsed) < 1e-6);
        assert(std::abs(clocks.at("presentationElapsed").get<double>() - presentationElapsed) < 1e-6);
        assert(clocks.at("gameplayDelta") == gameplayDt && clocks.at("presentationDelta") == presentationDt);
        if (mode == "recording_dimensions" || mode == "recording_budget" ||
            (mode == "recording_incomplete" && frame == 120)) continue;
        char name[32]{}; std::snprintf(name, sizeof(name), "frame_%05u", sequence);
        // CPU contract test: byte counts are injected; no PNG/GPU evidence is produced.
        if (frame == session.GetSettings().recording.firstFrame) {
            if (mode == "recording_order") session.ReportRecordedFrame(sequence+1,name,snapshot,100,200);
            if (mode == "recording_bytes") session.ReportRecordedFrame(sequence,name,snapshot,
                session.GetSettings().recording.maxOutputBytes,1);
            if (mode == "recording_name") session.ReportRecordedFrame(sequence,"wrong",snapshot,100,200);
        }
        session.ReportRecordedFrame(sequence,name,snapshot,100,200);
        if (mode == "recording_duplicate" && sequence == 0)
            session.ReportRecordedFrame(sequence,name,snapshot,100,200);
    }
    if (mode == "recording_capture_limit") {
        for (unsigned i = 0; i < 64; ++i) session.ReportCapture("sparse",Healthy(120));
        assert(session.GetErrors().empty());
        session.ReportCapture("overflow",Healthy(120));
    }
    session.ReportDetail("cpuRecordingContract", {{"injectedByteCounts",true},{"gpuFramesProduced",false}});
    assert(session.ShouldFinish());
    if (successful) assert(session.GetErrors().empty());
    assert(session.Finish() == successful && session.Finish() == successful);
    assert(session.IsFinished() && !session.ShouldFinish());
    if (!successful) assert(!session.GetErrors().empty());
}
void ActualSession(const std::string& mode) {
    auto& session = GameplayScenarioSession::Get();
    if (mode == "disabled") {
        assert(!session.IsActive() && session.GetErrors().empty());
        return;
    }
    if (mode.rfind("recording_",0) == 0) { RecordingSession(session,mode); return; }
    if (mode == "invalid") {
        assert(!session.IsActive() && session.IsFinished() && !session.GetErrors().empty());
        MSG message{};
        assert(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) && message.wParam == 9);
        return;
    }
    if (!session.IsActive()) for (const auto& error : session.GetErrors()) std::cerr << error << '\n';
    assert(session.IsActive() && !session.IsFinished() && session.GetSettings().frames == 120);
    assert(session.GetSettings().upgrades.size() == 2 && session.GetSettings().enemyWave.size() == 2);
    assert(session.GetSettings().input.front().input.dash);
    session.BeginScene();
    assert(session.GetFrame() == 0 && session.GetSceneEpoch() == 1);
    for (unsigned frame = 0; frame < session.GetSettings().frames; ++frame) {
        auto snapshot = Healthy();
        if (frame >= 40 && frame < 50) { snapshot.playerHp = 0; snapshot.playerDead = true; }
        if (frame == 50) session.BeginScene();
        if (frame >= 70 && frame < 80) { snapshot.bossDead = true; snapshot.bossHp = 0; }
        if (frame >= 80) { snapshot.bossEncounterGeneration = 2; snapshot.bossPosition[0] = 3; }
        session.Record(snapshot);
        assert(session.GetFrame() == frame + 1);
        if (frame == 119) {
            snapshot.frame = session.GetFrame(); snapshot.sceneEpoch = session.GetSceneEpoch();
            session.ReportCapture("final", snapshot);
        }
    }
    session.ReportDetail("previewLifecycle", {{"initial", 15}, {"after", 15}, {"passed", true}});
    assert(session.ShouldFinish());
    const auto metadata = session.MakeMetadata();
    assert(metadata.at("frame") == 120 && metadata.at("sceneEpoch") == 2);
    assert(metadata.at("snapshot").at("bossEncounterGeneration") == 2);
    if (mode == "failure") {
        session.Fail("Injected adapter failure"); session.Fail("Injected adapter failure");
        session.ReportDetail("invalid", {{"nan", std::numeric_limits<double>::quiet_NaN()}});
        assert(session.GetErrors().size() == 2 && !session.Finish());
    } else {
        assert(session.GetErrors().empty() && session.Finish() && session.Finish());
    }
    assert(session.IsFinished() && !session.ShouldFinish());
}
}
int main(int argc, char** argv) {
    SettingsAndInput(); FixedTimePresentationPolicy(); RecordingPolicy(); DeathAndEncounterInvariants();
    ActualSession(argc > 1 ? argv[1] : "valid");
    std::cout << "PASS: scenario settings/input/invariants and actual manifest/session lifecycle\n";
}
