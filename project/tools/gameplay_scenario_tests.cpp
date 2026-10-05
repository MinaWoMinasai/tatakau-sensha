#include "game/debug/GameplayScenarioSession.h"
#include <Windows.h>
#include <cassert>
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
void ActualSession(const std::string& mode) {
    auto& session = GameplayScenarioSession::Get();
    if (mode == "disabled") {
        assert(!session.IsActive() && session.GetErrors().empty());
        return;
    }
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
    SettingsAndInput(); FixedTimePresentationPolicy(); DeathAndEncounterInvariants();
    ActualSession(argc > 1 ? argv[1] : "valid");
    std::cout << "PASS: scenario settings/input/invariants and actual manifest/session lifecycle\n";
}
