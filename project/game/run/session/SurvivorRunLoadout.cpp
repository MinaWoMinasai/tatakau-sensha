#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/player/TankRunModifiers.h"
#include <cmath>
#include <iomanip>

// 育つ戦車の試作の経験値とレベル・部品の3択・形態の2択・部品の効果の反映。

namespace gameplay {

using namespace survivorrun;

void SurvivorRunController::AddExperience(int amount)
{
    if (amount <= 0) {
        return;
    }
    expCollected_ += amount;
    if (level_ >= config_.maxLevel) {
        return;
    }
    exp_ += amount;
    while (level_ < config_.maxLevel && exp_ >= config_.ExpToNextLevel(level_)) {
        exp_ -= config_.ExpToNextLevel(level_);
        ++level_;
        ++pendingChoices_;
        levelTimes_.push_back(elapsed_);
        pendingSince_.push_back(elapsed_);
        world_.combatFlow->SetEventCallout("レベル " + std::to_string(level_), 0.8f);
    }
    choiceHud_.SetPending(pendingChoices_, choiceOpen_);
    // 3レベルごとに積載量が増えるので、左下の表示を作り直す。
    loadoutHud_.SetLoadout(config_.parts, partLevels_, Capacity());
    if (!formChosen_ && level_ >= config_.formLevel) {
        OpenFormChoice();
    } else if (config_.choiceAutoOpen && pendingChoices_ > 0 && !choiceOpen_) {
        OpenChoices();
    }
    if (level_ >= config_.maxLevel) {
        exp_ = 0;
    }
}

void SurvivorRunController::UpdateChoiceInput()
{
    auto& input = *world_.resources.input_;
    if (input.IsKeyTriggered(DIK_TAB)) {
        if (choiceOpen_) {
            CloseChoices();
        } else {
            OpenChoices();
        }
        return;
    }
    if (!choiceOpen_) {
        return;
    }
    if (removalOpen_) {
        UpdateRemovalInput();
        return;
    }
    // 1〜3キーは3択を開いている間だけ使う。
    for (size_t i = 0; i < offers_.size() && i < 3; ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            ChooseOffer(i);
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    choiceHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = choiceHud_.HitTest(mouse);
        if (hit >= 0) {
            ChooseOffer(static_cast<size_t>(hit));
        }
    }
}

void SurvivorRunController::OpenChoices()
{
    if (pendingChoices_ <= 0 || choiceOpen_ || formChoiceOpen_) {
        return;
    }
    choiceOpen_ = true;
    DrawNextOffers();
}

void SurvivorRunController::DrawNextOffers()
{
    // 宝箱の選択が貯まっていれば、先にそれを出す。
    offersPremium_ = pendingPremium_ > 0;
    offers_ = survivor::DrawOffers(config_.parts, partLevels_, config_.choiceLightBias, random_, offersPremium_);
    choiceHud_.SetOffers(offers_, config_.parts, offersPremium_, partLevels_, Capacity());
    choiceHud_.SetPending(pendingChoices_, true);
}

void SurvivorRunController::CloseChoices()
{
    choiceOpen_ = false;
    removalOpen_ = false;
    offers_.clear();
    choiceHud_.SetPending(pendingChoices_, false);
}

void SurvivorRunController::ChooseOffer(size_t index)
{
    if (!choiceOpen_ || index >= offers_.size() || pendingChoices_ <= 0) {
        return;
    }
    const survivor::PartOffer offer = offers_[index];
    // 積載を超える札は、外す部品を選んでから積む（RemovePart が外し終えたら、もう一度ここへ来る）。
    if (survivor::ExceedsLoad(config_.parts, partLevels_, offer.part, Capacity())) {
        OpenRemoval(index);
        return;
    }
    auto& player = *world_.resources.player_;
    // 遊んだ記録：出た3枚・選んだ札・貯まってから選ぶまでの区画の秒・3択を開いていた秒。
    auto key = [&](const survivor::PartOffer& item) {
        return item.part == survivor::PartId::Count ? std::string("repair") : std::string(config_.parts[static_cast<size_t>(item.part)].key);
    };
    float waited = 0.0f;
    if (!pendingSince_.empty()) {
        waited = elapsed_ - pendingSince_.front();
        pendingSince_.erase(pendingSince_.begin());
    }
    choiceRecord_ << (choiceRecord_.tellp() > 0 ? "," : "") << "{\"t\":" << std::lround(elapsed_) << ",\"offered\":[";
    for (size_t i = 0; i < offers_.size(); ++i) {
        choiceRecord_ << (i ? "," : "") << "\"" << key(offers_[i]) << "\"";
    }
    choiceRecord_ << "],\"picked\":\"" << key(offer) << "\",\"premium\":" << (offersPremium_ ? "true" : "false")
                  << ",\"waited\":" << std::lround(waited) << ",\"open\":" << std::round(choiceOpenSeconds_ * 10.0f) / 10.0f << "}";
    choiceOpenSeconds_ = 0.0f;
    if (offer.part == survivor::PartId::Count) {
        player.HealRunPlayer(static_cast<int>(static_cast<float>(player.GetMaxHp()) * 0.3f));
        world_.combatFlow->SetEventCallout("修理キット", 0.7f);
    } else {
        // Q の枠は1つ。別の Q の部品を積んでいれば、外して入れ替える（D6）。
        const survivor::PartId replaced = survivor::ReplacedQSkill(config_.parts, partLevels_, offer.part);
        if (replaced != survivor::PartId::Count) {
            DiscardPart(replaced, offer.part);
            qCooldown_ = 0.0f;
        }
        const size_t part = static_cast<size_t>(offer.part);
        partLevels_[part] = (std::min)(partLevels_[part] + 1, config_.parts[part].maxLevel);
        ApplyParts();
        world_.combatFlow->SetEventCallout(std::string(config_.parts[part].name) + " Lv" + std::to_string(partLevels_[part]), 0.7f);
    }
    --pendingChoices_;
    ++choicesTaken_;
    if (offersPremium_ && pendingPremium_ > 0) {
        --pendingPremium_;
    }
    if (pendingChoices_ > 0) {
        // まだ貯まっていれば、続けて次の3択を出す。
        DrawNextOffers();
    } else {
        CloseChoices();
    }
}

int SurvivorRunController::Capacity() const
{
    return config_.CapacityAt(level_);
}

void SurvivorRunController::OpenRemoval(size_t offerIndex)
{
    removalOpen_ = true;
    removalOffer_ = offerIndex;
    loadoutHud_.SetRemoval(config_.parts, partLevels_, Capacity(), offers_[offerIndex].part, config_.load.refundPerLevel);
}

void SurvivorRunController::UpdateRemovalInput()
{
    auto& input = *world_.resources.input_;
    if (input.IsKeyTriggered(DIK_ESCAPE)) {
        // やめたら、積まずに同じ3択へ戻る（選択は使わない）。Tab は UpdateChoiceInput が3択ごと閉じる。
        removalOpen_ = false;
        return;
    }
    // 1〜9 と 0（10番目）で外す。
    for (size_t row = 0; row < loadoutHud_.RowCount() && row < 10; ++row) {
        const uint8_t key = row < 9 ? static_cast<uint8_t>(DIK_1 + row) : static_cast<uint8_t>(DIK_0);
        if (input.IsKeyTriggered(key)) {
            RemovePart(loadoutHud_.RowPart(row));
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    loadoutHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = loadoutHud_.HitTest(mouse);
        if (hit >= 0) {
            RemovePart(loadoutHud_.RowPart(static_cast<size_t>(hit)));
        }
    }
}

void SurvivorRunController::DiscardPart(survivor::PartId id, survivor::PartId incoming)
{
    const size_t index = static_cast<size_t>(id);
    const int level = partLevels_[index];
    // 外した部品は、Lv に応じた経験値の欠片を自機の足もとに落とす。
    const int refund = level * (std::max)(0, config_.load.refundPerLevel);
    if (refund > 0) {
        pickups_.Drop(world_.resources.player_->GetWorldPosition(), static_cast<uint32_t>(refund), random_);
        expDropped_ += refund;
    }
    partLevels_[index] = 0;
    ApplyParts();
    ++removals_;
    removalRecord_ << (removalRecord_.tellp() > 0 ? "," : "") << "{\"t\":" << std::lround(elapsed_) << ",\"removed\":\""
                   << config_.parts[index].key << "\",\"level\":" << level << ",\"for\":\""
                   << (incoming == survivor::PartId::Count ? "" : config_.parts[static_cast<size_t>(incoming)].key) << "\"}";
    world_.combatFlow->SetEventCallout(std::string(config_.parts[index].name) + " を外した", 0.7f);
}

void SurvivorRunController::RemovePart(survivor::PartId id)
{
    if (!removalOpen_ || id == survivor::PartId::Count || removalOffer_ >= offers_.size()) {
        return;
    }
    if (partLevels_[static_cast<size_t>(id)] <= 0) {
        return;
    }
    DiscardPart(id, offers_[removalOffer_].part);
    // まだ足りなければ外す画面のまま。足りたら、その札を積む。
    if (survivor::ExceedsLoad(config_.parts, partLevels_, offers_[removalOffer_].part, Capacity())) {
        loadoutHud_.SetRemoval(config_.parts, partLevels_, Capacity(), offers_[removalOffer_].part, config_.load.refundPerLevel);
        return;
    }
    removalOpen_ = false;
    ChooseOffer(removalOffer_);
}

void SurvivorRunController::UpdateQ(float gameDt)
{
    qCooldown_ = (std::max)(0.0f, qCooldown_ - gameDt);
    if (world_.resources.input_->IsKeyTriggered(DIK_Q)) {
        UseQ();
    }
}

bool SurvivorRunController::UseQ()
{
    const survivor::PartId skill = survivor::OwnedQSkill(config_.parts, partLevels_);
    if (skill == survivor::PartId::Count || qCooldown_ > 0.0f) {
        return false;
    }
    auto& player = *world_.resources.player_;
    if (skill == survivor::PartId::HeatVent) {
        // 排熱砲：熱が少なすぎると撃てない。撃つと熱は0に戻る。
        const float heat = player.GetHeat().GetHeat();
        if (heat < config_.heatVent.minHeat) {
            world_.combatFlow->SetEventCallout("排熱砲：熱が足りない", 0.6f);
            return false;
        }
        const float power = survivor::EffectPower(config_.parts, partLevels_, skill);
        const float vented = player.FireSurvivorHeatVent(config_.heatVent.damagePerHeat * power);
        if (vented <= 0.0f) {
            return false;
        }
        qCooldown_ = config_.heatVent.cooldown;
        ++qUses_;
        qRecord_ << (qRecord_.tellp() > 0 ? "," : "") << "{\"t\":" << std::round(elapsed_ * 10.0f) / 10.0f << ",\"skill\":\"heatVent\",\"heat\":"
                 << std::lround(vented) << ",\"damageScale\":" << std::round(vented * config_.heatVent.damagePerHeat * power * 10.0f) / 10.0f
                 << "}";
        world_.combatFlow->SetEventCallout("排熱砲", 0.5f);
        return true;
    }
    return false;
}

std::string SurvivorRunController::DescribeQ() const
{
    const survivor::PartId skill = survivor::OwnedQSkill(config_.parts, partLevels_);
    if (skill == survivor::PartId::Count) {
        return {};
    }
    const auto& part = config_.parts[static_cast<size_t>(skill)];
    std::ostringstream text;
    text << "[Q] " << part.name << " Lv" << survivor::LevelOf(partLevels_, skill) << "　";
    if (qCooldown_ > 0.0f) {
        text << "待ち " << std::fixed << std::setprecision(1) << qCooldown_ << "秒";
    } else if (skill == survivor::PartId::HeatVent) {
        const float heat = world_.resources.player_->GetHeat().GetHeat();
        if (heat < config_.heatVent.minHeat) {
            text << "熱が足りない";
        } else {
            const float scale = heat * config_.heatVent.damagePerHeat * survivor::EffectPower(config_.parts, partLevels_, skill);
            text << "威力 x" << std::fixed << std::setprecision(1) << scale;
        }
    }
    return text.str();
}

void SurvivorRunController::OpenFormChoice()
{
    if (formChoiceOpen_ || formChosen_) {
        return;
    }
    if (choiceOpen_) {
        CloseChoices();
    }
    formChoiceOpen_ = true;
    std::vector<std::pair<std::string, std::string>> cards;
    for (size_t i = 0; i < survivor::kFormCount; ++i) {
        const auto& form = config_.forms[i];
        cards.emplace_back(std::to_string(i + 1) + "  " + form.name, survivor::DescribeForm(form, static_cast<survivor::FormId>(i)));
    }
    choiceHud_.SetCards("形態を選ぶ（Lv" + std::to_string(config_.formLevel) + "）", "1・2 キーかクリックで選ぶ　/　選ぶまで止まる",
                        cards);
    world_.combatFlow->SetEventCallout("形態の進化", 0.8f);
}

void SurvivorRunController::UpdateFormChoiceInput()
{
    auto& input = *world_.resources.input_;
    for (size_t i = 0; i < survivor::kFormCount; ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            ChooseForm(i);
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    choiceHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = choiceHud_.HitTest(mouse);
        if (hit >= 0) {
            ChooseForm(static_cast<size_t>(hit));
        }
    }
}

void SurvivorRunController::ChooseForm(size_t index)
{
    if (!formChoiceOpen_ || index >= survivor::kFormCount) {
        return;
    }
    auto& player = *world_.resources.player_;
    const auto id = static_cast<survivor::FormId>(index);
    const auto& form = config_.forms[index];
    const PlayerClassConfig* source = player.FindCatalogClassConfig(form.sourceClass);
    if (!source) {
        source = player.GetActiveClassConfig();
    }
    if (source) {
        Player::SurvivorFormTuning tuning{};
        tuning.heatScale = form.heatScale;
        tuning.railPower = form.railPower;
        tuning.railHeatBase = form.railHeatBase;
        tuning.railHeatPerCharge = form.railHeatPerCharge;
        player.ApplySurvivorForm(survivor::BuildFormConfig(*source, form, id), tuning);
    }
    formKey_ = form.key;
    formChosen_ = true;
    formChoiceOpen_ = false;
    choiceHud_.SetPending(pendingChoices_, false);
    world_.combatFlow->SetEventCallout(std::string("形態：") + form.name, 1.0f);
}

void SurvivorRunController::ApplyParts()
{
    using survivor::PartId;
    auto& player = *world_.resources.player_;
    TankRunModifiers modifiers{};
    modifiers.enabled = true;
    modifiers.survivor = true;
    modifiers.partScales = survivor::MakePartScales(config_.parts, partLevels_);
    survivor::ApplyPartCards(config_.parts, partLevels_, modifiers);
    player.SetRunModifiers(modifiers);

    // 熱とシールドの部品。冷却器は冷える速さ（撃っている間も）、3段で上限+30。シールド容量は上限、3段で回復までの時間が半分。
    PlayerHeat::Config heat = config_.heat;
    const float cool = survivor::LightBonus(config_.parts, partLevels_, PartId::Cooler);
    heat.coolPerSecond *= cool;
    heat.coolPerSecondWhileFiring *= cool;
    if (survivor::HasChange(partLevels_, PartId::Cooler)) {
        heat.maxHeat += 30.0f;
    }
    player.SetHeatConfig(heat);
    Player::ShieldConfig shield{};
    shield.enabled = true;
    shield.maxHpRatio = config_.shieldMaxHpRatio * survivor::LightBonus(config_.parts, partLevels_, PartId::ShieldCapacity);
    shield.regenDelay = config_.shieldRegenDelay * (survivor::HasChange(partLevels_, PartId::ShieldCapacity) ? 0.5f : 1.0f);
    shield.regenPerSecond = config_.shieldRegenPerSecond;
    player.SetShieldConfig(shield);

    // 軽い部品の3段の変化と、重弾頭の熱。
    Player::SurvivorPartEffects effects{};
    effects.extraHeatPerShot = 2.0f * survivor::EffectPower(config_.parts, partLevels_, PartId::HeavyWarhead);
    effects.critChance = survivor::HasChange(partLevels_, PartId::Warhead) ? 0.15f : 0.0f;
    effects.loaderRamp = survivor::HasChange(partLevels_, PartId::Loader);
    effects.spikes = survivor::HasChange(partLevels_, PartId::Ram);
    effects.invincibleScale = survivor::HasChange(partLevels_, PartId::Armor) ? 1.8f : 1.0f;
    player.SetSurvivorPartEffects(effects);
    loadoutHud_.SetLoadout(config_.parts, partLevels_, Capacity());
    maxLoad_ = (std::max)(maxLoad_, survivor::LoadOf(config_.parts, partLevels_));
}

void SurvivorRunController::UpdateSpikes()
{
    // 衝角の3段：トゲの生えた車体に触れた小さなシェイプ（四角・三角・ふつうの五角）は、その場で壊れる。
    auto& player = *world_.resources.player_;
    const myEngine::Vector3 position = player.GetWorldPosition();
    const float reach = player.GetRadius() + 0.35f;
    for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!enemy || enemy->IsDead() || enemy->IsRunResource() || enemy->GetSizeScale() > 1.01f) {
            continue;
        }
        const ExpEnemyType type = enemy->GetType();
        if (type != ExpEnemyType::Square && type != ExpEnemyType::Triangle && type != ExpEnemyType::Pentagon) {
            continue;
        }
        const myEngine::Vector3 offset = enemy->GetWorldPosition() - position;
        const float limit = reach + enemy->GetRadius();
        if (offset.x * offset.x + offset.y * offset.y <= limit * limit && enemy->TakeDamageFromPlayer(1000000u)) {
            ++spikeKills_;
        }
    }
}

} // namespace gameplay
