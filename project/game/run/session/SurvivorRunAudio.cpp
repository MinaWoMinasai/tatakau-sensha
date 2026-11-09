#include "game/run/session/SurvivorRunController.h"
#include "game/session/GameplaySystems.h"

// 育つ戦車の試作の BGM と効果音のつなぎ。戦闘の処理には手を入れず、毎フレームの回数の増え方（撃った数・倒した数など）から鳴らす。

namespace gameplay {

SurvivorRunController::AudioCounters SurvivorRunController::ReadAudioCounters() const
{
    const auto& player = *world_.resources.player_;
    AudioCounters counters;
    counters.shots = player.GetPrimaryAttackCount() + static_cast<uint32_t>(player.GetSpecialCombatStats().railShots);
    counters.hits = damageNumbers_.GetSpawnedCount();
    counters.kills = world_.combat.defeatedEnemies_;
    counters.blasts = world_.resources.bulletManager_->GetShooterStats().impactBlasts;
    counters.hp = player.GetHp();
    counters.reloads = reloads_;
    counters.swapping = swapTarget_ >= 0;
    counters.barrierOn = barrierOn_;
    counters.blocked = barrierBlocked_;
    counters.breaks = barrierBreaks_;
    counters.ammoPicked = lootPicked_[static_cast<size_t>(survivor::LootKind::Ammo)] + lootPicked_[static_cast<size_t>(survivor::LootKind::Material)];
    counters.rarePicked = lootPicked_[static_cast<size_t>(survivor::LootKind::Chip)] + lootPicked_[static_cast<size_t>(survivor::LootKind::Weapon)];
    counters.mines = eliteStats_.mineExplosions;
    counters.warps = eliteStats_.warps;
    counters.heals = eliteStats_.heals;
    counters.sales = restSales_;
    counters.crafted = crafted_;
    counters.evolved = evolved_;
    counters.sorties = base_.sorties;
    counters.cleared = cleared_;
    counters.bossSpawned = rivalSpawned_;
    counters.bossDefeated = rivalDefeated_;
    return counters;
}

void SurvivorRunController::UpdateAudio(float dt)
{
    if (!WeaponMode() || world_.demo.titleDemo_) {
        return;
    }
    const AudioCounters now = ReadAudioCounters();
    const AudioCounters& before = audioCounters_;
    if (audioCountersReady_) {
        if (now.shots > before.shots) {
            const survivor::WeaponDefinition* weapon = ActiveWeapon();
            if (!weapon || !audio_.Play("fire_" + weapon->key)) {
                audio_.Play("fire");
            }
        }
        if (now.hits > before.hits) {
            audio_.Play("hit");
        }
        if (now.kills > before.kills) {
            audio_.Play("kill");
        }
        if (now.blasts > before.blasts) {
            audio_.Play("explosion");
        }
        if (now.hp < before.hp) {
            audio_.Play("player_hurt");
        }
        if (now.reloads > before.reloads) {
            audio_.Play("reload");
        }
        if (now.swapping && !before.swapping) {
            audio_.Play("swap");
        }
        if (now.barrierOn && !before.barrierOn) {
            audio_.Play("barrier_on");
        }
        if (now.blocked > before.blocked) {
            audio_.Play("barrier_block");
        }
        if (now.breaks > before.breaks) {
            audio_.Play("stamina_out");
        }
        if (now.rarePicked > before.rarePicked) {
            audio_.Play("pickup_rare");
        } else if (now.ammoPicked > before.ammoPicked) {
            audio_.Play("pickup");
        }
        if (now.mines > before.mines) {
            audio_.Play("mine_explode");
        }
        if (now.warps > before.warps) {
            audio_.Play("warp");
        }
        if (now.heals > before.heals) {
            audio_.Play("heal");
        }
        if (now.sales > before.sales) {
            audio_.Play("sell");
        }
        if (now.evolved > before.evolved) {
            audio_.Play("evolve");
        } else if (now.crafted > before.crafted) {
            audio_.Play("craft");
        }
        if (now.sorties > before.sorties) {
            audio_.Play("sortie");
        }
        if (now.cleared && !before.cleared) {
            audio_.Play("stage_clear");
        }
        if (now.bossSpawned && !before.bossSpawned) {
            audio_.Play("boss_appear");
        }
        if (now.bossDefeated && !before.bossDefeated) {
            audio_.Play("boss_defeat");
        }
    }
    audioCounters_ = now;
    audioCountersReady_ = true;

    // 曲：拠点・休憩部屋・ステージ1・ステージ2・ボスステージ。持ち物の画面と調整パネルを開いている間は下げる。
    std::string track;
    if (baseScreenOpen_) {
        track = "base";
    } else if (restRoomOpen_ || IsResultVisible()) {
        track = "rest";
    } else {
        track = IsBossStage() ? "boss" : stage_ == 0 ? "stage1" : "stage2";
    }
    audio_.SetMusic(track);
    audio_.Update(dt, inventoryOpen_ || (tuningOpen_ && tuningPause_));
}

} // namespace gameplay
