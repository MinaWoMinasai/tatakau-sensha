#include "game/session/SessionBootstrap.h"
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
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    wchar_t bossTest[8]{};
    world_.resources.neonBossAutoTest_ = GetEnvironmentVariableW(L"CG2_NEON_BOSS_AUTOTEST", bossTest, 8) > 0 && bossTest[0] == L'1';
    if (!world_.demo.titleDemo_ && GameplayScenarioSession::Get().IsActive()) {
        cg2::rng.seed(GameplayScenarioSession::Get().GetSettings().seed);
        cg2::ParticleManager::GetInstance()->SetPresentationSeed(GameplayScenarioSession::Get().GetSettings().seed);
    }
#endif
    cg2::StartupTrace::Scope startupScope(world_.demo.titleDemo_ ? "GameScene.Initialize.Demo" : "GameScene.Initialize.Play");

    world_.resources.worldTransform_ = cg2::InitWorldTransform();

    world_.resources.input_ = cg2::Input::GetInstance();
    world_.combatTutorial->LoadTutorialConfig();
    world_.depthEncounter->LoadNeonDepthConfig();
    world_.combat.tutorialConfig_.enabled = !world_.run.prototypeRun_ && GameStartSession::GetMode() == GameStartMode::Tutorial;
    world_.combat.screenEffectDirector_.LoadConfig("resources/configs/screenEffects.json");

    world_.resources.debugCamera = std::make_unique<cg2::DebugCamera>();

    world_.resources.camera = std::make_unique<cg2::Camera>();

    world_.resources.camera->SetTranslate(cg2::Vector3(17.0f, 21.0f, -80.0f));

    cg2::Object3dCommon::GetInstance()->SetDefaultCamera(world_.resources.camera.get());
    cg2::Object3dCommon::GetInstance()->SetDebugDefaultCamera(world_.resources.debugCamera.get());
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (!world_.demo.titleDemo_) {
        world_.resources.neonSkinnedPreview_ = std::make_unique<NeonSkinnedPreview>();
        world_.resources.neonSkinnedPreview_->Initialize(world_.resources.camera.get(), world_.resources.debugCamera.get());
    }
#endif

    {
        cg2::StartupTrace::Scope scope("GameScene.ObjectPostEffects");
        world_.resources.playerPostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.playerPostEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                       cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            cg2::BloomParam& playerPost = world_.resources.playerPostEffect_->GetParam();
            playerPost.threshold = 0.0f;
            playerPost.intensity = 1.0f;
            ApplyGameplayNeonBloomPreset(playerPost);
            playerPost.outlineWidth = 0.0f;
            playerPost.outlineThreshold = 0.0f;
            playerPost.outlineColor = {0.12f, 1.0f, 0.32f};
            playerPost.outlineBloomIntensity = 0.27f;
            playerPost.outlineBloomWidth = 2.7f;
        }

        world_.resources.enemyPostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.enemyPostEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            cg2::BloomParam& enemyPost = world_.resources.enemyPostEffect_->GetParam();
            enemyPost.intensity = 1.2f;
            ApplyGameplayNeonBloomPreset(enemyPost);
            enemyPost.outlineWidth = 0.0f;
            enemyPost.outlineThreshold = 0.0f;
            enemyPost.outlineColor = {1.0f, 0.2f, 0.1f};
            enemyPost.chromAbAmount = 0.0f;
            enemyPost.outlineBloomIntensity = 0.27f;
            enemyPost.outlineBloomWidth = 2.7f;
        }

        world_.resources.expEnemyPostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.expEnemyPostEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                         cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            cg2::BloomParam& expEnemyPost = world_.resources.expEnemyPostEffect_->GetParam();
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

        world_.resources.stagePostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.stagePostEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f);
        {
            cg2::BloomParam& stagePost = world_.resources.stagePostEffect_->GetParam();
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

        world_.resources.neonGridPostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.neonGridPostEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                         cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 1.0f);
        {
            cg2::BloomParam& gridPost = world_.resources.neonGridPostEffect_->GetParam();
            gridPost.threshold = 0.0f;
            gridPost.intensity = 1.5f;
            ApplyGameplayNeonBloomPreset(gridPost);
            gridPost.outlineWidth = 0.0f;
            gridPost.outlineThreshold = 0.0f;
            gridPost.outlineBloomIntensity = 0.0f;
            gridPost.outlineBloomWidth = 0.0f;
        }

        world_.resources.bulletTrailPostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.bulletTrailPostEffect_->Initialize(
            cg2::Object3dCommon::GetInstance()->GetDxCommon(), cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f,
            1.0f // Quality/OFF preserve projectile cores at full resolution; Legacy retains its original source size.
        );
        {
            cg2::BloomParam& bulletTrailPost = world_.resources.bulletTrailPostEffect_->GetParam();
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

        world_.resources.particlePostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.particlePostEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                         cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 1.0f);
        {
            cg2::BloomParam& particlePost = world_.resources.particlePostEffect_->GetParam();
            particlePost.threshold = 0.0f;
            particlePost.intensity = 1.65f;
            // Small, fading model particles need more energy than the HDR projectile heads.
            ApplyGameplayNeonBloomPreset(particlePost, 2.0f);
            particlePost.outlineWidth = 0.0f;
            particlePost.outlineThreshold = 0.0f;
            particlePost.outlineBloomIntensity = 0.0f;
            particlePost.outlineBloomWidth = 0.0f;
        }

        world_.resources.sharedObjectBloomPostEffect_ = std::make_unique<cg2::ObjectPostEffect>();
        world_.resources.sharedObjectBloomPostEffect_->Initialize(
            cg2::Object3dCommon::GetInstance()->GetDxCommon(), cg2::Object3dCommon::GetInstance()->GetSrvManager(), nullptr, 0.5f,
            1.0f // Preserve thin model emitters in Quality/Light; keep Legacy's original capture.
        );
        {
            cg2::BloomParam& objectBloomPost = world_.resources.sharedObjectBloomPostEffect_->GetParam();
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

    {
        cg2::StartupTrace::Scope scope("GameScene.WorldAndActors");
        world_.resources.enemyObject_ = std::make_unique<cg2::Object3d>();
        world_.resources.enemyObject_->Initialize();

        world_.resources.object3d3 = std::make_unique<cg2::Object3d>();
        world_.resources.object3d3->Initialize();

        world_.resources.playerObject_ = std::make_unique<cg2::Object3d>();
        world_.resources.playerObject_->Initialize();

        world_.resources.ballObj_ = std::make_unique<cg2::Object3d>();
        world_.resources.ballObj_->Initialize();
        world_.resources.ballObj_->SetTranslate(cg2::Vector3(20.0f, 0.0f, 0.0f));
        world_.resources.ballObj_->Update();

        world_.resources.ball_ = std::make_unique<cg2::Object3d>();
        world_.resources.ball_->Initialize();
        // ライティングを有効か
        world_.resources.ball_->SetLighting(true);
        world_.resources.ball_->SetTranslate(cg2::Vector3(-20.0f, 0.0f, 0.0f));
        world_.resources.ball_->Update();

        world_.resources.groundObj_ = std::make_unique<cg2::Object3d>();
        world_.resources.groundObj_->Initialize();
        world_.resources.groundObj_->SetModel("ground.obj");
        world_.resources.groundObj_->SetTranslate(cg2::Vector3(0.0f, -30.0f, 0.0f));

        cg2::TextureManager::GetInstance()->LoadTexture("resources/skybox.dds");
        world_.resources.skybox_ = std::make_unique<cg2::Skybox>();
        world_.resources.skybox_->Initialize("resources/skybox.dds");
        const uint32_t skyboxTextureIndex = cg2::TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds");

        world_.resources.enemyObject_->SetModel("enemy3D.obj");
        world_.resources.enemyObject_->SetLighting(true);
        world_.resources.enemyObject_->SetEnvironmentMap(skyboxTextureIndex);
        world_.resources.enemyObject_->SetEnvironmentCoefficient(0.75f);
        world_.resources.object3d3->SetModel("plane.obj");
        world_.resources.playerObject_->SetModel("player3D.obj");
        world_.resources.playerObject_->SetColor(cg2::Vector4(0.48f, 0.86f, 0.22f, 1.0f));

        world_.resources.ballObj_->SetModel("bloomBlock.obj");
        world_.resources.ballObj_->SetColor(cg2::Vector4(0.06f, 0.45f, 0.08f, 1.0f));
        world_.resources.ballObj_->SetLighting(true);

        world_.resources.ball_->SetModel("jewelry.obj");

        world_.resources.stage_ = std::make_unique<Stage>();
        world_.resources.stage_->Initialize();

        // 弾マネージャの生成
        world_.resources.bulletManager_ = std::make_unique<BulletManager>();
        world_.resources.bulletManager_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(), cg2::Object3dCommon::GetInstance());

        LevelData levelData;
        const bool hasLevelData = world_.levelRuntime->LoadLevelFile(levelData);
        cg2::Vector3 playerSpawnPosition = cg2::Vector3(30.0f, 30.0f, 0.0f);
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

        world_.resources.player_ = std::make_unique<Player>();
        world_.resources.player_->Initialize(world_.resources.playerObject_.get(), playerSpawnPosition, !world_.run.expeditionRun_);
        world_.resources.player_->SetAttackControllerBulletManager(world_.resources.bulletManager_.get());

        // 敵キャラの生成
        world_.resources.enemy_ = std::make_unique<Enemy>();
        // 敵キャラの初期化
        world_.resources.enemy_->SetPlayer(world_.resources.player_.get());
        cg2::Vector3 bossSpawnPosition = cg2::Vector3(20.0f, 20.0f, 0.0f);
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
        ExpEnemy::SetPlayerDefeatCallback([this](const cg2::Vector3& position) {
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

    {
        cg2::StartupTrace::Scope scope("GameScene.PresentationAndUi");
        // 衝突マネージャの生成
        world_.resources.collisionManager_ = std::make_unique<CollisionManager>();
        world_.resources.collisionDebugRingManager_ = std::make_unique<cg2::RingManager>();
        world_.resources.collisionDebugRingManager_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                                "resources/gradationLine.png");
        world_.resources.neonGridRenderer_ = std::make_unique<cg2::NeonGridRenderer>();
        world_.resources.neonGridRenderer_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(), "resources/white512x512.png");
        world_.resources.neonProjectileRenderer_ = std::make_unique<NeonProjectileRenderer>();
        world_.resources.neonProjectileRenderer_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon());
        world_.resources.playerMeleeTrailManager_ = std::make_unique<cg2::TrailManager>();
        world_.resources.playerMeleeTrailManager_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                              cg2::Object3dCommon::GetInstance(), "resources/white512x512.png");
        world_.gameplaySettings->LoadGameVisualConfig();

        world_.combat.fade_ = std::make_unique<Fade>();
        world_.combat.fade_->Initialize();
        world_.combat.fade_->Start(Fade::Status::FadeIn, 1.0f);
        world_.presentation.sceneFadeBlurTimer_ = world_.presentation.sceneFadeBlurDuration_;
        world_.presentation.sceneFadeBlurIntensity_ = 1.0f;

        const std::filesystem::path shotGuideTexturePath = "resources/LivePhoto.png";
        if (std::filesystem::exists(shotGuideTexturePath)) {
            world_.combat.shotGide = std::make_unique<cg2::Sprite>();
            world_.combat.shotGide->Initialize(cg2::SpriteCommon::GetInstance(), shotGuideTexturePath.string());
            world_.combat.shotGide->SetPosition({100.0f, 100.0f});
        }

        world_.combat.wasdGide = std::make_unique<cg2::Sprite>();
        world_.combat.wasdGide->Initialize(cg2::SpriteCommon::GetInstance(), "resources/wasd.png");
        world_.combat.wasdGide->SetPosition({20.0f, 530.0f});
        world_.combat.wasdGide->SetSize({200.0f, 50.0f});

        world_.combat.dashGide = std::make_unique<cg2::Sprite>();
        world_.combat.dashGide->Initialize(cg2::SpriteCommon::GetInstance(), "resources/dashGide.png");
        world_.combat.dashGide->SetPosition({20.0f, 450.0f});
        world_.combat.dashGide->SetSize({200.0f, 50.0f});

        world_.combat.toTitleGide = std::make_unique<cg2::Sprite>();
        world_.combat.toTitleGide->Initialize(cg2::SpriteCommon::GetInstance(), "resources/toTitle.png");
        world_.combat.toTitleGide->SetPosition({20.0f, 610.0f});
        world_.combat.toTitleGide->SetSize({200.0f, 50.0f});

        world_.combat.dashGuideText_ = std::make_unique<cg2::TextLabel>();
        world_.combat.dashGuideText_->InitializeFromJson(cg2::SpriteCommon::GetInstance(), "resources/configs/gameText.json", "dashGuide");

        world_.combat.moveGuideText_ = std::make_unique<cg2::TextLabel>();
        world_.combat.moveGuideText_->InitializeFromJson(cg2::SpriteCommon::GetInstance(), "resources/configs/gameText.json", "moveGuide");

        world_.combat.titleGuideText_ = std::make_unique<cg2::TextLabel>();
        world_.combat.titleGuideText_->InitializeFromJson(cg2::SpriteCommon::GetInstance(), "resources/configs/gameText.json",
                                                          "titleGuide");

        cg2::TextStyle controlGuideStyle{};
        controlGuideStyle.fontFamily = "Meiryo";
        controlGuideStyle.fontSize = 14.0f;
        controlGuideStyle.color = {0.88f, 0.94f, 1.0f, 0.78f};
        controlGuideStyle.outlineColor = {0.0f, 0.02f, 0.04f, 0.88f};
        controlGuideStyle.outlineThickness = 2.0f;
        controlGuideStyle.padding = 5.0f;
        world_.combat.controlGuideText_ = std::make_unique<cg2::TextLabel>();
        world_.combat.controlGuideText_->Initialize(
            cg2::SpriteCommon::GetInstance(),
            "WASD 移動 / 左クリック 射撃 / 右クリック ダッシュ\nC 進化ツリー / ESC タイトルへ / H ヘルプ切替", controlGuideStyle);
        world_.combat.controlGuideText_->SetPosition({22.0f, 636.0f});

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        cg2::TextStyle fpsStyle{};
        fpsStyle.fontFamily = "Meiryo";
        fpsStyle.fontSize = 24.0f;
        fpsStyle.color = {0.70f, 1.0f, 0.78f, 1.0f};
        fpsStyle.outlineColor = {0.0f, 0.0f, 0.0f, 0.88f};
        fpsStyle.outlineThickness = 2.0f;
        fpsStyle.padding = 5.0f;
        world_.combat.fpsText_ = std::make_unique<cg2::TextLabel>();
        world_.combat.fpsText_->Initialize(cg2::SpriteCommon::GetInstance(), "FPS: --", fpsStyle);
        world_.combat.fpsText_->SetPosition({16.0f, 14.0f});
        world_.combat.fpsLastSampleTime_ = std::chrono::steady_clock::now();

        cg2::TextStyle profileStyle = fpsStyle;
        profileStyle.fontSize = 18.0f;
        profileStyle.color = {0.75f, 0.95f, 1.0f, 1.0f};
        profileStyle.outlineThickness = 2.0f;
        world_.combat.postProfileText_ = std::make_unique<cg2::TextLabel>();
        world_.combat.postProfileText_->Initialize(cg2::SpriteCommon::GetInstance(), "Post Profile  F8:hide  F9:mode", profileStyle);
        world_.combat.postProfileText_->SetPosition({16.0f, 46.0f});
#endif // !defined(NDEBUG)

        world_.gameplayHud->InitializeFollowHpBarBatch();
        world_.combatTutorial->InitializeSubmissionUi();
        world_.combat.gameTextNeonEffect_ = std::make_unique<NeonTextEffect>();
        world_.combat.gameTextNeonEffect_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),
                                                      cg2::Object3dCommon::GetInstance()->GetSrvManager());
        world_.gameplayHud->ApplyGameTextAppearance();
        world_.combatTutorial->InitializeTutorialUi();
        if (cg2::kDeveloperTools)
            world_.classConfigWatcher->InitializePlayerClassConfigWatch();
    }
    if (world_.run.prototypeRun_)
        world_.arenaRunController->InitializeTankRun();
    world_.combat.previousPlayerHp_ = world_.resources.player_ ? world_.resources.player_->GetHp() : 0;
    world_.combat.previousBossHp_ = world_.resources.enemy_ ? world_.resources.enemy_->GetHp() : 0;
    if (world_.demo.titleDemo_)
        world_.titleDemoController->InitializeTitleDemo();
    if (!world_.demo.titleDemo_) {
        world_.resources.neonBossVisual_ = std::make_unique<NeonBossVisual>();
        world_.resources.neonBossVisual_->Initialize(world_.resources.camera.get(), world_.resources.debugCamera.get());
        if (world_.resources.enemy_->IsNeonDepthEncounterEnabled())
            world_.resources.neonBossVisual_->SetDepthProfile(world_.resources.neonDepthConfig_.visual);
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        world_.resources.neonBossDeveloperStartPending_ = world_.resources.neonBossAutoTest_;
        if (world_.run.expeditionMapEnabled_ && GameStartSession::ConsumeDeveloperBossStart())
            world_.resources.neonBossDeveloperStartPending_ = true;
#endif
    }
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.gameplayScenarioRunner->InitializeGameplayScenario();
    world_.routeReplay->InitializeNormalRouteReplay();
#endif
}
} // namespace gameplay
