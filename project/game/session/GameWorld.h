#pragma once
#include "game/session/state/TitleDemoState.h"
#include "game/session/state/RunSessionState.h"
#include "game/session/state/WorldResources.h"
#include "game/session/state/CombatUiState.h"
#include "game/session/state/NeonPresentationState.h"
#include "game/session/state/ValidationState.h"

namespace gameplay {
class PerformanceMonitor;
class SessionBootstrap;
class CombatFramePipeline;
class CombatTutorial;
class CombatFlow;
class ClassConfigWatcher;
class GameplayQueries;
class GameplayRenderer;
class CombatEffects;
class StageNeonRenderer;
class PlayerAttackEffects;
class ActorNeonRenderer;
class ResourceNeonRenderer;
class LevelRuntime;
class GameplaySettings;
class GameplayHud;
class GameplayEditor;
class TitleDemoController;
class ArenaRunController;
class RunVisualSettings;
class ExpeditionController;
class ExpeditionMapController;
class ExpeditionBuildSelection;
class ExpeditionExperience;
class ExpeditionBalanceEditor;
class ExpeditionAuthoring;
class BossPresentation;
class DepthEncounter;
class CombatValidation;
class SpecialValidation;
class ExperienceValidation;
class GameplayScenarioRunner;
class DepthValidation;
class RouteReplay;

/// @brief 1回のプレイが所有する共有資源と実行状態。処理は各担当クラスが行う。
struct GameWorld {
    /// @brief 全担当を同じワールドへ接続する。
    GameWorld(bool prototypeRun, bool expeditionRun);
    /// @brief 担当クラスと共有資源を解放する。
    ~GameWorld();
    GameWorld(const GameWorld&) = delete;
    GameWorld& operator=(const GameWorld&) = delete;
    TitleDemoState demo;
    RunSessionState run;
    WorldResources resources;
    CombatUiState combat;
    NeonPresentationState presentation;
    ValidationState validation;

    std::unique_ptr<PerformanceMonitor> performanceMonitor;
    std::unique_ptr<SessionBootstrap> sessionBootstrap;
    std::unique_ptr<CombatFramePipeline> combatFramePipeline;
    std::unique_ptr<CombatTutorial> combatTutorial;
    std::unique_ptr<CombatFlow> combatFlow;
    std::unique_ptr<ClassConfigWatcher> classConfigWatcher;
    std::unique_ptr<GameplayQueries> gameplayQueries;
    std::unique_ptr<GameplayRenderer> gameplayRenderer;
    std::unique_ptr<CombatEffects> combatEffects;
    std::unique_ptr<StageNeonRenderer> stageNeonRenderer;
    std::unique_ptr<PlayerAttackEffects> playerAttackEffects;
    std::unique_ptr<ActorNeonRenderer> actorNeonRenderer;
    std::unique_ptr<ResourceNeonRenderer> resourceNeonRenderer;
    std::unique_ptr<LevelRuntime> levelRuntime;
    std::unique_ptr<GameplaySettings> gameplaySettings;
    std::unique_ptr<GameplayHud> gameplayHud;
    std::unique_ptr<GameplayEditor> gameplayEditor;
    std::unique_ptr<TitleDemoController> titleDemoController;
    std::unique_ptr<ArenaRunController> arenaRunController;
    std::unique_ptr<RunVisualSettings> runVisualSettings;
    std::unique_ptr<ExpeditionController> expeditionController;
    std::unique_ptr<ExpeditionMapController> expeditionMapController;
    std::unique_ptr<ExpeditionBuildSelection> expeditionBuildSelection;
    std::unique_ptr<ExpeditionExperience> expeditionExperience;
    std::unique_ptr<ExpeditionBalanceEditor> expeditionBalanceEditor;
    std::unique_ptr<ExpeditionAuthoring> expeditionAuthoring;
    std::unique_ptr<BossPresentation> bossPresentation;
    std::unique_ptr<DepthEncounter> depthEncounter;
    std::unique_ptr<CombatValidation> combatValidation;
    std::unique_ptr<SpecialValidation> specialValidation;
    std::unique_ptr<ExperienceValidation> experienceValidation;
    std::unique_ptr<GameplayScenarioRunner> gameplayScenarioRunner;
    std::unique_ptr<DepthValidation> depthValidation;
    std::unique_ptr<RouteReplay> routeReplay;
};
} // namespace gameplay
