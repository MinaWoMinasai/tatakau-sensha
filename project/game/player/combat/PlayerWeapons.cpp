#include "game/player/combat/PlayerWeapons.h"
#include "game/player/combat/PlayerSystems.h"
#include "game/player/ui/PlayerPresentation.h"
#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "game/player/PlayerMovement.h"
#include "PlayerUiHelpers.h"
#include "game/enemy/visual/NeonDepthPlacement.h"
#include "StartupTrace.h"
#include "Stage.h"
#include "game/exp/ExpEnemy.h"
#include "game/enemy/actor/Enemy.h"
#include "game/exp/EnemyManager.h"
#include "Audio.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonTextEffect.h"
#include "ObjectPostEffect.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void PlayerWeapons::Attack(BulletManager* bulletManager, float deltaTime)
{
    // 部屋移動後の入力解除待ちでは、射撃だけでなく発射間隔の時計も進めない。
    if (player_.runModifiers_.enabled && player_.runRoomAwaitInputRelease_)
        return;

    // 発射間隔は秒で管理する。砲身グループ別の待ち時間も同じ更新で進める。
    player_.bulletCoolTime = (std::max)(0.0f, player_.bulletCoolTime - deltaTime);
    for (float& cooldown : player_.weaponGroupCooldowns_) {
        cooldown = (std::max)(0.0f, cooldown - deltaTime);
    }

    bool wantsPrimaryAttack =
        player_.demoInputEnabled_ ? player_.demoShoot_ : player_.input_->IsPress(player_.input_->GetMouseState().rgbButtons[0]);
#if defined(USE_IMGUI) && !defined(NDEBUG)
    wantsPrimaryAttack = wantsPrimaryAttack || player_.debugAutoFireEnabled_;
#endif
    if (player_.runModifiers_.enabled && player_.runModifiers_.railCannon && player_.expeditionCombatStyleSelected_ &&
        player_.expeditionCombatStyle_ == tankbuild::Style::Shooter) {
        AttackRailCannon(bulletManager, wantsPrimaryAttack && !player_.ui_->upgradeHudMouseCaptured_, deltaTime);
        return;
    }
    if (wantsPrimaryAttack && !player_.ui_->upgradeHudMouseCaptured_) {
        // ドローン装備の射撃はPlayer::Update内の各ドローンが行うため、本体からは撃たない。
        if (player_.IsDroneBuild())
            return;
        if (player_.IsMeleeBuild()) {
            if (player_.spinCycle_.remaining > 0)
                return;
            if (player_.bulletCoolTime > 0.0f)
                return;
            if (TryStartSpinBlade(wantsPrimaryAttack))
                return;
            const int step = player_.meleeComboTimer_ > 0.0f ? player_.meleeComboStep_ : 0;
            const auto combo = MakeTankMeleeCombo(step, player_.runModifiers_);
            const auto* config = player_.GetCurrentClassConfig();
            const auto& profile = player_.GetCombatStyleProfile(tankbuild::Style::Melee);
            // リロード補正を斬撃の各段階へ共通に掛け、準備・有効・硬直の合計を最低0.05秒にする。
            const float timing = (std::max)(0.05f / (combo.windup + combo.duration + combo.recovery),
                                            player_.stats_.reloadSpeed / player_.GetRunBaseReloadFrames() *
                                                (config ? config->reloadScale : 1.0f) * GetRunFireIntervalScale() *
                                                (player_.expeditionCombatStyleSelected_ ? profile.attackIntervalSeconds / 0.33f : 1.0f));
            MeleeSlashEvent event{};
            event.origin = player_.GetWorldPosition();
            event.direction = cg2::Length(player_.dir_) > 0.001f ? cg2::Normalize(player_.dir_) : cg2::Vector3{1, 0, 0};
            event.range = (player_.expeditionCombatStyleSelected_ ? profile.meleeRange : 4.3f) * combo.range;
            event.arcDeg = combo.arc;
            event.width = step == 2 ? 0.38f : 0.26f;
            event.windupDuration = combo.windup * timing;
            event.duration = combo.duration * timing;
            event.recoveryDuration = combo.recovery * timing;
            event.comboStep = step;
            event.knockback = combo.knockback * (player_.expeditionCombatStyleSelected_ ? profile.meleeKnockback / 0.16f : 1.0f);
            event.damage = static_cast<uint32_t>((std::max)(1.0f, std::round(player_.stats_.bulletDamage * 3.8f * combo.damage *
                                                                             (config ? config->bulletDamageScale : 1.0f))));
            event.color = step == 2 ? cg2::Vector4{1.8f, 0.85f, 0.25f, 1.0f} : cg2::Vector4{0.25f, 1.50f, 1.75f, 1.0f};
            const bool dashSlash =
                tankspecial::CanDashSlash(player_.runModifiers_.dashSlash, player_.isDashing_, player_.recentDashTimer_) &&
                !player_.dashSlashActive_;
            if (dashSlash) {
                // 移動軌跡への命中はUpdateAdditionalAbilitiesで処理する。通常斬撃の予約とは分ける。
                event.comboStep = 0;
                event.windupDuration = 0;
                event.duration = .18f;
                event.recoveryDuration = .12f;
                event.direction = cg2::Length(player_.velocity_) > .001f ? cg2::Normalize(player_.velocity_) : event.direction;
                player_.dashSlashActive_ = true;
                player_.dashSlashTimer_ = .22f;
                player_.dashSlashPrevious_ = player_.GetWorldPosition();
                player_.dashSlashDirection_ = event.direction;
                player_.dashSlashDamage_ = static_cast<uint32_t>(
                    (std::max)(1.0f, std::round(player_.stats_.bulletDamage * 3.8f * MakeTankMeleeCombo(0, player_.runModifiers_).damage *
                                                (config ? config->bulletDamageScale : 1.0f) * 1.2f *
                                                TankEffectPower(player_.runModifiers_, 39))));
                player_.dashSlashTargets_.clear();
                player_.velocity_ += event.direction * .28f;
                player_.recentDashTimer_ = 0;
                ++player_.specialCombatStats_.dashSlashes;
                if (player_.pendingSpecialCombatEvents_.size() < 32)
                    player_.pendingSpecialCombatEvents_.push_back(
                        {SpecialEventKind::DashSlash, event.origin, event.direction, event.range});
            } else
                player_.pendingMeleeSlashes_.push_back(event);
            // 新しい振りごとにパリィ履歴を解除し、同じ弾への再処理と斬撃波の重複生成を防ぐ。
            player_.specialMeleeSwing_ = event;
            player_.specialMeleeElapsed_ = 0;
            player_.specialWaveEmitted_ = false;
            player_.specialPerfectFeedback_ = false;
            player_.specialParriedBullets_.clear();
            player_.meleeComboStep_ = dashSlash ? 1 : (step + 1) % 3;
            player_.finisherSpinReady_ = !dashSlash && step == 2;
            player_.bulletCoolTime = event.windupDuration + event.duration + event.recoveryDuration;
            player_.meleeComboTimer_ = player_.bulletCoolTime + 0.65f;
            // 斬撃方向の速度を加える。位置は通常の移動処理で更新し、操作による方向転換も受け付ける。
            player_.velocity_ += event.direction * (step == 2 ? 0.055f : 0.025f);
            player_.primaryAttackPerformedEvent_ = true;
            ++player_.primaryAttackCount_;
            return;
        }

        if (const PlayerClassConfig* config = player_.GetCurrentClassConfig()) {
            const float baseReload =
                (player_.isBuffActive_ ? (player_.stats_.reloadSpeed * 0.7f) / 60.0f : player_.stats_.reloadSpeed / 60.0f) *
                GetRunFireIntervalScale();
            cg2::Vector3 recoilDir = cg2::Normalize(player_.dir_) * -1.0f;
            float recoilPower = 0.01f;
            if (FireConfiguredClass(*config, bulletManager, baseReload, recoilDir, recoilPower)) {
                // 遠征のドローン生成では射撃を記録しない。Player::Updateでドローン射撃による弾数増加を記録する。
                if (!player_.runCheckpointEvolution_ || !config->usesDrone) {
                    player_.primaryAttackPerformedEvent_ = true;
                    ++player_.primaryAttackCount_;
                }
                player_.velocity_ += recoilDir * recoilPower;
                if (!player_.runCheckpointEvolution_)
                    cg2::Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
            }
            return;
        }

        // 現在の機体設定がない場合だけ、旧機体ごとの互換射撃へ進む。
        if (player_.bulletCoolTime <= 0.0f) {

            // 発射位置
            cg2::Vector3 origin = player_.GetWorldPosition();

            // 攻撃パラメータを設定
            AttackParam param{};
            param.bulletSpeed = player_.stats_.bulletSpeed;
            param.bulletCount = 1;
            param.spreadAngleDeg = 5.0f;
            param.randomSpread = false;

            param.reflect = false;
            param.penetrate = false;
            param.cooldown = 1.0f;
            param.damage = static_cast<uint32_t>(player_.stats_.bulletDamage);
            ApplyRunProjectileRules(param);
            bool firedByClass = false;

            cg2::Vector3 recoilDir = cg2::Normalize(player_.dir_) * -1.0f;
            float recoilPower = 0.01f; // 射撃方向と逆向きに加える速度の大きさ。

            // 個別にクールタイムを設定するために先に設定
            float baseReload = (player_.isBuffActive_ ? (player_.stats_.reloadSpeed * 0.7f) / 60.0f : player_.stats_.reloadSpeed / 60.0f) *
                               GetRunFireIntervalScale();
            player_.bulletCoolTime = baseReload;

            switch (player_.currentClass_) {
            case ClassType::Basic:
                // 既存の単発攻撃
                param.spreadAngleDeg = 10.0f;
                param.randomSpread = true;
                player_.bulletCoolTime = baseReload;
                if (player_.isBuffActive_) {
                    param.reflect = true;
                    player_.bulletCoolTime = baseReload * 0.6f;
                    param.spreadAngleDeg = 20.0f;
                }
                player_.attackController_.Fire(origin, player_.dir_, param, BulletOwner::kPlayer);
                firedByClass = true;
                if (!player_.barrels_.empty()) {
                    player_.barrels_[0].recoilOffset = 0.22f;
                    player_.barrels_[0].muzzleFlashTimer = player_.kMuzzleFlashDuration;
                }

                player_.SpawnCasing();
                break;

            case ClassType::Twin: {
                param.spreadAngleDeg = 2.0f;
                param.randomSpread = true;
                float offsetValue = 0.6f;                                        // 砲身の横幅
                cg2::Vector3 rightDir = {-player_.dir_.y, player_.dir_.x, 0.0f}; // dir_に垂直なベクトル（右方向）

                if (player_.shootBarrelIndex_ == 0) {
                    // 左から発射
                    player_.attackController_.Fire(origin - rightDir * offsetValue, player_.dir_, param, BulletOwner::kPlayer);
                    firedByClass = true;
                    if (!player_.barrels_.empty()) {
                        player_.barrels_[0].recoilOffset = 0.22f;
                        player_.barrels_[0].muzzleFlashTimer = player_.kMuzzleFlashDuration;
                    }
                    player_.shootBarrelIndex_ = 1; // 次は右
                } else {
                    // 右から発射
                    player_.attackController_.Fire(origin + rightDir * offsetValue, player_.dir_, param, BulletOwner::kPlayer);
                    firedByClass = true;
                    if (player_.barrels_.size() > 1) {
                        player_.barrels_[1].recoilOffset = 0.22f;
                        player_.barrels_[1].muzzleFlashTimer = player_.kMuzzleFlashDuration;
                    }
                    player_.shootBarrelIndex_ = 0; // 次は左
                }

                // 次の砲身を撃つまでの待ち時間を、基準発射間隔の1/2.5にする。
                player_.bulletCoolTime = baseReload / 2.5f;
            } break;

            case ClassType::MachineGun:
                // 角度をランダムにずらす
                param.spreadAngleDeg = 30.0f;
                param.randomSpread = true;
                player_.attackController_.Fire(origin, player_.dir_, param, BulletOwner::kPlayer);
                firedByClass = true;
                if (!player_.barrels_.empty()) {
                    player_.barrels_[0].muzzleFlashTimer = player_.kMuzzleFlashDuration;
                }
                // リロード補正0.6倍
                player_.bulletCoolTime = baseReload * 0.6f;
                break;

            case ClassType::Overseer:
                // 弾は撃たず、ドローン管理関数を呼ぶ
                DroneShoot(bulletManager);
                firedByClass = true;
                // 反動なし
                recoilPower = 0.0f;
                break;

            case ClassType::Smasher:

                break;
            case ClassType::Triple:
                param.bulletCount = 1;
                param.spreadAngleDeg = 45.0f; // 扇状に広がる
                break;

            case ClassType::Bounder:
                param.reflect = true; // 壁で反射する互換射撃。
                param.bulletCount = 1;
                break;

            case ClassType::Assassin:
                // 弾速を1.5倍にし、射撃時にステルスを解除する。
                param.bulletSpeed *= 1.5f;
                player_.isStealth_ = false; // 撃ったら解除
                player_.stealthTimer_ = 0.0f;
                break;

            case ClassType::Ninja:
                // Ninjaの互換射撃条件。1発と拡散角15度を設定する。
                param.bulletCount = 1;
                param.spreadAngleDeg = 15.0f;
                // この互換射撃ではステルスを解除しない。
                break;
            }

            if (!firedByClass && player_.currentClass_ != ClassType::Smasher) {
                player_.attackController_.Fire(origin, player_.dir_, param, BulletOwner::kPlayer);
                if (!player_.barrels_.empty()) {
                    player_.barrels_[0].muzzleFlashTimer = player_.kMuzzleFlashDuration;
                }
                player_.SpawnCasing();
            }

            if (firedByClass || player_.currentClass_ != ClassType::Smasher) {
                player_.primaryAttackPerformedEvent_ = true;
                ++player_.primaryAttackCount_;
            }
            player_.velocity_ += recoilDir * recoilPower;
            if (!player_.runCheckpointEvolution_)
                cg2::Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
        }
    } else if (!wantsPrimaryAttack)
        player_.finisherSpinReady_ = false;
}

void PlayerWeapons::DroneShoot(BulletManager* BulletManager)
{

    // 旧ドローン生成の待ち時間は呼び出しごとに1を減らす。Attackの秒単位の更新とは異なる。
    player_.bulletCoolTime--;

    const PlayerClassConfig* config = player_.GetCurrentClassConfig();
    int droneLimit = (std::max)(0, config ? config->maxDrones : 7);
    if (player_.runModifiers_.enabled) {
        droneLimit = TankExpeditionDroneLimit(player_.runEvolutionActive_ ? FindTankExpeditionSpecialization(player_.runEvolutionConfig_.id)
                                                                          : nullptr,
                                              droneLimit, player_.runModifiers_.drones, player_.runModifiers_.core == TankRunCore::Drone);
    }
    const size_t maxDrones = static_cast<size_t>(droneLimit);
    if (player_.drones_.size() >= maxDrones) {
        player_.bulletCoolTime = 0.0f;
        return;
    }

    if (player_.bulletCoolTime <= 0.0f) {

        // 生成したドローンへ渡す初期速度。
        const float kBulletSpeed = 0.2f;
        cg2::Vector3 velocity = player_.dir_ * kBulletSpeed;

        auto drone = std::make_unique<PlayerDrone>();
        drone->Initialize(player_.dir_ * 0.3f + player_.worldTransform_.translate, velocity);
        drone->SetAttackControllerBulletManager(BulletManager);
        ConfigureRunDrone(*drone);
        player_.drones_.push_back(std::move(drone));

        player_.bulletCoolTime = 1.0f;
    }
}

void PlayerWeapons::Smash(float deltaTime)
{

    if (player_.isSmash_) {
        return;
    }

    // キーを離したら突撃する
    if (player_.input_->IsMomentRelease(player_.input_->GetMouseState().rgbButtons[0], player_.input_->GetPreMouseState().rgbButtons[0])) {
        player_.smashDir_ = player_.dir_;
        player_.isSmash_ = true;
        cg2::Vector3 recoilDir = cg2::Normalize(player_.smashDir_);
        float recoilPower = cg2::EaseInQuad(player_.smashCharge_) / 2.0f; // 蓄積量から突進速度の大きさを求め、次の行で上限を0.7に制限する。
        recoilPower = std::min(recoilPower, 0.7f);
        player_.velocity_ = recoilDir * recoilPower;
        return;
    }

    if (player_.input_->IsPress(player_.input_->GetMouseState().rgbButtons[0])) {

        if (player_.isBuffActive_) {
            player_.smashCharge_ += deltaTime * 5.0f;
        } else {
            player_.smashCharge_ += deltaTime;
        }
        if (player_.smashCharge_ >= player_.maxCharge_) {
            player_.smashCharge_ = player_.maxCharge_;
        }
    }
}

void PlayerWeapons::ApplyRunProjectileRules(AttackParam& param, bool applyFan) const
{
    (void)applyFan; // 旧APIの呼び出し互換用。現在はこの引数による扇状の複数弾を生成しない。
    // 遠征が無効でも、発射数と衝突時の分裂数はここで初期化する。
    param.bulletCount = 1;
    param.impactSplitCount = 0;
    if (!player_.runModifiers_.enabled) {
        return;
    }
    const TankRunTuning tuning = MakeTankRunTuning(player_.runModifiers_, player_.runGrowth_);
    if (player_.expeditionCombatStyleSelected_ && player_.expeditionCombatStyle_ == tankbuild::Style::Shooter) {
        param.shooterChain = player_.runModifiers_.chainLightning;
        param.shooterMark = player_.runModifiers_.markDetonation;
        param.shooterBoomerang = player_.runModifiers_.boomerangShell;
        param.shooterKillBurst = player_.runModifiers_.killBurst;
        param.shooterChainPower = TankEffectPower(player_.runModifiers_, 31);
        param.shooterMarkPower = TankEffectPower(player_.runModifiers_, 32);
        param.shooterBoomerangPower = TankEffectPower(player_.runModifiers_, 33);
        param.shooterKillBurstPower = TankEffectPower(player_.runModifiers_, 34);
    }
    param.reflect = param.reflect || tuning.reflects;
    param.bulletHp = tuning.bulletHp;
    param.bulletPenetration = tuning.bulletInterception;
    if (player_.runModifiers_.expedition) {
        param.maxWallBounces = tuning.maxWallBounces;
        const auto* active = player_.GetCurrentClassConfig();
        if (active && active->reflect)
            param.maxWallBounces = (std::max)(3, param.maxWallBounces);
        if (player_.isBuffActive_) {
            param.reflect = true;
            param.maxWallBounces = (std::max)(1, param.maxWallBounces);
        }
        param.actorPierceCount = tuning.actorPierceCount;
        if (active && active->penetrate)
            param.actorPierceCount = (std::max)(1, param.actorPierceCount);
        param.impactSplitCount = tuning.impactSplitCount;
        param.impactSplitDamageScale = 0.55f;
    }
}

float PlayerWeapons::GetRunFireIntervalScale() const
{
    if (!player_.runModifiers_.enabled)
        return 1.0f;
    // 発射頻度の増加を間隔の逆数へ変換する。1未満の値ほど次の射撃までの待ち時間が短い。
    float scale = player_.runOverdriveTimer_ > 0.0f ? 1.0f / (1.0f + TankEffectPower(player_.runModifiers_, 11)) : 1.0f;
    if (player_.runModifiers_.core == TankRunCore::Assault && player_.runDashAttackTimer_ > 0.0f)
        scale *= 0.65f;
    return scale;
}

cg2::Vector3 PlayerWeapons::GetRailChargeMuzzle() const
{
    const cg2::Vector3 aim = cg2::Length(player_.dir_) > .001f ? cg2::Normalize(player_.dir_) : cg2::Vector3{1, 0, 0};
    const auto* config = player_.GetCurrentClassConfig();
    if (config && !config->barrels.empty()) {
        const auto& mount = config->barrels.front();
        const cg2::Vector3 side{-aim.y, aim.x, 0};
        return player_.GetWorldPosition() + aim * mount.offset.x + side * mount.offset.y +
               player_.RotateDirection(aim, mount.angleDeg) * mount.muzzleForward;
    }
    return player_.GetWorldPosition() + aim * 1.7f;
}

void PlayerWeapons::AttackRailCannon(BulletManager* bullets, bool pressed, float dt)
{
    if (!player_.runModifiers_.enabled || !player_.runModifiers_.railCannon || !player_.expeditionCombatStyleSelected_ ||
        player_.expeditionCombatStyle_ != tankbuild::Style::Shooter) {
        player_.railCharge_.Reset();
        return;
    }
    // 戻り値は蓄積した秒数。負値は発射なしで、0秒の短い押下も有効な発射として扱う。
    const float charge = player_.railCharge_.Step(pressed, dt, player_.bulletCoolTime <= 0.0f && bullets != nullptr);
    if (charge < 0)
        return;
    const auto* config = player_.GetCurrentClassConfig();
    const cg2::Vector3 aim = cg2::Length(player_.dir_) > .001f ? cg2::Normalize(player_.dir_) : cg2::Vector3{1, 0, 0};
    const cg2::Vector3 side{-aim.y, aim.x, 0};
    AttackParam param{};
    param.damage = static_cast<uint32_t>(
        (std::max)(1.0f, std::round(player_.stats_.bulletDamage * (config ? config->bulletDamageScale : 1.0f) *
                                    tankspecial::RailDamageScale(charge, TankEffectPower(player_.runModifiers_, 20)))));
    param.bulletSpeed = player_.stats_.bulletSpeed * (config ? config->bulletSpeedScale : 1.0f) * tankspecial::RailSpeedScale(charge);
    param.reflect = config && config->reflect;
    param.penetrate = true;
    ApplyRunProjectileRules(param);
    param.actorPierceCount = (std::max)(3, param.actorPierceCount);
    param.bulletHp = (std::max)(2.0f + charge * 3.0f, param.bulletHp);
    const size_t barrelCount = config && !config->barrels.empty() ? (std::min)(config->barrels.size(), size_t{8}) : 1;
    const bool alternate = config && config->alternateBarrels && barrelCount > 1;
    const size_t count = alternate ? 1 : barrelCount;
    for (size_t shot = 0; shot < count; ++shot) {
        const size_t index = alternate ? static_cast<size_t>(player_.shootBarrelIndex_) % barrelCount : shot;
        cg2::Vector3 fire = aim, muzzle = player_.GetWorldPosition() + aim * 1.7f;
        float damageScale = 1, speedScale = 1;
        if (config && !config->barrels.empty()) {
            const auto& mount = config->barrels[index];
            if (!mount.fires)
                continue;
            fire = player_.RotateDirection(aim, mount.angleDeg);
            muzzle = player_.GetWorldPosition() + aim * mount.offset.x + side * mount.offset.y + fire * mount.muzzleForward;
            damageScale = mount.damageScale;
            speedScale = mount.projectileSpeedScale;
        }
        // 交互射撃では1本の威力に装備砲身数を掛ける。入力を離すたび、次の砲身へ進める。
        if (alternate)
            damageScale *= static_cast<float>(barrelCount);
        auto bullet = std::make_unique<Bullet>();
        bullet->Initialize(muzzle, fire * param.bulletSpeed * speedScale,
                           static_cast<uint32_t>((std::max)(1.0f, std::round(static_cast<float>(param.damage) * damageScale))), kPlayer,
                           param.reflect, param.bulletHp, param.bulletPenetration);
        bullet->ConfigureGrowth(param.maxWallBounces, param.actorPierceCount, 0);
        bullet->ConfigureSpecial(Bullet::SpecialKind::Rail, .35f + .30f * charge, 1.6f);
        bullet->ConfigureShooterAbilities(param.shooterChain, param.shooterMark, param.shooterBoomerang, param.shooterKillBurst,
                                          param.shooterChainPower, param.shooterMarkPower, param.shooterBoomerangPower,
                                          param.shooterKillBurstPower);
        bullets->Add(std::move(bullet));
        if (index < player_.barrels_.size()) {
            player_.barrels_[index].muzzleFlashTimer = player_.kMuzzleFlashDuration;
            player_.barrels_[index].recoilOffset = .32f;
        }
        if (player_.pendingSpecialCombatEvents_.size() < 32)
            player_.pendingSpecialCombatEvents_.push_back({SpecialEventKind::RailShot, muzzle, fire, charge});
    }
    if (alternate)
        player_.shootBarrelIndex_ = static_cast<int>((static_cast<size_t>(player_.shootBarrelIndex_) + 1) % barrelCount);
    player_.bulletCoolTime =
        tankspecial::RailRecovery(player_.stats_.reloadSpeed / 60.0f * (config ? config->reloadScale : 1.0f) * GetRunFireIntervalScale());
    player_.velocity_ += aim * (-.04f - .035f * charge);
    player_.primaryAttackPerformedEvent_ = true;
    ++player_.primaryAttackCount_;
    ++player_.specialCombatStats_.railShots;
}

void PlayerWeapons::UpdateSpecialCombat(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt)
{
    // 表示用の接続線は毎回作り直す。更新対象外でも前回の接触表示を残さない。
    player_.droneLaserLinks_.clear();
    if (!player_.runModifiers_.enabled || player_.isDead_ || !bullets || dt <= 0)
        return;
    UpdateAdditionalAbilities(stage, bullets, boss, enemies, dt);
    player_.linkDamageClock_.Advance(dt);
    auto blocked = [&](const cg2::Vector3& a, const cg2::Vector3& b) {
        for (const auto& block : stage.GetMergedBlocks()) {
            if (tankspecial::SegmentCrossesBox(a.x, a.y, b.x, b.y, block.aabb.min.x, block.aabb.min.y, block.aabb.max.x, block.aabb.max.y))
                return true;
        }
        return false;
    };
    auto emit = [&](SpecialEventKind kind, const cg2::Vector3& origin, const cg2::Vector3& direction, float strength = 1.0f) {
        if (player_.pendingSpecialCombatEvents_.size() < 32)
            player_.pendingSpecialCombatEvents_.push_back({kind, origin, direction, strength});
    };
    // 再構築中・死亡済みのドローンを除き、2機なら1本、3機以上なら輪になるよう接続する。
    if (player_.IsDroneBuild() && player_.runModifiers_.droneLaserLink) {
        std::vector<cg2::Vector3> positions;
        positions.reserve(player_.drones_.size());
        for (const auto& drone : player_.drones_)
            if (drone && drone->IsRunAvailable())
                positions.push_back(drone->GetWorldPosition());
        const int count = tankspecial::LinkCount(static_cast<int>(positions.size()));
        for (int i = 0; i < count; ++i) {
            const auto& a = positions[static_cast<size_t>(i)];
            const auto& b = positions[(static_cast<size_t>(i) + 1) % positions.size()];
            if (cg2::Length(b - a) > .10f && !blocked(a, b))
                player_.droneLaserLinks_.push_back({a, b, false});
        }
        const auto tuning = MakeTankDroneTuning(player_.runModifiers_, true);
        const auto* config = player_.GetCurrentClassConfig();
        const float baseDamage = player_.stats_.bulletDamage * tuning.damageScale * (config ? config->bulletDamageScale : 1.0f) * .45f *
                                 TankEffectPower(player_.runModifiers_, 21);
        auto contact = [&](Collider* target, bool bossTarget) {
            if (!target)
                return;
            const auto p = target->GetWorldPosition();
            for (auto& link : player_.droneLaserLinks_) {
                if (!tankspecial::SegmentTouches(link.start.x, link.start.y, link.end.x, link.end.y, p.x, p.y, target->GetRadius() + .10f))
                    continue;
                const float t = tankspecial::SegmentClosestFraction(link.start.x, link.start.y, link.end.x, link.end.y, p.x, p.y);
                const cg2::Vector3 nearest = link.start + (link.end - link.start) * t;
                if (blocked(nearest, p))
                    continue;
                // 接触表示はダメージ間隔とは独立する。同じ対象への適用許可は全ての線で共有する。
                link.contact = true;
                if (player_.linkDamageClock_.Claim(target->GetCollisionId())) {
                    const auto damage = static_cast<uint32_t>((std::max)(1.0f, std::round(baseDamage * (bossTarget ? .65f : 1.0f) *
                                                                                          GetDroneTargetDamageScale(target, bossTarget))));
                    if (bossTarget)
                        static_cast<Enemy*>(target)->TakeDamage(damage);
                    else
                        static_cast<ExpEnemy*>(target)->TakeDirectionalDamage(damage, nearest);
                    ++player_.specialCombatStats_.linkTicks;
                    emit(SpecialEventKind::LinkHit, nearest, cg2::Normalize(link.end - link.start), .5f);
                }
            }
        };
        if (boss && !boss->IsDead())
            contact(boss, true);
        if (enemies)
            for (auto* enemy : enemies->GetEnemyPtrs())
                if (enemy && !enemy->IsDead() && !enemy->IsRunResource())
                    contact(enemy, false);
    }
    if (!player_.IsMeleeBuild() || player_.specialMeleeElapsed_ < 0)
        return;
    player_.specialMeleeElapsed_ += dt;
    const auto& swing = player_.specialMeleeSwing_;
    // 準備時間を引いた秒数で斬撃の有効時間とジャストパリィの窓を判定する。
    const float active = player_.specialMeleeElapsed_ - swing.windupDuration;
    if (active < 0)
        return;
    if (player_.runModifiers_.slashWave && !player_.specialWaveEmitted_ && tankspecial::EmitsSlashWave(swing.comboStep)) {
        // 壁で生成できない場合も、この振りでの生成試行は消費する。
        player_.specialWaveEmitted_ = true;
        const float speed = .48f;
        const float radius = (std::clamp)(swing.range * .25f, .7f, 2.2f);
        const cg2::Vector3 origin = player_.GetWorldPosition() + swing.direction * (swing.range * .5f);
        if (!blocked(player_.GetWorldPosition(), origin)) {
            auto wave = std::make_unique<Bullet>();
            wave->Initialize(origin, swing.direction * speed,
                             tankspecial::SlashDamage(swing.damage, TankEffectPower(player_.runModifiers_, 22)), kPlayer, false, 10.0f,
                             tankspecial::kOrdinaryEnemyBulletHp);
            wave->ConfigureGrowth(0, 2, 0);
            // 60基準フレームの速度を秒単位へ直し、移動距離から寿命を求める。
            wave->ConfigureSpecial(Bullet::SpecialKind::SlashWave, radius, swing.range * 1.5f / (speed * 60.0f));
            bullets->Add(std::move(wave));
            ++player_.specialCombatStats_.slashWaves;
        }
    }
    if (active > swing.duration) {
        player_.specialMeleeElapsed_ = -1;
        return;
    }
    if (!player_.runModifiers_.parryBlade)
        return;
    const bool perfect = swing.comboStep >= 0 && tankspecial::IsPerfectParry(active);
    const cg2::Vector3 origin = player_.GetWorldPosition();
    const float minDot = std::cos(swing.arcDeg * .5f * 3.1415926535f / 180.0f);
    // ポインター配列のコピーを走査する。反射弾を追加しても今回の走査対象へ混ぜない。
    for (auto* bullet : bullets->GetBulletPtrs()) {
        if (!bullet || bullet->IsDead() || bullet->GetOwner() != kEnemy)
            continue;
        if (std::find(player_.specialParriedBullets_.begin(), player_.specialParriedBullets_.end(), bullet->GetCollisionId()) !=
            player_.specialParriedBullets_.end())
            continue;
        const auto p = bullet->GetWorldPosition();
        const cg2::Vector3 delta = p - origin;
        const float distance = cg2::Length(delta);
        if (distance > swing.range + bullet->GetRadius() || (distance > .001f && cg2::Dot(delta / distance, swing.direction) < minDot) ||
            blocked(origin, p))
            continue;
        // 耐久ダメージが0の場合も履歴へ記録し、同じ振りでは再判定しない。
        player_.specialParriedBullets_.push_back(bullet->GetCollisionId());
        const float durability =
            tankspecial::ParryDurabilityDamage(bullet->GetBulletHp(), perfect, TankEffectPower(player_.runModifiers_, 23));
        if (durability <= 0)
            continue;
        const cg2::Vector3 incoming = bullet->GetMove();
        bullet->ApplyBulletDurabilityDamage(durability);
        ++player_.specialCombatStats_.parries;
        if (perfect) {
            ++player_.specialCombatStats_.perfectParries;
            if (bullet->IsDead()) {
                const cg2::Vector3 reflected = cg2::Length(incoming) > .001f ? cg2::Normalize(incoming) * -1.0f : swing.direction;
                auto shot = std::make_unique<Bullet>();
                const auto damage = static_cast<uint32_t>(
                    (std::max)(1.0f, (std::min)(static_cast<float>(bullet->GetDamage()), player_.stats_.bulletDamage * 3.8f)));
                shot->Initialize(p, reflected * (std::max)(.25f, cg2::Length(incoming)), damage, kPlayer, false, 1, 1);
                shot->ConfigureGrowth(0, 0, 0);
                shot->ConfigureSpecial(Bullet::SpecialKind::ParryReflection, .35f, 1.4f);
                shot->SetArmorReflected(bullet->WasArmorReflected());
                bullets->Add(std::move(shot));
            }
            if (!player_.specialPerfectFeedback_) {
                emit(SpecialEventKind::PerfectParry, p, swing.direction);
                player_.specialPerfectFeedback_ = true;
            } else
                emit(SpecialEventKind::Parry, p, swing.direction, .5f);
        } else
            emit(SpecialEventKind::Parry, p, swing.direction);
    }
}

void PlayerWeapons::SetRunHomingTargets(const std::vector<cg2::Vector3>& targets)
{
    const size_t count = (std::min)(targets.size(), size_t{48});
    player_.runHomingTargets_.assign(targets.begin(), targets.begin() + count);
}

void PlayerWeapons::UpdateRunProjectiles(BulletManager* bulletManager, float deltaTime)
{
    if (!player_.runModifiers_.enabled ||
        (!player_.runModifiers_.homing && !player_.runModifiers_.droneFocus && !player_.runModifiers_.targetPainter) || deltaTime <= 0.0f ||
        player_.runHomingTargets_.empty())
        return;
    const float homingTurnRate = (std::max)(MakeTankRunSynergy(player_.runModifiers_, player_.runOverdriveTimer_ > 0.0f).homingTurnRate,
                                            player_.IsDroneBuild() && player_.runModifiers_.droneFocus ? .70f : 0.0f) *
                                 (player_.empJammerTimer_ > 0 ? .55f : 1.0f);
    // 帰還中を除く自機弾を最大240個まで調べる。静止弾もこの調査枠には含める。
    size_t steered = 0;
    for (Bullet* bullet : bulletManager->GetBulletPtrs()) {
        if (!bullet || bullet->IsDead() || bullet->GetOwner() != BulletOwner::kPlayer)
            continue;
        if (bullet->GetIsReturning())
            continue;
        if (++steered > 240)
            break;
        cg2::Vector3 direction = bullet->GetMove();
        const float speed = cg2::Length(direction);
        if (speed < 0.001f)
            continue;
        direction = direction / speed;
        float nearestSquared = 14.0f * 14.0f;
        cg2::Vector3 targetDirection{};
        bool found = false;
        // XY平面で距離0.2以上・14未満、かつ前方の対象から最も近いものを選ぶ。壁の遮蔽は調べない。
        for (const cg2::Vector3& target : player_.runHomingTargets_) {
            cg2::Vector3 offset = target - bullet->GetWorldPosition();
            offset.z = 0.0f;
            const float distanceSquared = offset.x * offset.x + offset.y * offset.y;
            if (distanceSquared < 0.04f || distanceSquared >= nearestSquared)
                continue;
            const cg2::Vector3 candidate = offset / std::sqrt(distanceSquared);
            if (direction.x * candidate.x + direction.y * candidate.y < 0.35f)
                continue;
            nearestSquared = distanceSquared;
            targetDirection = candidate;
            found = true;
        }
        if (!found)
            continue;
        const float dot = (std::clamp)(direction.x * targetDirection.x + direction.y * targetDirection.y, -1.0f, 1.0f);
        float targetTurn = homingTurnRate;
        if (player_.IsDroneBuild() && player_.runModifiers_.targetPainter && bullet->GetSourceDroneIndex() >= 0) {
            for (const auto& lock : player_.targetLockVisuals_)
                if (lock.remaining > 0) {
                    const auto offset = lock.position - bullet->GetWorldPosition();
                    if (cg2::Length(offset) > .001f && cg2::Length(cg2::Normalize(offset) - targetDirection) < .1f)
                        targetTurn = (std::max)(targetTurn, 1.05f * (player_.empJammerTimer_ > 0 ? .55f : 1.0f));
                }
        }
        // 回頭速度はラジアン/秒。目標を越えない角度に制限し、度へ直して元の速さを保った速度の向きを設定する。
        // ここでは位置を進めない。設定した速度による位置更新はBullet::Updateが行う。
        const float turn = (std::min)(std::acos(dot), targetTurn * deltaTime);
        const float cross = direction.x * targetDirection.y - direction.y * targetDirection.x;
        const float signedDegrees = turn * (cross < 0.0f ? -1.0f : 1.0f) * (180.0f / 3.1415926535f);
        bullet->SetVelocity(player_.RotateDirection(direction, signedDegrees) * speed);
    }
}

void PlayerWeapons::ConfigureRunDrone(PlayerDrone& drone) const
{
    if (!player_.runModifiers_.enabled) {
        return;
    }
    const PlayerClassConfig* config = player_.GetCurrentClassConfig();
    const bool primaryStyle = player_.IsDroneBuild();
    const bool isSwarm = config && config->usesDrone;
    auto droneTuning = MakeTankDroneTuning(player_.runModifiers_, primaryStyle);
    if (!primaryStyle && isSwarm) {
        droneTuning.damageScale *= 0.6f / 0.35f;
        droneTuning.reloadSeconds *= 0.5f / 0.75f;
    }
    AttackParam param{};
    param.bulletSpeed = player_.stats_.bulletSpeed * (config ? config->bulletSpeedScale : 1.0f);
    if (primaryStyle && player_.runModifiers_.droneFocus)
        param.bulletSpeed *= 1.15f;
    param.bulletCount = 1;
    param.spreadAngleDeg = primaryStyle || player_.runModifiers_.droneFocus ? droneTuning.spreadDegrees
                           : isSwarm && player_.runEvolutionActive_         ? config->spreadAngleDeg
                                                                            : (isSwarm ? 10.0f : 6.0f);
    param.randomSpread = param.spreadAngleDeg > 0.0f;
    param.reflect = config && config->reflect;
    param.damage = static_cast<uint32_t>(
        (std::max)(1.0f, std::round(player_.stats_.bulletDamage * (config ? config->bulletDamageScale : 1.0f) * droneTuning.damageScale *
                                    (primaryStyle && player_.runModifiers_.autonomousSpread
                                         ? (std::max)(.2f, 1.0f - .18f * TankEffectPower(player_.runModifiers_, 38))
                                         : 1.0f))));
    if (player_.isBuffActive_) {
        param.reflect = true;
    }
    ApplyRunProjectileRules(param);
    param.bulletHp = (std::max)(param.bulletHp, droneTuning.bulletHp);
    param.bulletPenetration = (std::max)(param.bulletPenetration, droneTuning.interception);
    param.bulletVisualScale = droneTuning.sizeScale;
    param.bulletTrailScale = droneTuning.trailScale;
    // 基準フレーム同士の比率を秒単位のドローン発射間隔へ掛ける。
    const float reloadRatio = player_.stats_.reloadSpeed / player_.GetRunBaseReloadFrames();
    const float coreRate = player_.runModifiers_.core == TankRunCore::Drone ? 0.8f : 1.0f;
    const auto* specialization = player_.runEvolutionActive_ ? FindTankExpeditionSpecialization(player_.runEvolutionConfig_.id) : nullptr;
    const float interval = droneTuning.reloadSeconds * reloadRatio * coreRate * (player_.isBuffActive_ ? 0.7f : 1.0f) *
                           GetRunFireIntervalScale() *
                           (primaryStyle && config      ? config->reloadScale
                            : isSwarm && specialization ? specialization->reloadScale
                                                        : 1.0f) *
                           (primaryStyle ? player_.GetCombatStyleProfile(tankbuild::Style::Drone).attackIntervalSeconds / .5f : 1.0f) *
                           (player_.empJammerTimer_ > 0 ? 1.5f : 1.0f);
    drone.ConfigureRunAttack(param, interval);
    // 主装備のドローンは小数の威力を別に保持し、射撃ごとの余りを繰り越す。
    // 例えば3×0.82を毎回整数へ切り捨てて2にする場合の、過剰な威力低下を避ける。
    drone.SetRunExactDamage(
        primaryStyle
            ? (std::max)(1.0f, player_.stats_.bulletDamage * (config ? config->bulletDamageScale : 1.0f) * droneTuning.damageScale *
                                   (player_.runModifiers_.autonomousSpread
                                        ? (std::max)(.2f, 1.0f - .18f * TankEffectPower(player_.runModifiers_, 38))
                                        : 1.0f))
            : 0.0f);
    if (primaryStyle) {
        const auto& profile = player_.GetCombatStyleProfile(tankbuild::Style::Drone);
        drone.SetRunFollowTuning(profile.droneFollowSpeed, profile.droneCatchupSpeed, profile.droneResponse);
    }
}

bool PlayerWeapons::TryActivateSpecialAction()
{
    const PlayerClassConfig* config = player_.GetCurrentClassConfig();
    if (!config || player_.dashCooldown_ > 0.0f) {
        return false;
    }
    if (player_.runModifiers_.enabled)
        return ActivatePerfectDodge(*config);
    if (config->specialActionId == "none")
        return false;
    if (config->specialActionId == "perfect_dodge") {
        return ActivatePerfectDodge(*config);
    }
    if (config->specialActionId == "saber_counter") {
        return ActivateSaberCounter(*config);
    }
    // charge_beamを含む未対応のIDでは、特殊行動を開始せずfalseを返す。
    return false;
}

bool PlayerWeapons::ActivatePerfectDodge(const PlayerClassConfig& config)
{
    const float staminaCost = (std::max)(0.0f, config.specialActionStaminaCost);
    if (player_.stats_.stamina < staminaCost) {
        return false;
    }

    cg2::Vector3 dashDir = player_.inputDir_;
    if (cg2::Length(dashDir) < 0.01f) {
        dashDir = player_.dir_;
    }
    if (cg2::Length(dashDir) < 0.01f) {
        return false;
    }

    const TankRunTuning runTuning = MakeTankRunTuning(player_.runModifiers_, player_.runGrowth_);
    const cg2::Vector3 direction = cg2::Normalize(dashDir);
    const float speed = player_.kDashSpeed * runTuning.dashSpeed;
    player_.velocity_ = {TankDashMomentum(player_.velocity_.x, direction.x, speed),
                         TankDashMomentum(player_.velocity_.y, direction.y, speed), 0};
    // 新しいダッシュでだけ体当たり履歴を解除する。接触が続いても同じ対象へ繰り返し適用しない。
    player_.dashImpactTargets_.clear();
    ++player_.dashStartedCount_;
    player_.isDashing_ = true;
    player_.dashStartedEvent_ = true;
    player_.dashTimer_ = player_.kDashDuration;
    player_.recentDashTimer_ = .30f;
    player_.dashCooldown_ = player_.kDashCooldown * (std::max)(0.05f, config.specialActionCooldownScale) * runTuning.dashCooldown;
    player_.stats_.stamina = (std::max)(0.0f, player_.stats_.stamina - staminaCost);
    if (player_.runModifiers_.enabled) {
        player_.runDashAttackTimer_ = 1.0f;
        // 追加射撃はPlayer::Updateの攻撃処理まで保留し、その時点の向きで生成する。
        player_.runDashBurstPending_ = player_.runModifiers_.dashBurst;
        if (player_.runModifiers_.core == TankRunCore::Assault) {
            player_.bulletCoolTime = 0.0f;
            std::fill(player_.weaponGroupCooldowns_.begin(), player_.weaponGroupCooldowns_.end(), 0.0f);
        }
        if (player_.runModifiers_.overdrive && player_.runOverdriveCooldown_ <= 0.0f) {
            player_.runOverdriveTimer_ = 1.2f * TankEffectPower(player_.runModifiers_, 11);
            player_.runOverdriveCooldown_ = 4.0f;
        }
        if (player_.runModifiers_.core == TankRunCore::Drone) {
            for (auto& drone : player_.drones_)
                drone->RallyRunAttack();
        }
    }
    return true;
}

bool PlayerWeapons::ActivateSaberCounter(const PlayerClassConfig& config)
{
    const bool hasMeleeWeapon = std::any_of(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
        return mount.fires && mount.weaponType == WeaponType::Melee;
    });
    const float staminaCost = (std::max)(0.0f, config.specialActionStaminaCost);
    if (!hasMeleeWeapon || player_.stats_.stamina < staminaCost) {
        return false;
    }

    player_.saberCounterTimer_ = (std::max)(0.01f, config.saberCounterWindow);
    player_.dashCooldown_ = 1.0f * (std::max)(0.05f, config.specialActionCooldownScale);
    player_.stats_.stamina = (std::max)(0.0f, player_.stats_.stamina - staminaCost);
    return true;
}

void PlayerWeapons::TriggerSaberCounter(const PlayerClassConfig& config)
{
    const auto mountIt = std::find_if(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
        return mount.fires && mount.weaponType == WeaponType::Melee;
    });
    if (mountIt == config.barrels.end()) {
        return;
    }

    const WeaponMountConfig& mount = *mountIt;
    const cg2::Vector3 forward = cg2::Length(player_.dir_) > 0.0001f ? cg2::Normalize(player_.dir_) : cg2::Vector3{1.0f, 0.0f, 0.0f};
    const cg2::Vector3 right = {-forward.y, forward.x, 0.0f};
    MeleeSlashEvent event{};
    event.origin =
        player_.worldTransform_.translate + forward * mount.offset.x + right * mount.offset.y + cg2::Vector3{0.0f, 0.0f, mount.offset.z};
    event.direction = player_.RotateDirection(forward, mount.angleDeg);
    event.range = mount.meleeRange * config.saberCounterRangeScale;
    event.arcDeg = (std::max)(180.0f, mount.meleeArcDeg);
    event.width = mount.meleeWidth * 1.35f;
    event.duration = (std::max)(0.08f, mount.meleeDuration * 0.85f);
    event.windupDuration = 0.0f;
    event.recoveryDuration = 0.22f;
    event.comboStep = 2;
    event.damage =
        static_cast<uint32_t>((std::max)(1.0f, player_.stats_.bulletDamage * mount.damageScale * config.saberCounterDamageScale));
    event.color = {0.65f, 1.45f, 1.25f, 1.0f};
    // 命中判定と描画はシーンへ渡す斬撃イベントに予約し、ここでは受付を閉じて無敵とスローを要求する。
    player_.pendingMeleeSlashes_.push_back(event);
    player_.saberCounterTimer_ = 0.0f;
    player_.invincibleTimer_ = 0.28f;
    player_.requestSlow_ = true;
}

bool PlayerWeapons::FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload,
                                        cg2::Vector3& recoilDir, float& recoilPower)
{
    // ドローン生成の成功判定は射撃処理の受付を表し、新しい弾が登録された保証ではない。
    if (config.usesDrone) {
        if (player_.bulletCoolTime > 0.0f) {
            return false;
        }
        DroneShoot(bulletManager);
        recoilPower = 0.0f;
        player_.bulletCoolTime = baseReload * config.reloadScale;
        return true;
    }
    // この内訳は削除待ちの死亡弾も含む。遠征の受付上限は生存数によるAddの上限とは別に判定する。
    if (player_.runModifiers_.enabled && bulletManager->GetBulletCounts().player >= 240)
        return false;

    AttackParam param{};
    param.bulletSpeed = player_.stats_.bulletSpeed * config.bulletSpeedScale;
    param.bulletCount = 1;
    param.spreadAngleDeg = config.spreadAngleDeg;
    param.randomSpread = config.randomSpread;
    param.reflect = config.reflect;
    param.penetrate = config.penetrate;
    param.cooldown = 1.0f;
    const float shotDamage = player_.stats_.bulletDamage * config.bulletDamageScale;
    param.damage = static_cast<uint32_t>((std::max)(1.0f, player_.runModifiers_.enabled ? std::round(shotDamage) : shotDamage));
    ApplyRunProjectileRules(param);

    if (player_.isBuffActive_) {
        param.reflect = true;
        param.spreadAngleDeg += 10.0f;
    }

    std::vector<size_t> fireIndices;
    for (size_t i = 0; i < config.barrels.size(); ++i) {
        if (config.barrels[i].fires &&
            (config.barrels[i].weaponType == WeaponType::Projectile || config.barrels[i].weaponType == WeaponType::Laser ||
             config.barrels[i].weaponType == WeaponType::Mine || config.barrels[i].weaponType == WeaponType::Melee)) {
            fireIndices.push_back(i);
        }
    }
    if (fireIndices.empty()) {
        return false;
    }

    // 発射可能な砲身だけを集め、交互射撃で複数グループがある場合はグループ別の待ち時間を使う。
    std::vector<int> fireGroups;
    for (size_t index : fireIndices) {
        const int group = (std::max)(0, config.barrels[index].fireGroup);
        if (std::find(fireGroups.begin(), fireGroups.end(), group) == fireGroups.end()) {
            fireGroups.push_back(group);
        }
    }
    std::sort(fireGroups.begin(), fireGroups.end());
    const bool usesGroupCooldowns = config.alternateBarrels && !config.fireAllBarrels && fireGroups.size() > 1;
    int selectedGroupSlot = -1;

    if (config.alternateBarrels && !config.fireAllBarrels) {
        if (usesGroupCooldowns) {
            if (player_.weaponGroupCooldowns_.size() != fireGroups.size()) {
                player_.weaponGroupCooldowns_.assign(fireGroups.size(), 0.0f);
                player_.shootGroupIndex_ = 0;
            }
            // 前回の次のグループから、待ち時間が終わったグループを探す。
            for (size_t attempt = 0; attempt < fireGroups.size(); ++attempt) {
                const size_t slot = (static_cast<size_t>(player_.shootGroupIndex_) + attempt) % fireGroups.size();
                if (player_.weaponGroupCooldowns_[slot] <= 0.0f) {
                    selectedGroupSlot = static_cast<int>(slot);
                    break;
                }
            }
            if (selectedGroupSlot < 0) {
                player_.bulletCoolTime = 0.0f;
                return false;
            }
            const int selectedGroup = fireGroups[static_cast<size_t>(selectedGroupSlot)];
            std::vector<size_t> groupIndices;
            for (size_t index : fireIndices) {
                if ((std::max)(0, config.barrels[index].fireGroup) == selectedGroup) {
                    groupIndices.push_back(index);
                }
            }
            fireIndices = groupIndices;
            player_.shootGroupIndex_ = (selectedGroupSlot + 1) % static_cast<int>(fireGroups.size());
        } else {
            if (player_.bulletCoolTime > 0.0f) {
                return false;
            }
            const size_t selectableCount = fireIndices.size();
            const size_t index = fireIndices[player_.shootBarrelIndex_ % selectableCount];
            fireIndices = {index};
            player_.shootBarrelIndex_ = static_cast<int>((player_.shootBarrelIndex_ + 1) % selectableCount);
        }
    } else if (player_.bulletCoolTime > 0.0f) {
        return false;
    }

    const cg2::Vector3 forward = cg2::Length(player_.dir_) > 0.0001f ? cg2::Normalize(player_.dir_) : cg2::Vector3{1.0f, 0.0f, 0.0f};
    const cg2::Vector3 right = {-forward.y, forward.x, 0.0f};
    bool firesMelee = false;
    float meleeComboResetTime = 0.90f;
    for (size_t index : fireIndices) {
        if (index < config.barrels.size() && config.barrels[index].weaponType == WeaponType::Melee) {
            firesMelee = true;
            meleeComboResetTime = config.barrels[index].meleeComboResetTime;
            break;
        }
    }
    if (firesMelee && player_.meleeComboTimer_ <= 0.0f) {
        player_.meleeComboStep_ = 0;
    }
    // 今回選ばれた全ての近接砲身でコンボ段階を共有し、次回用の段階は一度だけ進める。
    const int meleeComboStepForShot = (std::clamp)(player_.meleeComboStep_, 0, 2);
    float meleeActionDuration = 0.0f;
    if (firesMelee) {
        player_.meleeComboStep_ = (meleeComboStepForShot + 1) % 3;
        player_.meleeComboTimer_ = (std::max)(0.05f, meleeComboResetTime);
    }

    cg2::Vector3 combinedRecoil{};
    float firedReloadScale = 1.0f;
    for (size_t index : fireIndices) {
        const WeaponMountConfig& barrelConfig = config.barrels[index];
        const cg2::Vector3 fireDir = player_.RotateDirection(forward, barrelConfig.angleDeg);
        combinedRecoil = combinedRecoil + fireDir * (-(std::max)(0.0f, barrelConfig.recoilScale));
        firedReloadScale = (std::max)(firedReloadScale, barrelConfig.reloadScale);
        // 砲身の前後・横方向のオフセットを自機のXY軸へ変換し、Zオフセットはそのまま加える。
        const cg2::Vector3 mountBase = player_.worldTransform_.translate + forward * barrelConfig.offset.x + right * barrelConfig.offset.y +
                                       cg2::Vector3{0.0f, 0.0f, barrelConfig.offset.z};
        const cg2::Vector3 muzzle = mountBase + fireDir * barrelConfig.muzzleForward;
        AttackParam mountParam = param;
        mountParam.damage = static_cast<uint32_t>(
            (std::max)(1.0f, player_.runModifiers_.expedition ? std::round(static_cast<float>(param.damage) * barrelConfig.damageScale)
                                                              : static_cast<float>(param.damage) * barrelConfig.damageScale));
        // レーザー・地雷・斬撃はシーン側の更新へ予約する。通常の弾はAttackControllerを通して追加する。
        if (barrelConfig.weaponType == WeaponType::Laser) {
            LaserShotEvent event{};
            event.origin = muzzle;
            event.direction = fireDir;
            event.range = barrelConfig.laserRange;
            event.width = barrelConfig.laserWidth;
            event.duration = barrelConfig.laserDuration;
            event.damageInterval = barrelConfig.laserDamageInterval;
            event.damage = mountParam.damage;
            event.color = barrelConfig.effectColor;
            player_.pendingLaserShots_.push_back(event);
        } else if (barrelConfig.weaponType == WeaponType::Mine) {
            MineDropEvent event{};
            event.position = muzzle;
            event.radius = barrelConfig.mineRadius;
            event.fuseTime = barrelConfig.mineFuseTime;
            event.lifeTime = barrelConfig.mineLifeTime;
            event.damage = mountParam.damage;
            event.color = barrelConfig.effectColor;
            player_.pendingMineDrops_.push_back(event);
        } else if (barrelConfig.weaponType == WeaponType::Melee) {
            const float meleeDamageScale = meleeComboStepForShot == 0   ? barrelConfig.meleeCombo1DamageScale
                                           : meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2DamageScale
                                                                        : barrelConfig.meleeCombo3DamageScale;
            const float meleeRangeScale = meleeComboStepForShot == 0   ? barrelConfig.meleeCombo1RangeScale
                                          : meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2RangeScale
                                                                       : barrelConfig.meleeCombo3RangeScale;
            const float meleeWindup = meleeComboStepForShot == 0   ? barrelConfig.meleeCombo1Windup
                                      : meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2Windup
                                                                   : barrelConfig.meleeCombo3Windup;
            const float meleeRecovery = meleeComboStepForShot == 0   ? barrelConfig.meleeCombo1Recovery
                                        : meleeComboStepForShot == 1 ? barrelConfig.meleeCombo2Recovery
                                                                     : barrelConfig.meleeCombo3Recovery;
            MeleeSlashEvent event{};
            event.origin = mountBase;
            event.direction = fireDir;
            event.range = barrelConfig.meleeRange * meleeRangeScale;
            event.arcDeg = barrelConfig.meleeArcDeg;
            event.width = barrelConfig.meleeWidth;
            event.duration = barrelConfig.meleeDuration;
            event.windupDuration = meleeWindup;
            event.recoveryDuration = meleeRecovery;
            event.damage = static_cast<uint32_t>((std::max)(1.0f, static_cast<float>(mountParam.damage) * meleeDamageScale));
            event.color = barrelConfig.effectColor;
            event.comboStep = meleeComboStepForShot;
            player_.pendingMeleeSlashes_.push_back(event);
            meleeActionDuration = (std::max)(meleeActionDuration, meleeWindup + barrelConfig.meleeDuration + meleeRecovery);
        } else {
            if (player_.runModifiers_.enabled) {
                const size_t active = bulletManager->GetBulletCounts().player;
                if (active >= 240)
                    break;
                mountParam.bulletCount = (std::min)(mountParam.bulletCount, static_cast<int>(240 - active));
            }
            mountParam.bulletSpeed *= barrelConfig.projectileSpeedScale;
            player_.attackController_.FireFromMuzzle(muzzle, fireDir, mountParam, BulletOwner::kPlayer);
            if (index < player_.barrels_.size()) {
                player_.barrels_[index].muzzleFlashTimer = player_.kMuzzleFlashDuration;
            }
            player_.SpawnCasing();
        }
        if (index < player_.barrels_.size()) {
            player_.barrels_[index].recoilOffset = 0.22f;
        }
    }

    // 発射間隔は秒。今回の斬撃が終わる時間も含め、選択したグループだけ、または全体の待ち時間を更新する。
    const float reloadTime = (std::max)({baseReload * config.reloadScale * firedReloadScale, meleeActionDuration,
                                         player_.expeditionCombatStyleSelected_ ? 0.05f : 0.0f});
    if (usesGroupCooldowns && selectedGroupSlot >= 0 && static_cast<size_t>(selectedGroupSlot) < player_.weaponGroupCooldowns_.size()) {
        player_.weaponGroupCooldowns_[static_cast<size_t>(selectedGroupSlot)] = reloadTime;
        player_.bulletCoolTime = 0.0f;
    } else {
        player_.bulletCoolTime = reloadTime;
    }
    const float combinedRecoilLength = cg2::Length(combinedRecoil);
    if (combinedRecoilLength > 0.0001f) {
        recoilDir = combinedRecoil * (1.0f / combinedRecoilLength);
        recoilPower = config.recoilPower * combinedRecoilLength;
    } else {
        recoilDir = cg2::Normalize(player_.dir_) * -1.0f;
        recoilPower = 0.0f;
    }
    return true;
}

void PlayerWeapons::RefreshAdditiveArmaments()
{
    if (!player_.runModifiers_.enabled || !player_.runModifiers_.expedition || !player_.expeditionCombatStyleSelected_ ||
        player_.expeditionCombatStyle_ != tankbuild::Style::Shooter || player_.runEvolutionActive_ ||
        player_.runStarterConfig_.barrels.empty())
        return;
    // 初期射撃装備だけを補正する。進化済み装備を上書きせず、描画の再生成が必要な差を保存する。
    const size_t oldCount = player_.runStarterConfig_.barrels.size();
    const bool oldAlternate = player_.runStarterConfig_.alternateBarrels;
    const float oldAngle = player_.runStarterConfig_.barrels.front().angleDeg;
    const int count = 1 + (player_.runModifiers_.extraBarrel1 ? 1 : 0) + (player_.runModifiers_.extraBarrel2 ? 1 : 0);
    const auto prototype = player_.runStarterConfig_.barrels.front();
    player_.runStarterConfig_.barrels.resize(static_cast<size_t>(count), prototype);
    for (int i = 0; i < count; ++i) {
        auto& mount = player_.runStarterConfig_.barrels[static_cast<size_t>(i)];
        // 添字を-1～1へ写して左右の配置・角度を求める。1砲身は中心とし、0除算を避ける。
        const float normalized = count == 1 ? 0.0f : static_cast<float>(i) * 2.0f / static_cast<float>(count - 1) - 1.0f;
        mount.offset = {.72f, normalized * (count == 2 ? .48f : .72f), 0};
        mount.angleDeg = player_.runModifiers_.fanMount ? normalized * (count == 2 ? 8.0f : 14.0f) : 0.0f;
        mount.damageScale = count == 1 ? 1.0f : count == 2 ? .65f : .50f;
        mount.reloadScale = mount.projectileSpeedScale = 1;
        mount.fires = true;
        mount.weaponType = WeaponType::Projectile;
        mount.fireGroup = 0;
    }
    player_.runStarterConfig_.alternateBarrels = count > 1 && player_.runModifiers_.alternatingFire;
    player_.runStarterConfig_.fireAllBarrels = !player_.runStarterConfig_.alternateBarrels;
    // 交互射撃は1回分の一斉射撃を同じ周期へ分散する。発射間隔を砲身数で割り、交互化だけで火力を増やさない。
    player_.runStarterConfig_.reloadScale = player_.runStarterConfig_.alternateBarrels ? 1.0f / static_cast<float>(count) : 1.0f;
    if (player_.object_ && (oldCount != static_cast<size_t>(count) || oldAlternate != player_.runStarterConfig_.alternateBarrels ||
                            oldAngle != player_.runStarterConfig_.barrels.front().angleDeg)) {
        player_.shootBarrelIndex_ = player_.shootGroupIndex_ = 0;
        player_.weaponGroupCooldowns_.clear();
        player_.InitializeBarrels();
        player_.UpdateBarrelLayout();
    }
}

void PlayerWeapons::ResetAdditionalAbilities()
{
    player_.empJammerTimer_ = 0;
    player_.droneChargeCooldown_ = 1.5f;
    player_.droneBombCooldown_ = 4.0f;
    player_.recentDashTimer_ = 0;
    player_.nextMissionDrone_ = 0;
    player_.painterLocks_ = {};
    player_.targetLockVisuals_.clear();
    player_.spinCycle_ = {};
    player_.finisherSpinReady_ = false;
    player_.dashSlashActive_ = false;
    player_.dashSlashTimer_ = 0;
    player_.dashSlashTargets_.clear();
    player_.wallSmashTargets_ = {};
}

bool PlayerWeapons::TryStartSpinBlade(bool pressed)
{
    if (!player_.IsMeleeBuild() || !player_.runModifiers_.spinBlade)
        return false;
    if (!player_.spinCycle_.Start(pressed, player_.finisherSpinReady_, player_.stats_.stamina))
        return false;
    player_.finisherSpinReady_ = false;
    player_.meleeComboStep_ = 0;
    player_.bulletCoolTime = .85f;
    player_.meleeComboTimer_ = 1.4f;
    if (player_.pendingSpecialCombatEvents_.size() < 32)
        player_.pendingSpecialCombatEvents_.push_back({SpecialEventKind::SpinBlade, player_.GetWorldPosition(), player_.dir_, 1});
    return true;
}

uint32_t PlayerWeapons::NotifyDroneHit(int drone, Collider* target, uint32_t originalDamage)
{
    if (!player_.IsDroneBuild() || !player_.runModifiers_.targetPainter || !target || drone < 0)
        return originalDamage;
    auto* regular = dynamic_cast<ExpEnemy*>(target);
    auto* boss = dynamic_cast<Enemy*>(target);
    if ((!regular && !boss) || (regular && (regular->IsDead() || regular->IsRunResource())) || (boss && boss->IsDead()))
        return originalDamage;
    const uint64_t id = target->GetCollisionId();
    tankspecial::PainterLock *entry = nullptr, *free = nullptr;
    // ポインターではなく衝突IDで照合する。ロック・蓄積が切れた枠だけを別の対象へ再利用する。
    for (auto& lock : player_.painterLocks_) {
        if (lock.id == id) {
            entry = &lock;
            break;
        }
        if (!free && (lock.id == 0 || (lock.remaining <= 0 && lock.buildup <= 0)))
            free = &lock;
    }
    if (!entry && free) {
        entry = free;
        *entry = {};
        entry->id = id;
    }
    if (!entry)
        return originalDamage;
    // Hitのtrueは今回のロック成立。蓄積だけの命中でも、後段では現在の倍率でダメージを返す。
    if (entry->Hit(drone)) {
        ++player_.specialCombatStats_.targetLocks;
        if (player_.pendingSpecialCombatEvents_.size() < 32)
            player_.pendingSpecialCombatEvents_.push_back({SpecialEventKind::TargetLock, target->GetWorldPosition(), {}, 1});
    }
    return static_cast<uint32_t>(
        (std::max)(1.0f, std::round(static_cast<float>(originalDamage) *
                                    entry->DamageScale(boss != nullptr, TankEffectPower(player_.runModifiers_, 37)))));
}

float PlayerWeapons::GetDroneTargetDamageScale(const Collider* target, bool boss) const
{
    if (!player_.IsDroneBuild() || !player_.runModifiers_.targetPainter || !target)
        return 1;
    for (const auto& lock : player_.painterLocks_)
        if (lock.id == target->GetCollisionId())
            return lock.DamageScale(boss, TankEffectPower(player_.runModifiers_, 37));
    return 1;
}

void PlayerWeapons::ArmWallSmash(ExpEnemy* target, float strength)
{
    if (!player_.IsMeleeBuild() || !player_.runModifiers_.wallSmash || !target || target->IsDead() || target->IsRunResource())
        return;
    WallSmashTarget* slot = nullptr;
    for (auto& pending : player_.wallSmashTargets_) {
        if (pending.id == target->GetCollisionId()) {
            slot = &pending;
            break;
        }
        if (!slot && pending.seconds <= 0)
            slot = &pending;
    }
    if (slot) {
        // 登録時より壁衝突回数が増えた候補は未処理の衝突を持つ。同じフレームで次の斬撃が当たっても、
        // 壁衝突の処理前に登録時の回数で上書きしない。新しい回数へ更新すると衝突を見失うため。
        if (slot->id == target->GetCollisionId() && slot->seconds > 0 && slot->collision != target->GetWallCollisionCount())
            return;
        *slot = {target->GetCollisionId(), target->GetWallCollisionCount(), 1.0f, (std::clamp)(strength, .5f, 2.0f)};
    }
}

void PlayerWeapons::UpdateAdditionalAbilities(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt)
{
    (void)bullets;
    if (!player_.runModifiers_.droneCharge && !player_.runModifiers_.droneRebuildBomb && !player_.runModifiers_.targetPainter &&
        !player_.runModifiers_.dashSlash && !player_.runModifiers_.spinBlade && !player_.runModifiers_.wallSmash)
        return;
    // 呼び出し元が正の秒数と戦闘中の状態を確認する。遮蔽は統合ブロックとのXY線分交差で調べる。
    auto blocked = [&](const cg2::Vector3& from, const cg2::Vector3& to) {
        for (const auto& block : stage.GetMergedBlocks())
            if (tankspecial::SegmentCrossesBox(from.x, from.y, to.x, to.y, block.aabb.min.x, block.aabb.min.y, block.aabb.max.x,
                                               block.aabb.max.y))
                return true;
        return false;
    };
    auto emit = [&](SpecialEventKind kind, const cg2::Vector3& point, const cg2::Vector3& direction, float power = 1.0f) {
        if (player_.pendingSpecialCombatEvents_.size() < 32)
            player_.pendingSpecialCombatEvents_.push_back({kind, point, direction, power});
    };
    // この処理中だけ敵を借用する。ダメージで死亡状態が変わっても、敵の実体は後の管理側の削除まで残る。
    const auto regulars = enemies ? enemies->GetEnemyPtrs() : std::vector<ExpEnemy*>{};
    std::vector<Collider*> targets;
    targets.reserve((std::min)(size_t{128}, regulars.size() + 1));
    if (boss && !boss->IsDead())
        targets.push_back(boss);
    for (auto* enemy : regulars)
        if (enemy && !enemy->IsDead() && !enemy->IsRunResource() && targets.size() < 128)
            targets.push_back(enemy);
    auto damage = [&](Collider* target, uint32_t amount, const cg2::Vector3& source, bool melee, float knockback = 0.0f) {
        if (target == boss) {
            if (boss && !boss->IsDead()) {
                boss->TakeDamage(amount);
                const auto delta = target->GetWorldPosition() - source;
                if (knockback > 0 && cg2::Length(delta) > .001f)
                    boss->ApplyKnockback(cg2::Normalize(delta), knockback);
            }
        } else if (auto* enemy = dynamic_cast<ExpEnemy*>(target); enemy && !enemy->IsDead()) {
            enemy->TakeDirectionalDamage(amount, source, melee);
            if (knockback > 0) {
                const auto difference = enemy->GetWorldPosition() - source;
                enemy->ApplyKnockback(cg2::Length(difference) > .001f ? cg2::Normalize(difference) : cg2::Vector3{1, 0, 0}, knockback);
                if (melee)
                    ArmWallSmash(enemy, 1);
            }
        }
    };
    // 先に時間を進めて表示情報を作る。この後の突撃命中で成立した新しいロックは次の更新で表示へ反映する。
    for (auto& lock : player_.painterLocks_)
        lock.Advance(dt);
    player_.targetLockVisuals_.clear();
    if (player_.IsDroneBuild()) {
        for (auto* target : targets)
            for (const auto& lock : player_.painterLocks_)
                if (lock.id == target->GetCollisionId() && (lock.remaining > 0 || lock.buildup > 0))
                    player_.targetLockVisuals_.push_back({target->GetWorldPosition(), lock.hits, lock.remaining});
        player_.droneChargeCooldown_ = (std::max)(0.0f, player_.droneChargeCooldown_ - dt);
        player_.droneBombCooldown_ = (std::max)(0.0f, player_.droneBombCooldown_ - dt);
        const bool held =
            !player_.runRoomAwaitInputRelease_ && !player_.ui_->upgradeHudMouseCaptured_ &&
            (player_.demoInputEnabled_ ? player_.demoShoot_ : player_.input_->IsPress(player_.input_->GetMouseState().rgbButtons[0]));
        // 照準に最も近い対象位置を選び、順番待ちの添字から開始可能なドローンを探す。対象を追跡する予約ではない。
        auto launch = [&](bool bomb) {
            if (!held || player_.drones_.empty() || targets.empty())
                return false;
            Collider* chosen = nullptr;
            float best = 1e30f;
            for (auto* target : targets) {
                if (cg2::Length(target->GetWorldPosition() - player_.GetWorldPosition()) > 26)
                    continue;
                const float score = cg2::Length(target->GetWorldPosition() - player_.runAimWorld_);
                if (score < best) {
                    best = score;
                    chosen = target;
                }
            }
            if (!chosen)
                return false;
            for (size_t n = 0; n < player_.drones_.size(); ++n) {
                const size_t index = (player_.nextMissionDrone_ + n) % player_.drones_.size();
                auto& drone = player_.drones_[index];
                if (!drone || drone->IsDead() || blocked(drone->GetWorldPosition(), chosen->GetWorldPosition()))
                    continue;
                if (drone->StartRunMission(chosen->GetWorldPosition(), bomb)) {
                    player_.nextMissionDrone_ = (index + 1) % player_.drones_.size();
                    if (!bomb)
                        ++player_.specialCombatStats_.droneCharges;
                    emit(SpecialEventKind::DroneCharge, drone->GetWorldPosition(), chosen->GetWorldPosition() - drone->GetWorldPosition(),
                         bomb ? 1.5f : .5f);
                    return true;
                }
            }
            return false;
        };
        // 自爆の開始を先に試す。開始成功時だけ待ち時間を更新し、次の通常突撃は別の開始可能な機を探す。
        if (player_.runModifiers_.droneRebuildBomb && player_.droneBombCooldown_ <= 0 && launch(true))
            player_.droneBombCooldown_ = 9.0f;
        if (player_.runModifiers_.droneCharge && player_.droneChargeCooldown_ <= 0 && launch(false))
            player_.droneChargeCooldown_ = 2.8f;
        const auto tuning = MakeTankDroneTuning(player_.runModifiers_, true);
        const auto* config = player_.GetCurrentClassConfig();
        const float base = player_.stats_.bulletDamage * tuning.damageScale * (config ? config->bulletDamageScale : 1.0f);
        for (size_t i = 0; i < player_.drones_.size(); ++i) {
            auto& drone = player_.drones_[i];
            if (!drone || drone->IsDead())
                continue;
            if (drone->ConsumeRunRebuilt()) {
                ++player_.specialCombatStats_.droneRebuilds;
                emit(SpecialEventKind::DroneRebuild, drone->GetWorldPosition(), {}, 1);
            }
            // Arriveが残した到着イベントを1回だけ消費する。時間切れでの帰還では到着イベントは発生しない。
            if (!drone->ConsumeRunMissionImpact())
                continue;
            const bool bomb = drone->GetRunMission().IsBomb();
            const auto origin = drone->GetWorldPosition();
            const float radius = bomb ? 4.5f : 2.2f;
            const float scale =
                bomb ? 8.0f * TankEffectPower(player_.runModifiers_, 36) : 3.0f * TankEffectPower(player_.runModifiers_, 35);
            if (bomb) {
                ++player_.specialCombatStats_.droneBombs;
                emit(SpecialEventKind::DroneBomb, origin, {}, radius);
            }
            for (auto* target : targets) {
                if (cg2::Length(target->GetWorldPosition() - origin) > radius + target->GetRadius() ||
                    blocked(origin, target->GetWorldPosition()))
                    continue;
                const uint32_t amount =
                    NotifyDroneHit(static_cast<int>(i), target,
                                   static_cast<uint32_t>((std::max)(1.0f, std::round(base * scale * (target == boss ? .75f : 1.0f)))));
                damage(target, amount, origin, false, bomb ? .26f : .14f);
                // 通常突撃は条件を満たした先頭1体で止める。自爆は範囲内の遮られない全対象へ適用する。
                if (!bomb) {
                    ++player_.specialCombatStats_.droneChargeHits;
                    emit(SpecialEventKind::DroneCharge, target->GetWorldPosition(), {}, 1);
                    break;
                }
            }
        }
    }
    // ここからは近接系統のみ。ドローン側のロック・待ち時間の更新までを終えてから対象外を戻す。
    if (!player_.IsMeleeBuild())
        return;
    const auto combo = MakeTankMeleeCombo(0, player_.runModifiers_);
    const auto* config = player_.GetCurrentClassConfig();
    const float baseMelee = player_.stats_.bulletDamage * 3.8f * combo.damage * (config ? config->bulletDamageScale : 1.0f);
    if (player_.dashSlashActive_) {
        player_.dashSlashTimer_ -= dt;
        const cg2::Vector3 end = player_.GetWorldPosition() + player_.dashSlashDirection_ * 1.5f;
        // ダッシュの移動区間で接触を拾う。同じダッシュ斬撃の衝突IDは記録し、再び重なっても1回にする。
        for (auto* target : targets) {
            if (std::find(player_.dashSlashTargets_.begin(), player_.dashSlashTargets_.end(), target->GetCollisionId()) !=
                player_.dashSlashTargets_.end())
                continue;
            const auto p = target->GetWorldPosition();
            if (!tankspecial::SegmentTouches(player_.dashSlashPrevious_.x, player_.dashSlashPrevious_.y, end.x, end.y, p.x, p.y,
                                             target->GetRadius() + 1.25f) ||
                blocked(player_.GetWorldPosition(), p))
                continue;
            player_.dashSlashTargets_.push_back(target->GetCollisionId());
            damage(target, player_.dashSlashDamage_, player_.GetWorldPosition(), true, .28f);
            ++player_.specialCombatStats_.dashSlashHits;
            emit(SpecialEventKind::DashSlash, p, player_.dashSlashDirection_, .6f);
        }
        player_.dashSlashPrevious_ = player_.GetWorldPosition();
        if (player_.dashSlashTimer_ <= 0)
            player_.dashSlashActive_ = false;
    }
    // Stepの攻撃時刻ごとに斬撃を予約する。comboStep=-1と発生済みフラグで斬撃波・ジャスト演出を分ける。
    if (player_.runModifiers_.spinBlade && player_.spinCycle_.Step(dt)) {
        MeleeSlashEvent swing{};
        swing.origin = player_.GetWorldPosition();
        swing.direction = {1, 0, 0};
        swing.range = player_.GetCombatStyleProfile(tankbuild::Style::Melee).meleeRange * combo.range * .85f;
        swing.arcDeg = 360;
        swing.windupDuration = 0;
        swing.duration = .18f;
        swing.recoveryDuration = 0;
        swing.comboStep = -1;
        swing.damage = static_cast<uint32_t>((std::max)(1.0f, std::round(baseMelee * .30f * TankEffectPower(player_.runModifiers_, 40))));
        swing.knockback = .07f;
        swing.color = {.8f, .3f, 1.6f, 1};
        player_.pendingMeleeSlashes_.push_back(swing);
        player_.specialMeleeSwing_ = swing;
        player_.specialMeleeElapsed_ = 0;
        player_.specialWaveEmitted_ = true;
        player_.specialPerfectFeedback_ = true;
        player_.specialParriedBullets_.clear();
        ++player_.specialCombatStats_.spinTicks;
    }
    if (player_.runModifiers_.wallSmash)
        for (auto& pending : player_.wallSmashTargets_) {
            if (pending.seconds <= 0)
                continue;
            pending.seconds -= dt;
            for (auto* target : regulars)
                if (target && !target->IsDead() && target->GetCollisionId() == pending.id) {
                    // 壁衝突回数が増えた対象だけを処理し、候補を消費する。周囲への追加ダメージは遮蔽も確認する。
                    if (target->GetWallCollisionCount() == pending.collision)
                        break;
                    pending.seconds = 0;
                    const auto origin = target->GetWorldPosition();
                    const auto amount =
                        static_cast<uint32_t>(tankspecial::WallSmashDamage(baseMelee, TankEffectPower(player_.runModifiers_, 41)));
                    target->TakeDirectionalDamage(amount, origin, true);
                    ++player_.specialCombatStats_.wallSmashes;
                    emit(SpecialEventKind::WallSmash, origin, {}, 2.8f);
                    for (auto* other : targets)
                        if (other != target && cg2::Length(other->GetWorldPosition() - origin) <= 2.8f + other->GetRadius() &&
                            !blocked(origin, other->GetWorldPosition()))
                            damage(other, (std::max)(1u, static_cast<uint32_t>(static_cast<float>(amount) * .4f)), origin, false);
                    break;
                }
        }
}
