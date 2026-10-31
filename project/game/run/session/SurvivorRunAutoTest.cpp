#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>

// 育つ戦車の試作の自動確認（区画・四隅・200体の fps）。MYENGINE_SURVIVOR_AUTOTEST で動き、結果を書いて終了する。

namespace gameplay {

using namespace survivorrun;

namespace {
struct AutoTestWaypoint {
    const char* name;
    myEngine::Vector3 target;
};

// 中央から四隅を回って中央へ戻る。柱の並ぶ線（x=16〜18・30・58・70〜72、y=8・28〜30・50）を避けた経路にする。
const std::array<AutoTestWaypoint, 5> kTourWaypoints{
    {{"northeast", {82, 54, 0}}, {"southeast", {82, 4, 0}}, {"southwest", {6, 4, 0}}, {"northwest", {6, 54, 0}}, {"center", {44, 28, 0}}}};
// 入力を止めても少し滑るので、着いたかどうかはこの距離で見る。
constexpr float kAutoTestReachDistance = 4.0f;
// 区画の自動確認で画面を保存する時刻（秒）。
constexpr std::array<float, 3> kSectorCaptureSeconds{30.0f, 125.0f, 183.0f};
// 区画の自動確認が出口へ向かい始める時刻（秒）。
constexpr float kSectorAutoTestExitSeconds = 240.0f;
} // namespace

float SurvivorRunController::MeasureAimRoundTripError() const
{
    const myEngine::Vector3 target = world_.resources.player_->GetWorldPosition() + myEngine::Vector3{5.0f, 3.0f, 0.0f};
    const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(target);
    const myEngine::Vector3 back =
        myEngine::ScreenToWorldOnZ0(screen, world_.resources.camera->GetViewMatrix(), world_.resources.camera->GetProjectionMatrix(),
                                    myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight);
    return myEngine::Length(myEngine::Vector3{back.x - target.x, back.y - target.y, 0.0f});
}

void SurvivorRunController::RequestCapture(const std::string& name)
{
    if (world_.run.tankRunCapturePath_.empty()) {
        world_.run.tankRunCapturePath_ = std::string(kAutoTestDirectory) + "/" + name + ".png";
    }
}

void SurvivorRunController::UpdateSectorAutoTest(float dt)
{
    // 部屋の中央の周りを回りながら近い敵を撃ち続け、近くの欠片は拾いに行く。4:00 を過ぎたら出口へ向かう
    // （レベルの曲線を4分で確かめるため、出口が開いてもすぐには入らない）。自機は無敵。
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (const ExpEnemy* nearest = world_.resources.enemyManager_->FindNearestEnemy(position, 30.0f)) {
        aim = nearest->GetWorldPosition();
    }
    // 宿敵がいて近ければ、宿敵を撃つ。
    if (IsRivalActive()) {
        const myEngine::Vector3 rival = world_.resources.enemy_->GetWorldPosition();
        if (myEngine::Length(myEngine::Vector3{rival.x - position.x, rival.y - position.y, 0.0f}) < 30.0f) {
            aim = rival;
        }
    }
    myEngine::Vector3 goal{};
    myEngine::Vector3 shard{};
    if (exitOpen_ && elapsed_ >= kSectorAutoTestExitSeconds) {
        goal = exitPosition_;
    } else if (IsRivalActive()) {
        // 宿敵がいる間は、宿敵から8ほど離れた位置へ寄る。
        const myEngine::Vector3 rival = world_.resources.enemy_->GetWorldPosition();
        myEngine::Vector3 away{position.x - rival.x, position.y - rival.y, 0.0f};
        const float distance = myEngine::Length(away);
        goal = distance > 0.1f ? rival + away * (8.0f / distance) : rival;
    } else if (pickups_.FindNearest(position, 12.0f, shard)) {
        goal = shard;
    } else {
        const float angle = autoTestTime_ * 0.35f;
        goal = {44.0f + std::cos(angle) * 18.0f, 28.0f + std::sin(angle) * 11.0f, 0.0f};
    }
    myEngine::Vector3 move{goal.x - position.x, goal.y - position.y, 0.0f};
    const float length = myEngine::Length(move);
    move = length > 0.5f ? move / length : myEngine::Vector3{};
    // トゲ：5秒で一度、自機のすぐ横に四角を置き、すぐにトゲの判定をして壊れるかを見る
    // （同じフレームの体当たりや弾で壊れる前に確かめる）。
    if (player.HasSpikes() && autoSpikeProbe_ == 0 && autoTestTime_ >= 5.0f) {
        const myEngine::Vector3 beside = position + myEngine::Vector3{0.0f, -(player.GetRadius() + 0.5f), 0.0f};
        const int before = spikeKills_;
        if (SpawnShape("Square", beside, false)) {
            UpdateSpikes();
            autoSpikeProbe_ = spikeKills_ > before ? 2 : 3;
        } else {
            autoSpikeProbe_ = -1;
        }
    }
    // 3秒ごとに1回ダッシュする（衝撃放電・推進器の確認）。
    const bool dash = static_cast<int>(autoTestTime_ / 3.0f) != static_cast<int>((autoTestTime_ - dt) / 3.0f);
    // 不動砲台を持っていれば、6秒のうち2秒は止まって撃つ（止まっている間の威力の確認）。
    const bool standStill = relics_[static_cast<size_t>(survivor::RelicId::FixedTurret)] && std::fmod(autoTestTime_, 6.0f) < 2.0f;
    player.SetDemoInput(true, standStill ? myEngine::Vector2{} : myEngine::Vector2{move.x, move.y}, aim, true, dash);
    autoMaxHeat_ = (std::max)(autoMaxHeat_, player.GetHeat().GetHeat());

    // 特性：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で選ぶ（既定は冷却炉）。
    if (traitChoiceOpen_) {
        autoTraitTimer_ += dt;
        if (!autoTraitCaptured_ && autoTraitTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("trait_open");
            autoTraitCaptured_ = true;
        }
        if (autoTraitCaptured_ && autoTraitTimer_ > 0.6f) {
            ChooseTrait(autoTraitIndex_);
        }
        return;
    }
    // レリック：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で1枚目を選ぶ。選ぶ前後の主砲の威力・連射・ダッシュを記録する。
    if (relicChoiceOpen_) {
        autoRelicTimer_ += dt;
        if (!autoRelicCaptured_ && autoRelicTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("relic_open");
            autoRelicCaptured_ = true;
        }
        if (autoRelicTimer_ > 0.6f) {
            const auto before = player.GetStats();
            const survivor::RelicId id = relicOffers_.front();
            ChooseRelic(0);
            autoRelicTimer_ = 0.0f;
            autoRelicLog_ << (autoRelicLog_.tellp() > 0 ? "," : "") << "{\"t\":" << std::lround(elapsed_) << ",\"relic\":\""
                          << (id == survivor::RelicId::Count ? "premium" : survivor::RelicKey(id)) << "\",\"bulletDamage\":["
                          << before.bulletDamage << "," << player.GetStats().bulletDamage << "],\"reloadSpeed\":[" << before.reloadSpeed
                          << "," << player.GetStats().reloadSpeed << "]}";
        }
        return;
    }
    // 形態：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で選ぶ。選んで2秒後に、変わった自機の画面を保存する。
    if (formChoiceOpen_) {
        autoFormTimer_ += dt;
        if (!autoFormCaptured_ && autoFormTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture(formStage_ == 0 ? "form_open" : "evolve_open");
            autoFormCaptured_ = true;
        }
        if (autoFormCaptured_ && autoFormTimer_ > 0.6f) {
            // 第1段は連装か長砲身（MYENGINE_SURVIVOR_FORM）、第2段は MYENGINE_SURVIVOR_FORM2 の形態（無ければ1枚目）。
            size_t index = formStage_ == 0 ? autoFormIndex_ : 0;
            for (size_t i = 0; formStage_ >= 1 && i < formOffers_.size(); ++i) {
                if (autoForm2Key_ == config_.forms[static_cast<size_t>(formOffers_[i])].key) {
                    index = i;
                }
            }
            if (formStage_ >= 1) {
                autoEvolveOffers_ = static_cast<int>(formOffers_.size());
            }
            ChooseForm(index);
            if (formStage_ >= 2) {
                autoEvolvedAt_ = elapsed_;
            } else {
                autoFormChosenAt_ = elapsed_;
            }
            autoFormCaptureAt_ = autoTestTime_ + 2.0f;
            autoFormCaptured_ = false;
            autoFormTimer_ = 0.0f;
        }
        return;
    }
    if (autoFormCaptureAt_ > 0.0f && autoTestTime_ >= autoFormCaptureAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("form_" + formKey_);
        autoFormCaptureAt_ = -1.0f;
    }
    // Q の入れ替え（MYENGINE_SURVIVOR_Q_SWAP=1）：10秒で一度、排熱砲を積んでいれば、緊急放出の札だけの3択を作って選び、
    // 排熱砲が外れて緊急放出に替わるかを見る。
    if (autoQSwapProbe_ == 0 && autoTestTime_ >= 10.0f && !choiceOpen_ &&
        survivor::OwnedQSkill(config_.parts, partLevels_) == survivor::PartId::HeatVent) {
        wchar_t swap[4]{};
        if (GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_Q_SWAP", swap, 4) > 0 && swap[0] == L'1') {
            ++pendingChoices_;
            pendingSince_.push_back(elapsed_);
            choiceOpen_ = true;
            offers_ = {{survivor::PartId::EmergencyPurge, 1}};
            ChooseOffer(0);
            autoQSwapProbe_ = survivor::OwnedQSkill(config_.parts, partLevels_) == survivor::PartId::EmergencyPurge &&
                                      survivor::LevelOf(partLevels_, survivor::PartId::HeatVent) == 0
                                  ? 2
                                  : 3;
        } else {
            autoQSwapProbe_ = -1;
        }
    }
    // Q：排熱砲は、熱が上限の2割（使える下限より低ければ下限）を超え、待ち時間が無ければ使う。
    // 緊急放出は、0:20 を過ぎて部品を3つ以上積んでいれば使う。使った0.15秒後の画面を保存する（それぞれ最初の1回）。
    const survivor::PartId qSkill = survivor::OwnedQSkill(config_.parts, partLevels_);
    bool qReady = false;
    if (qSkill == survivor::PartId::HeatVent) {
        qReady = player.GetHeat().GetHeat() >= (std::max)(config_.heatVent.minHeat, player.GetHeat().GetMaxHeat() * 0.2f);
    } else if (qSkill == survivor::PartId::EmergencyPurge) {
        int owned = 0;
        for (int level : partLevels_) {
            owned += level > 0 ? 1 : 0;
        }
        // 部品を指定して始めた確認（START_PARTS）では、確かめたい部品を壊さないよう使わない。
        qReady = elapsed_ >= 20.0f && owned >= 3 && !autoKeepParts_;
    }
    if (!choiceOpen_ && qCooldown_ <= 0.0f && qReady && UseQ()) {
        if (qSkill == survivor::PartId::HeatVent && autoQCaptureAt_ < 0.0f) {
            autoQCaptureAt_ = autoTestTime_ + 0.15f;
            autoQCaptureName_ = "q_vent";
        } else if (qSkill == survivor::PartId::EmergencyPurge && autoPurgeCaptured_ == false) {
            autoQCaptureAt_ = autoTestTime_ + 0.15f;
            autoQCaptureName_ = "q_purge";
            autoPurgeCaptured_ = true;
        }
    }
    if (autoQCaptureAt_ > 0.0f && autoTestTime_ >= autoQCaptureAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture(autoQCaptureName_);
        autoQCaptureAt_ = 0.0f;
    }
    // 長砲身：5秒ごとに0.7秒、右クリックを押してため撃ちする。
    player.SetDemoSpecialHold(player.HasRailChargeForm() && std::fmod(autoTestTime_, 5.0f) < 0.7f);

    // シールド：8秒で一度だけ無敵を外して2回当て、先にシールドが減ること、3秒は戻らず、その後に満タンへ戻ることを見る。
    // 無敵の間の被弾は TakeDamage の入口で弾かれるので、回復の待ち時間は動かない。
    constexpr std::array<float, 4> kShieldCheckSeconds{8.0f, 10.5f, 12.0f, 16.0f};
    if (autoShieldStep_ < static_cast<int>(kShieldCheckSeconds.size()) &&
        autoTestTime_ >= kShieldCheckSeconds[static_cast<size_t>(autoShieldStep_)]) {
        auto record = [&](const char* label) {
            autoShieldLog_ << (autoShieldLog_.tellp() > 0 ? "," : "") << "{\"at\":\"" << label << "\",\"t\":" << autoTestTime_
                           << ",\"shield\":" << player.GetShield() << ",\"shieldMax\":" << player.GetShieldMax()
                           << ",\"hp\":" << player.GetHp() << "}";
        };
        if (autoShieldStep_ == 0) {
            record("before");
            player.SetDebugNoDamage(false);
            player.TakeDamage(10, 0.0f);
            record("hit10");
            player.TakeDamage(40, 0.0f);
            record("hit40");
            player.SetDebugNoDamage(true);
        } else {
            record(autoShieldStep_ == 1 ? "wait2.5" : autoShieldStep_ == 2 ? "wait4" : "wait8");
        }
        ++autoShieldStep_;
    }

    // 3択：選択が2つ以上貯まるか、1つを3秒持ったら開き、0.6秒（実時間）見てから札を順番に選ぶ。
    if (choiceOpen_) {
        autoMinTimeScale_ = (std::min)(autoMinTimeScale_, world_.combat.finalDeltaTime / myEngine::kFixedStepSeconds);
        autoChoiceTimer_ += dt;
        if (!autoChoiceCaptured_ && autoChoiceTimer_ > 0.3f) {
            RequestCapture("choice_open");
            autoChoiceCaptured_ = true;
        }
        // 積載を超えて外す画面が開いたら、0.3秒で画面を保存し、0.6秒で一番 Lv の低い部品を外す。
        if (removalOpen_) {
            if (!autoRemovalCaptured_ && autoChoiceTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
                RequestCapture("removal_open");
                autoRemovalCaptured_ = true;
            }
            if (autoChoiceTimer_ > 0.6f && loadoutHud_.RowCount() > 0) {
                size_t lowest = 0;
                for (size_t row = 1; row < loadoutHud_.RowCount(); ++row) {
                    if (partLevels_[static_cast<size_t>(loadoutHud_.RowPart(row))] <
                        partLevels_[static_cast<size_t>(loadoutHud_.RowPart(lowest))]) {
                        lowest = row;
                    }
                }
                RemovePart(loadoutHud_.RowPart(lowest));
                autoChoiceTimer_ = 0.0f;
            }
            return;
        }
        // 合体の札が出たら、0.3秒で画面を保存してから、ほかの札より先に選ぶ。
        const auto fusion = std::find_if(offers_.begin(), offers_.end(), [&](const survivor::PartOffer& item) {
            return item.part != survivor::PartId::Count && survivor::IsFusion(config_.parts[static_cast<size_t>(item.part)]);
        });
        if (fusion != offers_.end() && !autoFusionCaptured_) {
            if (autoChoiceTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
                RequestCapture("fusion_offer");
                autoFusionCaptured_ = true;
            }
            return;
        }
        if (autoChoiceTimer_ > 0.6f && !offers_.empty()) {
            const size_t index = fusion != offers_.end() ? static_cast<size_t>(fusion - offers_.begin()) : autoPick_++ % offers_.size();
            if (fusion != offers_.end()) {
                autoFusionLoad_[0] = survivor::LoadOf(config_.parts, partLevels_);
                autoFusionShotAt_ = autoTestTime_ + 4.0f;
            }
            const survivor::PartOffer offer = offers_[index];
            const int maxHpBefore = player.GetMaxHp();
            ChooseOffer(index);
            if (autoChoiceLog_.tellp() > 0) {
                autoChoiceLog_ << ",";
            }
            autoChoiceLog_ << "{\"t\":" << static_cast<int>(elapsed_) << ",\"part\":\""
                           << (offer.part == survivor::PartId::Count ? "repair" : config_.parts[static_cast<size_t>(offer.part)].key)
                           << "\",\"level\":" << offer.nextLevel << ",\"maxHp\":[" << maxHpBefore << "," << player.GetMaxHp() << "]}";
            autoChoiceTimer_ = 0.0f;
        }
    } else if (pendingChoices_ > 0) {
        autoChoiceTimer_ += dt;
        if (pendingChoices_ >= 2 || autoChoiceTimer_ > 3.0f) {
            OpenChoices();
            autoChoiceTimer_ = 0.0f;
        }
    }

    // 弾の集計は区画を出るときに消えるので、毎フレーム控える。再発動は、連鎖崩壊を積んでいる間の回数も分けて控える。
    {
        const auto& stats = world_.resources.bulletManager_->GetShooterStats();
        autoChainHits_ = (std::max)(autoChainHits_, stats.chainHits);
        autoDetonations_ = (std::max)(autoDetonations_, stats.detonations);
        autoWallBounces_ = (std::max)(autoWallBounces_, world_.resources.bulletManager_->GetGrowthStats().wallBounces);
        autoPierces_ = (std::max)(autoPierces_, world_.resources.bulletManager_->GetGrowthStats().actorPierces);
        autoBurstStats_ = {(std::max)(autoBurstStats_[0], stats.burstTriggers), (std::max)(autoBurstStats_[1], stats.burstRepeats),
                           (std::max)(autoBurstStats_[2], stats.burstChildren)};
        if (survivor::LevelOf(partLevels_, survivor::PartId::ChainCollapse) > 0) {
            autoFusedSeconds_ += dt;
            autoRepeatsWhileFused_ = (std::max)(autoRepeatsWhileFused_, stats.burstRepeats);
        }
    }
    // 合体の4秒後（再発動が起きている頃）の画面を保存する。
    if (autoFusionShotAt_ > 0.0f && autoTestTime_ >= autoFusionShotAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("fusion_after");
        autoFusionShotAt_ = -1.0f;
        autoFusionLoad_[1] = survivor::LoadOf(config_.parts, partLevels_);
    }

    // 案内：それぞれの案内が出て0.8秒の画面を保存する。
    if (hintShowing_ >= 0 && hintTimer_ >= 0.8f && !autoHintCaptured_[static_cast<size_t>(hintShowing_)] &&
        world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("hint_" + std::to_string(hintShowing_));
        autoHintCaptured_[static_cast<size_t>(hintShowing_)] = true;
    }
    // 演出：宿敵の登場（1.6秒）・撃破（0.9秒）・自機が倒れた演出（1.0秒）の画面を保存する。
    if (world_.run.tankRunCapturePath_.empty()) {
        if (cineMode_ == CineMode::RivalIntro && !autoIntroCaptured_ && cineTime_ >= 1.6f) {
            RequestCapture("rival_intro");
            autoIntroCaptured_ = true;
        } else if (cineMode_ == CineMode::RivalDefeat && !autoDefeatCaptured_ && cineTime_ >= 0.9f) {
            RequestCapture("rival_defeat");
            autoDefeatCaptured_ = true;
        } else if (cineMode_ == CineMode::PlayerDown && !autoDownCaptured_ && cineTime_ >= 1.0f) {
            RequestCapture("player_down");
            autoDownCaptured_ = true;
        }
    }
    // MYENGINE_SURVIVOR_DEATH=秒：その区画の時刻で無敵を外して倒れ、倒れた演出と結果画面を確かめる。
    if (autoDeathAt_ < 0.0f) {
        wchar_t death[16]{};
        autoDeathAt_ = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_DEATH", death, 16) > 0 ? static_cast<float>(_wtof(death)) : 1.0e9f;
    }
    if (!player.IsDead() && elapsed_ >= autoDeathAt_) {
        world_.presentation.debugPlayerNoDamage_ = false;
        player.SetDebugNoDamage(false);
        player.TakeDamage(1000000, 0.0f);
    }
    if (player.IsDead() && IsResultVisible() && autoTestClearedTime_ < 0.0f) {
        autoTestClearedTime_ = autoTestTime_;
    }
    // 予告：群れと輪のそれぞれ最初の予告を、湧く0.5秒前に保存する。寄ってくる敵の数の最大も見る。
    for (const PendingWave& wave : pendingWaves_) {
        bool& captured = wave.ring ? autoRingWarningCaptured_ : autoSwarmWarningCaptured_;
        if (!captured && elapsed_ >= wave.at - 0.5f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture(wave.ring ? "ring_warning" : "swarm_warning");
            captured = true;
        }
    }
    autoMaxChasers_ = (std::max)(autoMaxChasers_, CountChasers());
    // 包囲の輪は最初の輪の1秒後、宿敵は現れて10秒後の画面を保存する。
    if (ringsSpawned_ == 1 && !autoRingCaptured_ && elapsed_ >= nextRing_ - config_.threat.ringInterval + 1.0f &&
        world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("ring");
        autoRingCaptured_ = true;
    }
    if (IsRivalActive() && !autoRivalCaptured_ && elapsed_ >= rivalSpawnedAt_ + 10.0f && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("rival");
        autoRivalCaptured_ = true;
    }
    if (autoTestNextCapture_ < static_cast<int>(kSectorCaptureSeconds.size()) &&
        elapsed_ >= kSectorCaptureSeconds[static_cast<size_t>(autoTestNextCapture_)]) {
        RequestCapture("sector_" + std::to_string(static_cast<int>(kSectorCaptureSeconds[static_cast<size_t>(autoTestNextCapture_)])));
        ++autoTestNextCapture_;
    }

    const AliveCounts alive = CountAlive();
    if (elapsed_ >= 120.0f) {
        maxAliveAfterTwoMinutes_ = (std::max)(maxAliveAfterTwoMinutes_, alive.total);
    }
    if (autoTestTime_ >= autoTestNextSample_) {
        autoTestNextSample_ += 10.0f;
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sampleStart_).count();
        const double fps = seconds > 0.0 ? static_cast<double>(drawnFrames_ - sampleFrames_) / seconds : 0.0;
        sampleStart_ = now;
        sampleFrames_ = drawnFrames_;
        if (autoTestLog_.tellp() > 0) {
            autoTestLog_ << ",";
        }
        autoTestLog_ << "{\"t\":" << static_cast<int>(elapsed_) << ",\"shapes\":" << alive.shapes << ",\"tanks\":" << alive.tanks
                     << ",\"big\":" << alive.bigPentagons << ",\"total\":" << alive.total << ",\"fps\":" << std::fixed
                     << std::setprecision(1) << fps << std::defaultfloat << ",\"hp\":" << player.GetHp() << ",\"level\":" << level_
                     << ",\"collected\":" << expCollected_ << ",\"dropped\":" << expDropped_ << ",\"shards\":" << pickups_.GetCount()
                     << ",\"shots\":" << player.GetPrimaryAttackCount() - autoSampleShots_ << ",\"heat\":" << player.GetHeat().GetHeat()
                     << ",\"burst\":" << autoBurstStats_[0] << ",\"repeat\":" << autoBurstStats_[1]
                     << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"threat\":" << threatStage_
                     << ",\"rivalHp\":" << (IsRivalActive() ? world_.resources.enemy_->GetHp() : -1) << "}";
        autoSampleShots_ = player.GetPrimaryAttackCount();
    }

    if (cleared_ && autoTestClearedTime_ < 0.0f) {
        autoTestClearedTime_ = autoTestTime_;
    }
    const bool timedOut = autoTestTime_ > kSectorAutoTestExitSeconds + 90.0f;
    if ((autoTestClearedTime_ >= 0.0f && autoTestTime_ - autoTestClearedTime_ > 1.5f) || timedOut) {
        if (!autoTestCaptureQueued_) {
            RequestCapture(cleared_ ? "cleared" : "timeout");
            autoTestCaptureQueued_ = true;
            return;
        }
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        // 調整パネルの保存：今の設定を別のファイルへ書いて読み戻し、主な値が同じになるかを見る。
        bool roundTrip = false;
        {
            const std::string path = std::string(kAutoTestDirectory) + "/config_roundtrip.json";
            std::error_code ignored;
            std::filesystem::copy_file(kConfigPath, path, std::filesystem::copy_options::overwrite_existing, ignored);
            std::string error;
            survivor::SpawnConfig loaded;
            if (survivor::SaveSpawnConfig(path, config_, error) && survivor::LoadSpawnConfig(path, loaded, error)) {
                roundTrip = loaded.ExpToNextLevel(10) == config_.ExpToNextLevel(10) && loaded.heat.maxHeat == config_.heat.maxHeat &&
                            loaded.heat.coolPerSecondWhileFiring == config_.heat.coolPerSecondWhileFiring &&
                            loaded.shapeTargetAlive == config_.shapeTargetAlive && loaded.choiceSlowTimeScale == config_.choiceSlowTimeScale &&
                            loaded.parts[0].perLevel == config_.parts[0].perLevel && loaded.forms[1].damageScale == config_.forms[1].damageScale &&
                            loaded.tankTypes == config_.tankTypes;
            }
        }
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"configRoundTrip\":" << (roundTrip ? "true" : "false") << ",\"completed\":" << (cleared_ ? "true" : "false") << ",\"cleared\":" << (cleared_ ? "true" : "false")
            << ",\"sectorSeconds\":" << elapsed_ << ",\"exitOpen\":" << (exitOpen_ ? "true" : "false") << ",\"exit\":[" << exitPosition_.x
            << "," << exitPosition_.y << "],\"maxAliveAfterTwoMinutes\":" << maxAliveAfterTwoMinutes_
            << ",\"resultState\":" << static_cast<int>(world_.combat.combatFlow_.GetState()) << ",\"level\":" << level_
            << ",\"pendingChoices\":" << pendingChoices_ << ",\"choicesTaken\":" << choicesTaken_
            << ",\"minTimeScaleWhileOpen\":" << autoMinTimeScale_ << ",\"partLevels\":{" <<
            [&] {
                std::ostringstream levels;
                for (size_t i = 0; i < survivor::kPartCount; ++i) {
                    levels << (i ? "," : "") << "\"" << config_.parts[i].key << "\":" << partLevels_[i];
                }
                return levels.str();
            }()
            << "},\"maxHp\":" << player.GetMaxHp() << ",\"choices\":[" << autoChoiceLog_.str() << "]"
            << ",\"expCollected\":" << expCollected_ << ",\"expDropped\":" << expDropped_ << ",\"maxHeat\":" << autoMaxHeat_
            << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"form\":\"" << formKey_
            << "\",\"formChosenAt\":" << autoFormChosenAt_ << ",\"formPath\":\"" << formPath_ << "\",\"evolvedAt\":" << autoEvolvedAt_
            << ",\"evolveOffers\":" << autoEvolveOffers_ << ",\"formBarrels\":"
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->barrels.size() : 0) << ",\"formSpread\":"
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->spreadAngleDeg : 0.0f) << ",\"formFan\":"
            << (player.GetActiveClassConfig() && !player.GetActiveClassConfig()->barrels.empty()
                    ? player.GetActiveClassConfig()->barrels.front().angleDeg
                    : 0.0f)
            << ",\"loaderRamp\":" << (MakePartEffects().loaderRamp ? "true" : "false") << ",\"detonations\":" << autoDetonations_
            << ",\"wallBounces\":" << autoWallBounces_ << ",\"pierces\":" << autoPierces_ << ",\"formClass\":\""
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->id : std::string("none"))
            << "\",\"railShots\":" << player.GetSpecialCombatStats().railShots << ",\"critShots\":" << player.GetCritShots()
            << ",\"spikeKills\":" << spikeKills_ << ",\"spikeProbe\":" << autoSpikeProbe_ << ",\"premiumChests\":" << premiumTotal_
            << ",\"dashExplosions\":" << player.GetDashExplosionCount() << ",\"maxStamina\":" << player.GetStats().maxStamina
            << ",\"relics\":[" << autoRelicLog_.str() << "],\"dashBlocked\":" << player.GetDashBlocked()
            << ",\"stillShots\":" << player.GetStillShots() << ",\"trait\":\"" << survivor::TraitKey(trait_) << "\",\"coolPerSecond\":" << player.GetHeat().GetConfig().coolPerSecond
            << ",\"coolPerSecondWhileFiring\":" << player.GetHeat().GetConfig().coolPerSecondWhileFiring
            << ",\"heatMax\":" << player.GetHeat().GetMaxHeat() << ",\"shieldMax\":" << player.GetShieldMax() << ",\"shieldCheck\":[" << autoShieldLog_.str() << "]"
            << ",\"threatStage\":" << threatStage_ << ",\"rings\":" << ringsSpawned_ << ",\"rivalSpawnedAt\":" << rivalSpawnedAt_
            << ",\"rivalDefeatedAt\":" << rivalDefeatedAt_ << ",\"removals\":" << removals_ << ",\"maxLoad\":" << maxLoad_
            << ",\"loadAtEnd\":" << survivor::LoadOf(config_.parts, partLevels_) << ",\"capacityAtEnd\":" << Capacity()
            << ",\"removalLog\":[" << removalRecord_.str() << "]" << ",\"qUses\":" << qUses_ << ",\"qLog\":[" << qRecord_.str() << "]"
            << ",\"swarms\":" << swarmsSpawned_ << ",\"wavesSkipped\":" << wavesSkipped_ << ",\"heavySquares\":" << heavySpawned_
            << ",\"lungePentagons\":" << lungeSpawned_ << ",\"maxChasers\":" << autoMaxChasers_ << ",\"qSwapProbe\":" << autoQSwapProbe_ << ",\"purges\":" << purges_ << ",\"moraleKills\":" << moraleKills_
            << ",\"effects\":{\"shotDamageScale\":" << MakePartEffects().shotDamageScale << ",\"residualHeat\":" << MakePartEffects().residualHeat
            << ",\"ramShield\":" << MakePartEffects().ramShield << ",\"stillDamageBonus\":" << MakePartEffects().stillDamageBonus
            << ",\"bodyDamage\":" << player.GetStats().bodyDamage
            << ",\"colliderDamage\":" << player.GetDamage() << ",\"shield\":\"" << player.GetShield() << "/" << player.GetShieldMax() << "\"}"
            << ",\"fusions\":" << fusions_ << ",\"fusionAt\":" << fusionTime_ << ",\"fusionLoad\":[" << autoFusionLoad_[0] << ","
            << autoFusionLoad_[1] << "],\"burstTriggers\":" << autoBurstStats_[0] << ",\"burstRepeats\":" << autoBurstStats_[1]
            << ",\"burstChildren\":" << autoBurstStats_[2] << ",\"repeatsWhileFused\":" << autoRepeatsWhileFused_
            << ",\"damageNumbers\":" << damageNumbers_.GetSpawnedCount() << ",\"fusedSeconds\":" << autoFusedSeconds_ << ",\"chainHits\":" << autoChainHits_ << ",\"criticalBlasts\":" << criticalBlasts_
            << ",\"partsAtEnd\":\"" << DescribeParts(99) << "\"" 
            << ",\"samples\":[" << autoTestLog_.str() << "]}\n";
        autoTest_ = AutoTest::None;
        PostQuitMessage(0);
    }
}

void SurvivorRunController::UpdateTourAutoTest(float dt)
{
    // 各地点へまっすぐ進み、着いたら（または10秒で）止まってカメラが落ち着くのを待ち、画面を保存する。
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    autoTestLegTime_ += dt;
    if (autoTourForm_ && formStage_ == 0) {
        EvolveTo(static_cast<survivor::FormId>(autoFormIndex_));
    }
    const myEngine::Vector3 position = player.GetWorldPosition();
    const myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};

    if (autoTestCaptureQueued_) {
        // 保存の要求は描画で写され、次の更新の初めに書き出されて空に戻る。
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        autoTestCaptureQueued_ = false;
        autoTestSettling_ = false;
        autoTestLegTime_ = 0.0f;
        ++autoTestWaypoint_;
    }

    if (autoTestWaypoint_ >= kTourWaypoints.size()) {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), minX, minY, maxX, maxY);
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"completed\":true,\"arena\":[" << minX << "," << minY << "," << maxX << "," << maxY << "],\"halfView\":["
            << arenacamera::HalfViewHeight() * arenacamera::kAspect << "," << arenacamera::HalfViewHeight() << "],\"waypoints\":["
            << autoTestLog_.str() << "],\"enemies\":" << world_.resources.enemyManager_->GetEnemyCount()
            << ",\"rivalActive\":" << (world_.gameplayQueries->IsRunRivalActive() ? "true" : "false")
            << ",\"menuOpen\":" << (world_.arenaRunController->IsTankRunMenuOpen() ? "true" : "false")
            << ",\"playerDead\":" << (player.IsDead() ? "true" : "false") << ",\"seconds\":" << autoTestTime_ << "}\n";
        autoTest_ = AutoTest::None;
        PostQuitMessage(0);
        return;
    }

    const AutoTestWaypoint& waypoint = kTourWaypoints[autoTestWaypoint_];
    const myEngine::Vector3 offset{waypoint.target.x - position.x, waypoint.target.y - position.y, 0.0f};
    const float distance = myEngine::Length(offset);
    if (!autoTestSettling_ && (distance < 1.5f || autoTestLegTime_ > 10.0f)) {
        autoTestSettling_ = true;
        autoTestLegTime_ = 0.0f;
    }
    if (!autoTestSettling_) {
        player.SetDemoInput(true, {offset.x / distance, offset.y / distance}, aim, false, false);
        return;
    }
    // 止まっている間は撃ち、主砲のまわりの熱の輪が画面に写るようにする。
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, true, false);
    if (autoTestLegTime_ < 1.2f) {
        return;
    }
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    if (autoTestWaypoint_ > 0) {
        autoTestLog_ << ",";
    }
    autoTestLog_ << "{\"name\":\"" << waypoint.name << "\",\"target\":[" << waypoint.target.x << "," << waypoint.target.y
                 << "],\"player\":[" << position.x << "," << position.y << "],\"camera\":[" << camera.x << "," << camera.y
                 << "],\"reached\":" << (distance < kAutoTestReachDistance ? "true" : "false")
                 << ",\"aimError\":" << MeasureAimRoundTripError() << ",\"heat\":" << player.GetHeat().GetHeat() << "}";
    RequestCapture(waypoint.name);
    autoTestCaptureQueued_ = true;
}

void SurvivorRunController::UpdatePerfAutoTest(float dt)
{
    // 0〜15秒：敵を200体まで湧かせる（自機は中央で止まり、撃たない）。
    // 15〜35秒：散らばった場面を測る（敵はほぼ画面の外）。35秒から：画面の外の図形を2秒ごとに画面の中へ移し、
    // 自機は撃ち始める。45〜65秒：画面が敵で埋まった場面を測る。
    constexpr float kSpreadFrom = 15.0f, kSpreadTo = 35.0f, kCrowdFrom = 45.0f, kCrowdTo = 65.0f;
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (const ExpEnemy* nearest = world_.resources.enemyManager_->FindNearestEnemy(position, 30.0f)) {
        aim = nearest->GetWorldPosition();
    }
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, perfCrowded_, false);
    if (autoTestTime_ >= kSpreadTo && (!perfCrowded_ || std::fmod(autoTestTime_ - kSpreadTo, 2.0f) < dt)) {
        perfCrowded_ = true;
        const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
        const float halfHeight = arenacamera::HalfViewHeight() - 1.5f;
        const float halfWidth = arenacamera::HalfViewHeight() * arenacamera::kAspect - 1.5f;
        std::uniform_real_distribution<float> x(camera.x - halfWidth, camera.x + halfWidth), y(camera.y - halfHeight, camera.y + halfHeight);
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || enemy->IsExpeditionCombatRole() || enemy->GetType() == ExpEnemyType::Shooter) {
                continue;
            }
            const myEngine::Vector3 at = enemy->GetWorldPosition();
            if (std::abs(at.x - camera.x) <= halfWidth && std::abs(at.y - camera.y) <= halfHeight) {
                continue;
            }
            for (int attempt = 0; attempt < 8; ++attempt) {
                const myEngine::Vector3 target{x(random_), y(random_), at.z};
                const float dx = target.x - position.x, dy = target.y - position.y;
                if (dx * dx + dy * dy > 16.0f && !world_.resources.stage_->IsCollisionWithAnyBlock(target, 1.2f)) {
                    enemy->SetWorldPosition(target);
                    break;
                }
            }
        }
    }
    if (autoTestTime_ >= 30.0f && autoTestNextCapture_ == 0 && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("perf_spread");
        ++autoTestNextCapture_;
    }
    if (autoTestTime_ >= 60.0f && autoTestNextCapture_ == 1 && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("perf_crowd");
        ++autoTestNextCapture_;
    }

    // 1秒ごとに、その1秒の fps・敵の数・画面の中の敵の数を記録する。
    if (autoTestTime_ >= perfNextSample_) {
        perfNextSample_ += 1.0f;
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sampleStart_).count();
        const double fps = seconds > 0.0 ? static_cast<double>(drawnFrames_ - sampleFrames_) / seconds : 0.0;
        sampleStart_ = now;
        sampleFrames_ = drawnFrames_;
        const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
        const float halfHeight = arenacamera::HalfViewHeight();
        const float halfWidth = halfHeight * arenacamera::kAspect;
        int alive = 0, onScreen = 0;
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }
            ++alive;
            const myEngine::Vector3 at = enemy->GetWorldPosition();
            if (std::abs(at.x - camera.x) <= halfWidth && std::abs(at.y - camera.y) <= halfHeight) {
                ++onScreen;
            }
        }
        const float t = autoTestTime_ - 1.0f;
        const int phase = t >= kSpreadFrom && t < kSpreadTo ? 0 : t >= kCrowdFrom && t < kCrowdTo ? 1 : -1;
        if (phase >= 0) {
            PerfPhase& sum = perfPhases_[static_cast<size_t>(phase)];
            ++sum.samples;
            sum.fpsSum += fps;
            sum.fpsMin = (std::min)(sum.fpsMin, fps);
            sum.aliveSum += alive;
            sum.onScreenSum += onScreen;
        }
        perfLog_ << (perfLog_.tellp() > 0 ? "," : "") << "{\"t\":" << static_cast<int>(t) << ",\"fps\":" << std::fixed << std::setprecision(1)
                 << fps << std::defaultfloat << ",\"alive\":" << alive << ",\"onScreen\":" << onScreen
                 << ",\"bullets\":" << world_.resources.bulletManager_->GetBulletCount() << "}";
    }

    if (autoTestTime_ < kCrowdTo + 0.5f || !world_.run.tankRunCapturePath_.empty()) {
        return;
    }
    auto phaseJson = [](const char* name, const PerfPhase& sum) {
        std::ostringstream text;
        const double n = (std::max)(1, sum.samples);
        text << "{\"name\":\"" << name << "\",\"samples\":" << sum.samples << ",\"fpsAvg\":" << std::fixed << std::setprecision(1)
             << sum.fpsSum / n << ",\"fpsMin\":" << (sum.samples > 0 ? sum.fpsMin : 0.0) << ",\"aliveAvg\":" << sum.aliveSum / n
             << ",\"onScreenAvg\":" << sum.onScreenSum / n << "}";
        return text.str();
    };
    std::ofstream(std::string(kAutoTestDirectory) + "/perf.json")
        << "{\"completed\":true,\"phases\":[" << phaseJson("spread", perfPhases_[0]) << "," << phaseJson("crowd", perfPhases_[1])
        << "],\"separationMsAvg\":" << (separationCalls_ > 0 ? separationSeconds_ * 1000.0 / static_cast<double>(separationCalls_) : 0.0)
        << ",\"samples\":[" << perfLog_.str() << "]}\n";
    autoTest_ = AutoTest::None;
    PostQuitMessage(0);
}

} // namespace gameplay
