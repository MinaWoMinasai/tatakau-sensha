#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "GameStartMode.h"
#include "game/session/GameplaySystems.h"
#include "game/settings/UserSettings.h"
#include <algorithm>
#include <array>
#include <cmath>

// はじめての説明（計画書 p11 の R3）。文を読ませず、動く → 狙う → 拾う → えらぶ → Q を、実際にやると進む。
// 途中で倒れない・時間は進まない。終わったら、そのまま本番が始まる。絵は管の線で、自機の上に出す。

namespace gameplay {

using namespace survivorrun;

namespace {
constexpr float kUnit = 34.0f; // 絵の大きさ（論理座標）。
constexpr float kPi = 3.14159265f;
constexpr float kSkipSeconds = 0.9f;
// 1段目で通る2つの輪（始めの自機からの位置）。
constexpr std::array<std::array<float, 2>, 2> kMarks{{{7.5f, 2.5f}, {-5.5f, -3.5f}}};
// 2段目の止まった四角（段が始まったときの自機からの位置）。
constexpr std::array<std::array<float, 2>, 3> kTargets{{{9.0f, 3.0f}, {-8.0f, 4.5f}, {3.0f, -6.5f}}};
const myEngine::Vector4 kGrey{0.588f, 0.627f, 0.667f, 1.0f};
const myEngine::Vector4 kPlate{0.031f, 0.031f, 0.047f, 0.96f};

/// @brief キーやボタンの絵。押している間は光る。
void Cap(NeonMenuKit& kit, float x, float y, float size, const std::string& label, bool lit, bool round)
{
    const auto color = lit ? menucolor::kGreen : kGrey;
    const auto core = menucolor::Core(color, lit ? 0.86f : 0.5f);
    const float glow = lit ? 1.0f : 0.25f, line = 2.0f, h = size * 0.5f;
    if (round) {
        kit.Tube({x - 0.1f, y}, {x + 0.1f, y}, size * 1.1f, kPlate, kPlate, 0.0f, 0.0f);
        kit.Ring({x, y}, size * 0.55f, line, core, color, glow, 14.0f);
    } else {
        kit.Rect(x - h, y - h, size, size, kPlate);
        kit.Tube({x - h, y - h}, {x + h, y - h}, line, core, color, glow, 14.0f);
        kit.Tube({x - h, y + h}, {x + h, y + h}, line, core, color, glow, 14.0f);
        kit.Tube({x - h, y - h}, {x - h, y + h}, line, core, color, glow, 14.0f);
        kit.Tube({x + h, y - h}, {x + h, y + h}, line, core, color, glow, 14.0f);
    }
    kit.Text(label, size * 0.55f, x, y, 0.5f, lit ? menucolor::kWhite : myEngine::Vector4{0.784f, 0.816f, 0.831f, 1.0f}, true);
}

/// @brief スティックの絵（輪の中で、倒した向きへ丸が動く）。倒していなければ、ゆっくり回して見せる。
void Stick(NeonMenuKit& kit, float x, float y, const std::string& label, float sx, float sy, bool used, float time)
{
    kit.Ring({x, y}, kUnit, 2.0f, menucolor::Core(kGrey, 0.5f), kGrey, 0.25f, 12.0f);
    const float dx = used ? sx * 0.5f : std::cos(time * 2.0f) * 0.35f, dy = used ? -sy * 0.5f : std::sin(time * 2.0f) * 0.35f;
    Cap(kit, x + dx * kUnit, y + dy * kUnit, kUnit * 1.1f, label, used, true);
}

/// @brief 向きの矢印。
void Arrow(NeonMenuKit& kit, float x, float y, float angle, const myEngine::Vector4& color)
{
    const float c = std::cos(angle), s = std::sin(angle);
    auto at = [&](float u, float v) {
        return myEngine::Vector2{x + (u * c - v * s) * kUnit, y + (u * s + v * c) * kUnit};
    };
    kit.Tube(at(-0.5f, -0.45f), at(0.35f, 0.0f), 3.0f, menucolor::Core(color), color, 0.9f, 16.0f);
    kit.Tube(at(0.35f, 0.0f), at(-0.5f, 0.45f), 3.0f, menucolor::Core(color), color, 0.9f, 16.0f);
}

/// @brief 画面の外にある物の向きを、画面の端に出す。
void EdgeArrow(NeonMenuKit& kit, const myEngine::Vector2& self, const myEngine::Vector2& target, const myEngine::Vector4& color)
{
    constexpr float kMarginX = 70.0f, kMarginY = 64.0f;
    const float dx = target.x - self.x, dy = target.y - self.y;
    if (target.x > kMarginX && target.x < 1280.0f - kMarginX && target.y > kMarginY && target.y < 720.0f - kMarginY) {
        return;
    }
    const float roomX = dx > 0.0f ? 1280.0f - kMarginX - self.x : self.x - kMarginX;
    const float roomY = dy > 0.0f ? 720.0f - kMarginY - self.y : self.y - kMarginY;
    const float k = (std::min)(roomX / (std::max)(0.01f, std::abs(dx)), roomY / (std::max)(0.01f, std::abs(dy)));
    Arrow(kit, self.x + dx * k, self.y + dy * k, std::atan2(dy, dx), color);
}
} // namespace

void SurvivorRunController::StartTutorialIfNeeded()
{
    const bool requested = GameStartSession::ConsumeTutorialRequest();
    if (!HeroMode() || !config_.heroes.single || !config_.heroes.onlyUltimate || !LevelUpScreenOn() || autoTest_ != AutoTest::None || speedTest_ ||
        world_.demo.titleDemo_ || (!requested && usersettings::Get().tutorialDone)) {
        return;
    }
    guideKit_.Initialize(900);
    tutorialStep_ = 0;
    tutorialStage_ = 0;
    tutorialTime_ = 0.0f;
    tutorialSkip_ = 0.0f;
    tutorialOrigin_ = world_.resources.player_->GetWorldPosition();
    tutorialNoDamageBefore_ = world_.presentation.debugPlayerNoDamage_;
    world_.presentation.debugPlayerNoDamage_ = true;
}

void SurvivorRunController::NextTutorialStep()
{
    ++tutorialStep_;
    tutorialStage_ = 0;
    const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
    tutorialOrigin_ = self;
    audio_.Play("ui_drop");
    if (tutorialStep_ == 1) {
        for (const auto& [x, y] : kTargets) {
            if (SpawnShape("Square", self + myEngine::Vector3{x, y, 0.0f}, false)) {
                ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
                enemy->SetPlayerChase(0.0f, 0.0f);
                enemy->SetHp((std::max)(2, static_cast<int>(world_.resources.player_->GetStats().bulletDamage * 2.0f)));
            }
        }
    } else if (tutorialStep_ == 2) {
        // 倒した四角の場所に、欠片を1つずつ。
        pickups_.Clear();
        for (const myEngine::Vector3& at : tutorialDrops_) {
            pickups_.Drop(at, 1, random_);
        }
    } else if (tutorialStep_ == 3) {
        // 経験値が満タンになり、強化の画面が開く。
        exp_ = 0;
        ++level_;
        levelTimes_.push_back(elapsed_);
        ++heroPendingLevelUps_;
        RequestHeroLevelUp();
    } else if (tutorialStep_ == 4) {
        ultimateCharge_ = Hero().ultimateCharge;
        static constexpr std::array<const char*, 3> kShapes{"Square", "Triangle", "Pentagon"};
        for (int i = 0; i < 10; ++i) {
            const float angle = static_cast<float>(i) * 2.0f * kPi / 10.0f;
            const size_t kind = static_cast<size_t>(i % 3);
            if (SpawnShape(kShapes[kind], self + myEngine::Vector3{std::cos(angle) * 12.0f, std::sin(angle) * 8.5f, 0.0f}, false)) {
                ExpEnemy* enemy = world_.resources.enemyManager_->GetEnemyPtrs().back();
                enemy->SetPlayerChase(config_.shapeChase[kind].first * 0.4f, config_.shapeChase[kind].second * 0.4f);
                enemy->SetHp(1);
            }
        }
    } else if (tutorialStep_ > 4) {
        EndTutorial();
    }
}

void SurvivorRunController::EndTutorial()
{
    tutorialStep_ = -1;
    // そのまま本番へ。時間・湧き方・HP・Q を始めの値に戻す（レベルと選んだ強化は残す）。
    pickups_.Clear();
    healPickups_.clear();
    elapsed_ = 0.0f;
    ResetGrowth();
    exp_ = 0;
    ultimateCharge_ = Hero().ultimateCharge * 0.1f;
    world_.combat.defeatedEnemies_ = 0;
    world_.presentation.debugPlayerNoDamage_ = tutorialNoDamageBefore_;
    world_.resources.player_->HealRunPlayer(world_.resources.player_->GetMaxHp());
    usersettings::Get().tutorialDone = true;
    usersettings::Save();
}

void SurvivorRunController::UpdateTutorial(float dt)
{
    auto& input = *world_.resources.input_;
    tutorialTime_ += dt;
    const myEngine::Vector3 self = world_.resources.player_->GetWorldPosition();
    // ながおしで飛ばす（Esc／Start）。
    const bool held = (input.GetKey()[DIK_ESCAPE] & 0x80) != 0 || input.IsGamepadButtonPress(XINPUT_GAMEPAD_START);
    tutorialSkip_ = held ? tutorialSkip_ + dt : 0.0f;
    if (tutorialSkip_ >= kSkipSeconds) {
        world_.resources.enemyManager_->ClearRunActors();
        EndTutorial();
        return;
    }
    if (tutorialStep_ < 4) {
        ultimateCharge_ = 0.0f;
    }
    switch (tutorialStep_) {
    case 0: {
        const myEngine::Vector3 mark = TutorialMark();
        if (std::hypot(mark.x - self.x, mark.y - self.y) < 1.6f) {
            audio_.Play("ui_click");
            if (++tutorialStage_ > 1) {
                NextTutorialStep();
            }
        }
        break;
    }
    case 1: {
        tutorialDrops_.clear();
        for (const auto& [x, y] : kTargets) {
            tutorialDrops_.push_back(tutorialOrigin_ + myEngine::Vector3{x, y, 0.0f});
        }
        if (CountAlive().total == 0) {
            NextTutorialStep();
        }
        break;
    }
    case 2: {
        const size_t before = pickups_.GetCount();
        pickups_.Update(dt, self, 0.5f, false);
        const int picked = static_cast<int>(before - pickups_.GetCount());
        if (picked > 0) {
            tutorialStage_ += picked;
            exp_ = config_.ExpToNextLevel(level_) * (std::min)(3, tutorialStage_) / 3;
            audio_.Play("pickup");
        }
        if (pickups_.GetCount() == 0) {
            NextTutorialStep();
        }
        break;
    }
    case 3:
        // 選び終わったら次へ（画面が開くまでの間は待つ）。
        if (heroPendingLevelUps_ == 0 && !heroLevelUpOpen_ && levelUpDelay_ <= 0.0f) {
            NextTutorialStep();
        }
        break;
    case 4:
        if (barrageTimer_ > 0.0f) {
            tutorialStage_ = 1;
        } else if (tutorialStage_ == 1) {
            NextTutorialStep();
        } else {
            ultimateCharge_ = Hero().ultimateCharge;
        }
        break;
    default:
        break;
    }
}

myEngine::Vector3 SurvivorRunController::TutorialMark() const
{
    const auto& mark = kMarks[static_cast<size_t>((std::clamp)(tutorialStage_, 0, 1))];
    return tutorialOrigin_ + myEngine::Vector3{mark[0], mark[1], 0.0f};
}

void SurvivorRunController::QueueTutorial()
{
    guideKit_.Begin();
    if (tutorialStep_ < 0) {
        guideKit_.End();
        return;
    }
    auto& input = *world_.resources.input_;
    const bool pad = input.IsGamepadActive();
    const float pulse = 0.5f + 0.5f * std::sin(tutorialTime_ * 5.0f);
    const myEngine::Vector2 self = world_.gameplayHud->WorldToScreen(world_.resources.player_->GetWorldPosition());
    auto screen = [&](const myEngine::Vector3& world) {
        return world_.gameplayHud->WorldToScreen(world);
    };
    const float unit = screen(world_.resources.player_->GetWorldPosition() + myEngine::Vector3{1.0f, 0.0f, 0.0f}).x - self.x;
    auto down = [&](int code) {
        return (input.GetKey()[static_cast<size_t>(code)] & 0x80) != 0;
    };
    const float x = self.x, y = self.y - 3.3f * kUnit;

    // 上の5本の管：進み具合。
    for (int i = 0; i < 5; ++i) {
        const float cx = 640.0f + (static_cast<float>(i) - 2.0f) * 51.2f;
        const bool on = i < tutorialStep_, current = i == tutorialStep_;
        const float blink = current ? 0.35f + 0.65f * std::sin(tutorialTime_ * 5.0f) * std::sin(tutorialTime_ * 5.0f) : 1.0f;
        const myEngine::Vector4 dim{0.235f, 0.259f, 0.306f, 1.0f};
        auto core = on || current ? menucolor::Core(menucolor::kGreen) : dim;
        if (current) {
            core = {core.x * blink, core.y * blink, core.z * blink, 1.0f};
        }
        guideKit_.Tube({cx - 16.0f, 43.5f}, {cx + 16.0f, 43.5f}, 3.2f, core, menucolor::kGreen, on ? 0.5f : current ? blink : 0.0f, 16.0f);
    }

    if (tutorialStep_ == 0) {
        // 光る輪と、そこへ向かう点線。
        const myEngine::Vector2 mark = screen(TutorialMark());
        const float radius = (0.7f + 0.15f * pulse) * unit;
        guideKit_.Ring(mark, radius, 3.0f, menucolor::Core(menucolor::kGreen), menucolor::kGreen, 1.0f, 26.0f);
        const float dx = mark.x - self.x, dy = mark.y - self.y, distance = std::hypot(dx, dy);
        const int count = static_cast<int>(distance / (0.9f * unit));
        for (int i = 2; i < count; ++i) {
            const float k = (static_cast<float>(i) + std::fmod(tutorialTime_ * 2.0f, 1.0f)) / static_cast<float>(count);
            const float cx = self.x + dx * k, cy = self.y + dy * k, ux = dx / distance * 4.5f, uy = dy / distance * 4.5f;
            guideKit_.Tube({cx - ux, cy - uy}, {cx + ux, cy + uy}, 2.4f, menucolor::Alpha(menucolor::Core(menucolor::kGreen), 0.7f), menucolor::kGreen, 0.3f,
                           10.0f);
        }
        if (pad) {
            const myEngine::Vector2 stick = input.GetLeftStick();
            Stick(guideKit_, x, y, "L", stick.x, stick.y, std::hypot(stick.x, stick.y) > 0.2f, tutorialTime_);
        } else {
            Cap(guideKit_, x, y - 1.15f * kUnit, kUnit, "W", down(DIK_W), false);
            Cap(guideKit_, x - 1.15f * kUnit, y, kUnit, "A", down(DIK_A), false);
            Cap(guideKit_, x, y, kUnit, "S", down(DIK_S), false);
            Cap(guideKit_, x + 1.15f * kUnit, y, kUnit, "D", down(DIK_D), false);
        }
    } else if (tutorialStep_ == 1) {
        myEngine::Vector2 nearest{};
        float best = 1.0e9f;
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }
            const myEngine::Vector2 at = screen(enemy->GetWorldPosition());
            guideKit_.Ring(at, (1.9f + 0.15f * pulse) * unit, 2.0f, menucolor::Core(menucolor::kGreen), menucolor::kGreen, 0.6f, 16.0f);
            if (const float d = std::hypot(at.x - self.x, at.y - self.y); d < best) {
                best = d;
                nearest = at;
            }
        }
        if (best < 1.0e8f) {
            EdgeArrow(guideKit_, self, nearest, menucolor::kGreen);
        }
        const bool manual = !usersettings::Get().autoFire;
        if (pad) {
            myEngine::Vector2 aim{};
            const bool used = input.TryGetGamepadAim(aim);
            Stick(guideKit_, x, y, "R", aim.x, aim.y, used, tutorialTime_);
            if (manual) {
                Cap(guideKit_, x + 2.2f * kUnit, y, kUnit, "RT", input.GetCurrentGamepadState().Gamepad.bRightTrigger > 60, true);
            }
        } else {
            // マウスの絵（左右にゆれる）。自分で撃つ設定なら、左ボタンが光る。
            const float mx = x + std::sin(tutorialTime_ * 2.0f) * 0.3f * kUnit, r = 0.55f * kUnit;
            const auto core = menucolor::Core(kGrey, 0.6f);
            guideKit_.Rect(mx - r * 0.8f, y - 0.8f * kUnit, r * 1.6f, 1.6f * kUnit, kPlate);
            guideKit_.Ring({mx, y - 0.38f * kUnit}, r, 2.0f, core, kGrey, 0.0f, 0.0f, 0.75f, 1.25f);
            guideKit_.Ring({mx, y + 0.38f * kUnit}, r, 2.0f, core, kGrey, 0.0f, 0.0f, 0.25f, 0.75f);
            guideKit_.Tube({mx - r, y - 0.38f * kUnit}, {mx - r, y + 0.38f * kUnit}, 2.0f, core, kGrey, 0.0f, 0.0f);
            guideKit_.Tube({mx + r, y - 0.38f * kUnit}, {mx + r, y + 0.38f * kUnit}, 2.0f, core, kGrey, 0.0f, 0.0f);
            const float bx = manual ? mx - r * 0.45f : mx;
            guideKit_.Tube({bx, y - 0.62f * kUnit}, {bx, y - 0.24f * kUnit}, manual ? 4.0f + 2.0f * pulse : 3.0f, menucolor::Core(menucolor::kGreen),
                           menucolor::kGreen, 1.0f, 14.0f);
            Arrow(guideKit_, mx + 1.25f * kUnit, y, 0.0f, menucolor::kGreen);
            Arrow(guideKit_, mx - 1.25f * kUnit, y, kPi, menucolor::kGreen);
        }
    } else if (tutorialStep_ == 2) {
        for (const myEngine::Vector3& shard : pickups_.GetPositions()) {
            const myEngine::Vector2 at = screen(shard);
            Arrow(guideKit_, at.x, at.y - (1.2f + 0.2f * pulse) * kUnit, kPi * 0.5f, menucolor::kCyan);
            EdgeArrow(guideKit_, self, at, menucolor::kCyan);
        }
    } else if (tutorialStep_ == 4 && tutorialStage_ == 0) {
        guideKit_.Ring({self.x, self.y}, (2.1f + 0.2f * pulse) * unit, 2.4f, menucolor::Core(menucolor::kGold), menucolor::kGold, 0.4f + 0.6f * pulse, 30.0f);
        Cap(guideKit_, x, y, 1.35f * kUnit, pad ? "B" : "Q", pulse > 0.5f, pad);
    }

    // 右下：ながおしで飛ばす。
    const myEngine::Vector4 key{0.875f, 0.902f, 0.910f, 1.0f};
    const std::string text = "ながおしで飛ばす";
    const float textWidth = guideKit_.MeasureText(text, 17.0f);
    const float right = 1241.0f, keyX = right - textWidth - (pad ? 62.0f : 52.0f);
    const float keyWidth = guideKit_.Key(pad ? "Start" : "Esc", keyX, 669.0f, pad, key, pad ? 0.9f : 1.0f);
    guideKit_.Text(text, 17.0f, right, 669.0f, 1.0f, menucolor::kDim);
    if (tutorialSkip_ > 0.02f) {
        guideKit_.Ring({keyX + keyWidth * 0.5f, 669.0f}, keyWidth * 0.5f + 9.0f, 3.0f, menucolor::Core(menucolor::kGreen), menucolor::kGreen, 0.8f, 14.0f, 0.0f,
                       (std::min)(1.0f, tutorialSkip_ / kSkipSeconds));
    }
    guideKit_.End();
}

void SurvivorRunController::QueueCardHints()
{
    // 初めての強化の画面に、キーの絵を添える。
    menuKit_.Begin();
    const bool pad = world_.resources.input_->IsGamepadActive();
    const myEngine::Vector4 key{0.875f, 0.902f, 0.910f, 1.0f};
    const float pulse = 0.5f + 0.5f * std::sin(tutorialTime_ * 5.0f);
    const float y = 650.0f;
    float x = 660.0f;
    if (pad) {
        x += menuKit_.Key("L", x, y, true, key, 1.2f) + 10.0f;
        x += menuKit_.Text("えらぶ", 19.0f, x, y, 0.0f, menucolor::kInk) + 26.0f;
        const float at = x;
        x += menuKit_.Key("A", x, y, true, key, 1.2f) + 10.0f;
        menuKit_.Ring({at + 15.0f, y}, 20.0f, 2.4f, menucolor::Core(menucolor::kGreen), menucolor::kGreen, 0.3f + 0.7f * pulse, 14.0f);
        menuKit_.Text("きめる", 19.0f, x + 8.0f, y, 0.0f, menucolor::kInk);
    } else {
        const float at = x;
        for (const char* name : {"1", "2", "3"}) {
            x += menuKit_.Key(name, x, y, false, key, 1.2f) + 8.0f;
        }
        menuKit_.Tube({at + 4.0f, y + 24.0f}, {x - 12.0f, y + 24.0f}, 3.0f, menucolor::Core(menucolor::kGreen), menucolor::kGreen, 0.3f + 0.7f * pulse, 14.0f);
        // マウスの絵（左ボタン）。
        const float mx = x + 28.0f, r = 12.0f;
        const auto core = menucolor::Core(kGrey, 0.6f);
        menuKit_.Ring({mx, y - 8.0f}, r, 2.0f, core, kGrey, 0.0f, 0.0f, 0.75f, 1.25f);
        menuKit_.Ring({mx, y + 8.0f}, r, 2.0f, core, kGrey, 0.0f, 0.0f, 0.25f, 0.75f);
        menuKit_.Tube({mx - r, y - 8.0f}, {mx - r, y + 8.0f}, 2.0f, core, kGrey, 0.0f, 0.0f);
        menuKit_.Tube({mx + r, y - 8.0f}, {mx + r, y + 8.0f}, 2.0f, core, kGrey, 0.0f, 0.0f);
        menuKit_.Tube({mx - 5.0f, y - 12.0f}, {mx - 5.0f, y - 4.0f}, 4.0f, menucolor::Core(menucolor::kGreen), menucolor::kGreen, 0.3f + 0.7f * pulse, 12.0f);
    }
    menuKit_.End();
}

} // namespace gameplay
