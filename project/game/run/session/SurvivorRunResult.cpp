#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>

// 育つ戦車の試作の結果画面と、遊んだ記録・fps の記録。

namespace gameplay {

using namespace survivorrun;

bool SurvivorRunController::IsResultVisible() const
{
    const auto& flow = world_.combat.combatFlow_;
    return flow.GetState() == GameFlowState::StageClear || (flow.GetState() == GameFlowState::GameOver && flow.GetTimer() <= 0.0f);
}

std::string SurvivorRunController::DescribeRelic(survivor::RelicId id) const
{
    // 倍率は設定から作る（一発屋：威力 x3.5・連射 -70%、不動砲台：止まると威力 +80%）。
    std::ostringstream text;
    if (id == survivor::RelicId::OneShot) {
        text << "主砲の威力 x" << std::fixed << std::setprecision(1) << config_.relics.oneShotDamage << "\n連射 -"
             << std::lround((1.0f - config_.relics.oneShotFireRate) * 100.0f) << "%\n一発ずつ、よく狙って撃つ";
    } else {
        text << "止まっている間は\n主砲の威力 +" << std::lround(config_.relics.turretDamage * 100.0f)
             << "%\nダッシュできない";
    }
    return text.str();
}

std::string SurvivorRunController::DescribeTrait(survivor::TraitId id) const
{
    if (id != survivor::TraitId::CoolingFurnace) {
        return "変化なし\nいつもどおりの熱と冷え方";
    }
    // 倍率は設定から作る（熱の上限 x2、冷える速さ -30% など）。
    const int heat = static_cast<int>(std::lround(config_.traits.furnaceHeatMax * 100.0f));
    const int cool = static_cast<int>(std::lround((1.0f - config_.traits.furnaceCool) * 100.0f));
    std::ostringstream text;
    text << "熱の上限 x" << heat / 100 << (heat % 100 ? "." + std::to_string(heat % 100 / 10) : std::string()) << "\n冷える速さ "
         << (cool >= 0 ? "-" : "+") << std::abs(cool) << "%\n長く撃ち続けられるが、\n一度熱くなると冷めにくい";
    return text.str();
}

std::string SurvivorRunController::DescribeParts(size_t perLine) const
{
    std::string text;
    size_t count = 0;
    for (size_t i = 0; i < survivor::kPartCount; ++i) {
        if (partLevels_[i] <= 0) {
            continue;
        }
        if (count > 0) {
            text += count % perLine == 0 ? "\n" : "　";
        }
        text += std::string(config_.parts[i].name) + " Lv" + std::to_string(partLevels_[i]);
        ++count;
    }
    return count > 0 ? text : std::string("なし");
}

void SurvivorRunController::RefreshResult(int selection)
{
    if (!resultText_ || !resultMenu_) {
        return;
    }
    const auto& player = *world_.resources.player_;
    std::ostringstream text;
    // 機体の遊び方（計画書 p4 の K5）：機体・進んだ波・死因・組み立て・スコアの内訳。
    if (HeroMode()) {
        const HeroScore score = ComputeHeroScore();
        auto fixed = [](float value) {
            std::ostringstream out;
            out << value;
            return out.str();
        };
        text << "機体　　　　　" << Hero().name << "\n"
             << "進んだ所　　　" << (IsBossStage() ? std::string("ボスの波") : "波 " + std::to_string(stage_ + 1) + "/" + std::to_string(config_.waves.count))
             << (score.boss ? "　ボス撃破" : "") << "　　時間 " << Clock(elapsed_) << "\n"
             << (cleared_ ? std::string() : "死因　　　　　" + (player.GetSurvivorLastHit().empty() ? std::string("不明") : player.GetSurvivorLastHit()) + "\n")
             << "撃破　　　　　" << world_.combat.defeatedEnemies_ << "　　受けたダメージ " << player.GetSurvivorDamageTaken() << "　　修理 " << repairs_ << " 回\n"
             << "強化の段　　　主武器 " << UpgradeTier(0) << "・右クリック " << UpgradeTier(1) << "・Shift " << UpgradeTier(2) << "・E " << UpgradeTier(3)
             << "・Q " << UpgradeTier(4) << "\n"
             << "パーク　　　　" << [&] {
                    std::string names;
                    int owned = 0;
                    for (size_t i = 0; i < survivor::kPerkCount; ++i) {
                        if (perks_[i]) {
                            names += (owned == 0 ? "" : owned % 4 == 0 ? "\n　　　　　　　" : "・") +
                                     std::string(survivor::GetPerkInfo(static_cast<survivor::PerkId>(i)).name);
                            ++owned;
                        }
                    }
                    return owned > 0 ? names : std::string("なし");
                }() << "\n"
             << "スコア　　　　進んだ波 " << score.waves << " × " << fixed(config_.waves.scorePerWave) << "　＋　稼いだポイント " << score.points
             << " × " << fixed(config_.waves.scorePerPoint) << "　＋　ボス " << (score.boss ? fixed(config_.waves.scoreBoss) : std::string("0")) << "\n"
             << "　　　　　　　＝ " << score.total << "\n"
             << "解放ポイント　+" << heroAwardGained_ << (heroAwardBonus_ > 0 ? "（初クリア +" + std::to_string(heroAwardBonus_) + "）" : std::string())
             << "　　合計 " << heroSave_.points << "\n"
             << "次の機体　　　" << NextHeroText();
        resultText_->SetText(text.str());
        std::ostringstream menu;
        menu << (selection == 0 ? "> " : "   ") << "もう1回（R）\n" << (selection == 1 ? "> " : "   ") << "タイトルへ";
        resultMenu_->SetText(menu.str());
        return;
    }
    // 見出し（区画クリア・撃破された）は、上の帯の文字（CombatFlow）が出す。
    text << (cleared_ ?"区画の時間　　" : "生き延びた時間　") << Clock(elapsed_) << "\n"
         << (WeaponMode() ? std::string() : "レベル　　　　" + std::to_string(level_) + "\n")
         << (HeroMode()     ? (IsBossStage() ? std::string("ボスの波") : "波　　　　　　" + std::to_string(stage_ + 1) + "/" +
                                                                         std::to_string(config_.waves.count)) +
                                  (IsBossStage() && rivalDefeated_ ? "　ボス撃破" : "") + "\n"
             : WeaponMode() ? "ステージ　　　" + std::to_string(stage_ + 1) + "/" + std::to_string(StageCount()) + "（段" +
                                  std::to_string(config_.stages.tier) + "）" + (IsBossStage() && rivalDefeated_ ? "　ボス撃破" : "") + "\n"
                            : std::string())
         << "撃破　　　　　" << world_.combat.defeatedEnemies_ << "\n"
         << (WeaponMode() ? "武器　　　　　" + WeaponNames() + "\n"
                          : "形態　　　　　" + formPath_ + "　　特性 " + survivor::TraitName(trait_) + "\n")
         // 出撃中に拾った物は、倒れると全部失う（G4。拠点へ持ち帰るのは W5・W8）。
         << (WeaponMode() ? "拾った物　　　" + DescribeBag() + (cleared_ ? std::string("（拠点へ持ち帰った）") : std::string("（倒れたので失う）")) + "\n"
                          : std::string())
         << "レリック　　　" << RelicNames() << "\n"
         << "脅威　　　　　Lv" << threatStage_ << "　　宿敵 "
         << (rivalDefeated_ ? "撃破（" + Clock(rivalDefeatedAt_) + "）" : rivalSpawned_ ? std::string("未撃破") : std::string("出る前")) << "\n"
         << "受けたダメージ　" << world_.combat.damageTaken_ << "　　オーバーヒート " << player.GetHeat().GetOverheatCount() << " 回\n"
         << "部品\n"
         << DescribeParts(4);
    resultText_->SetText(text.str());
    std::ostringstream menu;
    menu << (selection == 0 ? "> " : "   ") << "もう1回（R）\n"
         << (selection == 1 ? "> " : "   ") << "タイトルへ";
    resultMenu_->SetText(menu.str());
}

void SurvivorRunController::SampleFps()
{
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - fpsClock_).count();
    if (seconds < 5.0) {
        return;
    }
    fpsSamples_.push_back(static_cast<float>(static_cast<double>(drawnFrames_ - fpsFrames_) / seconds));
    fpsClock_ = now;
    fpsFrames_ = drawnFrames_;
}

void SurvivorRunController::WriteRunLog()
{
    if (!myEngine::kDeveloperTools || world_.demo.titleDemo_) {
        return;
    }
    // 自分で遊んだ回を後で見比べるための記録。自動確認では同じ名前に上書きする。
    const std::string directory = std::string(kAutoTestDirectory) + "/runs";
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    // MYENGINE_SURVIVOR_RUN_LOG=名前：自動確認でも別の名前で残す（機体ごとに何回か通して比べる。K8）。
    std::string name = "autotest";
    wchar_t logName[64]{};
    if (const DWORD length = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_RUN_LOG", logName, 64); autoTest_ != AutoTest::None && length > 0 && length < 64) {
        name.clear();
        for (DWORD i = 0; i < length; ++i) {
            name.push_back(static_cast<char>(logName[i]));
        }
    }
    if (autoTest_ == AutoTest::None) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::ostringstream stamp;
        stamp << std::setfill('0') << time.wYear << std::setw(2) << time.wMonth << std::setw(2) << time.wDay << "_" << std::setw(2)
              << time.wHour << std::setw(2) << time.wMinute << std::setw(2) << time.wSecond;
        name = stamp.str();
    }
    const auto& player = *world_.resources.player_;
    float fpsMin = 0.0f, fpsSum = 0.0f;
    for (size_t i = 0; i < fpsSamples_.size(); ++i) {
        fpsMin = i == 0 ? fpsSamples_[i] : (std::min)(fpsMin, fpsSamples_[i]);
        fpsSum += fpsSamples_[i];
    }
    std::ofstream output(directory + "/run_" + name + ".json");
    output << "{\"cleared\":" << (cleared_ ? "true" : "false") << ",\"seconds\":" << elapsed_ << ",\"level\":" << level_
           << ",\"kills\":" << world_.combat.defeatedEnemies_ << ",\"damageTaken\":" << world_.combat.damageTaken_
           << ",\"form\":\"" << formKey_ << "\",\"formPath\":\"" << formPath_ << "\",\"formTimes\":[" << [&] {
                  std::ostringstream times;
                  for (size_t i = 0; i < formTimes_.size(); ++i) {
                      times << (i ? "," : "") << std::lround(formTimes_[i]);
                  }
                  return times.str();
              }() << "],\"trait\":\"" << survivor::TraitKey(trait_) << "\",\"relics\":[" << relicRecord_.str()
           << "],\"threatStage\":" << threatStage_ << ",\"rings\":" << ringsSpawned_
           << ",\"rivalSpawnedAt\":" << rivalSpawnedAt_ << ",\"rivalDefeatedAt\":" << rivalDefeatedAt_ << ",\"capacity\":" << Capacity()
           << ",\"load\":" << survivor::LoadOf(config_.parts, partLevels_) << ",\"removals\":[" << removalRecord_.str() << "],\"qUses\":" << qUses_
           << ",\"q\":[" << qRecord_.str() << "],\"purges\":" << purges_ << ",\"moraleKills\":" << moraleKills_ << ",\"maxLoad\":" << maxLoad_
           << ",\"fusions\":" << fusions_ << ",\"fusionAt\":" << fusionTime_ << ",\"criticalBlasts\":" << criticalBlasts_ << ",\"weaponMode\":" << (WeaponMode() ? "true" : "false")
           << ",\"weapons\":\"" << WeaponNames() << "\",\"weaponSwaps\":" << weaponSwaps_ << ",\"reloads\":" << reloads_ << ",\"stage\":" << stage_ + 1 << ",\"money\":" << bag_.money << ",\"materials\":["
           << bag_.materials[0] << "," << bag_.materials[1] << "," << bag_.materials[2] << "," << bag_.materials[3] << "],\"chips\":" << bag_.chips.size()
           << ",\"bagWeapons\":" << bag_.weapons.size() << ",\"relicsOwned\":\"" << RelicNames() << "\",\"levelTimes\":[";
    for (size_t i = 0; i < levelTimes_.size(); ++i) {
        output << (i ? "," : "") << std::lround(levelTimes_[i]);
    }
    output << "],\"partLevels\":{";
    for (size_t i = 0; i < survivor::kPartCount; ++i) {
        output << (i ? "," : "") << "\"" << config_.parts[i].key << "\":" << partLevels_[i];
    }
    output << "},\"choices\":[" << choiceRecord_.str() << "],\"pendingAtEnd\":" << pendingChoices_
           << ",\"heat\":{\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"secondsAbove70\":" << heatHighSeconds_
           << ",\"railShots\":" << player.GetSpecialCombatStats().railShots << "},\"fps\":{\"min\":" << fpsMin << ",\"avg\":"
           << (fpsSamples_.empty() ? 0.0f : fpsSum / static_cast<float>(fpsSamples_.size())) << ",\"samples\":"
           << fpsSamples_.size() << "}";
    // 機体の遊び方の記録（計画書 p4 の K8）：進んだ波・生き延びた秒・毎秒のダメージ・受けたダメージ・回復した量・死因・買った物・Q の回数。
    if (HeroMode()) {
        const HeroScore score = ComputeHeroScore();
        output << ",\"hero\":{\"key\":\"" << Hero().key << "\",\"wave\":" << stage_ + 1 << ",\"wavesCleared\":" << score.waves
               << ",\"waveCount\":" << config_.waves.count << ",\"boss\":" << (score.boss ? "true" : "false")
               << ",\"dead\":" << (player.IsDead() ? "true" : "false") << ",\"runSeconds\":" << runSeconds_
               << ",\"damageDealt\":" << damageDealt_ << ",\"dps\":" << (runSeconds_ > 0.0f ? static_cast<float>(damageDealt_) / runSeconds_ : 0.0f)
               << ",\"damageTaken\":" << player.GetSurvivorDamageTaken() << ",\"healed\":" << player.GetSurvivorHealed()
               << ",\"deathCause\":\"" << (player.IsDead() ? player.GetSurvivorLastHit() : std::string()) << "\",\"ultimates\":" << abilityUses_[3]
               << ",\"abilityUses\":[" << abilityUses_[0] << "," << abilityUses_[1] << "," << abilityUses_[2] << "," << abilityUses_[3] << "]"
               << ",\"pointsEarned\":" << pointsEarned_ << ",\"score\":" << score.total << ",\"repairs\":" << repairs_
               << ",\"damageBy\":{\"drain\":" << drainDamage_ << ",\"orb\":" << orbDamage_ << ",\"lance\":" << lanceDamage_
               << ",\"shockwave\":" << shockwaveDamage_ << ",\"charge\":" << chargeDamage_ << ",\"fortress\":" << fortressDamage_
               << ",\"rocketKills\":" << rocketKills_ << ",\"barrageShots\":" << barrageShots_ << ",\"primaryShots\":" << player.GetPrimaryAttackCount() << "}"
               << ",\"bossAt\":" << (score.boss ? rivalDefeatedAt_ : -1.0f) << ",\"kills\":" << world_.combat.defeatedEnemies_
               << ",\"repairSpent\":" << repairSpent_ << ",\"shopSpent\":" << shopSpent_ << ",\"tiers\":[" << upgradeTiers_[0] << "," << upgradeTiers_[1]
               << "," << upgradeTiers_[2] << "," << upgradeTiers_[3] << "," << upgradeTiers_[4] << "],\"bought\":[";
        for (size_t i = 0; i < shopLog_.size(); ++i) {
            output << (i ? "," : "") << "\"" << shopLog_[i] << "\"";
        }
        output << "],\"waves\":[";
        for (size_t i = 0; i < waveRecords_.size(); ++i) {
            const WaveRecord& r = waveRecords_[i];
            output << (i ? "," : "") << "{\"wave\":" << r.wave << ",\"points\":" << r.points << ",\"damage\":" << r.damage << ",\"wound\":" << r.wound
                   << ",\"hp\":" << r.hp << ",\"maxHp\":" << r.maxHp << "}";
        }
        output << "]}";
    }
    output << "}\n";
}

} // namespace gameplay
