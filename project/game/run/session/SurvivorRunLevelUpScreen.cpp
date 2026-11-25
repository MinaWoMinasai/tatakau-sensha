#include "game/run/session/SurvivorRunController.h"
#include "game/run/SurvivorUpgradeEffects.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 無限の場の強化の画面と Tab の持ち物の画面（計画書 p9 の L1、モックどおり）。中身を組み立て、入力を渡す。
// 画面そのものは SurvivorLevelUpScreen、左の動くプレビューは SurvivorUpgradePreview、1行の効果は SurvivorUpgradeEffects.h。

namespace gameplay {

using survivor::HeroSkill;
using survivor::PerkId;
using survivor::SubWeapon;
using LevelUpCard = SurvivorLevelUpScreen::Card;

namespace {
// 種類ごとの色（モック）：副武器は緑、パークは水色、専用スキルは紫、主武器・技は橙、埋め草は白に近い色。
constexpr myEngine::Vector4 kSubColor{0.24f, 1.0f, 0.66f, 1.0f};
constexpr myEngine::Vector4 kPerkColor{0.37f, 0.78f, 1.0f, 1.0f};
constexpr myEngine::Vector4 kSkillColor{0.75f, 0.48f, 1.0f, 1.0f};
constexpr myEngine::Vector4 kMainColor{1.0f, 0.70f, 0.28f, 1.0f};
constexpr myEngine::Vector4 kFillerColor{0.82f, 0.80f, 0.92f, 1.0f};

std::string UpgradeIcon(int target)
{
    return target == 0 ? "main" : target == 4 ? "ultimate" : "ability";
}
std::string LevelChange(int before)
{
    return before <= 0 ? std::string("Lv 1") : "Lv " + std::to_string(before) + " → " + std::to_string(before + 1);
}
} // namespace

LevelUpCard SurvivorRunController::OfferCard(const ShopOffer& offer) const
{
    LevelUpCard card;
    card.awaken = OfferAwakens(offer);
    const size_t index = static_cast<size_t>((std::max)(0, offer.index));
    if (offer.filler != Filler::None) {
        card.icon = offer.filler == Filler::Heal ? "filler_heal" : offer.filler == Filler::MaxHp ? "filler_maxHp" : "filler_power";
        card.color = kFillerColor;
        card.name = FillerName(offer.filler);
        card.effect = offer.filler == Filler::Heal    ? "HP を " + std::to_string(static_cast<int>(std::round(config_.heroes.fillerHeal * 100.0f))) + "% 回復"
                      : offer.filler == Filler::MaxHp ? "最大HP +" + std::to_string(config_.heroes.fillerMaxHp)
                                                      : "すべての威力 +" + std::to_string(static_cast<int>(std::round(config_.heroes.fillerPower * 100.0f))) + "%";
        return card;
    }
    int before = 0;
    if (offer.skill) {
        const auto& info = survivor::GetHeroSkillInfo(static_cast<HeroSkill>(offer.index));
        before = heroSkillTiers_[index];
        card.icon = std::string("skill_") + info.key;
        card.color = kSkillColor;
        card.name = info.name;
        card.effect = survivor::HeroSkillEffect(static_cast<HeroSkill>(offer.index), before);
    } else if (offer.sub) {
        const auto& info = survivor::GetSubWeaponInfo(static_cast<SubWeapon>(offer.index));
        before = subWeaponTiers_[index];
        card.icon = std::string("sub_") + info.key;
        card.color = kSubColor;
        card.name = info.name;
        card.effect = survivor::SubWeaponEffect(static_cast<SubWeapon>(offer.index), before);
    } else if (offer.upgrade) {
        const auto& ability = survivor::UpgradeTargetAbility(Hero(), offer.index);
        before = UpgradeTier(offer.index) + 1; // 技は始めから持っているので、Lv1 が段0
        card.icon = UpgradeIcon(offer.index);
        card.color = kMainColor;
        card.name = ability.kind != survivor::AbilityKind::None ? ability.name : std::string("主武器");
        card.effect = survivor::UpgradeEffect(Hero(), offer.index, UpgradeTier(offer.index));
    } else {
        const auto& info = survivor::GetPerkInfo(static_cast<PerkId>(offer.index));
        before = perks_[index];
        card.icon = std::string("perk_") + info.key;
        card.color = kPerkColor;
        card.name = info.name;
        card.effect = survivor::PerkEffect(static_cast<PerkId>(offer.index), config_.shop.perkValues[index], before);
    }
    const bool single = !offer.upgrade && !offer.sub && !offer.skill && PerkMaxTierOf(index) <= 1;
    card.level = single ? std::string() : LevelChange(before);
    card.badge = card.awaken ? "覚醒" : before == 0 ? "NEW" : "";
    card.tier = before + 1;
    return card;
}

std::vector<SurvivorLevelUpScreen::Group> SurvivorRunController::OwnedGroups() const
{
    using Group = SurvivorLevelUpScreen::Group;
    using Slot = SurvivorLevelUpScreen::Slot;
    Group main{"主武器・Q", {}}, subs{"副武器", {}}, perks{"パーク", {}}, skills{"専用", {}};
    for (const int target : {0, 4}) {
        const bool has = target == 0 || survivor::UpgradeTargetAbility(Hero(), target).kind != survivor::AbilityKind::None;
        if (has) {
            main.slots.push_back({UpgradeIcon(target), kMainColor, UpgradeTier(target) + 1, survivor::MaxUpgradeTier(Hero(), target) + 1});
        }
    }
    for (size_t i = 0; i < survivor::kSubWeaponCount; ++i) {
        if (subWeaponTiers_[i] > 0) {
            subs.slots.push_back({std::string("sub_") + survivor::GetSubWeaponInfo(static_cast<SubWeapon>(i)).key, kSubColor, subWeaponTiers_[i],
                                  survivor::kMaxSubWeaponTier});
        }
    }
    for (size_t i = 0; i < survivor::kPerkCount; ++i) {
        if (perks_[i] > 0) {
            perks.slots.push_back({std::string("perk_") + survivor::GetPerkInfo(static_cast<PerkId>(i)).key, kPerkColor, perks_[i], PerkMaxTierOf(i)});
        }
    }
    for (size_t i = 0; i < survivor::kHeroSkillCount; ++i) {
        if (heroSkillTiers_[i] > 0) {
            skills.slots.push_back({std::string("skill_") + survivor::GetHeroSkillInfo(static_cast<HeroSkill>(i)).key, kSkillColor, heroSkillTiers_[i],
                                    survivor::kHeroSkillMaxTier});
        }
    }
    // 空いている枠も出す（設定の枠の数まで）。
    auto fill = [](Group& group, int slots) {
        while (static_cast<int>(group.slots.size()) < slots) {
            group.slots.push_back(Slot{});
        }
    };
    if (config_.heroes.subWeapons) {
        fill(subs, config_.heroes.subWeaponSlots);
    }
    fill(perks, config_.heroes.perkSlots);
    return {main, subs, perks, skills};
}

void SurvivorRunController::OpenLevelUpView()
{
    SurvivorLevelUpScreen::View view;
    // 一度に2つ以上上がったときは、選んでいる方のレベルを出す。
    view.level = level_ - (std::max)(0, heroPendingLevelUps_ - 1);
    view.pending = heroPendingLevelUps_;
    for (const ShopOffer& offer : heroLevelUpOffers_) {
        view.cards.push_back(OfferCard(offer));
    }
    view.groups = OwnedGroups();
    levelUpScreen_.Open(view);
    levelUpMouse_ = world_.resources.input_->GetMousePosition();
}

void SurvivorRunController::OpenInventoryView()
{
    SurvivorLevelUpScreen::View view;
    view.inventory = true;
    view.level = level_;
    // 今の効き目を出す：持っている段に上げたときの1行（始めからある主武器の Lv1 だけは名前のみ）。
    auto add = [&](const std::string& icon, const myEngine::Vector4& color, const std::string& name, int tier, int maxTier, const std::string& effect) {
        LevelUpCard card;
        card.icon = icon;
        card.color = color;
        card.name = name;
        card.level = maxTier > 1 ? "Lv " + std::to_string(tier) + (tier >= maxTier ? "　MAX" : "") : std::string();
        card.effect = effect;
        card.tier = tier;
        card.awaken = maxTier > 1 && tier >= maxTier;
        view.cards.push_back(card);
    };
    for (const int target : {0, 4}) {
        const auto& ability = survivor::UpgradeTargetAbility(Hero(), target);
        if (target != 0 && ability.kind == survivor::AbilityKind::None) {
            continue;
        }
        const int tier = UpgradeTier(target);
        add(UpgradeIcon(target), kMainColor, ability.kind != survivor::AbilityKind::None ? ability.name : std::string("主武器"), tier + 1,
            survivor::MaxUpgradeTier(Hero(), target) + 1, tier > 0 ? survivor::UpgradeEffect(Hero(), target, tier - 1) : std::string(" "));
    }
    for (size_t i = 0; i < survivor::kSubWeaponCount; ++i) {
        if (subWeaponTiers_[i] > 0) {
            const auto& info = survivor::GetSubWeaponInfo(static_cast<SubWeapon>(i));
            add(std::string("sub_") + info.key, kSubColor, info.name, subWeaponTiers_[i], survivor::kMaxSubWeaponTier,
                survivor::SubWeaponEffect(static_cast<SubWeapon>(i), subWeaponTiers_[i] - 1));
        }
    }
    for (size_t i = 0; i < survivor::kHeroSkillCount; ++i) {
        if (heroSkillTiers_[i] > 0) {
            const auto& info = survivor::GetHeroSkillInfo(static_cast<HeroSkill>(i));
            add(std::string("skill_") + info.key, kSkillColor, info.name, heroSkillTiers_[i], survivor::kHeroSkillMaxTier,
                survivor::HeroSkillEffect(static_cast<HeroSkill>(i), heroSkillTiers_[i] - 1));
        }
    }
    for (size_t i = 0; i < survivor::kPerkCount; ++i) {
        if (perks_[i] > 0) {
            const auto& info = survivor::GetPerkInfo(static_cast<PerkId>(i));
            // パークは合計の値（段の無い物は説明）。
            add(std::string("perk_") + info.key, kPerkColor, info.name, perks_[i], PerkMaxTierOf(i),
                PerkMaxTierOf(i) > 1 ? survivor::PerkTierText(static_cast<PerkId>(i), config_.shop.perkValues[i], perks_[i]) : std::string(info.text));
        }
    }
    levelUpScreen_.Open(view);
    levelUpMouse_ = world_.resources.input_->GetMousePosition();
}

void SurvivorRunController::UpdateLevelUpScreen(float dt)
{
    SurvivorLevelUpScreen::Input in;
    // 自動確認は、決めるのを UpdateAutoTest が直接行う（本物の入力は読まない）。
    if (autoTest_ == AutoTest::None) {
        auto& input = *world_.resources.input_;
        in.mouse = input.GetMousePosition();
        in.mouseMoved = std::abs(in.mouse.x - levelUpMouse_.x) + std::abs(in.mouse.y - levelUpMouse_.y) > 1.0f;
        levelUpMouse_ = in.mouse;
        in.click = input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0]);
        const int columns = inventoryOpen_ && !heroLevelUpOpen_ ? 2 : 1;
        if (input.IsKeyTriggered(DIK_DOWN) || input.IsKeyTriggered(DIK_S) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_DOWN)) {
            in.move += columns;
        }
        if (input.IsKeyTriggered(DIK_UP) || input.IsKeyTriggered(DIK_W) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_UP)) {
            in.move -= columns;
        }
        if (columns > 1) {
            in.move += input.IsKeyTriggered(DIK_RIGHT) || input.IsKeyTriggered(DIK_D) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0;
            in.move -= input.IsKeyTriggered(DIK_LEFT) || input.IsKeyTriggered(DIK_A) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT) ? 1 : 0;
        }
        in.confirm = input.IsKeyTriggered(DIK_RETURN) || input.IsKeyTriggered(DIK_SPACE) || input.IsGamepadButtonTrigger(XINPUT_GAMEPAD_A);
        for (int i = 0; i < 3; ++i) {
            if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
                in.number = i;
            }
        }
    }
    const size_t before = levelUpScreen_.GetSelected();
    const int picked = levelUpScreen_.Update(dt, in);
    if (levelUpScreen_.GetSelected() != before) {
        audio_.Play("ui_click");
    }
    if (picked >= 0 && heroLevelUpOpen_) {
        audio_.Play("ui_drop");
        ChooseHeroLevelUp(static_cast<size_t>(picked));
    }
}

} // namespace gameplay
