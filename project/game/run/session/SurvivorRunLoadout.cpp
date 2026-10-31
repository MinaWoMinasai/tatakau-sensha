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
    // 形態：Lv5（formLevel）で第1段、stage2Level で第2段（進化）の選択を出す。
    if ((formStage_ == 0 && level_ >= config_.formLevel) || (formStage_ == 1 && level_ >= config_.stage2Level)) {
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
    if (pendingChoices_ <= 0 || choiceOpen_ || formChoiceOpen_ || traitChoiceOpen_ || relicChoiceOpen_) {
        return;
    }
    choiceOpen_ = true;
    QueueHint(Hint::Load);
    DrawNextOffers();
}

void SurvivorRunController::DrawNextOffers()
{
    // 宝箱の選択が貯まっていれば、先にそれを出す。
    offersPremium_ = pendingPremium_ > 0;
    offers_ = survivor::DrawOffers(config_.parts, partLevels_, config_.choiceLightBias, random_, offersPremium_, &destroyedParts_,
                                   config_.choiceFusionBias);
    const std::vector<std::string> notes = OfferNotes();
    choiceHud_.SetOffers(offers_, config_.parts, offersPremium_, partLevels_, Capacity(), &notes);
    choiceHud_.SetPending(pendingChoices_, true);
}

std::vector<std::string> SurvivorRunController::OfferNotes() const
{
    // 札の部品が、まだ進める鍵付きの進化の鍵なら「進化の鍵：○○」と書く（基本砲の間は、どちらの系統の鍵も書く）。
    const survivor::PartLevels levels = survivor::EffectiveLevels(config_.parts, partLevels_);
    std::vector<std::string> notes(offers_.size());
    if (formStage_ >= 2) {
        return notes;
    }
    for (size_t i = 0; i < offers_.size(); ++i) {
        const survivor::PartId part = offers_[i].part;
        if (part == survivor::PartId::Count || levels[static_cast<size_t>(part)] >= config_.parts[static_cast<size_t>(part)].maxLevel) {
            continue;
        }
        for (const auto& form : config_.forms) {
            if (form.keyPart == part && (formStage_ == 0 || form.parent == formId_)) {
                notes[i] = std::string("進化の鍵：") + form.name + "（Lv3）";
            }
        }
    }
    return notes;
}

void SurvivorRunController::RefreshEvolveText()
{
    // 次の形態の節目と、今の形態の鍵付きの進化の鍵の進み具合。進化しきったら出さない。
    if (!evolveText_) {
        return;
    }
    if (formStage_ == 0) {
        evolveText_->SetText("Lv" + std::to_string(config_.formLevel) + "：形態を選ぶ");
        return;
    }
    if (formStage_ >= 2) {
        evolveText_->SetText(" ");
        return;
    }
    std::string text = "Lv" + std::to_string(config_.stage2Level) + "：進化";
    const survivor::PartLevels levels = survivor::EffectiveLevels(config_.parts, partLevels_);
    for (const auto& form : config_.forms) {
        if (form.parent == formId_ && form.keyPart != survivor::PartId::Count) {
            const auto& key = config_.parts[static_cast<size_t>(form.keyPart)];
            text += std::string("　鍵［") + key.name + " " + std::to_string(levels[static_cast<size_t>(form.keyPart)]) + "/" +
                    std::to_string(key.maxLevel) + " → " + form.name + "］";
        }
    }
    evolveText_->SetText(text);
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
        const bool fusion = survivor::IsFusion(config_.parts[part]);
        if (fusion) {
            // 合体：元の2つを外して1つにする。外した部品の欠片は落とさない（重さが1軽くなるのが得）。
            partLevels_[static_cast<size_t>(config_.parts[part].fuseA)] = 0;
            partLevels_[static_cast<size_t>(config_.parts[part].fuseB)] = 0;
            ++fusions_;
            fusionTime_ = elapsed_;
        }
        partLevels_[part] = (std::min)(partLevels_[part] + 1, config_.parts[part].maxLevel);
        ApplyParts();
        world_.combatFlow->SetEventCallout(fusion ? std::string("合体　") + config_.parts[part].name
                                                  : std::string(config_.parts[part].name) + " Lv" + std::to_string(partLevels_[part]),
                                           fusion ? 1.4f : 0.7f);
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
    if (skill == survivor::PartId::EmergencyPurge) {
        return UseEmergencyPurge();
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

bool SurvivorRunController::UseEmergencyPurge()
{
    using survivor::PartId;
    // 壊すのは、緊急放出のほかで一番重い部品（同じ重さなら Lv の高い方）。重さ0しか無ければ使えない。
    size_t target = survivor::kPartCount;
    for (size_t i = 0; i < survivor::kPartCount; ++i) {
        if (partLevels_[i] <= 0 || static_cast<PartId>(i) == PartId::EmergencyPurge || config_.parts[i].weight <= 0) {
            continue;
        }
        if (target == survivor::kPartCount || config_.parts[i].weight > config_.parts[target].weight ||
            (config_.parts[i].weight == config_.parts[target].weight && partLevels_[i] > partLevels_[target])) {
            target = i;
        }
    }
    if (target == survivor::kPartCount) {
        world_.combatFlow->SetEventCallout("緊急放出：壊せる部品がない", 0.6f);
        return false;
    }
    const auto& tuning = config_.partTuning;
    const float power = survivor::EffectPower(config_.parts, partLevels_, PartId::EmergencyPurge);
    const float weight = static_cast<float>(config_.parts[target].weight);
    const float radius = tuning.purgeRadiusBase + tuning.purgeRadiusPerWeight * weight;
    const float damageScale = tuning.purgeDamagePerWeight * weight * power;
    world_.resources.player_->TriggerSurvivorBlast(radius, damageScale);
    // 壊した部品は区画の間なくなり（欠片も落とさない）、緊急放出も使い切る。
    const int destroyedLevel = partLevels_[target];
    partLevels_[target] = 0;
    destroyedParts_[target] = true;
    partLevels_[static_cast<size_t>(PartId::EmergencyPurge)] = 0;
    ApplyParts();
    ++qUses_;
    ++purges_;
    qRecord_ << (qRecord_.tellp() > 0 ? "," : "") << "{\"t\":" << std::round(elapsed_ * 10.0f) / 10.0f
             << ",\"skill\":\"emergencyPurge\",\"destroyed\":\"" << config_.parts[target].key << "\",\"level\":" << destroyedLevel
             << ",\"radius\":" << radius << ",\"damageScale\":" << damageScale << "}";
    world_.presentation.cameraShakeDuration_ = 0.45f;
    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ = 0.8f;
    world_.combatFlow->SetEventCallout(std::string("緊急放出：") + config_.parts[target].name + " を壊した", 1.0f);
    return true;
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
    } else if (skill == survivor::PartId::EmergencyPurge) {
        text << "一番重い部品を壊して爆発（1回）";
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

void SurvivorRunController::OpenTraitChoice()
{
    traitChoiceOpen_ = true;
    std::vector<std::pair<std::string, std::string>> cards;
    for (size_t i = 0; i < survivor::kTraitCount; ++i) {
        cards.emplace_back(std::to_string(i + 1) + "  " + survivor::TraitName(static_cast<survivor::TraitId>(i)),
                           DescribeTrait(static_cast<survivor::TraitId>(i)));
    }
    choiceHud_.SetCards("特性を選ぶ（この区画のあいだ続く）", "1・2 キーかクリックで選ぶ　/　選ぶまで止まる", cards);
}

void SurvivorRunController::UpdateTraitChoiceInput()
{
    auto& input = *world_.resources.input_;
    for (size_t i = 0; i < survivor::kTraitCount; ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            ChooseTrait(i);
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    choiceHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = choiceHud_.HitTest(mouse);
        if (hit >= 0) {
            ChooseTrait(static_cast<size_t>(hit));
        }
    }
}

void SurvivorRunController::ChooseTrait(size_t index)
{
    if (!traitChoiceOpen_ || index >= survivor::kTraitCount) {
        return;
    }
    trait_ = static_cast<survivor::TraitId>(index);
    traitChoiceOpen_ = false;
    ApplyParts();
    choiceHud_.SetPending(pendingChoices_, false);
    world_.combatFlow->SetEventCallout(std::string("特性：") + survivor::TraitName(trait_), 1.0f);
}

void SurvivorRunController::OpenRelicChoice()
{
    relicOffers_.clear();
    for (size_t i = 0; i < survivor::kRelicCount; ++i) {
        if (!relics_[i]) {
            relicOffers_.push_back(static_cast<survivor::RelicId>(i));
        }
    }
    if (relicOffers_.empty()) {
        pendingRelics_ = 0;
        return;
    }
    if (relicOffers_.size() < 2) {
        relicOffers_.push_back(survivor::RelicId::Count);
    }
    relicChoiceOpen_ = true;
    std::vector<std::pair<std::string, std::string>> cards;
    for (size_t i = 0; i < relicOffers_.size(); ++i) {
        const survivor::RelicId id = relicOffers_[i];
        cards.emplace_back(std::to_string(i + 1) + "  " + (id == survivor::RelicId::Count ? std::string("質の高い選択") : survivor::RelicName(id)),
                           id == survivor::RelicId::Count ? std::string("軽い部品の出ない3択を1つ貯める") : DescribeRelic(id));
    }
    choiceHud_.SetCards("宝箱：レリックを選ぶ（重さ0・積載を使わない）", "1・2 キーかクリックで選ぶ　/　選ぶまで止まる", cards);
    world_.combatFlow->SetEventCallout("宝箱：レリック", 0.9f);
}

void SurvivorRunController::UpdateRelicChoiceInput()
{
    auto& input = *world_.resources.input_;
    for (size_t i = 0; i < relicOffers_.size(); ++i) {
        if (input.IsKeyTriggered(static_cast<uint8_t>(DIK_1 + i))) {
            ChooseRelic(i);
            return;
        }
    }
    const myEngine::Vector2 mouse = input.GetMousePosition();
    choiceHud_.Update(mouse);
    if (input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0])) {
        const int hit = choiceHud_.HitTest(mouse);
        if (hit >= 0) {
            ChooseRelic(static_cast<size_t>(hit));
        }
    }
}

void SurvivorRunController::ChooseRelic(size_t index)
{
    if (!relicChoiceOpen_ || index >= relicOffers_.size()) {
        return;
    }
    const survivor::RelicId id = relicOffers_[index];
    relicChoiceOpen_ = false;
    pendingRelics_ = (std::max)(0, pendingRelics_ - 1);
    relicRecord_ << (relicRecord_.tellp() > 0 ? "," : "") << "{\"t\":" << std::lround(elapsed_) << ",\"picked\":\""
                 << (id == survivor::RelicId::Count ? "premium" : survivor::RelicKey(id)) << "\"}";
    if (id == survivor::RelicId::Count) {
        // 残りが1つのときの、もう1枚：質の高い3択を1つ貯める。
        ++pendingChoices_;
        ++pendingPremium_;
        ++premiumTotal_;
        pendingSince_.push_back(elapsed_);
        world_.combatFlow->SetEventCallout("宝箱：質の高い選択 +1", 0.9f);
    } else {
        relics_[static_cast<size_t>(id)] = true;
        ApplyParts();
        world_.combatFlow->SetEventCallout(std::string("レリック：") + survivor::RelicName(id), 1.0f);
    }
    relicText_->SetText("レリック：" + RelicNames());
    choiceHud_.SetPending(pendingChoices_, false);
}

std::string SurvivorRunController::RelicNames() const
{
    std::string names;
    for (size_t i = 0; i < survivor::kRelicCount; ++i) {
        if (relics_[i]) {
            names += (names.empty() ? "" : "・") + std::string(survivor::RelicName(static_cast<survivor::RelicId>(i)));
        }
    }
    return names.empty() ? std::string("なし") : names;
}

void SurvivorRunController::OpenFormChoice()
{
    if (formChoiceOpen_ || formStage_ >= 2 || traitChoiceOpen_ || relicChoiceOpen_) {
        return;
    }
    // 候補：第1段は連装と長砲身、第2段は今の形態の子（鍵付きは、鍵の部品が最大 Lv のときだけ）。
    formOffers_ = survivor::FormCandidates(config_.forms, formId_, config_.parts, survivor::EffectiveLevels(config_.parts, partLevels_));
    if (formOffers_.empty()) {
        formStage_ = 2;
        return;
    }
    if (choiceOpen_) {
        CloseChoices();
    }
    formChoiceOpen_ = true;
    std::vector<std::pair<std::string, std::string>> cards;
    for (size_t i = 0; i < formOffers_.size(); ++i) {
        const auto& form = config_.forms[static_cast<size_t>(formOffers_[i])];
        std::string title = std::to_string(i + 1) + "  " + form.name;
        if (form.keyPart != survivor::PartId::Count) {
            title += std::string("  ［鍵：") + config_.parts[static_cast<size_t>(form.keyPart)].name + "］";
        }
        cards.emplace_back(title, survivor::DescribeForm(form, formOffers_[i]));
    }
    const bool evolve = formStage_ >= 1;
    const std::string keys = formOffers_.size() >= 3 ? "1〜3" : "1・2";
    choiceHud_.SetCards(evolve ? "進化を選ぶ（" + std::string(config_.forms[static_cast<size_t>(formId_)].name) + "・Lv" +
                                     std::to_string(config_.stage2Level) + "）"
                               : "形態を選ぶ（Lv" + std::to_string(config_.formLevel) + "）",
                        keys + " キーかクリックで選ぶ　/　選ぶまで止まる", cards);
    world_.combatFlow->SetEventCallout(evolve ? "進化" : "形態の進化", 0.8f);
}

void SurvivorRunController::UpdateFormChoiceInput()
{
    auto& input = *world_.resources.input_;
    for (size_t i = 0; i < formOffers_.size(); ++i) {
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
    if (!formChoiceOpen_ || index >= formOffers_.size()) {
        return;
    }
    EvolveTo(formOffers_[index]);
}

void SurvivorRunController::EvolveTo(survivor::FormId id)
{
    if (id == survivor::FormId::Count) {
        return;
    }
    auto& player = *world_.resources.player_;
    const auto& form = config_.forms[static_cast<size_t>(id)];
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
        tuning.railWidthScale = form.railWidth;
        tuning.railExtraPierce = form.railPierce;
        tuning.bulletColor = form.color;
        player.ApplySurvivorForm(survivor::BuildFormConfig(*source, form, id), tuning);
    }
    formKey_ = form.key;
    formId_ = id;
    // 形態の固有の能力（機関砲の加速・狙撃の印・反射砲の跳ね返り）を、部品の効果と一緒に渡し直す。
    ApplyParts();
    formStage_ = form.stage;
    formPath_ += std::string(" → ") + form.name;
    formTimes_.push_back(elapsed_);
    formOffers_.clear();
    formChoiceOpen_ = false;
    RefreshEvolveText();
    // 進化の演出（止めて寄る・光の輪・題字）。四隅の自動確認は演出を進めないので出さない。
    if (autoTest_ != AutoTest::Tour) {
        cineReturnMode_ = cineMode_;
        cineMode_ = CineMode::Evolve;
        cineTime_ = 0.0f;
        cineEvolveBurst_ = false;
    }
    choiceHud_.SetPending(pendingChoices_, false);
    world_.combatFlow->SetEventCallout(std::string(form.stage >= 2 ? "進化：" : "形態：") + form.name, 1.0f);
}

void SurvivorRunController::ApplyParts()
{
    using survivor::PartId;
    auto& player = *world_.resources.player_;
    TankRunModifiers modifiers{};
    modifiers.enabled = true;
    modifiers.survivor = true;
    // 効果は、合体の部品の元の2つを最大 Lv として作る（積載と表示は partLevels_ のまま）。
    const survivor::PartLevels effective = survivor::EffectiveLevels(config_.parts, partLevels_);
    modifiers.partScales = survivor::MakePartScales(config_.parts, effective);
    survivor::ApplyPartCards(config_.parts, effective, modifiers);
    // 形態の固有の能力：狙撃の印の爆破、反射砲の跳ね返り。
    if (formId_ != survivor::FormId::Count) {
        const auto& form = config_.forms[static_cast<size_t>(formId_)];
        modifiers.markDetonation = form.markDetonation;
        modifiers.survivorWallBounces = form.wallBounces;
        world_.resources.bulletManager_->SetMarkThreshold(form.markDetonation ? form.markHits : 4, form.markDetonation ? form.markHits + 1 : 6,
                                                          form.markDetonation);
    }
    // レリックの一発屋：威力を上げ、1秒の発射数を落とす（間隔は割る）。
    if (relics_[static_cast<size_t>(survivor::RelicId::OneShot)]) {
        modifiers.partScales.bulletDamage *= config_.relics.oneShotDamage;
        modifiers.partScales.reloadInterval /= config_.relics.oneShotFireRate;
    }
    player.SetRunModifiers(modifiers);
    // 連鎖崩壊：撃破バーストの小さな弾が敵を倒すと、1回だけもう一度破裂する。
    world_.resources.bulletManager_->SetKillBurstRepeats(survivor::LevelOf(partLevels_, PartId::ChainCollapse) > 0 ? 1 : 0);
    // 雷鳴（合体）と雷砲（形態）：放電の届く数と距離を伸ばす。両方あれば足し合わせる（届く数は SetChainBoost が5まで）。
    const bool thunder = survivor::LevelOf(partLevels_, PartId::Thunderclap) > 0;
    int chainExtra = thunder ? config_.fusions.thunderExtraTargets : 0;
    float chainRadius = thunder ? config_.fusions.thunderRadiusScale : 1.0f;
    if (formId_ != survivor::FormId::Count) {
        chainExtra += config_.forms[static_cast<size_t>(formId_)].chainExtraTargets;
        chainRadius *= config_.forms[static_cast<size_t>(formId_)].chainRadiusScale;
    }
    world_.resources.bulletManager_->SetChainBoost(chainExtra, chainRadius);
    RefreshEvolveText();

    // 熱とシールドの部品。冷却器は冷える速さ（撃っている間も）、3段で上限+30。シールド容量は上限、3段で回復までの時間が半分。
    PlayerHeat::Config heat = config_.heat;
    const float cool = survivor::LightBonus(config_.parts, partLevels_, PartId::Cooler);
    heat.coolPerSecond *= cool;
    heat.coolPerSecondWhileFiring *= cool;
    if (survivor::HasChange(partLevels_, PartId::Cooler)) {
        heat.maxHeat += 30.0f;
    }
    // 特性の冷却炉：熱の上限を増やし、冷える速さを落とす（冷却器の効果の後に掛ける）。
    if (trait_ == survivor::TraitId::CoolingFurnace) {
        heat.maxHeat *= config_.traits.furnaceHeatMax;
        heat.coolPerSecond *= config_.traits.furnaceCool;
        heat.coolPerSecondWhileFiring *= config_.traits.furnaceCool;
    }
    player.SetHeatConfig(heat);
    Player::ShieldConfig shield{};
    shield.enabled = true;
    shield.maxHpRatio = config_.shieldMaxHpRatio * survivor::LightBonus(config_.parts, partLevels_, PartId::ShieldCapacity);
    shield.regenDelay = config_.shieldRegenDelay * (survivor::HasChange(partLevels_, PartId::ShieldCapacity) ? 0.5f : 1.0f);
    shield.regenPerSecond = config_.shieldRegenPerSecond;
    player.SetShieldConfig(shield);

    player.SetSurvivorPartEffects(MakePartEffects());
    moraleApplied_ = moraleKills_;
    if (survivor::OwnedQSkill(config_.parts, partLevels_) != survivor::PartId::Count) {
        QueueHint(Hint::QSkill);
    }
    loadoutHud_.SetLoadout(config_.parts, partLevels_, Capacity());
    maxLoad_ = (std::max)(maxLoad_, survivor::LoadOf(config_.parts, partLevels_));
}

Player::SurvivorPartEffects SurvivorRunController::MakePartEffects() const
{
    using survivor::PartId;
    // 軽い部品の3段の変化・重弾頭の熱・余熱・戦意・衝角シールド。
    const survivor::PartLevels levels = survivor::EffectiveLevels(config_.parts, partLevels_);
    const auto power = [&](PartId id) {
        return survivor::EffectPower(config_.parts, levels, id);
    };
    Player::SurvivorPartEffects effects{};
    effects.extraHeatPerShot = 2.0f * power(PartId::HeavyWarhead);
    effects.critChance = survivor::HasChange(partLevels_, PartId::Warhead) ? 0.15f : 0.0f;
    // 機関砲は、装填機の3段と同じ加速を最初から持つ。
    effects.loaderRamp = survivor::HasChange(partLevels_, PartId::Loader) ||
                         (formId_ != survivor::FormId::Count && config_.forms[static_cast<size_t>(formId_)].fireRamp);
    effects.spikes = survivor::HasChange(partLevels_, PartId::Ram);
    effects.invincibleScale = survivor::HasChange(partLevels_, PartId::Armor) ? 1.8f : 1.0f;
    effects.residualHeat = config_.partTuning.residualHeatBonus * power(PartId::ResidualHeat);
    effects.ramShield = config_.partTuning.ramShieldBonus * power(PartId::RamShield);
    effects.shotDamageScale = 1.0f + config_.partTuning.moralePerKill * power(PartId::Morale) * static_cast<float>(moraleKills_);
    // 合体の爆走：ダッシュの衝撃波を大きく強くし、シールドを回復する。
    if (survivor::LevelOf(partLevels_, PartId::Rampage) > 0) {
        effects.dashBlastRadiusScale = config_.fusions.rampageRadiusScale;
        effects.dashBlastDamageScale = config_.fusions.rampageDamageScale;
        effects.dashShieldRefill = config_.fusions.rampageShieldRefill;
    }
    // レリックの不動砲台：止まっている間の威力と、ダッシュの禁止。
    if (relics_[static_cast<size_t>(survivor::RelicId::FixedTurret)]) {
        effects.stillDamageBonus = config_.relics.turretDamage;
        effects.noDash = true;
    }
    return effects;
}

void SurvivorRunController::RefreshMorale(float dt)
{
    // 戦意の倍率は、倒すたびではなく0.5秒ごとにまとめて渡す（部品の倍率を作り直さず、効果だけを差し替える）。
    moraleRefresh_ -= dt;
    if (moraleRefresh_ > 0.0f || moraleKills_ == moraleApplied_) {
        return;
    }
    moraleRefresh_ = 0.5f;
    moraleApplied_ = moraleKills_;
    world_.resources.player_->SetSurvivorPartEffects(MakePartEffects());
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
