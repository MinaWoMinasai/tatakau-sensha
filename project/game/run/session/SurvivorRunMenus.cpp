#include "game/run/session/SurvivorRunController.h"
#include "game/debug/MenuFlow.h"
#include "game/run/session/SurvivorRunShared.h"
#include "GameStartMode.h"
#include "game/session/GameplaySystems.h"
#include "game/settings/UserSettings.h"
#include <algorithm>
#include <cmath>

// 遊んでいる間の出入り（計画書 p11 の R1〜）：止める画面と、タイトルの後ろで戦う見本の動き。

namespace gameplay {

using namespace survivorrun;

bool SurvivorRunController::CanOpenPause() const
{
    return HeroMode() && autoTest_ == AutoTest::None && !speedTest_ && !world_.demo.titleDemo_ &&
           world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_ && !IsPaused() && !world_.resources.player_->IsDead();
}

void SurvivorRunController::OpenPause()
{
    pauseOpen_ = true;
    pauseAge_ = 0.0f;
    menuMouse_ = world_.resources.input_->GetMousePosition();
    pauseMenu_.Open({{"つづける", ""}, {"やりなおす", "もう一度きめると、やりなおす"}, {"設定", ""}, {"タイトルへ", "もう一度きめると、タイトルへ"}}, 192.0f);
    UpdatePause(0.0f);
}

bool SurvivorRunController::TryOpenPause()
{
    auto& input = *world_.resources.input_;
    const bool left = input.GetFocusLostCount() != focusLostSeen_;
    focusLostSeen_ = input.GetFocusLostCount();
    // 説明の間の Esc・Start は、ながおしで飛ばす操作。
    if (!(left || (input.IsKeyTriggered(DIK_ESCAPE) && tutorialStep_ < 0)) || !CanOpenPause()) {
        return false;
    }
    OpenPause();
    return true;
}

bool SurvivorRunController::UpdatePauseMenu(float dt, const MenuInput& in)
{
    const int picked = dt > 0.0f ? pauseMenu_.Update(dt, in) : -1;
    if (pauseMenu_.Moved()) {
        audio_.Play("ui_click");
    }
    if (picked == 0 || (dt > 0.0f && in.back)) {
        // 2回きめる項目の途中なら、戻す操作はそれをやめるだけ。
        if (picked != 0 && pauseMenu_.IsArmed()) {
            pauseMenu_.Disarm();
        } else {
            pauseOpen_ = false;
            return true;
        }
    } else if (picked == 2) {
        settings_.Open(in, true);
    } else if (picked == 1 || picked == 3) {
        audio_.Play("ui_drop");
        LeaveRun(picked == 1 ? 0 : 1);
    }
    return false;
}

void SurvivorRunController::UpdatePause(float dt)
{
    pauseAge_ += dt;
    focusLostSeen_ = world_.resources.input_->GetFocusLostCount();
    const MenuInput in = ReadMenuInput(*world_.resources.input_, menuMouse_);
    if (settings_.IsOpen()) {
        if (settings_.Update(dt, in) == SettingsScreen::Result::ReplayTutorial) {
            GameStartSession::RequestTutorial();
            LeaveRun(0);
            return;
        }
        if (settings_.Moved()) {
            audio_.Play("ui_click");
        }
        menuKit_.Begin();
        if (settings_.IsOpen()) {
            settings_.Queue(menuKit_);
            menuKit_.End();
            return;
        }
        menuKit_.End();
    } else if (UpdatePauseMenu(dt, in)) {
        return;
    }
    menuKit_.Begin((std::min)(1.0f, pauseAge_ / 0.10f + 0.2f));
    menuKit_.Darken(NeonMenuKit::Shade::Pause);
    pauseMenu_.Queue(menuKit_);
    QueueGear(198.0f, false);
    menuKit_.Hints({{{"W", "S"}, "L", "えらぶ"}, {{"Enter"}, "A", "きめる"}, {{"Esc"}, "B", "もどる"}}, in.pad);
    menuKit_.End();
}

void SurvivorRunController::QueueGear(float top, bool oneRow)
{
    // 主武器と Q（と専用スキル）・副武器・パークの順に、右寄せで並べる。oneRow なら、空の枠を出さずに2段へ詰める。
    const auto groups = OwnedGroups();
    if (groups.size() < 4) {
        return;
    }
    std::vector<std::vector<SurvivorLevelUpScreen::Slot>> rows{groups[0].slots, groups[1].slots, groups[2].slots};
    rows[0].insert(rows[0].end(), groups[3].slots.begin(), groups[3].slots.end());
    if (oneRow) {
        for (auto& row : rows) {
            std::erase_if(row, [](const auto& slot) {
                return slot.icon.empty();
            });
        }
        rows[1].insert(rows[1].end(), rows[2].begin(), rows[2].end());
        rows.pop_back();
    }
    constexpr float kRight = 1203.2f, kStep = 76.8f, kRowStep = 89.6f;
    float y = top;
    for (const auto& row : rows) {
        float x = kRight - 32.0f - kStep * static_cast<float>(row.empty() ? 0 : row.size() - 1);
        for (const auto& slot : row) {
            menuKit_.Cell(slot.icon, x, y, slot.color, slot.tier, slot.maxTier);
            x += kStep;
        }
        y += row.empty() ? 0.0f : kRowStep;
    }
}

void SurvivorRunController::UpdateResultScreen(float dt)
{
    if (!resultOpen_) {
        resultOpen_ = true;
        resultAge_ = 0.0f;
        resultBest_ = usersettings::Get().bestSeconds;
        if (elapsed_ > resultBest_) {
            usersettings::Get().bestSeconds = elapsed_;
            usersettings::Save();
        }
        menuMouse_ = world_.resources.input_->GetMousePosition();
        resultList_.Open({{"もう1回", ""}, {"タイトルへ", ""}}, 553.0f);
    }
    resultAge_ += dt;
    const MenuInput in = ReadMenuInput(*world_.resources.input_, menuMouse_);
    // 倒れた瞬間の連打で、すぐに次へ進まないようにする。
    constexpr float kLockSeconds = 0.6f;
    if (resultAge_ > kLockSeconds) {
        const int picked = resultList_.Update(dt, in);
        if (resultList_.Moved()) {
            audio_.Play("ui_click");
        }
        if (picked >= 0 || world_.resources.input_->IsKeyTriggered(DIK_R)) {
            audio_.Play("ui_drop");
            LeaveRun((std::max)(0, picked));
        }
    }
    QueueResultScreen();
}

void SurvivorRunController::QueueResultScreen()
{
    const bool clear = cleared_;
    const auto color = clear ? menucolor::kGold : menucolor::kGreen;
    menuKit_.Begin((std::min)(1.0f, 0.25f + resultAge_ / 0.25f));
    menuKit_.Darken(NeonMenuKit::Shade::Full);
    // 生きた時間。
    const int total = static_cast<int>(elapsed_);
    const std::string seconds = std::to_string(total % 60);
    menuKit_.GlowText(std::to_string(total / 60) + ":" + (seconds.size() < 2 ? "0" : "") + seconds, 154.0f, 74.0f, 230.0f, 0.0f, menucolor::kWhite, color,
                      1.0f);
    // 10分の管：どこまで進んだか。目盛りは出来事、白い印は前の最高。
    const float whole = (std::max)(1.0f, config_.exitOpenSeconds);
    constexpr float kLeft = 99.3f, kRight = 1180.5f, kY = 396.8f;
    auto at = [&](float second) {
        return kLeft + (kRight - kLeft) * (std::clamp)(second / whole, 0.0f, 1.0f);
    };
    // 始めに、管が左から伸びる。
    const float grow = 1.0f - std::pow(1.0f - (std::min)(1.0f, resultAge_ / 0.7f), 3.0f);
    const myEngine::Vector4 track{0.149f, 0.169f, 0.204f, 1.0f};
    menuKit_.Tube({kLeft, kY}, {kRight, kY}, 5.4f, track, track, 0.0f, 0.0f);
    const float end = at(elapsed_ * grow);
    if (end > kLeft + 1.0f) {
        menuKit_.Tube({kLeft, kY}, {end, kY}, 5.4f, menucolor::kWhite, color, 1.0f, 26.0f);
    }
    for (const auto& event : config_.timeline) {
        if (event.kind == "surge") {
            continue;
        }
        const float x = at(event.at);
        const auto tick = event.at <= elapsed_ ? color : myEngine::Vector4{0.227f, 0.255f, 0.314f, 1.0f};
        menuKit_.Tube({x, kY - 18.0f}, {x, kY + 18.0f}, 2.0f, tick, tick, 0.0f, 0.0f);
    }
    if (resultBest_ > 0.5f) {
        const float x = at(resultBest_);
        menuKit_.Tube({x, kY - 30.0f}, {x, kY - 18.0f}, 4.0f, menucolor::kWhite, menucolor::kWhite, 0.4f, 10.0f);
    }
    // 倒した数・Lv・倒された相手の形。
    constexpr float kStatY = 500.0f;
    float x = 76.8f;
    menuKit_.Icon("pv_square", x + 21.0f, kStatY, 52.0f, menucolor::kGold);
    x += 54.0f;
    x += menuKit_.Text(std::to_string(world_.combat.defeatedEnemies_), 38.0f, x, kStatY, 0.0f, menucolor::kInk, true) + 52.0f;
    x += menuKit_.Text("Lv " + std::to_string(level_), 38.0f, x, kStatY, 0.0f, menucolor::kInk, true) + 52.0f;
    if (!clear) {
        // 四角・三角・五角・ボス（六角）。弾や地雷など、形の無い物は点。
        const std::string& hit = world_.resources.player_->GetSurvivorLastHit();
        auto has = [&](const char* word) {
            return hit.find(word) != std::string::npos;
        };
        const char* icon = has("地雷")   ? "sub_mine"
                           : has("三角") ? "pv_triangle"
                           : has("五角") ? "pv_pentagon"
                           : has("四角") ? "pv_square"
                           : has("ボス") ? "pv_hexagon"
                                         : "pv_dot";
        menuKit_.Icon(icon, x + 26.0f, kStatY, 52.0f, menucolor::kRed);
    }
    QueueGear(122.0f, true);
    resultList_.Queue(menuKit_);
    menuKit_.End();
}

void SurvivorRunController::LeaveRun(int selection)
{
    // 途中でやめた回も、遊んだ記録に残す（結果の画面まで行った回は、もう書いてある）。
    AppendPlayLog("quit");
    world_.combat.resultSelection_ = selection;
    world_.combatFlow->ConfirmResultSelection();
}

void SurvivorRunController::UpdateTitleDemoBot()
{
    // 敵がほどよく出る時刻を、行き来させる。出来事（壁・包囲など）・群れ・敵戦車は出さない。
    constexpr float kFrom = 120.0f, kTo = 180.0f;
    if (elapsed_ < kFrom || elapsed_ > kTo) {
        elapsed_ = kFrom;
    }
    nextSwarm_ = nextTank_ = nextBigPentagon_ = kTo * 100.0f;
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 self = player.GetWorldPosition();
    myEngine::Vector3 aim = self + myEngine::Vector3{10.0f, 0.0f, 0.0f};
    float nearest = 22.0f;
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const myEngine::Vector3 to = enemy->GetWorldPosition() - self;
        const float distance = std::sqrt(to.x * to.x + to.y * to.y);
        if (distance < nearest) {
            nearest = distance;
            aim = enemy->GetWorldPosition();
        }
    }
    const float time = world_.demo.titleDemoStatus_.totalSeconds;
    myEngine::Vector2 move{std::cos(time * 0.7f), std::sin(time * 1.2f) * 0.8f};
    // 場の端へ寄りすぎたら、真ん中へ戻る。
    const myEngine::Vector3 home = kPlayerStart - self;
    if (std::sqrt(home.x * home.x + home.y * home.y) > 14.0f) {
        move = {home.x, home.y};
    }
    const float length = std::sqrt(move.x * move.x + move.y * move.y);
    if (length > 1.0f) {
        move = {move.x / length, move.y / length};
    }
    player.SetDemoInput(true, move, aim, true, false);
    primaryHeld_ = true;
    world_.presentation.debugPlayerNoDamage_ = true;
}

void SurvivorRunController::ReportMenuFlow()
{
    if (!menuflow::IsActive() || world_.demo.titleDemo_) {
        return;
    }
    const bool pad = world_.resources.input_->IsGamepadActive();
    const std::string device = pad ? "p" : "k";
    std::string shot;
    if (world_.run.tankRunCapturePath_.empty() && menuflow::TakeShot(shot)) {
        RequestCapture(shot);
    }
    // 確認用の命令：倒れる・クリア・時間を進める・持ち物を増やす。
    if (menuflow::TakeCommand("die")) {
        menuFlowDie_ = true;
    }
    if (menuFlowDie_) {
        world_.resources.player_->Damage(9999);
    }
    if (menuflow::TakeCommand("clear")) {
        rivalSpawned_ = rivalDefeated_ = true;
        FinishWave();
    }
    if (menuflow::TakeCommand("late")) {
        elapsed_ = 402.0f;
        timelineNext_ = config_.timeline.size();
    }
    if (menuflow::TakeCommand("end")) {
        elapsed_ = 596.0f;
        timelineNext_ = config_.timeline.size();
    }
    if (menuflow::TakeCommand("gear")) {
        subWeaponTiers_[0] = 4;
        subWeaponTiers_[1] = 3;
        subWeaponTiers_[2] = 1;
        perks_[0] = 2;
        perks_[1] = 1;
        perks_[2] = 3;
        heroSkillTiers_[0] = 2;
        world_.combat.defeatedEnemies_ = 1207;
        level_ = 14;
    }
    // 目標：説明の輪・欠片・いちばん近い敵。
    const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
    myEngine::Vector3 target = self + myEngine::Vector3{1.0f, 0.0f, 0.0f};
    if (tutorialStep_ == 0) {
        target = TutorialMark();
    } else if (tutorialStep_ == 2) {
        pickups_.FindNearest(self, 1000.0f, target);
    } else {
        float best = 1.0e9f;
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (enemy && !enemy->IsDead()) {
                const myEngine::Vector3 at = enemy->GetWorldPosition();
                if (const float d = std::hypot(at.x - self.x, at.y - self.y); d < best) {
                    best = d;
                    target = at;
                }
            }
        }
    }
    const myEngine::Vector2 from = world_.gameplayHud->WorldToScreen(self), to = world_.gameplayHud->WorldToScreen(target);
    menuflow::SetTarget(from.x, from.y, to.x, to.y);
    if (pauseOpen_ && settings_.IsOpen()) {
        menuflow::SetScreen("settings", settings_.Describe());
    } else if (pauseOpen_) {
        menuflow::SetScreen("pause", std::to_string(pauseMenu_.GetSelected()) + (pauseMenu_.IsArmed() ? "!" : "") + device);
    } else if (heroLevelUpOpen_) {
        menuflow::SetScreen("cards", device);
    } else if (inventoryOpen_) {
        menuflow::SetScreen("inventory", device);
    } else if (IsResultVisible()) {
        menuflow::SetScreen("result", std::to_string(resultList_.GetSelected()) + device);
    } else if (tutorialStep_ >= 0) {
        menuflow::SetScreen("tutorial" + std::to_string(tutorialStep_), std::to_string(tutorialStage_) + device);
    } else if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_) {
        menuflow::SetScreen("play", device);
    } else {
        menuflow::SetScreen("dying");
    }
}

} // namespace gameplay
