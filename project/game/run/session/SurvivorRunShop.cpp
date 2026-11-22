#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <array>
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
    StripLockedAbilities();
    // 主武器が武器なら、威力（1段ごとに ×1.15、2段まで）と、3段目の小さな爆発。
    heroPrimaryDamage_ = survivor::PrimaryWeaponDamageScale(upgradeTiers_[0]);
    heroPrimaryBlast_ = survivor::PrimaryWeaponBlastRadius(upgradeTiers_[0]);
    if (HasPerk(PerkId::Armor) && heroDef_.maxHp > 0) {
        heroDef_.maxHp += static_cast<int>(PerkValue(PerkId::Armor));
    }
    if (heroDef_.maxHp > 0) {
        heroDef_.maxHp += fillerMaxHp_;
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

void SurvivorRunController::StripLockedAbilities()
{
    if (!config_.heroes.onlyUltimate) {
        return;
    }
    for (const auto slot : {survivor::AbilitySlot::Secondary, survivor::AbilitySlot::Mobility, survivor::AbilitySlot::Skill}) {
        heroDef_.abilities[static_cast<size_t>(slot)] = {};
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

std::string SurvivorRunController::FillerName(Filler filler) const
{
    switch (filler) {
    case Filler::Heal:
        return "修理キット";
    case Filler::MaxHp:
        return "装甲の継ぎ足し";
    case Filler::Power:
        return "火薬の増量";
    default:
        return "";
    }
}

std::string SurvivorRunController::FillerText(Filler filler) const
{
    switch (filler) {
    case Filler::Heal:
        return "HP を最大HPの " + std::to_string(static_cast<int>(std::round(config_.heroes.fillerHeal * 100.0f))) + "% 回復する（傷も同じだけ治る）";
    case Filler::MaxHp:
        return "最大HP +" + std::to_string(config_.heroes.fillerMaxHp) + "（今 +" + std::to_string(fillerMaxHp_) + "）";
    case Filler::Power:
        return "すべての威力 +" + std::to_string(static_cast<int>(std::round(config_.heroes.fillerPower * 100.0f))) + "%（今 +" +
               std::to_string(static_cast<int>(std::round(fillerPower_ * 100.0f))) + "%）";
    default:
        return "";
    }
}

std::string SurvivorRunController::OfferName(const ShopOffer& offer) const
{
    if (offer.filler != Filler::None) {
        return FillerName(offer.filler);
    }
    if (offer.skill) {
        const auto skill = static_cast<survivor::HeroSkill>(offer.index);
        return survivor::HeroSkillOfferName(skill, heroSkillTiers_[static_cast<size_t>(skill)]);
    }
    if (offer.sub) {
        const auto weapon = static_cast<survivor::SubWeapon>(offer.index);
        return survivor::SubWeaponOfferName(weapon, SubWeaponTier(weapon));
    }
    if (!offer.upgrade) {
        // 段のあるパークは、上げた後の Lv を付ける（計画書 p8 の F1）。
        const std::string name = survivor::GetPerkInfo(static_cast<PerkId>(offer.index)).name;
        return PerkMaxTierOf(static_cast<size_t>(offer.index)) > 1 ? name + " Lv" + std::to_string(perks_[static_cast<size_t>(offer.index)] + 1) : name;
    }
    return survivor::UpgradeOfferName(Hero(), offer.index, UpgradeTier(offer.index));
}

std::string SurvivorRunController::OfferText(const ShopOffer& offer) const
{
    if (offer.filler != Filler::None) {
        return FillerText(offer.filler) + "\n（選ぶ物が残っていないときに並ぶ）";
    }
    if (offer.skill) {
        const auto skill = static_cast<survivor::HeroSkill>(offer.index);
        return survivor::HeroSkillOfferText(skill, heroSkillTiers_[static_cast<size_t>(skill)]);
    }
    if (offer.sub) {
        const auto weapon = static_cast<survivor::SubWeapon>(offer.index);
        return survivor::SubWeaponOfferText(weapon, SubWeaponTier(weapon));
    }
    if (!offer.upgrade) {
        const auto& info = survivor::GetPerkInfo(static_cast<PerkId>(offer.index));
        if (PerkMaxTierOf(static_cast<size_t>(offer.index)) > 1) {
            const int next = perks_[static_cast<size_t>(offer.index)] + 1;
            return std::string(info.text) + "\nLv" + std::to_string(next) + "/" + std::to_string(PerkMaxTierOf(static_cast<size_t>(offer.index))) + "：" +
                   survivor::PerkTierText(static_cast<PerkId>(offer.index), config_.shop.perkValues[static_cast<size_t>(offer.index)], next);
        }
        return std::string(info.text) + (info.hero[0] != '\0' ? "（" + Hero().name + "だけ）" : std::string("（どの機体でも）"));
    }
    return survivor::UpgradeOfferText(Hero(), offer.index, UpgradeTier(offer.index));
}

bool SurvivorRunController::OfferAwakens(const ShopOffer& offer) const
{
    if (offer.skill) {
        return heroSkillTiers_[static_cast<size_t>(offer.index)] == survivor::kHeroSkillMaxTier - 1;
    }
    if (offer.sub) {
        return SubWeaponTier(static_cast<survivor::SubWeapon>(offer.index)) == survivor::kMaxSubWeaponTier - 1;
    }
    return offer.upgrade && UpgradeTier(offer.index) == survivor::MaxUpgradeTier(Hero(), offer.index) - 1;
}

std::vector<SurvivorRunController::ShopOffer> SurvivorRunController::ShopOfferPool(bool withSubWeapons) const
{
    // 候補：3段まで上がっていない技の強化と、まだ持っていないパーク（共通と、この機体の物）。
    std::vector<ShopOffer> pool;
    for (int target = 0; target < static_cast<int>(survivor::kUpgradeTargets); ++target) {
        const bool usable = target == 0 || UpgradeAbility(target).kind != AbilityKind::None;
        if (usable && UpgradeTier(target) < survivor::MaxUpgradeTier(Hero(), target)) {
            pool.push_back({true, target});
        }
    }
    // 枠（計画書 p8 の F1）：持っている数が枠に届いたら、持っていない物は並べない（持っている物の段上げだけ）。
    int ownedPerks = 0;
    for (const int tier : perks_) {
        ownedPerks += tier > 0 ? 1 : 0;
    }
    const bool perksFull = config_.heroes.perkSlots > 0 && ownedPerks >= config_.heroes.perkSlots;
    for (size_t i = 0; i < survivor::kPerkCount; ++i) {
        const auto& info = survivor::GetPerkInfo(static_cast<PerkId>(i));
        // 右クリック・Shift・E を使わない遊び方では、機体専用のパークと「連鎖の冷却」（技の待ち時間）は効かないので並べない。
        const bool locked = config_.heroes.onlyUltimate && (info.hero[0] != '\0' || static_cast<PerkId>(i) == PerkId::KillCooldown);
        const bool open = perks_[i] > 0 || !perksFull;
        if (perks_[i] < PerkMaxTierOf(i) && open && !locked && (info.hero[0] == '\0' || Hero().key == info.hero)) {
            pool.push_back({false, static_cast<int>(i)});
        }
    }
    // 機体専用スキル（計画書 p8 の F2）：この機体の物で、3段まで上がっていない物。
    if (config_.heroes.heroSkills && config_.heroes.levelUps) {
        for (size_t i = 0; i < survivor::kHeroSkillCount; ++i) {
            const auto& info = survivor::GetHeroSkillInfo(static_cast<survivor::HeroSkill>(i));
            if (Hero().key == info.hero && heroSkillTiers_[i] < survivor::kHeroSkillMaxTier) {
                ShopOffer offer;
                offer.index = static_cast<int>(i);
                offer.skill = true;
                pool.push_back(offer);
            }
        }
    }
    if (withSubWeapons) {
        const int ownedSubs = OwnedSubWeapons();
        const bool subsFull = config_.heroes.subWeaponSlots > 0 && ownedSubs >= config_.heroes.subWeaponSlots;
        for (size_t i = 0; i < survivor::kSubWeaponCount; ++i) {
            if (subWeaponTiers_[i] < survivor::kMaxSubWeaponTier && (subWeaponTiers_[i] > 0 || !subsFull)) {
                pool.push_back({false, static_cast<int>(i), true});
            }
        }
    }
    return pool;
}

int SurvivorRunController::OwnedSubWeapons() const
{
    int owned = 0;
    for (const int tier : subWeaponTiers_) {
        owned += tier > 0 ? 1 : 0;
    }
    return owned;
}

int SurvivorRunController::OwnedPerks() const
{
    int owned = 0;
    for (const int tier : perks_) {
        owned += tier > 0 ? 1 : 0;
    }
    return owned;
}

void SurvivorRunController::ApplyOffer(const ShopOffer& offer)
{
    if (offer.filler != Filler::None) {
        ++fillersTaken_;
        auto& player = *world_.resources.player_;
        if (offer.filler == Filler::Heal) {
            const int amount = static_cast<int>(std::round(static_cast<float>(player.GetMaxHp()) * config_.heroes.fillerHeal));
            player.ReduceSurvivorWound(static_cast<float>(amount));
            player.HealRunPlayer(amount);
        } else if (offer.filler == Filler::MaxHp) {
            fillerMaxHp_ += config_.heroes.fillerMaxHp;
            RebuildHero(false);
        } else {
            fillerPower_ += config_.heroes.fillerPower;
        }
        return;
    }
    if (offer.skill) {
        int& tier = heroSkillTiers_[static_cast<size_t>(offer.index)];
        tier = (std::min)(survivor::kHeroSkillMaxTier, tier + 1);
        return;
    }
    if (offer.sub) {
        int& tier = subWeaponTiers_[static_cast<size_t>(offer.index)];
        tier = (std::min)(survivor::kMaxSubWeaponTier, tier + 1);
        return;
    }
    if (offer.upgrade) {
        upgradeTiers_[static_cast<size_t>(offer.index)] = (std::min)(survivor::MaxUpgradeTier(Hero(), offer.index), UpgradeTier(offer.index) + 1);
    } else {
        int& tier = perks_[static_cast<size_t>(offer.index)];
        tier = (std::min)(PerkMaxTierOf(static_cast<size_t>(offer.index)), tier + 1);
    }
    RebuildHero(false);
}

void SurvivorRunController::OpenHeroLevelUp()
{
    const bool subWeapons = config_.heroes.subWeapons;
    std::vector<ShopOffer> pool = ShopOfferPool(subWeapons);
    const bool fillers = config_.heroes.fillerHeal > 0.0f || config_.heroes.fillerMaxHp > 0 || config_.heroes.fillerPower > 0.0f;
    if (pool.empty() && !fillers) {
        // 全部そろったら、選ぶ物は無い（レベルは上がるが止めない）。
        heroPendingLevelUps_ = 0;
        heroLevelUpOpen_ = false;
        return;
    }
    // 種類ごとの数の上限まで寄せる（足りなければ残りから）。副武器が無いときは技の強化2つ・パーク1つ、
    // あるときは技の強化・副武器・パークを1つずつ。覚醒（3段目）の手前の物は優先して並べる。
    std::shuffle(pool.begin(), pool.end(), random_);
    std::stable_sort(pool.begin(), pool.end(), [this](const ShopOffer& a, const ShopOffer& b) {
        return OfferAwakens(a) && !OfferAwakens(b);
    });
    heroLevelUpOffers_.clear();
    // [0] 技の強化、[1] パーク、[2] 副武器、[3] 専用スキル（計画書 p8 の F2）。種類ごとに1つまでで、並べた順に3つ。
    const std::array<int, 4> limits = subWeapons ? std::array<int, 4>{1, 1, 1, 1} : std::array<int, 4>{2, 1, 0, 1};
    std::array<int, 4> taken{};
    for (const ShopOffer& offer : pool) {
        if (heroLevelUpOffers_.size() >= 3) {
            break;
        }
        const size_t kind = offer.upgrade ? 0 : offer.sub ? 2 : offer.skill ? 3 : 1;
        if (taken[kind] < limits[kind]) {
            heroLevelUpOffers_.push_back(offer);
            ++taken[kind];
        }
    }
    for (const ShopOffer& offer : pool) {
        if (heroLevelUpOffers_.size() >= 3) {
            break;
        }
        if (std::find_if(heroLevelUpOffers_.begin(), heroLevelUpOffers_.end(), [&](const ShopOffer& o) {
                return o.Same(offer);
            }) == heroLevelUpOffers_.end()) {
            heroLevelUpOffers_.push_back(offer);
        }
    }
    // 3つに満たなければ、埋め草で足す（計画書 p8 の F1。HoloCure と同じく、全部そろっても選ぶ物は出る）。
    const std::array<std::pair<Filler, bool>, 3> kFillers{{{Filler::Heal, config_.heroes.fillerHeal > 0.0f},
                                                           {Filler::MaxHp, config_.heroes.fillerMaxHp > 0},
                                                           {Filler::Power, config_.heroes.fillerPower > 0.0f}}};
    for (const auto& [filler, on] : kFillers) {
        if (heroLevelUpOffers_.size() >= 3) {
            break;
        }
        if (on) {
            ShopOffer offer;
            offer.filler = filler;
            heroLevelUpOffers_.push_back(offer);
        }
    }
    heroLevelUpOpen_ = true;
    audio_.Play("ui_click");
    RefreshHeroLevelUp();
}

void SurvivorRunController::ChooseHeroLevelUp(size_t index)
{
    if (!heroLevelUpOpen_ || index >= heroLevelUpOffers_.size()) {
        return;
    }
    const ShopOffer offer = heroLevelUpOffers_[index];
    heroLevelUpLog_.push_back(std::to_string(level_ - heroPendingLevelUps_ + 1) + ":" + OfferName(offer));
    world_.combatFlow->SetEventCallout(OfferName(offer), 0.9f);
    ApplyOffer(offer);
    heroPendingLevelUps_ = (std::max)(0, heroPendingLevelUps_ - 1);
    heroLevelUpOpen_ = false;
    // まだ選べるレベルが残っていれば、続けて引き直す。
    if (heroPendingLevelUps_ > 0) {
        OpenHeroLevelUp();
    }
}

void SurvivorRunController::RollShop()
{
    std::vector<ShopOffer> pool = ShopOfferPool();
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
    shopOffers_.erase(shopOffers_.begin() + static_cast<std::ptrdiff_t>(index));
    ApplyOffer(offer);
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
