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

std::string SurvivorRunController::FormName() const
{
    for (const auto& form : config_.forms) {
        if (formKey_ == form.key) {
            return form.name;
        }
    }
    return "基本砲";
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
    // 見出し（区画クリア・撃破された）は、上の帯の文字（CombatFlow）が出す。
    text << (cleared_ ?"区画の時間　　" : "生き延びた時間　") << Clock(elapsed_) << "\n"
         << "レベル　　　　" << level_ << "\n"
         << "撃破　　　　　" << world_.combat.defeatedEnemies_ << "\n"
         << "形態　　　　　" << FormName() << "　　特性 " << survivor::TraitName(trait_) << "\n"
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
    std::string name = "autotest";
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
           << ",\"form\":\"" << formKey_ << "\",\"trait\":\"" << survivor::TraitKey(trait_) << "\",\"threatStage\":" << threatStage_ << ",\"rings\":" << ringsSpawned_
           << ",\"rivalSpawnedAt\":" << rivalSpawnedAt_ << ",\"rivalDefeatedAt\":" << rivalDefeatedAt_ << ",\"capacity\":" << Capacity()
           << ",\"load\":" << survivor::LoadOf(config_.parts, partLevels_) << ",\"removals\":[" << removalRecord_.str() << "],\"qUses\":" << qUses_
           << ",\"q\":[" << qRecord_.str() << "],\"purges\":" << purges_ << ",\"moraleKills\":" << moraleKills_ << ",\"levelTimes\":[";
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
           << fpsSamples_.size() << "}}\n";
}

} // namespace gameplay
