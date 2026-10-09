#include "game/session/GameplaySystems.h"

namespace gameplay {
GameWorld::GameWorld(bool prototypeRun, bool expeditionRun)
    : performanceMonitor(std::make_unique<PerformanceMonitor>(*this)), sessionBootstrap(std::make_unique<SessionBootstrap>(*this)),
      combatFramePipeline(std::make_unique<CombatFramePipeline>(*this)), combatTutorial(std::make_unique<CombatTutorial>(*this)),
      combatFlow(std::make_unique<CombatFlow>(*this)), classConfigWatcher(std::make_unique<ClassConfigWatcher>(*this)),
      gameplayQueries(std::make_unique<GameplayQueries>(*this)), gameplayRenderer(std::make_unique<GameplayRenderer>(*this)),
      combatEffects(std::make_unique<CombatEffects>(*this)), stageNeonRenderer(std::make_unique<StageNeonRenderer>(*this)),
      playerAttackEffects(std::make_unique<PlayerAttackEffects>(*this)), actorNeonRenderer(std::make_unique<ActorNeonRenderer>(*this)),
      resourceNeonRenderer(std::make_unique<ResourceNeonRenderer>(*this)), levelRuntime(std::make_unique<LevelRuntime>(*this)),
      gameplaySettings(std::make_unique<GameplaySettings>(*this)), gameplayHud(std::make_unique<GameplayHud>(*this)),
      gameplayEditor(std::make_unique<GameplayEditor>(*this)), titleDemoController(std::make_unique<TitleDemoController>(*this)),
      arenaRunController(std::make_unique<ArenaRunController>(*this)), runVisualSettings(std::make_unique<RunVisualSettings>(*this)),
      expeditionController(std::make_unique<ExpeditionController>(*this)),
      expeditionMapController(std::make_unique<ExpeditionMapController>(*this)),
      expeditionBuildSelection(std::make_unique<ExpeditionBuildSelection>(*this)),
      expeditionExperience(std::make_unique<ExpeditionExperience>(*this)),
      expeditionBalanceEditor(std::make_unique<ExpeditionBalanceEditor>(*this)),
      expeditionAuthoring(std::make_unique<ExpeditionAuthoring>(*this)), bossPresentation(std::make_unique<BossPresentation>(*this)),
      depthEncounter(std::make_unique<DepthEncounter>(*this)), combatValidation(std::make_unique<CombatValidation>(*this)),
      specialValidation(std::make_unique<SpecialValidation>(*this)), experienceValidation(std::make_unique<ExperienceValidation>(*this)),
      gameplayScenarioRunner(std::make_unique<GameplayScenarioRunner>(*this)), depthValidation(std::make_unique<DepthValidation>(*this)),
      routeReplay(std::make_unique<RouteReplay>(*this))
{
    run.expeditionRun_ = expeditionRun;
    run.prototypeRun_ = prototypeRun || expeditionRun;
}

GameWorld::~GameWorld()
{
    depthEncounter->RestoreNeonDepthCamera();
    run.tankExpeditionAudio_.Shutdown();
    ExpEnemy::SetEnemyKillCallback(nullptr);
    ExpEnemy::SetPlayerDefeatCallback(nullptr);
    ExpEnemy::SetShapeNeonRenderMode(0);
}
} // namespace gameplay
