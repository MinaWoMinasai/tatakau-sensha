#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Pure scenario policy has no Windows, actor, renderer or JSON dependencies.
// Only its Developer adapter can apply these instructions to the live game.
namespace gameplaytest {
enum class Scenario {
    Shooter, Drone, Melee, ProjectileStress, EnemyStress, RivalBoss,
    PrototypeBoss, NeonBoss, BossDeath, PlayerRestart, StageTransition,
    ExpeditionTransition, PreviewLifecycle
};

inline const char* Name(Scenario scenario) {
    switch (scenario) {
    case Scenario::Shooter: return "shooter";
    case Scenario::Drone: return "drone";
    case Scenario::Melee: return "melee";
    case Scenario::ProjectileStress: return "projectile_stress";
    case Scenario::EnemyStress: return "enemy_stress";
    case Scenario::RivalBoss: return "rival_boss";
    case Scenario::PrototypeBoss: return "prototype_boss";
    case Scenario::NeonBoss: return "neon_boss";
    case Scenario::BossDeath: return "boss_death";
    case Scenario::PlayerRestart: return "player_restart";
    case Scenario::StageTransition: return "stage_transition";
    case Scenario::ExpeditionTransition: return "expedition_transition";
    case Scenario::PreviewLifecycle: return "preview_lifecycle";
    }
    return "invalid";
}

inline bool ParseScenario(const std::string& name, Scenario& scenario) {
    for (int index = 0; index <= static_cast<int>(Scenario::PreviewLifecycle); ++index) {
        const auto candidate = static_cast<Scenario>(index);
        if (name == Name(candidate)) { scenario = candidate; return true; }
    }
    return false;
}

struct ScriptedInput {
    std::array<float, 2> movement{};
    std::array<float, 3> aim{50.0f, 29.0f, 0.0f};
    bool shoot = true, dash = false;
};
struct InputSegment {
    unsigned firstFrame = 0, endFrame = 0; // Half-open range, in simulation frames.
    ScriptedInput input;
};
struct EnemyWaveEntry {
    std::string type;
    std::array<float, 3> position{};
    int hp = 100;
};
inline bool IsSafeIdentifier(const std::string& value) {
    if (value.empty() || value.size() > 96) return false;
    auto alphanumeric = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
    if (!alphanumeric(value.front()) && value.front() != '_') return false;
    return std::all_of(value.begin(), value.end(), [&](char c) { return alphanumeric(c) || c == '_' || c == '-' || c == '.'; });
}
struct Settings {
    Scenario scenario = Scenario::Shooter;
    uint32_t seed = 20261005u;
    float fixedDeltaTime = 1.0f / 60.0f;
    unsigned frames = 480;
    int playerStyle = -1; // -1 selects the scenario's usual combat style.
    // A live room reset starts invulnerable. Use player_restart to exercise real
    // lethal damage; explicit initial HP must be positive and fit actual max HP.
    int playerHp = -1, bossHp = 10000;
    unsigned enemyCount = 3, initialProjectiles = 0;
    std::vector<std::string> upgrades;
    std::string room = "arena";
    // Explicit records replace the count-generated wave. The live adapter
    // resolves these IDs against the existing authored content catalog.
    std::vector<EnemyWaveEntry> enemyWave;
    std::vector<InputSegment> input;
    std::vector<unsigned> captureFrames{120, 360};
};

inline bool ValidateSettings(const Settings& settings, std::string& error) {
    if (std::string(Name(settings.scenario)) == "invalid") { error = "Unknown scenario enum."; return false; }
    if (!std::isfinite(settings.fixedDeltaTime) || settings.fixedDeltaTime < 1.0f/240.0f || settings.fixedDeltaTime > 1.0f/15.0f)
        { error = "fixedDeltaTime must be finite and between 1/240 and 1/15."; return false; }
    if (settings.frames < 120 || settings.frames > 3600 || settings.enemyCount > 128 || settings.initialProjectiles > 480)
        { error = "Scenario duration or entity count is out of range."; return false; }
    if (settings.playerStyle < -1 || settings.playerStyle > 2 || settings.playerHp < -1 || settings.playerHp == 0 || settings.playerHp > 1000000 ||
        settings.bossHp < 1 || settings.bossHp > 1000000)
        { error = "Invalid class or HP setting."; return false; }
    if (!IsSafeIdentifier(settings.room)) { error = "Room must be a bounded content identifier."; return false; }
    if (settings.upgrades.size() > 42 || settings.enemyWave.size() > 128 || settings.input.size() > 128 || settings.captureFrames.size() > 32)
        { error = "Scenario instruction count is out of range."; return false; }
    for (size_t i = 0; i < settings.upgrades.size(); ++i) {
        if (!IsSafeIdentifier(settings.upgrades[i]) ||
            std::find(settings.upgrades.begin(), settings.upgrades.begin() + i, settings.upgrades[i]) != settings.upgrades.begin() + i)
            { error = "Upgrade IDs must be safe and unique."; return false; }
    }
    for (const auto& enemy : settings.enemyWave) {
        if (!IsSafeIdentifier(enemy.type) || enemy.hp < 1 || enemy.hp > 1000000)
            { error = "Invalid enemy-wave ID or HP."; return false; }
        for (float value : enemy.position) if (!std::isfinite(value) || std::abs(value) > 10000.0f)
            { error = "Enemy-wave position must be finite and bounded."; return false; }
    }
    unsigned previousEnd = 0;
    for (const auto& segment : settings.input) {
        if (segment.firstFrame < previousEnd || segment.firstFrame >= segment.endFrame || segment.endFrame > settings.frames)
            { error = "Input segments must be ordered non-overlapping half-open ranges."; return false; }
        for (float value : segment.input.movement) if (!std::isfinite(value) || std::abs(value) > 1.0f)
            { error = "Movement must be finite and in [-1,1]."; return false; }
        for (float value : segment.input.aim) if (!std::isfinite(value) || std::abs(value) > 10000.0f)
            { error = "Aim must be finite and bounded."; return false; }
        previousEnd = segment.endFrame;
    }
    for (size_t i = 0; i < settings.captureFrames.size(); ++i) {
        if (settings.captureFrames[i] > settings.frames || (i != 0 && settings.captureFrames[i] <= settings.captureFrames[i - 1]))
            { error = "Capture frames must be ordered, unique and within the duration."; return false; }
    }
    error.clear(); return true;
}

inline ScriptedInput InputAtFrame(const Settings& settings, unsigned frame) {
    for (const auto& segment : settings.input)
        if (frame >= segment.firstFrame && frame < segment.endFrame) return segment.input;
    ScriptedInput result;
    const bool boss = settings.scenario == Scenario::RivalBoss || settings.scenario == Scenario::PrototypeBoss ||
        settings.scenario == Scenario::NeonBoss || settings.scenario == Scenario::BossDeath;
    if (boss || settings.scenario == Scenario::PlayerRestart || settings.scenario == Scenario::PreviewLifecycle)
        result.shoot = false;
    if (!boss && settings.scenario != Scenario::PlayerRestart && settings.scenario != Scenario::PreviewLifecycle) {
        // Repeated square route exercises real movement, firing and wall resolution.
        const unsigned leg = (frame / 90u) % 4u;
        result.movement = leg == 0 ? std::array<float,2>{0.4f,0.0f} : leg == 1 ? std::array<float,2>{0.0f,0.4f}
            : leg == 2 ? std::array<float,2>{-0.4f,0.0f} : std::array<float,2>{0.0f,-0.4f};
        result.dash = frame == 210;
    }
    return result;
}

// Developer adapters call these only with validated fixed base time. Keep the
// existing 60 Hz operation at its exact float boundary for default scenarios.
inline bool ScenarioShouldSnapTimeScale(float combatDt, float baseDt) {
    if (baseDt == 1.0f / 60.0f) return combatDt * 60.0f > 0.95f;
    return combatDt / baseDt > 0.95f;
}
inline bool ScenarioGrayscaleEnabled(float combatDt, float baseDt, bool comparisonFreeze, bool showcase) {
    return !comparisonFreeze && !showcase && combatDt < baseDt * 0.98f;
}

struct Snapshot {
    unsigned frame = 0, sceneEpoch = 0;
    uint64_t bossEncounterGeneration = 0;
    float simulationTime = 0.0f;
    std::array<float,3> playerPosition{}, bossPosition{};
    int playerHp = 0, playerMaxHp = 0, playerStyle = 0, bossHp = 0;
    bool playerDead = false, bossActive = false, bossDead = false;
    unsigned primaryAttacks = 0, projectiles = 0, enemies = 0, activeAttacks = 0;
    unsigned playerProjectiles = 0, enemyProjectiles = 0, activeDrones = 0;
    int minimumEnemyHp = -1;
    unsigned bossShots = 0, bossDashes = 0, bossPhase = 0;
    unsigned descriptors = 0, visualCreates = 0, visualReleases = 0, visualConstantBuffers = 0;
    float dissolveProgress = 0.0f;
    bool visualHasResources = false;
    int flow = 0, expeditionPhase = 0;
    unsigned visitedNodes = 0;
    std::string classId, roomId, nodeId;
};

// Bounded observable state validation, deliberately excluding wall-clock/GPU timings
// from determinism. Scene-epoch changes are explicit restart boundaries.
class InvariantValidator {
public:
    void Observe(const Snapshot& snapshot) {
        auto fail = [&](const char* text) {
            if (errors_.size() < 32 && std::find(errors_.begin(), errors_.end(), text) == errors_.end()) errors_.emplace_back(text);
        };
        for (float value : snapshot.playerPosition) if (!std::isfinite(value)) fail("Non-finite player position.");
        for (float value : snapshot.bossPosition) if (!std::isfinite(value)) fail("Non-finite boss position.");
        if (!std::isfinite(snapshot.simulationTime) || !std::isfinite(snapshot.dissolveProgress)) fail("Non-finite simulation/presentation time.");
        if (snapshot.playerHp < 0 || snapshot.playerHp > snapshot.playerMaxHp || snapshot.playerMaxHp <= 0)
            fail("Player HP is out of bounds.");
        if (snapshot.playerDead != (snapshot.playerHp == 0)) fail("Player death/HP mismatch.");
        if (snapshot.projectiles > 4096 || snapshot.enemies > 128) fail("Runaway entity growth.");
        if (snapshot.dissolveProgress < 0 || snapshot.dissolveProgress > 1) fail("Dissolve is out of bounds.");
        if (snapshot.dissolveProgress == 1 && (snapshot.visualHasResources || snapshot.visualConstantBuffers != 0))
            fail("Finished boss Visual retained resources.");
        if (hasPrevious_) {
            if (snapshot.frame != previous_.frame + 1) fail("Illegal simulation-frame transition.");
            if (snapshot.sceneEpoch < previous_.sceneEpoch) fail("Scene epoch moved backwards.");
        }
        if (hasPrevious_ && snapshot.sceneEpoch == previous_.sceneEpoch) {
            if (previous_.playerDead && (!snapshot.playerDead || snapshot.primaryAttacks != previous_.primaryAttacks ||
                snapshot.playerPosition != previous_.playerPosition)) fail("Player gameplay continued after death.");
            if (snapshot.bossEncounterGeneration < previous_.bossEncounterGeneration) fail("Boss encounter generation moved backwards.");
            if (snapshot.bossEncounterGeneration == previous_.bossEncounterGeneration &&
                previous_.bossActive && previous_.bossDead && snapshot.bossActive &&
                (!snapshot.bossDead || snapshot.bossShots != previous_.bossShots || snapshot.bossDashes != previous_.bossDashes ||
                    snapshot.bossPosition != previous_.bossPosition)) fail("Boss gameplay continued after death.");
        }
        maximumProjectiles_ = (std::max)(maximumProjectiles_, snapshot.projectiles);
        maximumEnemies_ = (std::max)(maximumEnemies_, snapshot.enemies);
        maximumPrimaryAttacks_ = (std::max)(maximumPrimaryAttacks_, snapshot.primaryAttacks);
        maximumActiveAttacks_ = (std::max)(maximumActiveAttacks_, snapshot.activeAttacks);
        sawPlayerDeath_ |= snapshot.playerDead;
        previous_ = snapshot; hasPrevious_ = true;
    }
    const std::vector<std::string>& GetErrors() const { return errors_; }
    unsigned GetMaximumProjectiles() const { return maximumProjectiles_; }
    unsigned GetMaximumEnemies() const { return maximumEnemies_; }
    unsigned GetMaximumPrimaryAttacks() const { return maximumPrimaryAttacks_; }
    unsigned GetMaximumActiveAttacks() const { return maximumActiveAttacks_; }
    bool SawPlayerDeath() const { return sawPlayerDeath_; }

private:
    Snapshot previous_;
    std::vector<std::string> errors_;
    unsigned maximumProjectiles_ = 0, maximumEnemies_ = 0, maximumPrimaryAttacks_ = 0, maximumActiveAttacks_ = 0;
    bool hasPrevious_ = false, sawPlayerDeath_ = false;
};
} // namespace gameplaytest
