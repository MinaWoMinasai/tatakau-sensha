#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>

// 育つ戦車の試作の機体の遊び方の、出撃の外の成長（計画書 p4 の K6）：ローグライトなので強化は残さず、
// スコアから入る解放ポイントで機体を解放していく。拠点では使える機体から1つ選んで出撃する。

namespace gameplay {

using namespace survivorrun;

std::string SurvivorRunController::HeroSavePath() const
{
    if (autoTest_ != AutoTest::None) {
        return std::string(kAutoTestDirectory) + "/autotest_heroes.json";
    }
    return "save/survivor_heroes.json";
}

void SurvivorRunController::LoadHeroSaveFile()
{
    heroSaveError_.clear();
    heroSave_ = {};
    // 自動確認は毎回新しいセーブから始める（MYENGINE_SURVIVOR_KEEP_SAVE=1 なら前の回を読む）。タイトルの見本はセーブを読まない。
    wchar_t keep[4]{};
    const bool keepSave = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_KEEP_SAVE", keep, 4) > 0 && keep[0] == L'1';
    const bool fresh = world_.demo.titleDemo_ || (autoTest_ != AutoTest::None && !keepSave);
    if (fresh || survivor::LoadHeroSave(HeroSavePath(), heroSave_, heroSaveError_)) {
        return;
    }
    if (!heroSaveError_.empty()) {
        // 壊れたセーブは消さずに名前を変えて残し、新しいセーブで始める。
        std::error_code ignored;
        std::filesystem::rename(HeroSavePath(), HeroSavePath() + ".broken", ignored);
        heroSave_ = {};
    }
}

void SurvivorRunController::SaveHeroSaveFile()
{
    if (world_.demo.titleDemo_) {
        return;
    }
    std::string error;
    if (!survivor::SaveHeroSave(HeroSavePath(), heroSave_, error)) {
        heroSaveError_ = error;
        world_.combatFlow->SetEventCallout(error, 1.6f);
    }
}

std::vector<int> SurvivorRunController::HeroCardOrder() const
{
    std::vector<int> order;
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < config_.heroes.heroes.size(); ++i) {
            if (survivor::IsHeroUnlocked(heroSave_, config_.heroes.heroes[i]) == (pass == 0)) {
                order.push_back(static_cast<int>(i));
            }
        }
    }
    // 札は3枚まで。
    if (order.size() > SurvivorItemScreen::kWeaponCards) {
        order.resize(SurvivorItemScreen::kWeaponCards);
    }
    return order;
}

void SurvivorRunController::SelectHeroCard(int card)
{
    const std::vector<int> order = HeroCardOrder();
    if (card < 0 || static_cast<size_t>(card) >= order.size()) {
        return;
    }
    heroCard_ = card;
    const survivor::HeroDefinition& hero = config_.heroes.heroes[static_cast<size_t>(order[static_cast<size_t>(card)])];
    if (!survivor::IsHeroUnlocked(heroSave_, hero) || hero_ == order[static_cast<size_t>(card)]) {
        return;
    }
    // 使える機体なら、その機体で出撃の準備をし直す（主武器・技・最大HP・案内）。
    config_.heroes.start = hero.key;
    StartHero();
    if (HeroMode()) {
        hintTexts_[static_cast<size_t>(Hint::Move)]->SetText(HeroControlsText());
    }
}

bool SurvivorRunController::UnlockHeroCard()
{
    const std::vector<int> order = HeroCardOrder();
    if (heroCard_ < 0 || static_cast<size_t>(heroCard_) >= order.size()) {
        return false;
    }
    const int index = order[static_cast<size_t>(heroCard_)];
    const survivor::HeroDefinition& hero = config_.heroes.heroes[static_cast<size_t>(index)];
    const std::vector<int> candidates = survivor::HeroUnlockCandidates(heroSave_, config_.heroes.heroes);
    if (survivor::IsHeroUnlocked(heroSave_, hero) || std::find(candidates.begin(), candidates.end(), index) == candidates.end() ||
        heroSave_.points < hero.unlockCost) {
        return false;
    }
    heroSave_.points -= hero.unlockCost;
    heroSave_.unlocked.push_back(hero.key);
    SaveHeroSaveFile();
    world_.combatFlow->SetEventCallout("解放した：" + hero.name, 1.4f);
    // 解放した機体を選ぶ（札の並びが変わるので、選び直す）。
    const std::vector<int> after = HeroCardOrder();
    const auto position = std::find(after.begin(), after.end(), index);
    SelectHeroCard(position != after.end() ? static_cast<int>(position - after.begin()) : 0);
    return true;
}

void SurvivorRunController::AwardHeroPoints()
{
    if (!HeroMode() || heroAwarded_ || world_.demo.titleDemo_) {
        return;
    }
    heroAwarded_ = true;
    // 倒れてもクリアしても、そこまでのスコアが解放ポイントになる。その機体で初めてボスを倒したら、ボーナスを足す。
    const HeroScore score = ComputeHeroScore();
    heroAwardGained_ = score.total;
    heroAwardBonus_ = 0;
    if (score.boss) {
        ++heroSave_.clears;
        if (std::find(heroSave_.cleared.begin(), heroSave_.cleared.end(), Hero().key) == heroSave_.cleared.end()) {
            heroSave_.cleared.push_back(Hero().key);
            heroAwardBonus_ = static_cast<int>(config_.waves.scoreFirstClear);
        }
    }
    heroSave_.points += heroAwardGained_ + heroAwardBonus_;
    SaveHeroSaveFile();
}

std::string SurvivorRunController::NextHeroText() const
{
    const std::vector<int> candidates = survivor::HeroUnlockCandidates(heroSave_, config_.heroes.heroes);
    if (candidates.empty()) {
        return "全部の機体を解放した";
    }
    // 候補のうち、一番安い機体までのあと。
    int best = candidates.front();
    for (int index : candidates) {
        if (config_.heroes.heroes[static_cast<size_t>(index)].unlockCost < config_.heroes.heroes[static_cast<size_t>(best)].unlockCost) {
            best = index;
        }
    }
    const survivor::HeroDefinition& hero = config_.heroes.heroes[static_cast<size_t>(best)];
    const int left = hero.unlockCost - heroSave_.points;
    return left <= 0 ? hero.name + " を拠点で解放できる" : hero.name + " まで あと " + std::to_string(left);
}

} // namespace gameplay
