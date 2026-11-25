#include "game/run/session/SurvivorRunController.h"
#include "game/debug/MenuFlow.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
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
    pauseMenu_.Open({{"つづける", ""}, {"やりなおす", "もう一度きめると、やりなおす"}, {"タイトルへ", "もう一度きめると、タイトルへ"}}, 192.0f);
    UpdatePause(0.0f);
}

bool SurvivorRunController::TryOpenPause()
{
    auto& input = *world_.resources.input_;
    const bool left = input.GetFocusLostCount() != focusLostSeen_;
    focusLostSeen_ = input.GetFocusLostCount();
    if (!(left || input.IsKeyTriggered(DIK_ESCAPE)) || !CanOpenPause()) {
        return false;
    }
    OpenPause();
    return true;
}

void SurvivorRunController::UpdatePause(float dt)
{
    pauseAge_ += dt;
    focusLostSeen_ = world_.resources.input_->GetFocusLostCount();
    const MenuInput in = ReadMenuInput(*world_.resources.input_, menuMouse_);
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
            return;
        }
    } else if (picked == 1 || picked == 2) {
        audio_.Play("ui_drop");
        LeaveRun(picked - 1);
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

void SurvivorRunController::LeaveRun(int selection)
{
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
    if (pauseOpen_) {
        menuflow::SetScreen("pause", std::to_string(pauseMenu_.GetSelected()) + (pauseMenu_.IsArmed() ? "!" : "") + device);
    } else if (heroLevelUpOpen_) {
        menuflow::SetScreen("cards", device);
    } else if (inventoryOpen_) {
        menuflow::SetScreen("inventory", device);
    } else if (IsResultVisible()) {
        menuflow::SetScreen("result", std::to_string(world_.combat.resultSelection_) + device);
    } else if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !cleared_) {
        menuflow::SetScreen("play", device);
    } else {
        menuflow::SetScreen("dying");
    }
}

} // namespace gameplay
