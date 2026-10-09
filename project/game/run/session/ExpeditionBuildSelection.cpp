#include "game/run/session/ExpeditionBuildSelection.h"
#include "game/session/GameplaySystems.h"
#include "StartupTrace.h"
#include "game/run/TankSubmissionValidation.h"

namespace gameplay {

bool ExpeditionBuildSelection::IsExpeditionBuildCardScreen() const
{
    if (!world_.run.expeditionMapEnabled_ || world_.run.tankRunPaused_ || world_.run.expeditionMapPreview_)
        return false;
    if (world_.run.expeditionBuildChoice_)
        return true;
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    return node && (node->kind == tankexp::NodeKind::Upgrade || node->kind == tankexp::NodeKind::Evolution);
}

void ExpeditionBuildSelection::InitializeExpeditionBuildCards()
{
    cg2::StartupTrace::Scope scope("Expedition.BuildCards");
    for (auto& card : world_.run.expeditionRewardCards_) {
        card = std::make_unique<TankRewardCard>();
        card->Initialize(cg2::SpriteCommon::GetInstance());
        card->InitializePreview(cg2::Object3dCommon::GetInstance()->GetSrvManager());
    }
    // Authored maps without the two introductory branches still offer a style
    // before their first encounter. The isolated AI fixture has no map input.
    const auto& nodes = world_.run.expeditionMapRun_.GetDefinition().nodes;
    world_.run.expeditionBuildChoice_ = !world_.run.combatValidationEnabled_ && !world_.run.specialValidationEnabled_ &&
                                        std::none_of(nodes.begin(), nodes.end(), [](const auto& n) {
                                            return tankexp::IsIntroUpgrade(n.role);
                                        });
    RefreshExpeditionBuildCards();
    UpdateExpeditionBuildCards(0);
}

void ExpeditionBuildSelection::SelectExpeditionBuildStyle(int index)
{
    if (!world_.run.expeditionBuildChoice_ || world_.run.expeditionBuildChosen_ || index < 0 || index > 2)
        return;
    const auto style = static_cast<tankbuild::Style>(index);
    if (!world_.run.expeditionTransition_.IsActive()) {
        world_.run.expeditionPendingBuild_ = index;
        world_.run.expeditionRewardCards_[index]->PlayAcquire();
        world_.expeditionMapController->BeginExpeditionPresentation(4, std::string(tankbuild::Name(style)) + " / 機体準備",
                                                                    "取得した強化・現在HP・通貨を引き継ぎます", {0.65f, 0.94f, 1, 1});
        return;
    }
    const int hp = world_.resources.player_->GetHp(), maxHp = world_.resources.player_->GetMaxHp(),
              credits = world_.run.expeditionMapRun_.GetCurrency();
    const auto cards = world_.run.tankRun_.GetCardCounts();
    if (!world_.resources.player_->SetExpeditionCombatStyle(style))
        return;
    world_.run.expeditionBuildStyle_ = style;
    world_.run.expeditionBuildChosen_ = true;
    world_.run.expeditionBuildChoice_ = false;
    world_.arenaRunController->ApplyTankRunCards();
    if (world_.validation.experienceValidationVariant_) {
        world_.validation.experienceBuildPreserved_ =
            hp == world_.resources.player_->GetHp() && maxHp == world_.resources.player_->GetMaxHp() &&
            credits == world_.run.expeditionMapRun_.GetCurrency() && cards == world_.run.tankRun_.GetCardCounts();
        if (!world_.validation.experienceBuildPreserved_)
            world_.validation.experienceValidationErrors_.push_back("Style choice changed health, currency or purchased modules");
        if (style == tankbuild::Style::Drone && world_.resources.player_->GetDronePtrs().size() != 3)
            world_.validation.experienceValidationErrors_.push_back("Drone style did not equip three companions immediately");
    }
    world_.run.expeditionGuideActive_ = false;
    world_.run.tankRunMenuAge_ = 0;
    world_.run.tankRunSelection_ = 0;
    world_.run.expeditionLastFocus_.clear();
    world_.run.expeditionMapStatus_.clear();
    world_.run.tankExpeditionAudio_.Upgrade();
    world_.expeditionController->RefreshTankExpeditionUi();
}

void ExpeditionBuildSelection::RefreshExpeditionBuildCards()
{
    if (!IsExpeditionBuildCardScreen() || !world_.run.expeditionRewardCards_[0])
        return;
    const auto current = world_.resources.player_->GetRunCombatSnapshot();
    for (int i = 0; i < 3; ++i) {
        const size_t offerIndex = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + i);
        if (!world_.run.expeditionBuildChoice_ &&
            (offerIndex >= world_.run.expeditionServiceOffers_.size() ||
             !tankcontent::FindUpgrade(world_.run.expeditionContent_, world_.run.expeditionServiceOffers_[offerIndex])))
            continue;
        TankRewardCardModel model;
        model.style = world_.run.expeditionBuildStyle_;
        model.ownedEffects = world_.run.tankRun_.GetCardCounts();
        model.profile = world_.resources.player_->GetCombatStyleProfile(model.style);
        model.ownedEffectPower = world_.arenaRunController->ExpeditionEffectPowers();
        const auto& growth = world_.run.tankExpeditionBalance_["playerUpgrades"];
        model.growth = {growth.value("maxHp", .15f), growth.value("bulletDamage", .25f), growth.value("bulletSpeed", .2f),
                        growth.value("reloadSpeed", .25f), growth.value("moveSpeed", .12f)};
        model.drones = model.style == tankbuild::Style::Drone
                           ? (std::clamp)(current.baseDroneCount + (current.isAuthored ? current.classDroneCount - 3 : 0), 1, 12)
                           : 0;
        model.currentDamageScale = model.damageScale = current.classDamageScale;
        model.currentReloadScale = model.reloadScale = current.classReloadScale;
        model.currentBulletSpeedScale = model.bulletSpeedScale = current.classBulletSpeedScale;
        model.barrels = (std::max)(1, current.barrels);
        model.reflect = current.classReflects;
        model.penetrate = current.classPenetrates;
        model.currentBarrels = model.barrels;
        model.currentDrones = model.drones;
        model.currentReflect = current.classReflects;
        model.currentPenetrate = current.classPenetrates;
        if (const auto* variant = tankcontent::FindPlayer(world_.run.expeditionContent_, current.classId)) {
            model.fanAngle = model.currentFanAngle = variant->fanAngle;
            model.alternate = model.currentAlternate = variant->alternate;
        }
        if (world_.run.expeditionBuildChoice_) {
            model.style = static_cast<tankbuild::Style>(i);
            model.profile = world_.resources.player_->GetCombatStyleProfile(model.style);
            model.styleChoice = true;
            model.rarity = 0;
            model.id = std::string("style_") + tankbuild::Id(model.style);
            model.title = tankbuild::Name(model.style);
            model.description = i == 0   ? "狙って撃つ。追尾・跳弾・貫通で射撃を育てる。"
                                : i == 1 ? "追従するドローンを左クリックで指揮。集中射撃や迎撃で支える。"
                                         : "左クリックで3段斬り。間合い・連撃・フィニッシュを育てる。";
            model.footer = "左クリックで選択";
            model.drones = i == 1 ? model.profile.droneCount : 0;
            model.currentDrones = model.drones;
            model.barrels = model.currentBarrels = 1;
            model.fanAngle = model.currentFanAngle = 0;
            model.alternate = model.currentAlternate = false;
            model.currentDamageScale = model.damageScale = model.currentReloadScale = model.reloadScale = model.currentBulletSpeedScale =
                model.bulletSpeedScale = 1.0f;
        } else {
            model.id = world_.run.expeditionServiceOffers_[offerIndex];
            const int price = world_.expeditionMapController->ExpeditionServicePrice(model.id);
            model.footer =
                std::to_string(price) + (world_.run.expeditionMapRun_.CanAfford(price) ? "  / 左クリックで装備" : "  / 通貨が足りません");
            const auto* u = tankcontent::FindUpgrade(world_.run.expeditionContent_, model.id);
            model.title = u->name;
            model.description = u->description;
            model.rarity = u->rarity;
            model.effects = u->effects;
            model.effectPower = u->effectPower;
        }
        if (world_.validation.experienceValidationVariant_ && !tanksubmission::Enabled() && world_.run.expeditionBuildChoice_ &&
            world_.validation.experienceValidationStateAge_ > 0.55f) {
            model.rarity = world_.validation.experiencePreviewRarity_;
            model.styleChoice = false;
            model.authoredVariant = "visual_fixture";
        }
        TankRewardPreviewAppearance appearance;
        appearance.playerRadius = world_.presentation.playerNeonBillboardRadius_;
        appearance.lineWidth = world_.presentation.actorNeonBillboardLineWidth_;
        appearance.softEdgeRatio = world_.presentation.neonLineSoftEdgeRatio_;
        appearance.coreIntensity = world_.presentation.neonLineCoreIntensity_;
        appearance.bodyFill = world_.presentation.actorNeonBodyFillColor_;
        appearance.gridColor = world_.presentation.worldGridColor_;
        const auto body = world_.resources.player_->GetNeonBodyLayout();
        appearance.playerColor = body.outlineColor;
        appearance.bodyScale = body.scale;
        const int shapes[] = {28, 4, 3, 5};
        appearance.bodySegments = shapes[(std::clamp)(static_cast<int>(body.shape), 0, 3)];
        appearance.separateCurrentBody = !world_.run.expeditionBuildChoice_;
        appearance.currentPlayerColor = appearance.playerColor;
        appearance.currentBodySegments = appearance.bodySegments;
        appearance.currentBodyScale = appearance.bodyScale;
        appearance.currentPlayerColor.x *= world_.presentation.playerNeonEmission_;
        appearance.currentPlayerColor.y *= world_.presentation.playerNeonEmission_;
        appearance.currentPlayerColor.z *= world_.presentation.playerNeonEmission_;
        if (world_.run.expeditionBuildChoice_) {
            appearance.playerColor = {0.5f, 1, 0.35f, 1};
            appearance.bodySegments = 28;
            appearance.bodyScale = {1, 1};
        }
        if (const auto* variant = tankcontent::FindPlayer(world_.run.expeditionContent_, model.authoredVariant)) {
            appearance.playerColor = {variant->color[0], variant->color[1], variant->color[2], variant->color[3]};
            appearance.bodySegments = shapes[variant->bodyShape];
        }
        appearance.playerColor.x *= world_.presentation.playerNeonEmission_;
        appearance.playerColor.y *= world_.presentation.playerNeonEmission_;
        appearance.playerColor.z *= world_.presentation.playerNeonEmission_;
        appearance.meleeColor.x *= world_.presentation.playerNeonEmission_;
        appearance.meleeColor.y *= world_.presentation.playerNeonEmission_;
        appearance.meleeColor.z *= world_.presentation.playerNeonEmission_;
        appearance.bloomThreshold = world_.resources.neonGridPostEffect_->GetParam().threshold;
        appearance.bloomIntensity =
            world_.presentation.enableNeonGridPostEffect_ ? world_.resources.neonGridPostEffect_->GetParam().intensity : 0;
        appearance.blade = {world_.presentation.playerMeleeBladeOuterWidthScale_, world_.presentation.playerMeleeBladeHaloWidthScale_,
                            world_.presentation.playerMeleeBladeCoreWidthScale_};
        appearance.bulletTrail = world_.resources.bulletManager_->GetTrailSettings();
        world_.run.expeditionRewardCards_[i]->SetPreviewAppearance(appearance);
        world_.run.expeditionRewardCards_[i]->SetModel(model);
    }
}

void ExpeditionBuildSelection::UpdateExpeditionBuildCards(float dt)
{
    if (!IsExpeditionBuildCardScreen() || !world_.run.expeditionRewardCards_[0])
        return;
    const auto mouse = world_.resources.input_->GetMousePosition();
    int hovered = -1;
    for (int i = 0; i < 3; ++i) {
        const size_t index = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + i);
        const bool present = world_.run.expeditionBuildChoice_ ||
                             (index < world_.run.expeditionServiceOffers_.size() &&
                              tankcontent::FindUpgrade(world_.run.expeditionContent_, world_.run.expeditionServiceOffers_[index]));
        if (present && mouse.x >= 64 + i * 388 && mouse.x <= 432 + i * 388 && mouse.y >= 260 && mouse.y <= 590)
            hovered = i;
    }
    if (world_.validation.experienceValidationVariant_)
        hovered = world_.run.expeditionBuildChoice_ ? world_.validation.experienceValidationStyle_ : world_.run.tankRunSelection_;
    for (int i = 0; i < 3; ++i) {
        const size_t index = static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + i);
        const bool present = world_.run.expeditionBuildChoice_ ||
                             (index < world_.run.expeditionServiceOffers_.size() &&
                              tankcontent::FindUpgrade(world_.run.expeditionContent_, world_.run.expeditionServiceOffers_[index]));
        if (!present)
            continue;
        world_.run.expeditionRewardCards_[i]->Update({248.0f + i * 388.0f, 425}, {368, 330}, dt,
                                                     i == hovered && !world_.run.expeditionTransition_.IsActive(), true);
    }
    const std::string focus = "card_" + std::to_string(hovered);
    if (world_.run.expeditionBuildChoice_ && focus != world_.run.expeditionLastFocus_) {
        if (hovered >= 0 && !world_.run.expeditionLastFocus_.empty() && !world_.run.expeditionTransition_.IsActive())
            world_.run.tankExpeditionAudio_.UiHover();
        world_.run.expeditionLastFocus_ = focus;
    }
}

} // namespace gameplay
