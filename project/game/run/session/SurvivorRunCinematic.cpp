#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の演出：宿敵の登場（止めて寄る）・戦闘中の傾いたカメラ・宿敵の撃破（スローで寄る）・自機の撃破（寄る）。

namespace gameplay {

using namespace survivorrun;

namespace {
// 登場は止めて見せる秒数、撃破はスローの間の秒数（どちらも実時間）。
constexpr float kIntroSeconds = 2.6f;
constexpr float kDefeatSeconds = 2.4f;
// 撃破の演出で、追加の爆発を起こす時刻。
constexpr std::array<float, 3> kDefeatBurstSeconds{0.35f, 0.8f, 1.3f};
} // namespace

void SurvivorRunController::UpdateCinematic(float dt)
{
    const auto& player = *world_.resources.player_;
    const GameFlowState state = world_.combat.combatFlow_.GetState();
    // 自機が倒れたら、結果画面が出るまで自機へ寄る。結果画面では演出を終える。
    if (state == GameFlowState::GameOver && !IsResultVisible() && cineMode_ != CineMode::PlayerDown) {
        cineMode_ = CineMode::PlayerDown;
        cineTime_ = 0.0f;
        cineAnchor_ = player.GetWorldPosition();
    }
    if (IsResultVisible() || state == GameFlowState::StageClear) {
        cineMode_ = CineMode::None;
    }
    cineTime_ += dt;

    // 目標の値。演出が無ければ、追従カメラと同じく自機（部屋の枠で止める）を真上から見る。
    myEngine::Vector3 focus = player.GetWorldPosition();
    float distance = 1.0f, tilt = 0.0f, letterbox = 0.0f;
    const myEngine::Vector3 rival = world_.resources.enemy_ ? world_.resources.enemy_->GetWorldPosition() : focus;
    switch (cineMode_) {
    case CineMode::RivalIntro:
        // 宿敵へ寄り、低く傾けて立ち上がる姿を見せる。
        focus = rival;
        distance = 0.55f;
        tilt = 28.0f;
        letterbox = 1.0f;
        if (cineTime_ >= kIntroSeconds) {
            cineMode_ = CineMode::RivalFight;
            cineTime_ = 0.0f;
        }
        break;
    case CineMode::RivalFight: {
        if (!IsRivalActive()) {
            cineMode_ = CineMode::None;
            break;
        }
        // 自機と宿敵の間（自機寄り）を見て、離れていれば引く。少し傾けて、宿敵の立体の姿が読めるようにする。
        focus = focus + (rival - focus) * 0.4f;
        const float spread = (std::max)(std::abs(rival.x - player.GetWorldPosition().x) * 0.5f / arenacamera::kAspect,
                                        std::abs(rival.y - player.GetWorldPosition().y) * 0.5f);
        distance = (std::clamp)((spread + 7.0f) / arenacamera::HalfViewHeight(), 1.0f, 1.5f);
        tilt = 14.0f;
        break;
    }
    case CineMode::RivalDefeat:
        // 倒れた位置へ寄り、スローの間に爆発を重ねる。終わったら片付けて出口を開く。
        focus = cineAnchor_;
        distance = 0.6f;
        tilt = 24.0f;
        letterbox = 1.0f;
        while (cineBursts_ < static_cast<int>(kDefeatBurstSeconds.size()) && cineTime_ >= kDefeatBurstSeconds[static_cast<size_t>(cineBursts_)]) {
            std::uniform_real_distribution<float> offset(-2.5f, 2.5f);
            world_.combatEffects->TriggerDeathPostPulse(cineAnchor_ + myEngine::Vector3{offset(random_), offset(random_), 0.0f},
                                                        1.1f + 0.2f * static_cast<float>(cineBursts_));
            world_.presentation.cameraShakeDuration_ = 0.3f;
            world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
            world_.presentation.cameraShakePower_ = 0.5f;
            ++cineBursts_;
        }
        if (cineTime_ >= kDefeatSeconds) {
            cineMode_ = CineMode::None;
            FinishRivalDefeat();
        }
        break;
    case CineMode::PlayerDown:
        focus = cineAnchor_;
        distance = 0.5f;
        tilt = 22.0f;
        letterbox = 1.0f;
        break;
    case CineMode::None:
        break;
    }
    // 演出の無いときと戦闘中は、追従カメラと同じく部屋の枠の内側で止める（演出が終わったときに跳ばないように）。
    if (cineMode_ == CineMode::None || cineMode_ == CineMode::RivalFight) {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        if (arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), minX, minY, maxX, maxY)) {
            const float half = arenacamera::HalfViewHeight() * distance;
            focus.x = arenacamera::ClampFocus(focus.x, minX, maxX, half * arenacamera::kAspect);
            focus.y = arenacamera::ClampFocus(focus.y, minY, maxY, half);
        }
    }

    // なめらかに寄せる（約0.25秒で目標の6割）。最初は今の目標から始める。
    if (!cineFocusReady_) {
        cineFocus_ = focus;
        cineFocusReady_ = true;
    }
    const float follow = 1.0f - std::exp(-dt * 4.0f);
    cineFocus_ = cineFocus_ + (focus - cineFocus_) * follow;
    cineDistance_ += (distance - cineDistance_) * follow;
    cineTilt_ += (tilt - cineTilt_) * follow;
    cineLetterbox_ += (letterbox - cineLetterbox_) * (1.0f - std::exp(-dt * 6.0f));
}

SurvivorRunController::CameraDirective SurvivorRunController::GetCameraDirective() const
{
    CameraDirective directive;
    // 演出が終わっても、傾きと距離が戻りきるまではこちらでカメラを動かす。
    directive.active = cineFocusReady_ && (cineMode_ != CineMode::None || cineTilt_ > 0.2f || std::abs(cineDistance_ - 1.0f) > 0.01f);
    directive.focus = cineFocus_;
    directive.distanceScale = cineDistance_;
    directive.tiltDegrees = cineTilt_;
    return directive;
}

void SurvivorRunController::ApplyCinematicCamera()
{
    auto& camera = *world_.resources.camera;
    auto& player = *world_.resources.player_;
    const CameraDirective directive = GetCameraDirective();
    if (!directive.active) {
        if (cameraTilted_) {
            camera.SetRotate(cameraBaseRotation_);
            player.SetNeonDepthAimEnabled(false);
            cameraTilted_ = false;
        }
        return;
    }
    if (!cameraTilted_) {
        cameraBaseRotation_ = camera.GetRotate();
        cameraTilted_ = true;
    }
    // 床の注視点を画面の中央に置いたまま、手前へ傾けて引く（開発用のステージの傾きと同じ置き方）。
    const float distance = arenacamera::Distance() * directive.distanceScale;
    const float angle = directive.tiltDegrees * 3.14159265f / 180.0f;
    myEngine::Vector3 eye{directive.focus.x, directive.focus.y + std::sin(angle) * distance, -std::cos(angle) * distance};
    // 画面の揺れ（残り時間は追従カメラの処理が減らしている）。
    const auto& presentation = world_.presentation;
    if (presentation.cameraShakeTimer_ > 0.0f && presentation.cameraShakeDuration_ > 0.0f) {
        const float t = (std::clamp)(presentation.cameraShakeTimer_ / presentation.cameraShakeDuration_, 0.0f, 1.0f);
        const float power = presentation.cameraShakePower_ * t * t;
        eye.x += myEngine::Rand(-power, power);
        eye.y += myEngine::Rand(-power, power);
    }
    myEngine::Vector3 rotation = cameraBaseRotation_;
    rotation.x = cameraBaseRotation_.x + angle;
    camera.SetTranslate(eye);
    camera.SetRotate(rotation);
    // 傾いたカメラでは、照準を床の平面とのレイで求める。
    player.SetNeonDepthAimEnabled(true);
}

void SurvivorRunController::DrawCinematic()
{
    if (cineLetterbox_ < 0.01f || !cineBarTop_ || !cineBarBottom_ || !cineTitleIntro_ || !cineTitleDefeat_) {
        return;
    }
    const float width = static_cast<float>(myEngine::WinApp::kClientWidth);
    const float height = static_cast<float>(myEngine::WinApp::kClientHeight);
    const float bar = 86.0f * cineLetterbox_;
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    cineBarTop_->SetPosition({0.0f, 0.0f});
    cineBarTop_->SetSize({width, bar});
    cineBarTop_->Update();
    cineBarBottom_->SetPosition({0.0f, height - bar});
    cineBarBottom_->SetSize({width, bar});
    cineBarBottom_->Update();
    cineBarTop_->Draw();
    cineBarBottom_->Draw();
    // 題字は文字を変えずに2つ持つ。描画の途中で SetText すると文字の画像を作り直し、GPU が使用中の画像を解放して落ちるため。
    myEngine::TextLabel* title = cineMode_ == CineMode::RivalIntro    ? cineTitleIntro_.get()
                                 : cineMode_ == CineMode::RivalDefeat ? cineTitleDefeat_.get()
                                                                      : nullptr;
    if (title) {
        title->SetAlpha(cineLetterbox_);
        title->Draw();
    }
}

} // namespace gameplay
