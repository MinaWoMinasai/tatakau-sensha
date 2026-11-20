#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の波の間のショップ（計画書 p4 の K4）：ランダムに並ぶ技の強化とパークを、レベルポイントで買う。
// 技の強化は3段で、1段目は威力、2段目は待ち時間（待ち時間の無い技は効く秒）、3段目で挙動が変わる。修理はいつも並ぶ。

namespace gameplay {

using survivor::AbilityKind;
using survivor::PerkId;

const survivor::AbilityDefinition& SurvivorRunController::UpgradeAbility(int target) const
{
    return survivor::UpgradeTargetAbility(Hero(), target);
}

void SurvivorRunController::RebuildHero(bool heal)
{
    if (hero_ < 0) {
        return;
    }
    heroDef_ = config_.heroes.heroes[static_cast<size_t>(hero_)];
    // 技の強化（SurvivorShop.h の ApplyUpgradeTiers。遠征の工房と同じ計算）。3段目の挙動は使うところで見る。
    survivor::ApplyUpgradeTiers(heroDef_, upgradeTiers_);
    // 主武器が武器なら、威力（1段ごとに ×1.15、2段まで）と、3段目の小さな爆発。
    heroPrimaryDamage_ = survivor::PrimaryWeaponDamageScale(upgradeTiers_[0]);
    heroPrimaryBlast_ = survivor::PrimaryWeaponBlastRadius(upgradeTiers_[0]);
    if (HasPerk(PerkId::Armor) && heroDef_.maxHp > 0) {
        heroDef_.maxHp += static_cast<int>(PerkValue(PerkId::Armor));
    }
    auto& player = *world_.resources.player_;
    const int missing = player.GetMaxHp() - player.GetHp();
    player.SetSurvivorMaxHp(heroDef_.maxHp, heal);
    // 最大HPが変わっても、減っていた分はそのまま（増えた分だけ HP も増える。傷があればそこまで）。
    if (!heal) {
        const int target = (std::max)(1, player.GetMaxHp() - (std::max)(0, missing));
        if (player.GetHp() < target) {
            player.HealRunPlayer(target - player.GetHp());
        } else if (player.GetHp() > target) {
            player.SpendRunHealth(player.GetHp() - target);
        }
    }
    // 主武器の値（熱・威力・爆発）を入れ直す。今の熱は控えて戻す。
    if (weaponEquipped_) {
        weaponHeat_[activeSlot_] = player.GetHeat().Save();
        RefreshSlotsAfterChange(survivor::kWeaponSlots);
    }
}

int SurvivorRunController::RerollPrice() const
{
    return config_.shop.rerollCost + config_.shop.rerollStep * shopRerolls_;
}

int SurvivorRunController::OfferPrice(const ShopOffer& offer) const
{
    if (offer.upgrade) {
        return config_.shop.upgradePrices[static_cast<size_t>((std::min)(UpgradeTier(offer.index), survivor::kMaxUpgradeTier - 1))];
    }
    return config_.shop.perkPrices[static_cast<size_t>(offer.index)];
}

std::string SurvivorRunController::OfferName(const ShopOffer& offer) const
{
    if (!offer.upgrade) {
        return survivor::GetPerkInfo(static_cast<PerkId>(offer.index)).name;
    }
    return survivor::UpgradeOfferName(Hero(), offer.index, UpgradeTier(offer.index));
}

std::string SurvivorRunController::OfferText(const ShopOffer& offer) const
{
    if (!offer.upgrade) {
        const auto& info = survivor::GetPerkInfo(static_cast<PerkId>(offer.index));
        return std::string(info.text) + (info.hero[0] != '\0' ? "（" + Hero().name + "だけ）" : std::string("（どの機体でも）"));
    }
    return survivor::UpgradeOfferText(Hero(), offer.index, UpgradeTier(offer.index));
}

void SurvivorRunController::RollShop()
{
    // 候補：3段まで上がっていない技の強化と、まだ持っていないパーク（共通と、この機体の物）。
    std::vector<ShopOffer> pool;
    for (int target = 0; target < static_cast<int>(survivor::kUpgradeTargets); ++target) {
        const bool usable = target == 0 || UpgradeAbility(target).kind != AbilityKind::None;
        if (usable && UpgradeTier(target) < survivor::kMaxUpgradeTier) {
            pool.push_back({true, target});
        }
    }
    for (size_t i = 0; i < survivor::kPerkCount; ++i) {
        const auto& info = survivor::GetPerkInfo(static_cast<PerkId>(i));
        if (!perks_[i] && (info.hero[0] == '\0' || Hero().key == info.hero)) {
            pool.push_back({false, static_cast<int>(i)});
        }
    }
    std::shuffle(pool.begin(), pool.end(), random_);
    if (pool.size() > static_cast<size_t>(config_.shop.offerCount)) {
        pool.resize(static_cast<size_t>(config_.shop.offerCount));
    }
    shopOffers_ = pool;
}

bool SurvivorRunController::BuyOffer(size_t index)
{
    if (index >= shopOffers_.size()) {
        return false;
    }
    const ShopOffer offer = shopOffers_[index];
    const int price = OfferPrice(offer);
    if (bag_.money < price) {
        world_.combatFlow->SetEventCallout("レベルポイントが足りない", 0.8f);
        return false;
    }
    shopLog_.push_back(std::to_string(stage_ + 1) + ":" + OfferName(offer) + ":" + std::to_string(price));
    bag_.money -= price;
    shopSpent_ += price;
    if (offer.upgrade) {
        upgradeTiers_[static_cast<size_t>(offer.index)] = (std::min)(survivor::kMaxUpgradeTier, UpgradeTier(offer.index) + 1);
    } else {
        perks_[static_cast<size_t>(offer.index)] = true;
    }
    shopOffers_.erase(shopOffers_.begin() + static_cast<std::ptrdiff_t>(index));
    RebuildHero(false);
    world_.combatFlow->SetEventCallout("買った：" + shopLog_.back().substr(shopLog_.back().find(':') + 1), 0.9f);
    return true;
}

bool SurvivorRunController::RerollShop()
{
    const int price = RerollPrice();
    if (bag_.money < price) {
        return false;
    }
    bag_.money -= price;
    shopSpent_ += price;
    ++shopRerolls_;
    ++shopRerollsTotal_;
    RollShop();
    return true;
}

} // namespace gameplay
