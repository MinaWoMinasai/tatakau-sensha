#include "game/session/SessionBootstrap.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "GameStartMode.h"
#include "game/ui/TankCombatNeonGeometry.h"
#include "game/effects/TankSpecialNeonGeometry.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

void SessionBootstrap::Initialize()
{
    myEngine::StartupTrace::Scope scope(world_.demo.titleDemo_ ? "GameScene.Initialize.Demo" : "GameScene.Initialize.Play");
    while (!InitializeStep()) {
    }
}

bool SessionBootstrap::InitializeStep()
{
    myEngine::ModelManager::GetInstance()->PumpPreloads();
    if (!myEngine::ModelManager::GetInstance()->ArePreloadsReady() || !myEngine::TextureManager::GetInstance()->ArePreloadsReady()) {
        return false;
    }
    if (!task_) {
        task_ = Build();
    }
    myEngine::StartupTrace::Scope scope("GameScene.InitializeStep");
    return task_.Advance();
}

myEngine::InitializationTask SessionBootstrap::Build()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    wchar_t bossTest[8]{};
    world_.resources.neonBossAutoTest_ = GetEnvironmentVariableW(L"MYENGINE_NEON_BOSS_AUTOTEST", bossTest, 8) > 0 && bossTest[0] == L'1';
    if (!world_.demo.titleDemo_ && GameplayScenarioSession::Get().IsActive()) {
        myEngine::rng.seed(GameplayScenarioSession::Get().GetSettings().seed);
        myEngine::ParticleManager::GetInstance()->SetPresentationSeed(GameplayScenarioSession::Get().GetSettings().seed);
    }
#endif
    myEngine::StartupTrace::Mark(world_.demo.titleDemo_ ? "demo.initialize_begin" : "gameplay.initialize_begin");

    world_.resources.worldTransform_ = myEngine::InitWorldTransform();

    world_.resources.input_ = myEngine::Input::GetInstance();
    world_.combatTutorial->LoadTutorialConfig();
    world_.depthEncounter->LoadNeonDepthConfig();
    world_.combat.tutorialConfig_.enabled = !world_.run.prototypeRun_ && GameStartSession::GetMode() == GameStartMode::Tutorial;
    world_.combat.screenEffectDirector_.LoadConfig("resources/configs/screenEffects.json");

    world_.resources.debugCamera = std::make_unique<myEngine::DebugCamera>();

    world_.resources.camera = std::make_unique<myEngine::Camera>();
    {
        // 再現可能なBloomの比較キャプチャ（シナリオの同じフレームで、1回の実行につき1つの表示）。
        char view[8]{};
        const DWORD length = GetEnvironmentVariableA("MYENGINE_BLOOM_VIEW", view, sizeof(view));
        if (length > 0 && length < sizeof(view) && view[0] >= '0' && view[0] <= '7' && view[1] == '\0') {
            world_.presentation.bloomView_ = view[0] - '0';
        }
    }

    world_.resources.camera->SetFovY(arenacamera::kFovY);
    world_.resources.camera->SetTranslate(myEngine::Vector3(17.0f, 21.0f, arenacamera::CameraZ() * 1.45f));

    myEngine::Object3dCommon::GetInstance()->SetDefaultCamera(world_.resources.camera.get());
    myEngine::Object3dCommon::GetInstance()->SetDebugDefaultCamera(world_.resources.debugCamera.get());
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (!world_.demo.titleDemo_) {
        world_.resources.neonSkinnedPreview_ = std::make_unique<NeonSkinnedPreview>();
        world_.resources.neonSkinnedPreview_->Initialize(world_.resources.camera.get(), world_.resources.debugCamera.get());
    }
#endif

    co_await std::suspend_always{};
    {
        myEngine::StartupTrace::Mark("GameScene.ObjectPostEffects");
        world_.resources.playerPostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.playerPostEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                       myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            myEngine::BloomParam& playerPost = world_.resources.playerPostEffect_->GetParam();
            playerPost.threshold = 0.0f;
            playerPost.intensity = 1.0f;
            ApplyGameplayNeonBloomPreset(playerPost);
            playerPost.outlineWidth = 0.0f;
            playerPost.outlineThreshold = 0.0f;
            playerPost.outlineColor = {0.12f, 1.0f, 0.32f};
            playerPost.outlineBloomIntensity = 0.27f;
            playerPost.outlineBloomWidth = 2.7f;
        }

        co_await std::suspend_always{};
        world_.resources.enemyPostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.enemyPostEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            myEngine::BloomParam& enemyPost = world_.resources.enemyPostEffect_->GetParam();
            enemyPost.intensity = 1.2f;
            ApplyGameplayNeonBloomPreset(enemyPost);
            enemyPost.outlineWidth = 0.0f;
            enemyPost.outlineThreshold = 0.0f;
            enemyPost.outlineColor = {1.0f, 0.2f, 0.1f};
            enemyPost.chromAbAmount = 0.0f;
            enemyPost.outlineBloomIntensity = 0.27f;
            enemyPost.outlineBloomWidth = 2.7f;
        }

        co_await std::suspend_always{};
        world_.resources.expEnemyPostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.expEnemyPostEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                         myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            myEngine::BloomParam& expEnemyPost = world_.resources.expEnemyPostEffect_->GetParam();
            expEnemyPost.intensity = 1.0f;
            ApplyGameplayNeonBloomPreset(expEnemyPost);
            expEnemyPost.threshold = 0.0f;
            expEnemyPost.outlineWidth = 0.0f;
            expEnemyPost.outlineThreshold = 0.0f;
            expEnemyPost.outlineColor = {0.9f, 0.15f, 0.98f};
            expEnemyPost.chromAbAmount = 0.0f;
            expEnemyPost.outlineBloomIntensity = 0.27f;
            expEnemyPost.outlineBloomWidth = 2.7f;
        }

        co_await std::suspend_always{};
        world_.resources.stagePostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.stagePostEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            myEngine::BloomParam& stagePost = world_.resources.stagePostEffect_->GetParam();
            stagePost.threshold = 0.0f;
            stagePost.intensity = 0.75f;
            ApplyGameplayNeonBloomPreset(stagePost);
            stagePost.outlineWidth = 0.0f;
            stagePost.outlineThreshold = 0.0f;
            stagePost.outlineColor = {1.0f, 0.55f, 0.58f};
            stagePost.chromAbAmount = 0.0f;
            stagePost.outlineBloomIntensity = 0.0f;
            stagePost.outlineBloomWidth = 0.0f;
        }

        co_await std::suspend_always{};
        world_.resources.neonGridPostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.neonGridPostEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                         myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 1.0f);
        {
            myEngine::BloomParam& gridPost = world_.resources.neonGridPostEffect_->GetParam();
            gridPost.threshold = 0.0f;
            gridPost.intensity = 1.5f;
            ApplyGameplayNeonBloomPreset(gridPost);
            gridPost.outlineWidth = 0.0f;
            gridPost.outlineThreshold = 0.0f;
            gridPost.outlineBloomIntensity = 0.0f;
            gridPost.outlineBloomWidth = 0.0f;
        }

        co_await std::suspend_always{};
        world_.resources.bulletTrailPostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.bulletTrailPostEffect_->Initialize(
            myEngine::Object3dCommon::GetInstance()->GetDxCommon(), myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f,
            1.0f // Quality/OFFは弾の芯をフル解像度で保ち、Legacyは元のサイズのままにする。
        );
        {
            myEngine::BloomParam& bulletTrailPost = world_.resources.bulletTrailPostEffect_->GetParam();
            bulletTrailPost.threshold = 0.0f;
            bulletTrailPost.intensity = 2.0f;
            bulletTrailPost.bloomGain = 1.2f;
            bulletTrailPost.bloomScatter = 0.55f;
            bulletTrailPost.bloomRadius = 1.0f;
            bulletTrailPost.outlineWidth = 0.0f;
            bulletTrailPost.outlineThreshold = 0.0f;
            bulletTrailPost.outlineBloomIntensity = 0.0f;
            bulletTrailPost.outlineBloomWidth = 0.0f;
        }

        co_await std::suspend_always{};
        world_.resources.particlePostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.particlePostEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                         myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 1.0f);
        {
            myEngine::BloomParam& particlePost = world_.resources.particlePostEffect_->GetParam();
            particlePost.threshold = 0.0f;
            particlePost.intensity = 1.65f;
            // 小さく消えていくモデルパーティクルは、HDRの弾の先端より多くのエネルギーが必要。
            ApplyGameplayNeonBloomPreset(particlePost, 2.0f);
            particlePost.outlineWidth = 0.0f;
            particlePost.outlineThreshold = 0.0f;
            particlePost.outlineBloomIntensity = 0.0f;
            particlePost.outlineBloomWidth = 0.0f;
        }

        co_await std::suspend_always{};
        world_.resources.sharedObjectBloomPostEffect_ = std::make_unique<myEngine::ObjectPostEffect>();
        world_.resources.sharedObjectBloomPostEffect_->Initialize(
            myEngine::Object3dCommon::GetInstance()->GetDxCommon(), myEngine::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f,
            1.0f // Quality/Lightでは細いモデルのエミッターを残し、Legacyは元のキャプチャのままにする。
        );
        {
            myEngine::BloomParam& objectBloomPost = world_.resources.sharedObjectBloomPostEffect_->GetParam();
            objectBloomPost.threshold = 0.0f;
            objectBloomPost.intensity = 1.15f;
            ApplyGameplayNeonBloomPreset(objectBloomPost);
            objectBloomPost.outlineWidth = 0.0f;
            objectBloomPost.outlineThreshold = 0.0f;
            objectBloomPost.outlineBloomIntensity = 0.0f;
            objectBloomPost.outlineBloomWidth = 0.0f;
        }
        world_.gameplaySettings->LoadGamePostEffectConfig();
    }

    co_await std::suspend_always{};
    {
        myEngine::StartupTrace::Mark("GameScene.WorldAndActors");
        world_.resources.enemyObject_ = std::make_unique<myEngine::Object3d>();
        world_.resources.enemyObject_->Initialize();

        world_.resources.object3d3 = std::make_unique<myEngine::Object3d>();
        world_.resources.object3d3->Initialize();

        world_.resources.playerObject_ = std::make_unique<myEngine::Object3d>();
        world_.resources.playerObject_->Initialize();

        world_.resources.ballObj_ = std::make_unique<myEngine::Object3d>();
        world_.resources.ballObj_->Initialize();
        world_.resources.ballObj_->SetTranslate(myEngine::Vector3(20.0f, 0.0f, 0.0f));
        world_.resources.ballObj_->Update();

        world_.resources.ball_ = std::make_unique<myEngine::Object3d>();
        world_.resources.ball_->Initialize();
        // ライティングを有効か
        world_.resources.ball_->SetLighting(true);
        world_.resources.ball_->SetTranslate(myEngine::Vector3(-20.0f, 0.0f, 0.0f));
        world_.resources.ball_->Update();

        co_await std::suspend_always{};
        world_.resources.groundObj_ = std::make_unique<myEngine::Object3d>();
        world_.resources.groundObj_->Initialize();
        world_.resources.groundObj_->SetModel("ground.obj");
        world_.resources.groundObj_->SetTranslate(myEngine::Vector3(0.0f, -30.0f, 0.0f));

        myEngine::TextureManager::GetInstance()->LoadTexture("resources/skybox.dds");
        world_.resources.skybox_ = std::make_unique<myEngine::Skybox>();
        world_.resources.skybox_->Initialize("resources/skybox.dds");
        const uint32_t skyboxTextureIndex = myEngine::TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds");

        co_await std::suspend_always{};
        world_.resources.enemyObject_->SetModel("enemy3D.obj");
        world_.resources.enemyObject_->SetLighting(true);
        world_.resources.enemyObject_->SetEnvironmentMap(skyboxTextureIndex);
        world_.resources.enemyObject_->SetEnvironmentCoefficient(0.75f);
        world_.resources.object3d3->SetModel("plane.obj");
        world_.resources.playerObject_->SetModel("player3D.obj");
        world_.resources.playerObject_->SetColor(myEngine::Vector4(0.48f, 0.86f, 0.22f, 1.0f));

        world_.resources.ballObj_->SetModel("bloomBlock.obj");
        world_.resources.ballObj_->SetColor(myEngine::Vector4(0.06f, 0.45f, 0.08f, 1.0f));
        world_.resources.ballObj_->SetLighting(true);

        world_.resources.ball_->SetModel("jewelry.obj");

        co_await std::suspend_always{};
        world_.resources.stage_ = std::make_unique<Stage>();
        auto stage = world_.resources.stage_->BuildInitialization();
        while (!stage.Advance()) {
            co_await std::suspend_always{};
        }

        co_await std::suspend_always{};
        // 弾マネージャの生成
        world_.resources.bulletManager_ = std::make_unique<BulletManager>();
        world_.resources.bulletManager_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                    myEngine::Object3dCommon::GetInstance());

        LevelData levelData;
        const bool hasLevelData = world_.levelRuntime->LoadLevelFile(levelData);
        myEngine::Vector3 playerSpawnPosition = myEngine::Vector3(30.0f, 30.0f, 0.0f);
        if (hasLevelData) {
            for (const LevelObject& object : levelData.objects) {
                if (object.type == "PlayerSpawn") {
                    if (object.prefab != "Default") {
                        std::cerr << "[LevelLoader] Unsupported PlayerSpawn prefab: " << object.prefab << std::endl;
                    }
                    playerSpawnPosition = object.transform.translate;
                    break;
                }
            }
        }

        co_await std::suspend_always{};
        world_.resources.player_ = std::make_unique<Player>();
        world_.resources.player_->Initialize(world_.resources.playerObject_.get(), playerSpawnPosition, !world_.run.expeditionRun_);
        world_.resources.player_->SetAttackControllerBulletManager(world_.resources.bulletManager_.get());

        co_await std::suspend_always{};
        // 敵キャラの生成
        world_.resources.enemy_ = std::make_unique<Enemy>();
        // 敵キャラの初期化
        world_.resources.enemy_->SetPlayer(world_.resources.player_.get());
        myEngine::Vector3 bossSpawnPosition = myEngine::Vector3(20.0f, 20.0f, 0.0f);
        if (hasLevelData) {
            for (const LevelObject& object : levelData.objects) {
                if (object.type == "BossSpawn") {
                    if (object.prefab != "Default") {
                        std::cerr << "[LevelLoader] Unsupported BossSpawn prefab: " << object.prefab << std::endl;
                    }
                    bossSpawnPosition = object.transform.translate;
                    break;
                }
            }
        }
        if (world_.gameplayQueries->IsTutorialCombatSuppressed()) {
            bossSpawnPosition = {-10000.0f, -10000.0f, 0.0f};
        }
        world_.resources.enemy_->Initialize(world_.resources.enemyObject_.get(), bossSpawnPosition, world_.resources.stage_.get());
        world_.resources.enemy_->SetAttackControllerBulletManager(world_.resources.bulletManager_.get());

        co_await std::suspend_always{};
        // 経験値敵マネージャの生成
        world_.resources.enemyManager_ = std::make_unique<EnemyManager>();
        world_.resources.enemyManager_->Initialize(world_.resources.player_.get(), world_.resources.bulletManager_.get(),
                                                   world_.resources.enemy_.get());
        world_.resources.enemy_->SetEnemyManager(world_.resources.enemyManager_.get());
        ExpEnemy::SetEnemyKillCallback([this](uint32_t expValue) {
            if (world_.resources.enemy_) {
                world_.resources.enemy_->RegisterExpEnemyKill(expValue);
            }
        });
        ExpEnemy::SetPlayerDefeatCallback([this](const myEngine::Vector3& position) {
            ++world_.combat.defeatedEnemies_;
            if (world_.run.prototypeRun_) {
                world_.arenaRunController->OnTankRunEnemyDefeated(position);
                return;
            }
            world_.combat.screenEffectDirector_.TriggerEnemyDefeat(world_.gameplayQueries->WorldToScreenUv(position), 1.0f);
            world_.combatFlow->SetEventCallout("敵を撃破", 0.55f);
        });
        if (hasLevelData) {
            world_.levelRuntime->ApplyLevelData(levelData);
        }
    }

    co_await std::suspend_always{};
    {
        myEngine::StartupTrace::Mark("GameScene.PresentationAndUi");
        // 衝突マネージャの生成
        world_.resources.collisionManager_ = std::make_unique<CollisionManager>();
        world_.resources.collisionDebugRingManager_ = std::make_unique<myEngine::RingManager>();
        world_.resources.collisionDebugRingManager_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                                "resources/gradationLine.png");
        world_.resources.neonGridRenderer_ = std::make_unique<myEngine::NeonGridRenderer>();
        world_.resources.neonGridRenderer_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                       "resources/white512x512.png");
        world_.resources.neonProjectileRenderer_ = std::make_unique<NeonProjectileRenderer>();
        world_.resources.neonProjectileRenderer_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon());
        world_.resources.playerMeleeTrailManager_ = std::make_unique<myEngine::TrailManager>();
        world_.resources.playerMeleeTrailManager_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                              myEngine::Object3dCommon::GetInstance(), "resources/white512x512.png");
        world_.gameplaySettings->LoadGameVisualConfig();

        co_await std::suspend_always{};
        world_.combat.fade_ = std::make_unique<Fade>();
        world_.combat.fade_->Initialize();
        world_.combat.fade_->Start(Fade::Status::FadeIn, 1.0f);
        world_.presentation.sceneFadeBlurTimer_ = world_.presentation.sceneFadeBlurDuration_;
        world_.presentation.sceneFadeBlurIntensity_ = 1.0f;

        const std::filesystem::path shotGuideTexturePath = "resources/LivePhoto.png";
        if (std::filesystem::exists(shotGuideTexturePath)) {
            world_.combat.shotGide = std::make_unique<myEngine::Sprite>();
            world_.combat.shotGide->Initialize(myEngine::SpriteCommon::GetInstance(), shotGuideTexturePath.string());
            world_.combat.shotGide->SetPosition({100.0f, 100.0f});
        }

        world_.combat.wasdGide = std::make_unique<myEngine::Sprite>();
        world_.combat.wasdGide->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/wasd.png");
        world_.combat.wasdGide->SetPosition({20.0f, 530.0f});
        world_.combat.wasdGide->SetSize({200.0f, 50.0f});

        world_.combat.dashGide = std::make_unique<myEngine::Sprite>();
        world_.combat.dashGide->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/dashGide.png");
        world_.combat.dashGide->SetPosition({20.0f, 450.0f});
        world_.combat.dashGide->SetSize({200.0f, 50.0f});

        world_.combat.toTitleGide = std::make_unique<myEngine::Sprite>();
        world_.combat.toTitleGide->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/toTitle.png");
        world_.combat.toTitleGide->SetPosition({20.0f, 610.0f});
        world_.combat.toTitleGide->SetSize({200.0f, 50.0f});

        co_await std::suspend_always{};
        world_.combat.dashGuideText_ = std::make_unique<myEngine::TextLabel>();
        world_.combat.dashGuideText_->InitializeFromJson(myEngine::SpriteCommon::GetInstance(), "resources/configs/gameText.json",
                                                         "dashGuide");

        world_.combat.moveGuideText_ = std::make_unique<myEngine::TextLabel>();
        world_.combat.moveGuideText_->InitializeFromJson(myEngine::SpriteCommon::GetInstance(), "resources/configs/gameText.json",
                                                         "moveGuide");

        world_.combat.titleGuideText_ = std::make_unique<myEngine::TextLabel>();
        world_.combat.titleGuideText_->InitializeFromJson(myEngine::SpriteCommon::GetInstance(), "resources/configs/gameText.json",
                                                          "titleGuide");

        co_await std::suspend_always{};
        myEngine::TextStyle controlGuideStyle{};
        controlGuideStyle.fontFamily = "Meiryo";
        controlGuideStyle.fontSize = 14.0f;
        controlGuideStyle.color = {0.88f, 0.94f, 1.0f, 0.78f};
        controlGuideStyle.outlineColor = {0.0f, 0.02f, 0.04f, 0.88f};
        controlGuideStyle.outlineThickness = 2.0f;
        controlGuideStyle.padding = 5.0f;
        world_.combat.controlGuideText_ = std::make_unique<myEngine::TextLabel>();
        world_.combat.controlGuideText_->Initialize(
            myEngine::SpriteCommon::GetInstance(),
            "WASD 移動 / 左クリック 射撃 / 右クリック ダッシュ\nC 進化ツリー / ESC タイトルへ / H ヘルプ切替", controlGuideStyle);
        world_.combat.controlGuideText_->SetPosition({22.0f, 636.0f});

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        myEngine::TextStyle fpsStyle{};
        fpsStyle.fontFamily = "Meiryo";
        fpsStyle.fontSize = 24.0f;
        fpsStyle.color = {0.70f, 1.0f, 0.78f, 1.0f};
        fpsStyle.outlineColor = {0.0f, 0.0f, 0.0f, 0.88f};
        fpsStyle.outlineThickness = 2.0f;
        fpsStyle.padding = 5.0f;
        world_.combat.fpsText_ = std::make_unique<myEngine::TextLabel>();
        world_.combat.fpsText_->Initialize(myEngine::SpriteCommon::GetInstance(), "FPS: --", fpsStyle);
        world_.combat.fpsText_->SetPosition({16.0f, 14.0f});
        world_.combat.fpsLastSampleTime_ = std::chrono::steady_clock::now();

        myEngine::TextStyle profileStyle = fpsStyle;
        profileStyle.fontSize = 18.0f;
        profileStyle.color = {0.75f, 0.95f, 1.0f, 1.0f};
        profileStyle.outlineThickness = 2.0f;
        world_.combat.postProfileText_ = std::make_unique<myEngine::TextLabel>();
        world_.combat.postProfileText_->Initialize(myEngine::SpriteCommon::GetInstance(), "Post Profile  F8:hide  F9:mode", profileStyle);
        world_.combat.postProfileText_->SetPosition({16.0f, 46.0f});
#endif // !defined(NDEBUG)

        co_await std::suspend_always{};
        world_.gameplayHud->InitializeFollowHpBarBatch();
        world_.combatTutorial->InitializeSubmissionUi();
        world_.combat.gameTextNeonEffect_ = std::make_unique<NeonTextEffect>();
        world_.combat.gameTextNeonEffect_->Initialize(myEngine::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      myEngine::Object3dCommon::GetInstance()->GetSrvManager());
        world_.gameplayHud->ApplyGameTextAppearance();
        co_await std::suspend_always{};
        world_.combatTutorial->InitializeTutorialUi();
        if (myEngine::kDeveloperTools) {
            world_.classConfigWatcher->InitializePlayerClassConfigWatch();
        }
    }
    if (world_.run.prototypeRun_) {
        auto run = world_.arenaRunController->BuildInitialization();
        while (!run.Advance()) {
            co_await std::suspend_always{};
        }
    }
    co_await std::suspend_always{};
    world_.combat.previousPlayerHp_ = world_.resources.player_ ? world_.resources.player_->GetHp() : 0;
    world_.combat.previousBossHp_ = world_.resources.enemy_ ? world_.resources.enemy_->GetHp() : 0;
    co_await std::suspend_always{};
    if (world_.demo.titleDemo_) {
        world_.titleDemoController->InitializeTitleDemo();
    }
    if (!world_.demo.titleDemo_) {
        world_.resources.neonBossVisual_ = std::make_unique<NeonBossVisual>();
        world_.resources.neonBossVisual_->Initialize(world_.resources.camera.get(), world_.resources.debugCamera.get());
        if (world_.resources.enemy_->IsNeonDepthEncounterEnabled()) {
            world_.resources.neonBossVisual_->SetDepthProfile(world_.resources.neonDepthConfig_.visual);
        }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        world_.resources.neonBossDeveloperStartPending_ = world_.resources.neonBossAutoTest_;
        if (world_.run.expeditionMapEnabled_ && GameStartSession::ConsumeDeveloperBossStart()) {
            world_.resources.neonBossDeveloperStartPending_ = true;
        }
#endif
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.gameplayScenarioRunner->InitializeGameplayScenario();
    world_.routeReplay->InitializeNormalRouteReplay();
#endif
    myEngine::StartupTrace::Mark(world_.demo.titleDemo_ ? "demo.initialize_ready" : "gameplay.initialize_ready");
}
} // namespace gameplay
