#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <cmath>

// 育つ戦車の試作の機体の遊び方の出撃の流れ（計画書 p4 の K3）：seconds 秒の波を count 回、そのあとボスの波。
// 波が終わると、欠片を拾ったことにして敵と弾を片付け、波の間の画面（K4 でショップ）を開く。HP は波の間に戻らない（H8）。

namespace gameplay {

int SurvivorRunController::StageThreatOffset() const
{
    if (HeroMode()) {
        const auto& offsets = config_.waves.threatOffset;
        return offsets.empty() ? 0 : offsets[(std::min)(static_cast<size_t>((std::max)(0, stage_)), offsets.size() - 1)];
    }
    return config_.stages.threatOffset[static_cast<size_t>((std::clamp)(stage_, 0, 2))];
}

float SurvivorRunController::StageEnemyHp() const
{
    if (HeroMode()) {
        const auto& scales = config_.waves.enemyHp;
        return scales.empty() ? 1.0f : scales[(std::min)(static_cast<size_t>((std::max)(0, stage_)), scales.size() - 1)];
    }
    return config_.stages.enemyHp[static_cast<size_t>((std::clamp)(stage_, 0, 2))];
}

int SurvivorRunController::WavePoints(const ExpEnemy& enemy) const
{
    // 重い四角と大五角は、ふつうの形より大きく作られている（scale 1.7 ほど）。
    switch (enemy.GetType()) {
    case ExpEnemyType::Triangle:
        return config_.waves.triangle;
    case ExpEnemyType::Square:
        return enemy.GetSizeScale() > 1.4f ? config_.waves.heavySquare : config_.waves.square;
    case ExpEnemyType::Pentagon:
        return enemy.GetSizeScale() > 1.4f ? config_.waves.bigPentagon : config_.waves.pentagon;
    default:
        return config_.waves.tank;
    }
}

void SurvivorRunController::FinishWave()
{
    auto& player = *world_.resources.player_;
    // 落ちている欠片は、拾ったことにする（波の終わりに取りこぼさない）。
    AddExperience(pickups_.GetPendingValue());
    WaveRecord record;
    record.wave = stage_ + 1;
    record.points = wavePoints_;
    record.damage = player.GetSurvivorDamageTaken() - waveDamageStart_;
    record.capped = player.GetSurvivorDamageCapped() - waveCappedStart_;
    record.wound = player.GetSurvivorWound();
    record.hp = player.GetHp();
    record.maxHp = player.GetMaxHp();
    record.seconds = elapsed_;
    waveRecords_.push_back(record);
    // 更新の初めか演出の終わりなので、衝突処理の途中ではない。敵と弾を片付ける。
    cleared_ = true;
    world_.presentation.playerSwapDissolve_ = 0.0f;
    CloseChoices();
    pickups_.Clear();
    lootField_.Clear();
    damageNumbers_.Clear();
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    if (IsBossStage()) {
        world_.combatFlow->EnterResultState(true);
        return;
    }
    // ショップの並ぶ物は波ごとに引き直す（引き直しの値段も戻す）。
    shopRerolls_ = 0;
    RollShop();
    OpenRestRoom();
    world_.combatFlow->SetEventCallout("波 " + std::to_string(stage_ + 1) + " をしのいだ", 1.4f);
}

SurvivorRunController::HeroScore SurvivorRunController::ComputeHeroScore() const
{
    // 進んだ波は、終えた波の数（ボスの波を倒せば、それも1つ）。生き延びた時間だけでは数えない（計画書 p4 の2章）。
    HeroScore score;
    score.waves = static_cast<int>(waveRecords_.size());
    score.points = pointsEarned_;
    score.boss = IsBossStage() && rivalDefeated_;
    score.total = static_cast<int>(std::lround(static_cast<float>(score.waves) * config_.waves.scorePerWave +
                                               static_cast<float>(score.points) * config_.waves.scorePerPoint +
                                               (score.boss ? config_.waves.scoreBoss : 0.0f)));
    return score;
}

bool SurvivorRunController::RepairHero()
{
    auto& player = *world_.resources.player_;
    if (!HeroMode() || bag_.money < config_.health.repairCost || (player.GetSurvivorWound() <= 0 && player.GetHp() >= player.GetMaxHp())) {
        return false;
    }
    bag_.money -= config_.health.repairCost;
    player.ClearSurvivorWound();
    player.HealRunPlayer(config_.health.repairHeal);
    ++repairs_;
    repairSpent_ += config_.health.repairCost;
    world_.combatFlow->SetEventCallout("修理した", 0.8f);
    return true;
}

} // namespace gameplay
