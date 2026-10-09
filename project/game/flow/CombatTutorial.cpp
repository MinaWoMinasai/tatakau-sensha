#include "game/flow/CombatTutorial.h"
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

void CombatTutorial::InitializeSubmissionUi()
{
    cg2::TextStyle bannerStyle{};
    bannerStyle.fontFamily = "Meiryo";
    bannerStyle.fontSize = 58.0f;
    bannerStyle.color = {0.78f, 1.0f, 0.96f, 1.0f};
    bannerStyle.outlineColor = {0.0f, 0.03f, 0.08f, 0.98f};
    bannerStyle.outlineThickness = 5.0f;
    bannerStyle.padding = 10.0f;
    world_.combat.flowBannerText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.flowBannerText_->Initialize(cg2::SpriteCommon::GetInstance(), "STAGE CLEAR", bannerStyle);
    world_.combat.flowBannerText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.flowBannerText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 170.0f});

    cg2::TextStyle eventStyle = bannerStyle;
    eventStyle.fontSize = 30.0f;
    eventStyle.color = {0.55f, 1.0f, 0.72f, 1.0f};
    eventStyle.outlineThickness = 3.0f;
    world_.combat.eventCalloutText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.eventCalloutText_->Initialize(cg2::SpriteCommon::GetInstance(), "ジャスト回避", eventStyle);
    world_.combat.eventCalloutText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.eventCalloutText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 112.0f});

    cg2::TextStyle resultStyle = bannerStyle;
    resultStyle.fontSize = 25.0f;
    resultStyle.color = {0.90f, 0.96f, 1.0f, 1.0f};
    resultStyle.outlineThickness = 3.0f;
    world_.combat.resultSummaryText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.resultSummaryText_->Initialize(cg2::SpriteCommon::GetInstance(), "", resultStyle);
    world_.combat.resultSummaryText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.resultSummaryText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 365.0f});

    cg2::TextStyle menuStyle = resultStyle;
    menuStyle.fontSize = 28.0f;
    menuStyle.color = {0.48f, 1.0f, 0.66f, 1.0f};
    world_.combat.resultMenuText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.resultMenuText_->Initialize(cg2::SpriteCommon::GetInstance(), "", menuStyle);
    world_.combat.resultMenuText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.resultMenuText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 555.0f});
}

bool CombatTutorial::LoadTutorialConfig(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[Tutorial] Failed to open config: " << filePath << std::endl;
        return false;
    }

    try {
        nlohmann::json configJson;
        file >> configJson;
        world_.combat.tutorialConfig_.enabled = configJson.value("enabled", world_.combat.tutorialConfig_.enabled);
        world_.combat.tutorialConfig_.moveDistance =
            (std::max)(0.1f, configJson.value("moveDistance", world_.combat.tutorialConfig_.moveDistance));
        world_.combat.tutorialConfig_.stepCompleteDelay =
            (std::max)(0.0f, configJson.value("stepCompleteDelay", world_.combat.tutorialConfig_.stepCompleteDelay));
        world_.combat.tutorialConfig_.phase1CompleteDisplayDuration =
            (std::max)(0.0f,
                       configJson.value("phase1CompleteDisplayDuration", world_.combat.tutorialConfig_.phase1CompleteDisplayDuration));
        world_.combat.tutorialConfig_.evolutionUnlockedDisplayDuration =
            (std::max)(0.0f, configJson.value("evolutionUnlockedDisplayDuration",
                                              world_.combat.tutorialConfig_.evolutionUnlockedDisplayDuration));
        world_.combat.tutorialConfig_.tutorialCompleteDisplayDuration =
            (std::max)(0.0f,
                       configJson.value("tutorialCompleteDisplayDuration", world_.combat.tutorialConfig_.tutorialCompleteDisplayDuration));
        return true;
    }
    catch (const std::exception& exception) {
        std::cerr << "[Tutorial] Invalid config: " << exception.what() << std::endl;
        return false;
    }
}

void CombatTutorial::InitializeTutorialUi()
{
    if (!world_.combat.tutorialConfig_.enabled || !world_.resources.player_) {
        world_.combat.tutorialUiVisible_ = false;
        return;
    }

    world_.combat.tutorialPanel_ = std::make_unique<cg2::Sprite>();
    world_.combat.tutorialPanel_->Initialize(cg2::SpriteCommon::GetInstance(), "resources/white512x512.png");
    world_.combat.tutorialPanel_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.tutorialPanel_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 128.0f});
    world_.combat.tutorialPanel_->SetSize({390.0f, 126.0f});
    world_.combat.tutorialPanel_->SetColor({0.004f, 0.012f, 0.030f, 0.78f});
    world_.combat.tutorialPanel_->Update();

    cg2::TextStyle titleStyle{};
    titleStyle.fontFamily = "Meiryo";
    titleStyle.fontSize = 28.0f;
    titleStyle.color = {0.42f, 1.0f, 0.82f, 1.0f};
    titleStyle.outlineColor = {0.0f, 0.03f, 0.08f, 0.96f};
    titleStyle.outlineThickness = 3.0f;
    titleStyle.padding = 7.0f;
    world_.combat.tutorialTitleText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.tutorialTitleText_->Initialize(cg2::SpriteCommon::GetInstance(), "MOVE", titleStyle);
    world_.combat.tutorialTitleText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.tutorialTitleText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 91.0f});

    cg2::TextStyle inputStyle = titleStyle;
    inputStyle.fontSize = 20.0f;
    inputStyle.color = {0.90f, 1.0f, 1.0f, 1.0f};
    inputStyle.outlineThickness = 2.0f;
    world_.combat.tutorialInputText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.tutorialInputText_->Initialize(cg2::SpriteCommon::GetInstance(), "W A S D", inputStyle);
    world_.combat.tutorialInputText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.tutorialInputText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 128.0f});

    cg2::TextStyle descriptionStyle = inputStyle;
    descriptionStyle.fontSize = 15.0f;
    descriptionStyle.color = {0.78f, 0.86f, 0.92f, 0.92f};
    world_.combat.tutorialDescriptionText_ = std::make_unique<cg2::TextLabel>();
    world_.combat.tutorialDescriptionText_->Initialize(cg2::SpriteCommon::GetInstance(), "移動してみよう", descriptionStyle);
    world_.combat.tutorialDescriptionText_->SetAnchorPoint({0.5f, 0.5f});
    world_.combat.tutorialDescriptionText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 160.0f});

    world_.combat.tutorialPreviousPlayerPosition_ = world_.resources.player_->GetWorldPosition();
    world_.combat.tutorialMoveDistance_ = 0.0f;
    world_.resources.player_->ConsumePrimaryAttackPerformedEvent();
    world_.resources.player_->ConsumeDashStartedEvent();
    world_.resources.player_->ConsumeStatUpgradePerformedEvent();
    world_.resources.player_->ConsumeEvolutionConfirmed();
    world_.resources.player_->ConsumeEvolutionCancelled();
    world_.combat.tutorialUiVisible_ = true;
    EnterTutorialStep(TutorialStep::Move);
}

void CombatTutorial::EnterTutorialStep(TutorialStep step)
{
    world_.combat.tutorialStep_ = step;
    world_.combat.tutorialStepCompleting_ = false;
    world_.combat.tutorialStepCompleteTimer_ = 0.0f;

    if (world_.resources.player_) {
        if (step == TutorialStep::Move) {
            world_.combat.tutorialMoveDistance_ = 0.0f;
            world_.combat.tutorialPreviousPlayerPosition_ = world_.resources.player_->GetWorldPosition();
            world_.resources.player_->ConsumePrimaryAttackPerformedEvent();
            world_.resources.player_->ConsumeDashStartedEvent();
        } else if (step == TutorialStep::Shoot) {
            world_.resources.player_->ConsumePrimaryAttackPerformedEvent();
            world_.resources.player_->ConsumeDashStartedEvent();
        } else if (step == TutorialStep::Dash) {
            world_.resources.player_->ConsumeDashStartedEvent();
        } else if (step == TutorialStep::Upgrade) {
            world_.resources.player_->ConsumeStatUpgradePerformedEvent();
            GrantTutorialUpgradeReward();
        } else if (step == TutorialStep::EvolutionUnlocked) {
            GrantTutorialEvolutionReward();
            world_.resources.player_->CloseEvolutionUiForTutorial();
            world_.resources.player_->ConsumeEvolutionConfirmed();
            world_.resources.player_->ConsumeEvolutionCancelled();
        } else if (step == TutorialStep::Evolution) {
            world_.resources.player_->ConsumeEvolutionConfirmed();
            world_.resources.player_->ConsumeEvolutionCancelled();
            world_.combat.tutorialEvolutionUiWasOpen_ = world_.resources.player_->IsChangeMode();
        }
    }

    if (step == TutorialStep::Phase1Complete) {
        world_.combat.tutorialPhase1CompleteTimer_ = world_.combat.tutorialConfig_.phase1CompleteDisplayDuration;
        world_.combat.tutorialUiVisible_ = true;
    } else if (step == TutorialStep::EvolutionUnlocked) {
        world_.combat.tutorialEvolutionUnlockedTimer_ = world_.combat.tutorialConfig_.evolutionUnlockedDisplayDuration;
        world_.combat.tutorialUiVisible_ = true;
    } else if (step == TutorialStep::TutorialComplete) {
        world_.combat.tutorialCompleteTimer_ = world_.combat.tutorialConfig_.tutorialCompleteDisplayDuration;
        world_.combat.tutorialCompleteExitReady_ = false;
        world_.combat.tutorialUiVisible_ = true;
    }
    UpdateTutorialText();
}

void CombatTutorial::CompleteTutorialStep()
{
    if (world_.combat.tutorialStepCompleting_ || world_.combat.tutorialStep_ == TutorialStep::Phase1Complete ||
        world_.combat.tutorialStep_ == TutorialStep::TutorialComplete) {
        return;
    }
    world_.combat.tutorialStepCompleting_ = true;
    world_.combat.tutorialStepCompleteTimer_ = world_.combat.tutorialConfig_.stepCompleteDelay;
    UpdateTutorialText();
}

void CombatTutorial::GrantTutorialUpgradeReward()
{
    if (!world_.combat.tutorialConfig_.enabled || world_.combat.tutorialUpgradeRewardGranted_ || !world_.resources.player_) {
        return;
    }
    world_.combat.tutorialUpgradeRewardGranted_ = true;
    if (world_.resources.player_->GetSkillPoints() > 0) {
        return;
    }

    const int requiredExp = world_.resources.player_->GetNextLevelExpValue() - world_.resources.player_->GetExp();
    world_.resources.player_->AddExp((std::max)(1, requiredExp));
}

void CombatTutorial::GrantTutorialEvolutionReward()
{
    if (!world_.combat.tutorialConfig_.enabled || world_.combat.tutorialEvolutionRewardGranted_ || !world_.resources.player_) {
        return;
    }
    world_.combat.tutorialEvolutionRewardGranted_ = true;
    while (world_.resources.player_->GetCurrentRank() < 2) {
        const int previousLevel = world_.resources.player_->GetLevel();
        const int requiredExp = world_.resources.player_->GetNextLevelExpValue() - world_.resources.player_->GetExp();
        world_.resources.player_->AddExp((std::max)(1, requiredExp));
        if (world_.resources.player_->GetLevel() <= previousLevel) {
            break;
        }
    }
}

void CombatTutorial::UpdateTutorial(float deltaTime)
{
    if (!world_.combat.tutorialConfig_.enabled || !world_.resources.player_) {
        return;
    }

    const cg2::Vector3 currentPosition = world_.resources.player_->GetWorldPosition();
    const float actualMoveDistance = cg2::Length(currentPosition - world_.combat.tutorialPreviousPlayerPosition_);
    world_.combat.tutorialPreviousPlayerPosition_ = currentPosition;

    if (world_.combat.tutorialStep_ == TutorialStep::Phase1Complete) {
        world_.combat.tutorialPhase1CompleteTimer_ = (std::max)(0.0f, world_.combat.tutorialPhase1CompleteTimer_ - deltaTime);
        if (world_.combat.tutorialPhase1CompleteTimer_ <= 0.0f) {
            EnterTutorialStep(TutorialStep::Upgrade);
        }
        return;
    }
    if (world_.combat.tutorialStep_ == TutorialStep::EvolutionUnlocked) {
        if (world_.resources.player_->IsChangeMode()) {
            world_.resources.player_->CloseEvolutionUiForTutorial();
        }
        world_.combat.tutorialEvolutionUnlockedTimer_ = (std::max)(0.0f, world_.combat.tutorialEvolutionUnlockedTimer_ - deltaTime);
        if (world_.combat.tutorialEvolutionUnlockedTimer_ <= 0.0f) {
            EnterTutorialStep(TutorialStep::Evolution);
        }
        return;
    }
    if (world_.combat.tutorialStep_ == TutorialStep::TutorialComplete) {
        if (!world_.combat.tutorialCompleteExitReady_) {
            world_.combat.tutorialCompleteTimer_ = (std::max)(0.0f, world_.combat.tutorialCompleteTimer_ - deltaTime);
            if (world_.combat.tutorialCompleteTimer_ <= 0.0f) {
                world_.combat.tutorialCompleteExitReady_ = true;
                UpdateTutorialText();
            }
        }
        if (world_.combat.tutorialCompleteExitReady_ &&
            world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_RETURN],
                                               world_.resources.input_->GetPreKey()[DIK_RETURN])) {
            world_.combat.nextSceneName_ = "TITLE";
            world_.combat.fade_->Start(Fade::Status::FadeOut, 0.75f);
            world_.combat.phase_ = Phase::kFadeOut;
        }
        return;
    }

    if (world_.combat.tutorialStepCompleting_) {
        world_.combat.tutorialStepCompleteTimer_ = (std::max)(0.0f, world_.combat.tutorialStepCompleteTimer_ - deltaTime);
        if (world_.combat.tutorialStepCompleteTimer_ <= 0.0f) {
            switch (world_.combat.tutorialStep_) {
            case TutorialStep::Move:
                EnterTutorialStep(TutorialStep::Shoot);
                break;
            case TutorialStep::Shoot:
                EnterTutorialStep(TutorialStep::Dash);
                break;
            case TutorialStep::Dash:
                EnterTutorialStep(TutorialStep::Phase1Complete);
                break;
            case TutorialStep::Upgrade:
                EnterTutorialStep(TutorialStep::EvolutionUnlocked);
                break;
            case TutorialStep::Evolution:
                EnterTutorialStep(TutorialStep::TutorialComplete);
                break;
            case TutorialStep::Phase1Complete:
            case TutorialStep::EvolutionUnlocked:
            case TutorialStep::TutorialComplete:
                break;
            }
        }
        return;
    }

    switch (world_.combat.tutorialStep_) {
    case TutorialStep::Move:
        if (world_.resources.player_->HasMovementInput() && !world_.resources.player_->IsDashing()) {
            world_.combat.tutorialMoveDistance_ += actualMoveDistance;
        }
        if (world_.combat.tutorialMoveDistance_ >= world_.combat.tutorialConfig_.moveDistance) {
            CompleteTutorialStep();
        }
        break;
    case TutorialStep::Shoot:
        if (world_.resources.player_->ConsumePrimaryAttackPerformedEvent()) {
            CompleteTutorialStep();
        }
        break;
    case TutorialStep::Dash:
        if (world_.resources.player_->ConsumeDashStartedEvent()) {
            CompleteTutorialStep();
        }
        break;
    case TutorialStep::Upgrade:
        if (world_.resources.player_->ConsumeStatUpgradePerformedEvent()) {
            CompleteTutorialStep();
        }
        break;
    case TutorialStep::Evolution: {
        const bool evolutionUiOpen = world_.resources.player_->IsChangeMode();
        if (evolutionUiOpen != world_.combat.tutorialEvolutionUiWasOpen_) {
            world_.combat.tutorialEvolutionUiWasOpen_ = evolutionUiOpen;
            UpdateTutorialText();
        }
        world_.resources.player_->ConsumeEvolutionCancelled();
        if (world_.resources.player_->ConsumeEvolutionConfirmed()) {
            world_.combat.screenEffectDirector_.TriggerUpgradeConfirmed(
                world_.gameplayQueries->WorldToScreenUv(world_.resources.player_->GetWorldPosition()));
            CompleteTutorialStep();
        }
        break;
    }
    case TutorialStep::Phase1Complete:
    case TutorialStep::EvolutionUnlocked:
    case TutorialStep::TutorialComplete:
        break;
    }
}

void CombatTutorial::UpdateTutorialText()
{
    if (!world_.combat.tutorialTitleText_ || !world_.combat.tutorialInputText_ || !world_.combat.tutorialDescriptionText_) {
        return;
    }

    cg2::TextStyle inputStyle = world_.combat.tutorialInputText_->GetStyle();
    inputStyle.color = world_.combat.tutorialStepCompleting_ || world_.combat.tutorialStep_ == TutorialStep::Phase1Complete ||
                               world_.combat.tutorialStep_ == TutorialStep::TutorialComplete
                           ? cg2::Vector4{0.42f, 1.0f, 0.62f, 1.0f}
                           : cg2::Vector4{0.90f, 1.0f, 1.0f, 1.0f};
    world_.combat.tutorialInputText_->SetStyle(inputStyle);

    if (world_.combat.tutorialStepCompleting_) {
        switch (world_.combat.tutorialStep_) {
        case TutorialStep::Move:
            world_.combat.tutorialTitleText_->SetText("MOVE");
            break;
        case TutorialStep::Shoot:
            world_.combat.tutorialTitleText_->SetText("SHOOT");
            break;
        case TutorialStep::Dash:
            world_.combat.tutorialTitleText_->SetText("DASH");
            break;
        case TutorialStep::Upgrade:
            world_.combat.tutorialTitleText_->SetText("UPGRADE");
            break;
        case TutorialStep::EvolutionUnlocked:
            world_.combat.tutorialTitleText_->SetText("RANK UP");
            break;
        case TutorialStep::Evolution:
            world_.combat.tutorialTitleText_->SetText("EVOLUTION");
            break;
        case TutorialStep::Phase1Complete:
            break;
        case TutorialStep::TutorialComplete:
            break;
        }
        world_.combat.tutorialInputText_->SetText("COMPLETE");
        world_.combat.tutorialDescriptionText_->SetText("");
    } else {
        switch (world_.combat.tutorialStep_) {
        case TutorialStep::Move:
            world_.combat.tutorialTitleText_->SetText("MOVE");
            world_.combat.tutorialInputText_->SetText("W A S D");
            world_.combat.tutorialDescriptionText_->SetText("移動してみよう");
            break;
        case TutorialStep::Shoot:
            world_.combat.tutorialTitleText_->SetText("SHOOT");
            world_.combat.tutorialInputText_->SetText("LEFT CLICK");
            world_.combat.tutorialDescriptionText_->SetText("弾を撃とう");
            break;
        case TutorialStep::Dash:
            world_.combat.tutorialTitleText_->SetText("DASH");
            world_.combat.tutorialInputText_->SetText("RIGHT CLICK");
            world_.combat.tutorialDescriptionText_->SetText("ダッシュしよう");
            break;
        case TutorialStep::Phase1Complete:
            world_.combat.tutorialTitleText_->SetText("BASIC CONTROLS");
            world_.combat.tutorialInputText_->SetText("COMPLETE");
            world_.combat.tutorialDescriptionText_->SetText("");
            break;
        case TutorialStep::Upgrade:
            world_.combat.tutorialTitleText_->SetText("UPGRADE");
            world_.combat.tutorialInputText_->SetText("1 - 7 / CLICK +");
            world_.combat.tutorialDescriptionText_->SetText("能力を1つ強化しよう");
            break;
        case TutorialStep::EvolutionUnlocked:
            world_.combat.tutorialTitleText_->SetText("RANK UP");
            world_.combat.tutorialInputText_->SetText("RANK 2");
            world_.combat.tutorialDescriptionText_->SetText("EVOLUTION UNLOCKED");
            break;
        case TutorialStep::Evolution:
            world_.combat.tutorialTitleText_->SetText("EVOLUTION");
            if (world_.resources.player_ && world_.resources.player_->IsChangeMode()) {
                world_.combat.tutorialInputText_->SetText("SELECT + ENTER");
                world_.combat.tutorialDescriptionText_->SetText("進化先を選ぼう");
            } else {
                world_.combat.tutorialInputText_->SetText("C");
                world_.combat.tutorialDescriptionText_->SetText("進化ツリーを開こう");
            }
            break;
        case TutorialStep::TutorialComplete:
            world_.combat.tutorialTitleText_->SetText("TUTORIAL");
            world_.combat.tutorialInputText_->SetText("COMPLETE");
            world_.combat.tutorialDescriptionText_->SetText(world_.combat.tutorialCompleteExitReady_ ? "ENTER : TITLE"
                                                                                                     : "C : EVOLUTION TREE");
            break;
        }
    }

    world_.combat.tutorialTitleText_->PrepareForDraw();
    world_.combat.tutorialInputText_->PrepareForDraw();
    world_.combat.tutorialDescriptionText_->PrepareForDraw();
}

void CombatTutorial::DrawTutorialUi()
{
    if (!world_.combat.tutorialConfig_.enabled || !world_.combat.tutorialUiVisible_ || !world_.resources.player_ ||
        world_.combat.combatFlow_.GetState() != GameFlowState::Playing) {
        return;
    }
    const bool compactEvolutionHint = world_.combat.tutorialStep_ == TutorialStep::Evolution && world_.resources.player_->IsChangeMode();
    if (world_.resources.player_->IsChangeMode() && !compactEvolutionHint) {
        return;
    }
    if (compactEvolutionHint) {
        if (world_.combat.tutorialPanel_) {
            world_.combat.tutorialPanel_->SetPosition({1090.0f, 38.0f});
            world_.combat.tutorialPanel_->SetSize({300.0f, 62.0f});
            world_.combat.tutorialPanel_->Update();
        }
        if (world_.combat.tutorialTitleText_)
            world_.combat.tutorialTitleText_->SetPosition({1090.0f, 23.0f});
        if (world_.combat.tutorialInputText_)
            world_.combat.tutorialInputText_->SetPosition({1090.0f, 50.0f});
    } else {
        if (world_.combat.tutorialPanel_) {
            world_.combat.tutorialPanel_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 128.0f});
            world_.combat.tutorialPanel_->SetSize({390.0f, 126.0f});
            world_.combat.tutorialPanel_->Update();
        }
        if (world_.combat.tutorialTitleText_)
            world_.combat.tutorialTitleText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 91.0f});
        if (world_.combat.tutorialInputText_)
            world_.combat.tutorialInputText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 128.0f});
        if (world_.combat.tutorialDescriptionText_)
            world_.combat.tutorialDescriptionText_->SetPosition({cg2::WinApp::kClientWidth * 0.5f, 160.0f});
    }
    if (world_.combat.tutorialPanel_)
        world_.combat.tutorialPanel_->Draw();
    if (world_.combat.tutorialTitleText_)
        world_.combat.tutorialTitleText_->Draw();
    if (world_.combat.tutorialInputText_)
        world_.combat.tutorialInputText_->Draw();
    if (!compactEvolutionHint && world_.combat.tutorialDescriptionText_)
        world_.combat.tutorialDescriptionText_->Draw();
}
} // namespace gameplay
