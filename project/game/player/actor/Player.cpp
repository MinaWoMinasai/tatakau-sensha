#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "PlayerUiHelpers.h"
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

namespace {
using playerui::ReadVector2Object;
using playerui::WriteVector2Object;
using playerui::SetLabel;
using playerui::Vector4ToJson;

/// @brief 係数を0〜1に制限し、2つのRGBA色を線形補間して返す。
cg2::Vector4 LerpColor(const cg2::Vector4& a, const cg2::Vector4& b, float t)
{
	t = (std::clamp)(t, 0.0f, 1.0f);
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
		a.w + (b.w - a.w) * t
	};
}

/// @brief 機体の種類に対応する画像パスを返す。
const char* ClassTexturePath(ClassType type)
{
	switch (type) {
	case ClassType::Twin: return "resources/twin.png";
	case ClassType::MachineGun: return "resources/machineGun.png";
	case ClassType::Overseer:
	case ClassType::Summoner: return "resources/drone.png";
	default: return "resources/normalTank.png";
	}
}

/// @brief 強化HUDの項目名一覧を返す。
const std::array<const char*, 7>& UpgradeHudNames()
{
	static const std::array<const char*, 7> names = {
		"自動回復",
		"最大HP",
		"体当たり",
		"弾速",
		"弾ダメージ",
		"リロード",
		"移動速度"
	};
	return names;
}

/// @brief 強化HUDの行ごとの表示色を返す。
const std::array<cg2::Vector4, 7>& UpgradeHudRowColors()
{
	// diepio風の能力ごとの色分け。ゲーム内のネオン表現と衝突しないよう、
	// 発光は塗り全体ではなく、セルと細い外周に限定する。
	static const std::array<cg2::Vector4, 7> colors = {{
		{ 0.90f, 0.36f, 0.86f, 1.0f }, // 自動回復
		{ 0.67f, 0.36f, 0.95f, 1.0f }, // 最大HP
		{ 0.49f, 0.39f, 0.98f, 1.0f }, // 体当たり
		{ 0.35f, 0.58f, 1.00f, 1.0f }, // 弾速
		{ 1.00f, 0.86f, 0.24f, 1.0f }, // 弾ダメージ
		{ 1.00f, 0.38f, 0.42f, 1.0f }, // リロード
		{ 0.32f, 1.00f, 0.56f, 1.0f }  // 移動速度
	}};
	return colors;
}

/// @brief 強化HUD区切り外観を作成して返す。
NeonSegmentedBarStyle MakeUpgradeHudSegmentStyle(int index)
{
	const cg2::Vector4 baseColor = UpgradeHudRowColors()[(std::clamp)(index, 0, 6)];
	NeonSegmentedBarStyle style{};
	// 外周の半円部にも未取得セルと同じ不透明な色を入れ、端だけが薄く
	// 見えないようにする。
	style.backgroundColor = {
		baseColor.x * 0.16f,
		baseColor.y * 0.16f,
		baseColor.z * 0.16f,
		0.98f
	};
	style.emptyColor = {
		baseColor.x * 0.16f,
		baseColor.y * 0.16f,
		baseColor.z * 0.16f,
		0.92f
	};
	style.filledColor = baseColor;
	style.outlineColor = {
		baseColor.x * 0.82f + 0.12f,
		baseColor.y * 0.82f + 0.12f,
		baseColor.z * 0.82f + 0.12f,
		0.94f
	};
	// 連続バーと同じく、丸端の外枠より内側へセルを収める。
	// 枠へ重ねないため、端部のはみ出し・細い線の乱れを防ぐ。
	style.innerPadding = 3.0f;
	style.segmentGap = 1.5f;
	style.bloomBrightness = 1.55f;
	style.bloomAlpha = 0.58f;
	style.backdropBloomBrightness = 1.70f;
	style.backdropBloomAlpha = 0.18f;
	style.roundedFrame = true;
	return style;
}

/// @brief 強化HUD下端バー文字外観を作成して返す。
cg2::TextStyle MakeUpgradeHudBottomBarTextStyle()
{
	cg2::TextStyle style{};
	style.fontFamily = "Meiryo";
	style.fontSize = 15.0f;
	style.color = { 0.90f, 0.94f, 1.0f, 1.0f };
	style.outlineColor = { 0.0f, 0.0f, 0.0f, 0.92f };
	style.outlineThickness = 1.0f;
	style.padding = 3.0f;
	style.preserveOutline = true;
	return style;
}

/// @brief 強化HUDSmall文字外観を作成して返す。
cg2::TextStyle MakeUpgradeHudSmallTextStyle()
{
	cg2::TextStyle style{};
	style.fontFamily = "Meiryo";
	style.fontSize = 15.0f;
	style.color = { 0.90f, 0.94f, 1.0f, 1.0f };
	style.outlineColor = { 0.0f, 0.03f, 0.05f, 0.95f };
	style.outlineThickness = 2.0f;
	style.padding = 5.0f;
	return style;
}

/// @brief 強化HUD重ね表示文字外観を作成して返す。
cg2::TextStyle MakeUpgradeHudOverlayTextStyle()
{
	cg2::TextStyle style = MakeUpgradeHudSmallTextStyle();
	style.outlineColor = { 0.0f, 0.0f, 0.0f, 0.92f };
	style.outlineThickness = 1.0f;
	style.padding = 3.0f;
	style.preserveOutline = true;
	return style;
}

/// @brief 強化HUD文字進行を返す。
float GetUpgradeHudTextAdvance(const cg2::TextLabel* label, const cg2::TextStyle& style)
{
	if (!label || !label->GetSprite()) {
		return style.fontSize * 0.55f;
	}
	const float padding = std::ceil(style.padding + style.outlineThickness);
	const float fallback = style.fontSize * (label->GetText() == " " ? 0.34f : 0.55f);
	return (std::max)(fallback, label->GetSprite()->GetSize().x - padding * 2.0f);
}

}

Player::~Player() {

}

void Player::Attack(BulletManager* bulletManager, float deltaTime)
{
    // 部屋移動後の入力解除待ちでは、射撃だけでなく発射間隔の時計も進めない。
    if (runModifiers_.enabled && runRoomAwaitInputRelease_)
        return;

    // 発射間隔は秒で管理する。砲身グループ別の待ち時間も同じ更新で進める。
    bulletCoolTime = (std::max)(0.0f, bulletCoolTime - deltaTime);
    for (float& cooldown : weaponGroupCooldowns_) {
        cooldown = (std::max)(0.0f, cooldown - deltaTime);
    }

    bool wantsPrimaryAttack = demoInputEnabled_ ? demoShoot_ : input_->IsPress(input_->GetMouseState().rgbButtons[0]);
#if defined(USE_IMGUI) && !defined(NDEBUG)
    wantsPrimaryAttack = wantsPrimaryAttack || debugAutoFireEnabled_;
#endif
    if (runModifiers_.enabled && runModifiers_.railCannon && expeditionCombatStyleSelected_ &&
        expeditionCombatStyle_ == tankbuild::Style::Shooter) {
        AttackRailCannon(bulletManager, wantsPrimaryAttack && !upgradeHudMouseCaptured_, deltaTime);
        return;
    }
    if (wantsPrimaryAttack && !upgradeHudMouseCaptured_) {
        // ドローン装備の射撃はPlayer::Update内の各ドローンが行うため、本体からは撃たない。
        if (IsDroneBuild())
            return;
        if (IsMeleeBuild()) {
            if (spinCycle_.remaining > 0)
                return;
            if (bulletCoolTime > 0.0f)
                return;
            if (TryStartSpinBlade(wantsPrimaryAttack))
                return;
            const int step = meleeComboTimer_ > 0.0f ? meleeComboStep_ : 0;
            const auto combo = MakeTankMeleeCombo(step, runModifiers_);
            const auto* config = GetCurrentClassConfig();
            const auto& profile = GetCombatStyleProfile(tankbuild::Style::Melee);
            // リロード補正を斬撃の各段階へ共通に掛け、準備・有効・硬直の合計を最低0.05秒にする。
            const float timing =
                (std::max)(0.05f / (combo.windup + combo.duration + combo.recovery),
                           stats_.reloadSpeed / GetRunBaseReloadFrames() * (config ? config->reloadScale : 1.0f) *
                               GetRunFireIntervalScale() * (expeditionCombatStyleSelected_ ? profile.attackIntervalSeconds / 0.33f : 1.0f));
            MeleeSlashEvent event{};
            event.origin = GetWorldPosition();
            event.direction = cg2::Length(dir_) > 0.001f ? cg2::Normalize(dir_) : cg2::Vector3{1, 0, 0};
            event.range = (expeditionCombatStyleSelected_ ? profile.meleeRange : 4.3f) * combo.range;
            event.arcDeg = combo.arc;
            event.width = step == 2 ? 0.38f : 0.26f;
            event.windupDuration = combo.windup * timing;
            event.duration = combo.duration * timing;
            event.recoveryDuration = combo.recovery * timing;
            event.comboStep = step;
            event.knockback = combo.knockback * (expeditionCombatStyleSelected_ ? profile.meleeKnockback / 0.16f : 1.0f);
            event.damage = static_cast<uint32_t>(
                (std::max)(1.0f, std::round(stats_.bulletDamage * 3.8f * combo.damage * (config ? config->bulletDamageScale : 1.0f))));
            event.color = step == 2 ? cg2::Vector4{1.8f, 0.85f, 0.25f, 1.0f} : cg2::Vector4{0.25f, 1.50f, 1.75f, 1.0f};
            const bool dashSlash = tankspecial::CanDashSlash(runModifiers_.dashSlash, isDashing_, recentDashTimer_) && !dashSlashActive_;
            if (dashSlash) {
                // 移動軌跡への命中はUpdateAdditionalAbilitiesで処理する。通常斬撃の予約とは分ける。
                event.comboStep = 0;
                event.windupDuration = 0;
                event.duration = .18f;
                event.recoveryDuration = .12f;
                event.direction = cg2::Length(velocity_) > .001f ? cg2::Normalize(velocity_) : event.direction;
                dashSlashActive_ = true;
                dashSlashTimer_ = .22f;
                dashSlashPrevious_ = GetWorldPosition();
                dashSlashDirection_ = event.direction;
                dashSlashDamage_ = static_cast<uint32_t>(
                    (std::max)(1.0f, std::round(stats_.bulletDamage * 3.8f * MakeTankMeleeCombo(0, runModifiers_).damage *
                                                (config ? config->bulletDamageScale : 1.0f) * 1.2f * TankEffectPower(runModifiers_, 39))));
                dashSlashTargets_.clear();
                velocity_ += event.direction * .28f;
                recentDashTimer_ = 0;
                ++specialCombatStats_.dashSlashes;
                if (pendingSpecialCombatEvents_.size() < 32)
                    pendingSpecialCombatEvents_.push_back({SpecialEventKind::DashSlash, event.origin, event.direction, event.range});
            } else
                pendingMeleeSlashes_.push_back(event);
            // 新しい振りごとにパリィ履歴を解除し、同じ弾への再処理と斬撃波の重複生成を防ぐ。
            specialMeleeSwing_ = event;
            specialMeleeElapsed_ = 0;
            specialWaveEmitted_ = false;
            specialPerfectFeedback_ = false;
            specialParriedBullets_.clear();
            meleeComboStep_ = dashSlash ? 1 : (step + 1) % 3;
            finisherSpinReady_ = !dashSlash && step == 2;
            bulletCoolTime = event.windupDuration + event.duration + event.recoveryDuration;
            meleeComboTimer_ = bulletCoolTime + 0.65f;
            // 斬撃方向の速度を加える。位置は通常の移動処理で更新し、操作による方向転換も受け付ける。
            velocity_ += event.direction * (step == 2 ? 0.055f : 0.025f);
            primaryAttackPerformedEvent_ = true;
            ++primaryAttackCount_;
            return;
        }

        if (const PlayerClassConfig* config = GetCurrentClassConfig()) {
            const float baseReload =
                (isBuffActive_ ? (stats_.reloadSpeed * 0.7f) / 60.0f : stats_.reloadSpeed / 60.0f) * GetRunFireIntervalScale();
            cg2::Vector3 recoilDir = cg2::Normalize(dir_) * -1.0f;
            float recoilPower = 0.01f;
            if (FireConfiguredClass(*config, bulletManager, baseReload, recoilDir, recoilPower)) {
                // 遠征のドローン生成では射撃を記録しない。Player::Updateでドローン射撃による弾数増加を記録する。
                if (!runCheckpointEvolution_ || !config->usesDrone) {
                    primaryAttackPerformedEvent_ = true;
                    ++primaryAttackCount_;
                }
                velocity_ += recoilDir * recoilPower;
                if (!runCheckpointEvolution_)
                    cg2::Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
            }
            return;
        }

        // 現在の機体設定がない場合だけ、旧機体ごとの互換射撃へ進む。
        if (bulletCoolTime <= 0.0f) {

            // 発射位置
            cg2::Vector3 origin = GetWorldPosition();

            // 攻撃パラメータを設定
            AttackParam param{};
            param.bulletSpeed = stats_.bulletSpeed;
            param.bulletCount = 1;
            param.spreadAngleDeg = 5.0f;
            param.randomSpread = false;

            param.reflect = false;
            param.penetrate = false;
            param.cooldown = 1.0f;
            param.damage = static_cast<uint32_t>(stats_.bulletDamage);
            ApplyRunProjectileRules(param);
            bool firedByClass = false;

            cg2::Vector3 recoilDir = cg2::Normalize(dir_) * -1.0f;
            float recoilPower = 0.01f; // 射撃方向と逆向きに加える速度の大きさ。

            // 個別にクールタイムを設定するために先に設定
            float baseReload =
                (isBuffActive_ ? (stats_.reloadSpeed * 0.7f) / 60.0f : stats_.reloadSpeed / 60.0f) * GetRunFireIntervalScale();
            bulletCoolTime = baseReload;

            switch (currentClass_) {
            case ClassType::Basic:
                // 既存の単発攻撃
                param.spreadAngleDeg = 10.0f;
                param.randomSpread = true;
                bulletCoolTime = baseReload;
                if (isBuffActive_) {
                    param.reflect = true;
                    bulletCoolTime = baseReload * 0.6f;
                    param.spreadAngleDeg = 20.0f;
                }
                attackController_.Fire(origin, dir_, param, BulletOwner::kPlayer);
                firedByClass = true;
                if (!barrels_.empty()) {
                    barrels_[0].recoilOffset = 0.22f;
                    barrels_[0].muzzleFlashTimer = kMuzzleFlashDuration;
                }

                SpawnCasing();
                break;

            case ClassType::Twin: {
                param.spreadAngleDeg = 2.0f;
                param.randomSpread = true;
                float offsetValue = 0.6f;                        // 砲身の横幅
                cg2::Vector3 rightDir = {-dir_.y, dir_.x, 0.0f}; // dir_に垂直なベクトル（右方向）

                if (shootBarrelIndex_ == 0) {
                    // 左から発射
                    attackController_.Fire(origin - rightDir * offsetValue, dir_, param, BulletOwner::kPlayer);
                    firedByClass = true;
                    if (!barrels_.empty()) {
                        barrels_[0].recoilOffset = 0.22f;
                        barrels_[0].muzzleFlashTimer = kMuzzleFlashDuration;
                    }
                    shootBarrelIndex_ = 1; // 次は右
                } else {
                    // 右から発射
                    attackController_.Fire(origin + rightDir * offsetValue, dir_, param, BulletOwner::kPlayer);
                    firedByClass = true;
                    if (barrels_.size() > 1) {
                        barrels_[1].recoilOffset = 0.22f;
                        barrels_[1].muzzleFlashTimer = kMuzzleFlashDuration;
                    }
                    shootBarrelIndex_ = 0; // 次は左
                }

                // 次の砲身を撃つまでの待ち時間を、基準発射間隔の1/2.5にする。
                bulletCoolTime = baseReload / 2.5f;
            } break;

            case ClassType::MachineGun:
                // 角度をランダムにずらす
                param.spreadAngleDeg = 30.0f;
                param.randomSpread = true;
                attackController_.Fire(origin, dir_, param, BulletOwner::kPlayer);
                firedByClass = true;
                if (!barrels_.empty()) {
                    barrels_[0].muzzleFlashTimer = kMuzzleFlashDuration;
                }
                // リロード補正0.6倍
                bulletCoolTime = baseReload * 0.6f;
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
                isStealth_ = false; // 撃ったら解除
                stealthTimer_ = 0.0f;
                break;

            case ClassType::Ninja:
                // Ninjaの互換射撃条件。1発と拡散角15度を設定する。
                param.bulletCount = 1;
                param.spreadAngleDeg = 15.0f;
                // この互換射撃ではステルスを解除しない。
                break;
            }

            if (!firedByClass && currentClass_ != ClassType::Smasher) {
                attackController_.Fire(origin, dir_, param, BulletOwner::kPlayer);
                if (!barrels_.empty()) {
                    barrels_[0].muzzleFlashTimer = kMuzzleFlashDuration;
                }
                SpawnCasing();
            }

            if (firedByClass || currentClass_ != ClassType::Smasher) {
                primaryAttackPerformedEvent_ = true;
                ++primaryAttackCount_;
            }
            velocity_ += recoilDir * recoilPower;
            if (!runCheckpointEvolution_)
                cg2::Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.6f);
        }
    } else if (!wantsPrimaryAttack)
        finisherSpinReady_ = false;
}

bool Player::ConsumePrimaryAttackPerformedEvent()
{
    const bool performed = primaryAttackPerformedEvent_;
    primaryAttackPerformedEvent_ = false;
    return performed;
}

bool Player::ConsumeDashStartedEvent()
{
	const bool started = dashStartedEvent_;
	dashStartedEvent_ = false;
	return started;
}

void Player::DroneShoot(BulletManager* BulletManager)
{

    // 旧ドローン生成の待ち時間は呼び出しごとに1を減らす。Attackの秒単位の更新とは異なる。
    bulletCoolTime--;

    const PlayerClassConfig* config = GetCurrentClassConfig();
    int droneLimit = (std::max)(0, config ? config->maxDrones : 7);
    if (runModifiers_.enabled) {
        droneLimit = TankExpeditionDroneLimit(runEvolutionActive_ ? FindTankExpeditionSpecialization(runEvolutionConfig_.id) : nullptr,
                                              droneLimit, runModifiers_.drones, runModifiers_.core == TankRunCore::Drone);
    }
    const size_t maxDrones = static_cast<size_t>(droneLimit);
    if (drones_.size() >= maxDrones) {
        bulletCoolTime = 0.0f;
        return;
    }

    if (bulletCoolTime <= 0.0f) {

        // 生成したドローンへ渡す初期速度。
        const float kBulletSpeed = 0.2f;
        cg2::Vector3 velocity = dir_ * kBulletSpeed;

        auto drone = std::make_unique<PlayerDrone>();
        drone->Initialize(dir_ * 0.3f + worldTransform_.translate, velocity);
        drone->SetAttackControllerBulletManager(BulletManager);
        ConfigureRunDrone(*drone);
        drones_.push_back(std::move(drone));

        bulletCoolTime = 1.0f;
    }
}

void Player::Smash(float deltaTime)
{

    if (isSmash_) {
        return;
    }

    // キーを離したら突撃する
    if (input_->IsMomentRelease(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
        smashDir_ = dir_;
        isSmash_ = true;
        cg2::Vector3 recoilDir = cg2::Normalize(smashDir_);
        float recoilPower = cg2::EaseInQuad(smashCharge_) / 2.0f; // 蓄積量から突進速度の大きさを求め、次の行で上限を0.7に制限する。
        recoilPower = std::min(recoilPower, 0.7f);
        velocity_ = recoilDir * recoilPower;
        return;
    }

    if (input_->IsPress(input_->GetMouseState().rgbButtons[0])) {

        if (isBuffActive_) {
            smashCharge_ += deltaTime * 5.0f;
        } else {
            smashCharge_ += deltaTime;
        }
        if (smashCharge_ >= maxCharge_) {
            smashCharge_ = maxCharge_;
        }
    }
}

cg2::Sphere Player::GetSphere() const
{
	cg2::Sphere s{};
	s.center = GetWorldPosition();

	// 半径は「横幅基準」が安定
	s.radius = kRadius;

	return s;
}

void Player::AddExp(int amount)
{
	if(runCurrencyMode_) {
		runCurrencyEarned_=(std::min)(1000000,runCurrencyEarned_+tankcontent::CreditsFromExperience(amount));
		return;
	}
	if (level_ >= kMaxLevel) return;

	exp_ += amount;
	// レベルアップ判定（余剰分も考慮してループ）
	while (exp_ >= nextLevelExp_) {
		const int previousRank = GetRankFromLevel(level_);
		exp_ -= nextLevelExp_;
		level_++;
		skillPoints_++;

		// 次の必要経験値を再計算（例: レベル * 100 + 補正）
		nextLevelExp_ = GetNextLevelExp();

		if (arenaUiEnabled_ && GetRankFromLevel(level_) > previousRank && !(runModifiers_.enabled && runCheckpointEvolution_)) {
			isChangeMode = true;
			// AddExpはPlayer::Update後の衝突処理から呼ばれる場合があるため、
			// 同じフレームの初回描画より先に遅延フォント更新を完了させる。
			PrepareStaticEvolutionTextTextures();
			PrepareEvolutionCircuitTextTextures();
		}

		if (level_ >= kMaxLevel) {
			exp_ = nextLevelExp_; // カンスト表示用
			break;
		}
	}
}

void Player::SetRunCurrencyMode(bool enabled)
{
	if(runCurrencyMode_==enabled)return;
	runCurrencyMode_=enabled;runCurrencyEarned_=0;
	if(enabled){level_=1;exp_=0;skillPoints_=0;nextLevelExp_=GetNextLevelExp();isChangeMode=false;}
}

void Player::InstallRunAuthoredClasses(const tankcontent::Catalog& catalog)
{
	std::string error;if(!tankcontent::ValidateCatalog(catalog,error))return;
	runAuthoredClasses_.clear();runAuthoredChoices_.clear();runAuthoredStyles_.clear();
	for(const auto& authored:catalog.players){
		const auto* base=GetClassConfig(authored.baseClass);
		if(!base||base->barrels.empty())continue;
		PlayerClassConfig config=*base;
		config.id=authored.id;config.displayName=authored.name;config.requiredRank=1;
		config.reloadScale=authored.reloadScale;config.bulletDamageScale=authored.damageScale;
		config.bulletSpeedScale=authored.bulletSpeedScale;config.bulletCount=1;
		config.usesDrone=authored.style==tankbuild::Style::Drone;config.maxDrones=authored.drones;
		config.reflect=authored.reflect;config.penetrate=authored.penetrate;
		config.alternateBarrels=authored.alternate;config.fireAllBarrels=!authored.alternate;
		config.randomSpread=false;config.spreadAngleDeg=10;
		config.bodyShape=static_cast<BodyShape>(authored.bodyShape);
		config.bodyOutlineColor={authored.color[0],authored.color[1],authored.color[2],authored.color[3]};
		config.bodyFillColor={authored.color[0]*0.12f,authored.color[1]*0.12f,authored.color[2]*0.12f,0.65f};
		const WeaponMountConfig prototype=base->barrels.front();
		config.barrels.assign(static_cast<std::size_t>(authored.barrels),prototype);
		for(std::size_t i=0;i<config.barrels.size();++i){auto& barrel=config.barrels[i];const float position=static_cast<float>(i)-static_cast<float>(config.barrels.size()-1)*0.5f;
			barrel.offset={0.72f,position*0.50f,0};barrel.angleDeg=position*authored.fanAngle;
			barrel.fires=true;barrel.weaponType=WeaponType::Projectile;barrel.fireGroup=0;
			barrel.reloadScale=1;barrel.damageScale=1;barrel.projectileSpeedScale=1;
		}
		runAuthoredStyles_[authored.id]=authored.style;
		runAuthoredClasses_.emplace(authored.id,std::move(config));
		runAuthoredChoices_.push_back({authored.id,authored.name,authored.description});
	}
	if(runEvolutionActive_&&runAuthoredEvolutionActive_) {
		const auto active=runAuthoredClasses_.find(runEvolutionConfig_.id);
		const auto style=runAuthoredStyles_.find(runEvolutionConfig_.id);
		if(active!=runAuthoredClasses_.end()&&(!expeditionCombatStyleSelected_||
			(style!=runAuthoredStyles_.end()&&style->second==expeditionCombatStyle_))) {
			runEvolutionConfig_=active->second;
			InitializeBarrels();UpdateBarrelLayout();EnsureExpeditionDrones();
			for(auto& drone:drones_)ConfigureRunDrone(*drone);
		}
	}
}

std::vector<RunEvolutionChoice> Player::GetRunAuthoredEvolutionChoices() const
{
	if(!runModifiers_.enabled||isDead_)return {};
	std::vector<RunEvolutionChoice> result;
	for(const auto& choice:runAuthoredChoices_) {
		const auto style=runAuthoredStyles_.find(choice.id);
		if(expeditionCombatStyleSelected_ && (style==runAuthoredStyles_.end() || style->second!=expeditionCombatStyle_))continue;
		if(!runEvolutionActive_||choice.id!=runEvolutionConfig_.id)result.push_back(choice);
	}
	return result;
}

bool Player::ChooseRunAuthoredClass(const std::string& id)
{
	if(!runModifiers_.enabled||isDead_)return false;
	const auto it=runAuthoredClasses_.find(id);if(it==runAuthoredClasses_.end())return false;
	const auto style=runAuthoredStyles_.find(id);
	if(expeditionCombatStyleSelected_ && (style==runAuthoredStyles_.end() || style->second!=expeditionCombatStyle_))return false;
	const int previousHp=hp_;
	runEvolutionConfig_=it->second;runEvolutionActive_=true;runAuthoredEvolutionActive_=true;runEvolutionPrepared_=false;
	railCharge_.Reset();specialMeleeElapsed_=-1;specialParriedBullets_.clear();droneLaserLinks_.clear();pendingSpecialCombatEvents_.clear();
	bulletCoolTime=0;shootBarrelIndex_=0;shootGroupIndex_=0;weaponGroupCooldowns_.clear();
	drones_.clear();runSupportDroneTimer_=0;isChangeMode=false;
	RecalculateStatsFromBase(false);hp_=(std::clamp)(previousHp,0,GetMaxHp());
	InitializeBarrels();UpdateBarrelLayout();EnsureExpeditionDrones();evolutionConfirmedEvent_=true;
	return true;
}

bool Player::RequestSlow()
{
	if (requestSlow_) {
		requestSlow_ = false;
		return true;
	}
	return false;
}


void Player::RotateToMouse(cg2::Camera* viewProjection) {
    if (demoInputEnabled_) {
        runAimWorld_=demoAim_;
        const cg2::Vector3 offset=demoAim_-worldTransform_.translate;
        if(cg2::Length(offset)>0.001f) dir_=cg2::Normalize(offset);
        angle_=std::atan2(dir_.y,dir_.x); worldTransform_.rotate.z=angle_;
        object_->SetRotate(worldTransform_.rotate); return;
    }
	// --- 1. マウス座標取得 ---
	POINT mousePosition;
	GetCursorPos(&mousePosition);
	HWND hwnd = cg2::WinApp::GetInstance()->GetHwnd();
	ScreenToClient(hwnd, &mousePosition);

	// --- 2. 逆変換用の行列を準備 ---
	cg2::Matrix4x4 matViewport = cg2::MakeViewportMatrix(0, 0, cg2::WinApp::kClientWidth, cg2::WinApp::kClientHeight, 0, 1);
	cg2::Matrix4x4 matVPV = viewProjection->GetViewMatrix() * viewProjection->GetProjectionMatrix() * matViewport;
	cg2::Matrix4x4 matInverseVPV = cg2::Inverse(matVPV);

	// --- 3. マウス座標をワールドに変換 ---
	cg2::Vector3 posNear = cg2::Vector3((float)mousePosition.x, (float)mousePosition.y, 0);
	cg2::Vector3 posFar = cg2::Vector3((float)mousePosition.x, (float)mousePosition.y, 1);

	posNear = cg2::TransformMatrix(posNear, matInverseVPV);
	posFar = cg2::TransformMatrix(posFar, matInverseVPV);

	// --- 4. レイと Z=0 平面の交差 ---
	cg2::Vector3 mouseDirection = posFar - posNear;
	cg2::Vector3 rayDir = cg2::Normalize(mouseDirection);
	float t = -posNear.z / rayDir.z;
	cg2::Vector3 target = posNear + rayDir * t;
	runAimWorld_ = target;

	// --- 5. プレイヤーの位置と方向ベクトル ---
	cg2::Vector3 playerPos = worldTransform_.translate;
	cg2::Vector3 targetPos = target - playerPos;
	dir_ = cg2::Normalize(targetPos);

	// --- 6. 回転角度を算出 ---
	angle_ = atan2(dir_.y, dir_.x);
	worldTransform_.rotate.z = angle_;
	object_->SetRotate(worldTransform_.rotate);
}

void Player::Initialize(cg2::Object3d* object, const cg2::Vector3& position, bool arenaUi) {
    cg2::StartupTrace::Scope scope("Player.Initialize");
	wchar_t startupCacheFlag[8]{};
	const bool baseline = GetEnvironmentVariableW(L"CG2_STARTUP_CACHE", startupCacheFlag, 8) > 0 && startupCacheFlag[0] == L'0';
	arenaUiEnabled_ = arenaUi || baseline;

	sprite = std::make_unique<cg2::Sprite>();
	sprite->Initialize(cg2::SpriteCommon::GetInstance(), "resources/fade.png");
	sprite->SetColor(cg2::Vector4(1.0f, 1.0f, 1.0f, 0.8f));

	object_ = object;
	baseVehicleColor_ = object_->GetColor();

	worldTransform_ = cg2::InitWorldTransform();
	worldTransform_.translate = position;
	object_->SetTransform(worldTransform_);
	object_->Update();
	if (!LoadPlayerClassConfigs()) {
		classCatalog_.ResetToDefaults();
		std::cerr << "[PlayerClass] Using built-in defaults." << std::endl;
	}
	InitializeBarrels();
	UpdateBarrelLayout();

	// シングルトンインスタンス
	input_ = cg2::Input::GetInstance();

	// 衝突属性を設定
	SetCollisionAttribute(kCollisionAttributePlayer);
	// 衝突対象を自分の属性以外に設定
	SetCollisionMask(kCollisionAttributeEnemy | kCollisionAttributeExpEnemy | kCollisionAttributeEnemyBullet);
	SetDamage(static_cast<uint32_t>(stats_.bodyDamage));

	level_ = 1;
	exp_ = 0;
	nextLevelExp_ = GetNextLevelExp();
	hp_ = GetMaxHp();

	machineGunBtnSprite_ = std::make_unique<cg2::Sprite>();
	machineGunBtnSprite_->Initialize(cg2::SpriteCommon::GetInstance(), "resources/white512x512.png");
	machineGunBtnSprite_->SetPosition({ btnPos_ });
	machineGunBtnSprite_->SetSize({ btnSize_ });

	LoadEvolutionUiStyle();
	if (arenaUiEnabled_) {
		InitializeEncyclopedia();
		InitializeStaticEvolutionPrototype();
		InitializeEvolutionCircuitPrototype();
		InitializeUpgradeHud();
		LoadUpgradeHudConfig();
		ApplyUpgradeHudLayout();
		PrepareUpgradeHudSegmentBars();
		PrepareUpgradeHudTextTextures();
	} else {
		// Keep authored progression rules available for combat/evolution logic.
		// Only the legacy arena's invisible UI is omitted, never deferred.
		LoadEvolutionCircuitTree();
		cg2::StartupTrace::Count("player.unusedArenaUiSkipped");
	}

}

void Player::Update(
	cg2::Camera* viewProjection,
	Stage& stage,
	BulletManager* BulletManager,
	float deltaTime,
	float uiDeltaTime)
{
	sprite->Update();

	POINT mousePos;
	GetCursorPos(&mousePos);
	ScreenToClient(cg2::WinApp::GetInstance()->GetHwnd(), &mousePos);
	mousePosition_ = { static_cast<float>(mousePos.x), static_cast<float>(mousePos.y) };
	// UIにはスローの影響を受けない時間を渡す。操作による開閉も同じフレーム内で判定する。
	const bool evolutionUiWasOpen = isChangeMode;
	if(!demoInputEnabled_ && arenaUiEnabled_) { UpdateEncyclopedia(uiDeltaTime); UpdateUpgradeHud(uiDeltaTime); }

	if (arenaUiEnabled_ && !demoInputEnabled_ && !(runModifiers_.enabled && runCheckpointEvolution_) &&
		input_->IsTrigger(input_->GetKey()[DIK_C], input_->GetPreKey()[DIK_C])) {
		if (isChangeMode) {
			isChangeMode = false;
			evolutionCancelledEvent_ = true;
		} else {
			isChangeMode = true;
			PrepareStaticEvolutionTextTextures();
			PrepareEvolutionCircuitTextTextures();
		}
	}
	// 進化UIを操作したクリックやキー入力を、そのまま射撃・移動へ流さない。
	// 確定やキャンセルでこのフレーム中に閉じた場合も、次フレームまでゲーム入力を抑止する。
	if (evolutionUiWasOpen || isChangeMode) {
		machineGunBtnSprite_->Update();
		return;
	}
	if (runModifiers_.enabled && runRoomAwaitInputRelease_ &&
		!input_->IsPress(input_->GetMouseState().rgbButtons[0]) &&
		!input_->IsPress(input_->GetMouseState().rgbButtons[1])) {
		runRoomAwaitInputRelease_ = false;
	}

	if (!demoInputEnabled_ && !runModifiers_.enabled && skillPoints_ > 0) {
		if (input_->IsTrigger(input_->GetKey()[DIK_1], input_->GetPreKey()[DIK_1])) ApplyStatUpgrade(0);
		if (input_->IsTrigger(input_->GetKey()[DIK_2], input_->GetPreKey()[DIK_2])) ApplyStatUpgrade(1);
		if (input_->IsTrigger(input_->GetKey()[DIK_3], input_->GetPreKey()[DIK_3])) ApplyStatUpgrade(2);
		if (input_->IsTrigger(input_->GetKey()[DIK_4], input_->GetPreKey()[DIK_4])) ApplyStatUpgrade(3);
		if (input_->IsTrigger(input_->GetKey()[DIK_5], input_->GetPreKey()[DIK_5])) ApplyStatUpgrade(4);
		if (input_->IsTrigger(input_->GetKey()[DIK_6], input_->GetPreKey()[DIK_6])) ApplyStatUpgrade(5);
		if (input_->IsTrigger(input_->GetKey()[DIK_7], input_->GetPreKey()[DIK_7])) ApplyStatUpgrade(6);
	}

	if (runModifiers_.enabled) {
		runDashAttackTimer_ = (std::max)(0.0f, runDashAttackTimer_ - deltaTime);
		runOverdriveTimer_ = (std::max)(0.0f, runOverdriveTimer_ - deltaTime);
		runOverdriveCooldown_ = (std::max)(0.0f, runOverdriveCooldown_ - deltaTime);
	}

	// バフタイマーの更新
	if (buffTimer_ > 0.0f) {
		buffTimer_ -= deltaTime;
		SpawnBuffParticle();
		if (buffTimer_ <= 0.0f) {
			isBuffActive_ = false;
		}
	}

	dt_ = deltaTime;
	empJammerTimer_=(std::max)(0.0f,empJammerTimer_-deltaTime);
	recentDashTimer_=(std::max)(0.0f,recentDashTimer_-deltaTime);

	invincibleTimer_ -= deltaTime;
	if (damageFeedbackTimer_ > 0.0f) {
		damageFeedbackTimer_ = (std::max)(0.0f, damageFeedbackTimer_ - deltaTime);
	}
	if (meleeComboTimer_ > 0.0f) {
		meleeComboTimer_ = (std::max)(0.0f, meleeComboTimer_ - deltaTime);
	} else {
		meleeComboStep_ = 0;
	}

	if (dashTimer_ > 0.0f) {
		dashTimer_ -= deltaTime;
		if (dashTimer_ <= 0.0f) {
			isDashing_ = false;
			isJustEvaded_ = false;
		}
	}

	if (dashCooldown_ > 0.0f) {
		dashCooldown_ -= deltaTime;
	}

	// 右クリックは機体ごとの特殊行動スロットとして扱う。
	if (runModifiers_.enabled) {
		// Read the current direction before a dash; a newly pressed movement
		// key should redirect the boost immediately, not on the following frame.
		inputDir_ = {};
		if (input_->IsPress(input_->GetKey()[DIK_A])) inputDir_.x -= 1.0f;
		if (input_->IsPress(input_->GetKey()[DIK_D])) inputDir_.x += 1.0f;
		if (input_->IsPress(input_->GetKey()[DIK_W])) inputDir_.y += 1.0f;
		if (input_->IsPress(input_->GetKey()[DIK_S])) inputDir_.y -= 1.0f;
		if(demoInputEnabled_) inputDir_={demoMove_.x,demoMove_.y,0};
		RotateToMouse(viewProjection);
	}
	if ((!runModifiers_.enabled || !runRoomAwaitInputRelease_) &&
		(demoInputEnabled_ ? demoDash_ :
            input_->IsTrigger(input_->GetMouseState().rgbButtons[1], input_->GetPreMouseState().rgbButtons[1]))) {
        demoDash_=false;
		TryActivateSpecialAction();
	}
	if (saberCounterTimer_ > 0.0f) {
		saberCounterTimer_ = (std::max)(0.0f, saberCounterTimer_ - deltaTime);
	}

	// スタミナ回復
	if (!isDashing_) {
		stats_.stamina += stats_.staminaRecovery * deltaTime * (empJammerTimer_>0?.6f:1.0f);
		if (stats_.stamina > stats_.maxStamina) {
			stats_.stamina = stats_.maxStamina;
		}
	}

	UpdateStealth(deltaTime);
	UpdateSummoner(deltaTime);


	// ----------------------
	// 横入力
	// ----------------------
	RotateToMouse(viewProjection);
	inputDir_ = { 0,0,0 };

	if (input_->IsPress(input_->GetKey()[DIK_A])) inputDir_.x -= 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_D])) inputDir_.x += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_W])) inputDir_.y += 1.0f;
	if (input_->IsPress(input_->GetKey()[DIK_S])) inputDir_.y -= 1.0f;

    if(demoInputEnabled_) inputDir_={demoMove_.x,demoMove_.y,0};
	if (cg2::Length(inputDir_) > 1.0f) {
		inputDir_ = cg2::Normalize(inputDir_);
	}

	// --- 目標速度 ---
	cg2::Vector3 targetVelocity = inputDir_ * stats_.moveSpeed;
	if(railCharge_.held && !isDashing_) targetVelocity=targetVelocity*.78f;
	if(spinCycle_.remaining>0 && !isDashing_)targetVelocity=targetVelocity*.55f;

	// --- 慣性処理 ---
	float accel = (cg2::Length(inputDir_) > 0.0f) ? accel_ : decel_;

	if (runModifiers_.enabled) {
		// Restore the original acceleration / coast rates using a stable timestep.
		const float response = isDashing_ ? 1.5f : accel;
		velocity_ += (targetVelocity - velocity_) * (1.0f - std::exp(-response * deltaTime));
	} else {
		velocity_ += (targetVelocity - velocity_) * accel * deltaTime;
	}

	float timeWeight = deltaTime * 60.0f;

	cg2::Vector3 frameMove = velocity_ * timeWeight;
	const float maxStep = 0.35f;
	const int subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(frameMove.x), std::abs(frameMove.y)) / maxStep) + 1);
	cg2::Vector3 stepMove = frameMove / static_cast<float>(subStepCount);
	for (int i = 0; i < subStepCount; ++i) {
		cg2::Vector3 pos = GetWorldPosition();
		pos.x += stepMove.x;
		SetWorldPosition(pos);
		stage.ResolvePlayerCollision(*this, cg2::X);

		pos = GetWorldPosition();
		pos.y += stepMove.y;
		SetWorldPosition(pos);
		stage.ResolvePlayerCollision(*this, cg2::Y);
	}

	if (!isDead_ && !isDashing_ && cg2::Length(inputDir_) > 0.05f) {
		movementParticleTimer_ -= deltaTime;
		if (movementParticleTimer_ <= 0.0f) {
			SpawnAfterimage();
			movementParticleTimer_ = movementParticleInterval_;
		}
	} else {
		movementParticleTimer_ = 0.0f;
	}

	// object_ の更新だけ（移動はしない）
	object_->SetTransform(worldTransform_);
	object_->Update();
	UpdateBarrelLayout();

	if (!isDead_) {
		// 攻撃処理
		if (runModifiers_.enabled && runDashBurstPending_) {
			runDashBurstPending_ = false;
            const auto synergy=MakeTankRunSynergy(runModifiers_,runOverdriveTimer_>0.0f);
            {
                MineDropEvent explosion{};
                explosion.position=GetWorldPosition();
                explosion.radius=synergy.explosionRadius;
                explosion.fuseTime=0.06f;
                explosion.lifeTime=0.12f;
                explosion.damage=static_cast<uint32_t>((std::max)(1.0f,
                    std::round(stats_.bulletDamage*synergy.explosionDamageScale)));
                explosion.color={1.8f,0.65f,0.14f,1.0f};
                pendingMineDrops_.push_back(explosion);
                ++runDashExplosionsEmitted_;
            }
		}

		if (currentClass_ == ClassType::Smasher && !IsMeleeBuild()) {
			Smash(deltaTime);
		} else {
			Attack(BulletManager, deltaTime);
		}

		const PlayerClassConfig* activeClass = GetCurrentClassConfig();
		const size_t authoredSupport = runEvolutionActive_ && runAuthoredEvolutionActive_
			? static_cast<size_t>((std::max)(0,runEvolutionConfig_.maxDrones)) : 0u;
		const size_t supportLimit = authoredSupport + (runModifiers_.drones ? static_cast<size_t>(TankEffectCount(runModifiers_,6,2)) : 0u)
			+ (runModifiers_.core == TankRunCore::Drone ? 2u : 0u);
		if (IsDroneBuild()) {
			runBulletManager_ = BulletManager;
			runSupportDroneTimer_ = (std::max)(0.0f, runSupportDroneTimer_ - deltaTime);
			if (runSupportDroneTimer_ <= 0.0f) EnsureExpeditionDrones();
		} else if (runModifiers_.enabled && !runRoomAwaitInputRelease_ && supportLimit > 0 &&
			(!activeClass || !activeClass->usesDrone)) {
			runSupportDroneTimer_ = (std::max)(0.0f, runSupportDroneTimer_ - deltaTime);
			if (drones_.size() < supportLimit && runSupportDroneTimer_ <= 0.0f) {
				auto drone = std::make_unique<PlayerDrone>();
				drone->Initialize(worldTransform_.translate + dir_ * 0.8f, dir_ * 0.1f);
				drone->SetAttackControllerBulletManager(BulletManager);
				ConfigureRunDrone(*drone);
				drones_.push_back(std::move(drone));
				runSupportDroneTimer_ = 1.2f;
			}
		}
		const size_t bulletsBeforeDroneUpdates = BulletManager->GetBulletCount();
		size_t companionIndex=0;
		std::array<bool,48> spreadUsed{};
		for (auto& drone : drones_) {
			if (runModifiers_.enabled) {
				ConfigureRunDrone(*drone);
				drone->SetRunOwner(this,static_cast<int>(companionIndex));
				const bool wantsAttack = !runRoomAwaitInputRelease_ && !upgradeHudMouseCaptured_
					&& (demoInputEnabled_ ? demoShoot_ : input_->IsPress(input_->GetMouseState().rgbButtons[0]));
				cg2::Vector3 aim=runAimWorld_;
				if(IsDroneBuild()&&!runHomingTargets_.empty()&&(runModifiers_.autonomousSpread||runModifiers_.droneFocus)) {
					std::array<float,48> distances{};const auto from=runModifiers_.autonomousSpread?drone->GetWorldPosition():runAimWorld_;
					const int count=static_cast<int>((std::min)(runHomingTargets_.size(),distances.size()));
					for(int n=0;n<count;++n)distances[static_cast<size_t>(n)]=cg2::Length(runHomingTargets_[static_cast<size_t>(n)]-from);
					const int chosen=tankspecial::ChooseSpreadTarget(distances.data(),spreadUsed.data(),count);
					if(chosen>=0&&(runModifiers_.autonomousSpread||distances[static_cast<size_t>(chosen)]<6.0f)) {
						aim=runHomingTargets_[static_cast<size_t>(chosen)];
						if(runModifiers_.autonomousSpread) {spreadUsed[static_cast<size_t>(chosen)]=true;if(wantsAttack)specialCombatStats_.spreadTargets|=uint32_t{1}<<(chosen%32);}
					}
				}
				if(expeditionCombatStyleSelected_ || demoInputEnabled_)drone->SetRunInput(aim,wantsAttack);
				if(IsDroneBuild()) {
					const float orbit = static_cast<float>(companionIndex) * 6.2831853f / static_cast<float>((std::max)(size_t{1},drones_.size()));
					const float radius=GetCombatStyleProfile(tankbuild::Style::Drone).droneFormationRadius+(runModifiers_.droneLaserLink?2.0f:0.0f);
					drone->SetRunFollowOffset({std::cos(orbit)*radius,std::sin(orbit)*radius,0});
				}
			}
			++companionIndex;
			drone->Update(viewProjection, stage, worldTransform_.translate,
				runModifiers_.enabled ? deltaTime : 1.0f / 60.0f);
		}
		if (runModifiers_.enabled && runCheckpointEvolution_ && BulletManager->GetBulletCount() > bulletsBeforeDroneUpdates) {
			primaryAttackPerformedEvent_ = true;
			++primaryAttackCount_;
		}

		drones_.erase(
			std::remove_if(
				drones_.begin(),
				drones_.end(),
				[](const std::unique_ptr<PlayerDrone>& drone) {
					return drone->IsDead();
				}),
			drones_.end());
		if(IsDroneBuild() && drones_.size()<static_cast<size_t>(GetExpeditionDroneLimit()) && runSupportDroneTimer_<=0.0f)
			runSupportDroneTimer_=1.2f;
		UpdateRunProjectiles(BulletManager, deltaTime);

	}

	machineGunBtnSprite_->Update();

	if (hp_ <= 0) {
		Die();
	}
	if (isExploding_) {
		UpdateParticles(deltaTime);
	}

}

void Player::Draw(bool drawBody) {

	// ドローンの描画
	for (auto& drone : drones_) {
		if (!drone->UsesNeonVisual()) drone->Draw();
	}

	if (drawBody) {
		DrawBodyOnly();
	}

}

void Player::DrawBodyOnly() {
	if (isDead_) {
		if (deathChargeTimer_ <= 0.0f) {
			return;
		}

		const float progress = 1.0f - deathChargeTimer_ / deathChargeDuration_;
		const float charge = progress * progress;
		cg2::Transform chargeTransform = worldTransform_;
		chargeTransform.scale = worldTransform_.scale * (1.0f + charge * 0.65f);
		const cg2::Vector4 savedColor = object_->GetColor();
		const bool savedLighting = object_->IsLightingEnabled();
		object_->SetTransform(chargeTransform);
		object_->SetLighting(false);
		object_->SetColor({ 2.4f, 2.4f, 2.4f, 1.0f });
		object_->Update();
		object_->Draw();
		object_->SetTransform(worldTransform_);
		object_->SetColor(savedColor);
		object_->SetLighting(savedLighting);
		object_->Update();
		return;
	}

	if (!isDead_) {
		// 無敵時間中は点滅

		if (invincibleTimer_ > 0.0f && !isJustEvaded_) {
			if (static_cast<int>(invincibleTimer_ * 10) % 5 == 0) {
				// 透明度を下げる
				if (isStealth_) {
					SetVehicleAlpha(0.2f);
				} else {
					SetVehicleAlpha(0.5f);
				}
				if (damageFeedbackTimer_ > 0.0f) {
					cg2::Transform drawTransform = worldTransform_;
					const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
					const float pulse = std::sin(t * 3.14159265f) * 0.10f;
					drawTransform.scale = worldTransform_.scale * (1.0f + pulse);
					object_->SetTransform(drawTransform);
					object_->Update();
				}
				object_->Draw();
				DrawBarrels();
				return;
			}
		}
		if (!isStealth_) {
			SetVehicleAlpha(1.0f);
		}
		if (damageFeedbackTimer_ > 0.0f) {
			cg2::Transform drawTransform = worldTransform_;
			const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
			const float pulse = std::sin(t * 3.14159265f) * 0.10f;
			drawTransform.scale = worldTransform_.scale * (1.0f + pulse);
			object_->SetTransform(drawTransform);
			object_->Update();
		}
		object_->Draw();
		DrawBarrels();
	}

}

void Player::DrawSprite()
{
	cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
	DrawUpgradeHud();
}

std::vector<PlayerDrone*> Player::GetDronePtrs() const {
	std::vector<PlayerDrone*> result;
	result.reserve(drones_.size());
	for (const auto& d : drones_) {
		result.push_back(d.get());
	}
	return result;
}

cg2::Vector3 Player::GetWorldPosition() const {

	// ワールド座標を入れる
	cg2::Vector3 worldPos;
	// ワールド行列の平行移動成分を取得(ワールド座標)
	worldPos.x = worldTransform_.translate.x;
	worldPos.y = worldTransform_.translate.y;
	worldPos.z = worldTransform_.translate.z;

	return worldPos;
}

void Player::OnCollision(Collider* other)
{
    if (const auto* resource = dynamic_cast<const ExpEnemy*>(other); resource && resource->IsRunResource()) {
        return;
    }

    if (currentClass_ == ClassType::Assassin) {
        isStealth_ = false;
        stealthTimer_ = 0.0f;
    }

    if ((invincibleTimer_ > 0.0f && !isDashing_) || isJustEvaded_ || isSmash_) {
        // 無敵中に敵の弾を受けてもノックバックしない
        if (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
            return;
        }
    }

    if (!isJustEvaded_ && isDashing_ &&
        TankCanPerfectDodge(runModifiers_.enabled && runModifiers_.perfectDodge,
                            other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet, kDashDuration - dashTimer_,
                            TankEffectPower(runModifiers_, 15))) {
        requestSlow_ = true;
        isJustEvaded_ = true;
        invincibleTimer_ = dashTimer_;
        buffTimer_ = kBuffDuration * TankEffectPower(runModifiers_, 15);
        if (runModifiers_.enabled && runModifiers_.capacitor) {
            const float power = TankEffectPower(runModifiers_, 4);
            buffTimer_ += 1.5f * power;
            stats_.stamina = (std::min)(stats_.maxStamina, stats_.stamina + power);
            HealRunPlayer((std::max)(1, static_cast<int>(std::round(4.0f * power))));
        }
        if (runModifiers_.enabled && runModifiers_.overdrive) {
            runOverdriveTimer_ = 2.4f * TankEffectPower(runModifiers_, 11);
            runOverdriveCooldown_ = 4.0f;
        }
        isBuffActive_ = !runCheckpointEvolution_ || runModifiers_.capacitor;
        maxCharge_ = 5.0f;
        return;
    }

    cg2::Vector3 hitDir = worldTransform_.translate - other->GetWorldPosition();

    if (cg2::Length(hitDir) < 0.0001f) {
        // 押し戻す方向が定まらない場合は、後続の被ダメージ処理にも進まない。
        return;
    }

    hitDir = cg2::Normalize(hitDir);

    const float kKnockBackPower = 0.15f;

    velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
    const float maxKnockSpeed = isDashing_ ? 0.32f : 0.20f;
    if (cg2::Length(velocity_) > maxKnockSpeed) {
        velocity_ = cg2::Normalize(velocity_) * maxKnockSpeed;
    }

    if (other->GetCollisionAttribute() == kCollisionAttributeEnemy || other->GetCollisionAttribute() == kCollisionAttributeExpEnemy ||
        other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
        TakeDamage(other->GetDamage(), 0.45f);
    }
}

bool Player::TryDashImpact(Collider* target)
{
    if (!target || isDead_ || !isDashing_)
        return false;
    auto* regular = dynamic_cast<ExpEnemy*>(target);
    auto* boss = dynamic_cast<Enemy*>(target);
    if ((!regular && !boss) || (regular && (regular->IsDead() || regular->IsRunResource())) ||
        (boss && (!boss->IsRunEncounterEnabled() || boss->IsDead())))
        return false;
    const uint64_t id = target->GetCollisionId();
    if (std::find(dashImpactTargets_.begin(), dashImpactTargets_.end(), id) != dashImpactTargets_.end()) {
        // このダッシュで処理済みなら、受付時間を過ぎてもtrueを返して通常の双方通知を抑える。
        return true;
    }
    if (kDashDuration - dashTimer_ > kJustEvadeWindow)
        return false;
    dashImpactTargets_.push_back(id);
    cg2::Vector3 direction = target->GetWorldPosition() - GetWorldPosition();
    if (cg2::Length(direction) < 0.001f)
        direction = velocity_;
    if (cg2::Length(direction) < 0.001f)
        direction = {1, 0, 0};
    direction = cg2::Normalize(direction);
    const bool powered = runModifiers_.enabled && runModifiers_.impactDrive;
    const float strength = powered ? 1.0f + 0.5f * TankEffectPower(runModifiers_, 14) : 1.0f;
    const uint32_t damage =
        static_cast<uint32_t>((std::max)(1.0f, std::round((stats_.bulletDamage * 2.5f + stats_.bodyDamage) * strength)));
    // 速度だけを与え、位置の更新と壁への補正は各敵の通常移動に任せる。
    if (regular) {
        regular->ApplyKnockback(direction, 0.62f * strength);
        ArmWallSmash(regular, strength);
        regular->TakeDirectionalDamage(damage, GetWorldPosition(), true);
    }
    if (boss) {
        boss->ApplyKnockback(direction, 0.62f * strength);
        boss->TakeDamage(damage);
    }
    pendingDashImpacts_.push_back({target->GetWorldPosition(), direction, boss != nullptr, powered});
    velocity_ = velocity_ * 0.72f;
    // 体当たりの成立はジャスト回避とは独立しており、回避状態・スロー要求・弾への無敵は付与しない。
    return true;
}

std::vector<Player::DashImpactEvent> Player::ConsumeDashImpactEvents()
{
    auto events = std::move(pendingDashImpacts_);
    pendingDashImpacts_.clear();
    return events;
}

cg2::AABB Player::GetAABB() {
	cg2::Vector3 worldPos = GetWorldPosition();

	cg2::AABB aabb;

	aabb.min = { worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f };
	aabb.max = { worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f };

	return aabb;
}

void Player::Damage(uint32_t amount)
{
    TakeDamage(amount, 0.45f);
}

void Player::TakeDamage(uint32_t amount, float invincibleTime)
{
    if (isDead_ || amount == 0 || invincibleTimer_ > 0.0f || debugNoDamage_) {
        return;
    }
    if (saberCounterTimer_ > 0.0f) {
        // 受付中の反撃はHP減少と被ダメージ回数の記録より先に処理する。
        const PlayerClassConfig* config = GetCurrentClassConfig();
        if (config && config->specialActionId == "saber_counter") {
            TriggerSaberCounter(*config);
            return;
        }
    }

    const uint32_t appliedDamage = runModifiers_.enabled && runCheckpointEvolution_ ? runMaintenance_.MitigateDamage(amount) : amount;
    ++damageTakenCount_;
    // 符号付きHPへ変換する前に残HPで制限し、極端な開発用ダメージでも整数変換を安全にする。
    hp_ -= static_cast<int>((std::min)(appliedDamage, static_cast<uint32_t>((std::max)(0, hp_))));
    if (hp_ < 0) {
        hp_ = 0;
    }

    TriggerDamageFeedback();
    invincibleTimer_ = invincibleTime;
    if (hp_ <= 0) {
        Die();
    }
}

void Player::ApplyBalanceConfig(const BalanceConfig& config)
{
    const int previousHp=hp_;
	baseStats_.maxHp = static_cast<float>((std::max)(1, config.maxHp));
	baseStats_.reloadSpeed = (std::max)(0.05f, config.reloadSpeed);
	baseStats_.bulletDamage = (std::max)(0.1f, config.bulletDamage);
	baseStats_.bulletSpeed = (std::max)(0.01f, config.bulletSpeed);
	baseStats_.moveSpeed = (std::max)(0.01f, config.moveSpeed);
	baseStats_.staminaRecovery = (std::max)(0.0f, config.staminaRecovery);
	baseStats_.maxStamina = (std::max)(0.0f, config.maxStamina);
	baseStats_.bodyDamage = static_cast<float>((std::max)(1u, config.bodyDamage));

	healthRegenUpgradeRate_ = (std::max)(0.0f, config.healthRegenUpgrade);
	maxHpUpgradeRate_ = (std::max)(0.0f, config.maxHpUpgradeAmount);
	bodyDamageUpgradeRate_ = (std::max)(0.0f, config.bodyDamageUpgrade);
	bulletSpeedUpgradeRate_ = config.bulletSpeedUpgrade;
	bulletDamageUpgradeRate_ = (std::max)(0.0f, config.bulletDamageUpgrade);
	reloadUpgradeRate_ = (std::max)(0.0f, config.reloadUpgrade);
	moveSpeedUpgradeRate_ = config.moveSpeedUpgrade;
	minReloadSpeed_ = (std::max)(0.05f, config.minReloadSpeed);

    runGrowth_={maxHpUpgradeRate_,bulletDamageUpgradeRate_,bulletSpeedUpgradeRate_,
        (std::min)(0.90f,reloadUpgradeRate_),moveSpeedUpgradeRate_};
	RecalculateStatsFromBase(config.healToFull);
    // Editing a running expedition must not grant a free heal when the old
    // maximum was full. Card acquisition keeps its separate growth/heal rule.
    if(runCheckpointEvolution_ && !config.healToFull)
        hp_=(std::clamp)(previousHp,0,GetMaxHp());
}

void Player::SetRunModifiers(const TankRunModifiers& modifiers)
{
	const bool wasEnabled = runModifiers_.enabled;
	const int previousHp=hp_;
	runModifiers_ = modifiers;
	RefreshAdditiveArmaments();
	if(!modifiers.enabled||!modifiers.railCannon)railCharge_.Reset();
	if (!wasEnabled || !runModifiers_.enabled) {
		ResetAdditionalAbilities();
		expeditionCombatStyleSelected_ = false;
		expeditionCombatStyle_ = tankbuild::Style::Shooter;
		runMaintenance_ = {};
		runEvolutionActive_ = false;
		runAuthoredEvolutionActive_ = false;
		runEvolutionPrepared_ = false;
	}
	if (runModifiers_.enabled) {
		upgradeHudListVisibility_ = 0.0f;
		upgradeHudMouseCaptured_ = false;
	}
	RecalculateStatsFromBase(false);
	// Refitting edited modules is not healing. A purchased repair grants its
	// explicit recovery in the scene, including before the style is selected.
	if(wasEnabled&&runModifiers_.enabled&&runModifiers_.expedition)
		hp_=(std::clamp)(previousHp,0,GetMaxHp());
	if (wasEnabled && !runModifiers_.enabled) {
		SetRunCurrencyMode(false);
		// A run's support units and attack configuration must not escape its mode.
		drones_.clear();
		runSupportDroneTimer_ = 0.0f;
		runDashAttackTimer_ = 0.0f;
		runOverdriveTimer_ = 0.0f;
		runOverdriveCooldown_ = 0.0f;
		runDashBurstPending_ = false;
		runRoomAwaitInputRelease_ = false;
		runCheckpointEvolution_ = false;
		runHomingTargets_.clear();
		if (object_) {
			InitializeBarrels();
			UpdateBarrelLayout();
		}
	}
}

void Player::ApplyCombatStyleBalance(const TankCombatStyleBalances& profiles)
{
	const int hp=hp_;const float stamina=stats_.stamina;
	const auto defaults=DefaultTankCombatStyleBalances();
	for(size_t i=0;i<profiles.size();++i)combatStyleBalances_[i]=SanitizeTankCombatStyleProfile(profiles[i],defaults[i]);
	RecalculateStatsFromBase(false);
	hp_=(std::clamp)(hp,0,GetMaxHp());stats_.stamina=(std::clamp)(stamina,0.0f,stats_.maxStamina);
	EnsureExpeditionDrones();
	for(auto& drone:drones_)ConfigureRunDrone(*drone);
}

float Player::GetRunBaseReloadFrames() const
{
	return (std::max)(0.05f,runModifiers_.enabled&&expeditionCombatStyleSelected_
		?GetCombatStyleProfile(expeditionCombatStyle_).attackIntervalSeconds*60.0f:baseStats_.reloadSpeed);
}

bool Player::SetExpeditionCombatStyle(tankbuild::Style style)
{
	if(!runModifiers_.enabled || !runModifiers_.expedition || isDead_ || !tankbuild::Valid(style)) return false;
	if(expeditionCombatStyleSelected_ && expeditionCombatStyle_==style) return true;
	const auto* basic=GetClassConfig("Basic");
	if(!basic || basic->barrels.empty()) return false;
	// Changing equipment must never rebuild the run or refill health/stamina.
	runStarterConfig_=*basic;
	runStarterConfig_.displayName=tankbuild::Name(style);
	runStarterConfig_.usesDrone=style==tankbuild::Style::Drone;
	runStarterConfig_.maxDrones=style==tankbuild::Style::Drone ? 3 : 0;
	runStarterConfig_.bulletCount=1;
	runStarterConfig_.reloadScale=runStarterConfig_.bulletDamageScale=runStarterConfig_.bulletSpeedScale=1.0f;
	runStarterConfig_.randomSpread=false;runStarterConfig_.spreadAngleDeg=0;
	runStarterConfig_.reflect=runStarterConfig_.penetrate=false;
	runStarterConfig_.barrels.resize(1);
	auto& mount=runStarterConfig_.barrels.front();
	mount.fires=style==tankbuild::Style::Shooter;mount.weaponType=WeaponType::Projectile;
	mount.angleDeg=0;mount.offset={.72f,0,0};mount.damageScale=mount.reloadScale=mount.projectileSpeedScale=1.0f;
	const int previousHp=hp_;const float previousStamina=stats_.stamina;
	expeditionCombatStyle_=style;expeditionCombatStyleSelected_=true;
	ResetAdditionalAbilities();RefreshAdditiveArmaments();
	railCharge_.Reset();specialMeleeElapsed_=-1;specialParriedBullets_.clear();droneLaserLinks_.clear();pendingSpecialCombatEvents_.clear();
	RecalculateStatsFromBase(false);
	hp_=(std::clamp)(previousHp,0,GetMaxHp());stats_.stamina=(std::min)(previousStamina,stats_.maxStamina);
	runEvolutionActive_=runAuthoredEvolutionActive_=runEvolutionPrepared_=false;
	bulletCoolTime=0;meleeComboStep_=0;meleeComboTimer_=0;
	shootBarrelIndex_=shootGroupIndex_=0;weaponGroupCooldowns_.clear();
	drones_.clear();runSupportDroneTimer_=0;
	pendingMeleeSlashes_.clear();pendingLaserShots_.clear();pendingMineDrops_.clear();
	if(object_) {InitializeBarrels();UpdateBarrelLayout();EnsureExpeditionDrones();}
	return true;
}

int Player::GetExpeditionDroneLimit() const
{
	if(!IsDroneBuild()) return 0;
	const auto* config=GetCurrentClassConfig();
	return TankCombatStyleDroneCount(GetCombatStyleProfile(tankbuild::Style::Drone).droneCount,
		config?config->maxDrones:3,runEvolutionActive_,runModifiers_.drones?TankEffectCount(runModifiers_,6,2):0);
}

void Player::EnsureExpeditionDrones()
{
	if(!IsDroneBuild() || !object_ || !runBulletManager_ || isDead_) return;
	const size_t limit=static_cast<size_t>(GetExpeditionDroneLimit());
	if(drones_.size()>limit)drones_.resize(limit);
	while(drones_.size()<limit) {
		auto drone=std::make_unique<PlayerDrone>();
		// Spawn at the owner; normal wall-aware movement forms the escort.
		drone->Initialize(GetWorldPosition(),{});
		drone->SetAttackControllerBulletManager(runBulletManager_);
		ConfigureRunDrone(*drone);
		drone->SetRunInput(runAimWorld_,false);
		drones_.push_back(std::move(drone));
	}
}

void Player::ConfigurePrototypeLoadout(int archetype)
{
	if (!runModifiers_.enabled) {
		return;
	}
	runMaintenance_ = {};
	expeditionCombatStyleSelected_=false;
	expeditionCombatStyle_=tankbuild::Style::Shooter;
	runEvolutionActive_ = false;
	runAuthoredEvolutionActive_ = false;
	runEvolutionPrepared_ = false;
	RecalculateStatsFromBase(false);
    if(runCheckpointEvolution_) {
        const char* branches[]={"Twin","MachineGun","Overseer"};
        runDashExplosionsEmitted_=0;
        runStarterBranch_=branches[(std::clamp)(archetype,0,2)];
        EvolveById("Basic");
        if(const auto* basic=GetClassConfig("Basic")) runStarterConfig_=*basic;
        runStarterConfig_.displayName="ベーシック";
        runStarterConfig_.usesDrone=false; runStarterConfig_.bulletCount=1;
        runStarterConfig_.reflect=false; runStarterConfig_.penetrate=false;
        runStarterConfig_.spreadAngleDeg=0; runStarterConfig_.randomSpread=false;
        runStarterConfig_.reloadScale=1; runStarterConfig_.bulletDamageScale=1; runStarterConfig_.bulletSpeedScale=1;
        runStarterConfig_.specialActionId="perfect_dodge";
        runStarterConfig_.alternateBarrels=false; runStarterConfig_.fireAllBarrels=false;
        if(!runStarterConfig_.barrels.empty()) {
            runStarterConfig_.barrels.resize(1);
            auto& barrel=runStarterConfig_.barrels.front();
            barrel.fires=true; barrel.weaponType=WeaponType::Projectile; barrel.angleDeg=0;
            barrel.offset={0.72f,0,0}; barrel.reloadScale=1; barrel.damageScale=1; barrel.projectileSpeedScale=1;
        }
        level_=1; exp_=0; skillPoints_=0; upgradeLevels_.fill(0); nextLevelExp_=GetNextLevelExp();
        drones_.clear(); RecalculateStatsFromBase(true);
        InitializeBarrels(); UpdateBarrelLayout(); return;
    }
	// Starting loadouts are rank two. Their next evolution becomes available
	// at rank three, rather than opening a locked menu at the first rank-up.
	if (level_ < 5) {
		level_ = 5;
		exp_ = 0;
		nextLevelExp_ = GetNextLevelExp();
	}
	const char* classIds[] = { "Twin", "MachineGun", "Overseer" };
	EvolveById(classIds[(std::clamp)(archetype, 0, 2)]);
}

void Player::HealRunPlayer(int amount)
{
	if (!runModifiers_.enabled || isDead_ || amount <= 0) {
		return;
	}
	hp_ += (std::min)(amount, (std::max)(0, GetMaxHp() - hp_));
}

bool Player::SpendRunHealth(int amount)
{
	if (!runModifiers_.enabled || isDead_ || amount <= 0 || hp_ <= amount) return false;
	// A chosen event cost is not an incoming hit and cannot be dodged or lethal.
	hp_ -= amount;
	return true;
}

std::vector<RunEvolutionChoice> Player::GetRunEvolutionChoices() const
{
	std::vector<RunEvolutionChoice> choices;
	if (!runModifiers_.enabled || isDead_) return choices;
	if (expeditionCombatStyleSelected_ && expeditionCombatStyle_!=tankbuild::Style::Shooter) return choices;
	if (runCheckpointEvolution_) {
		if (!runEvolutionPrepared_ || runEvolutionActive_) return choices;
		for (const auto& specialization : kTankExpeditionSpecializations) {
			if (expeditionCombatStyleSelected_ ? specialization.drones==0 : runStarterBranch_ == specialization.starter) {
				choices.push_back({ specialization.id, specialization.name, specialization.description });
			}
		}
		return choices;
	}
	const PlayerClassConfig* current = GetCurrentClassConfig();
	if (!current) return choices;
	const auto firingMode = [](const PlayerClassConfig& config) {
		if (config.usesDrone) return "ドローン上限" + std::to_string(config.maxDrones) + "機";
		const auto guns = std::count_if(config.barrels.begin(), config.barrels.end(),
			[](const WeaponMountConfig& mount) { return mount.fires && mount.weaponType == WeaponType::Projectile; });
		return std::to_string(guns) + "砲" + (guns > 1 ?
			(config.alternateBarrels && !config.fireAllBarrels ? "交互" : "同時") : "射撃");
	};
	const auto scale = [](float value) {
		char text[24]{};
		std::snprintf(text, sizeof(text), "x%.2f", value);
		return std::string(text);
	};
	for (const std::string& id : classCatalog_.OrderedIds()) {
		// The editor's temporary copy has no firing advantage over Twin. Keep it
		// in the original C tree, but omit it from expedition checkpoint rewards.
		if (id == "Triple_Copy" || !CanEvolveTo(id)) continue;
		const PlayerClassConfig* config = GetClassConfig(id);
		if (!config) continue;
		std::string description = "機体の基本性能を比較\n" + firingMode(*current) + "→" + firingMode(*config);
		description += "\n威力 " + scale(current->bulletDamageScale) + "→" + scale(config->bulletDamageScale);
		description += "\n発射間隔 " + scale(current->reloadScale) + "→" + scale(config->reloadScale);
		if (current->reflect != config->reflect) {
			description += std::string("\n壁反射 ") + (current->reflect ? "あり→なし" : "なし→あり");
		} else if (current->bulletCount != config->bulletCount) {
			description += "\n砲ごとの弾数 " + std::to_string(current->bulletCount) + "→" + std::to_string(config->bulletCount);
		} else if (current->bulletSpeedScale != config->bulletSpeedScale) {
			description += "\n弾速 " + scale(current->bulletSpeedScale) + "→" + scale(config->bulletSpeedScale);
		}
		description += "\n主軸・改造は引き継ぎ";
		choices.push_back({ config->id, config->displayName, std::move(description) });
		if (choices.size() == 3) break;
	}
	return choices;
}

void Player::PrepareRunEvolution()
{
	if (!runModifiers_.enabled || isDead_) return;
	isChangeMode = false;
	evolutionConfirmedEvent_ = false;
	evolutionCancelledEvent_ = false;
	if (runCheckpointEvolution_) {
		// A checkpoint specialization is independent of the arena's level/tree.
		runEvolutionPrepared_ = !runEvolutionActive_;
		return;
	}
	const PlayerClassConfig* current = GetCurrentClassConfig();
	if (!current || !evolutionCircuitLoaded_) return;
	const int targetRank = current->requiredRank + 1;
	if (targetRank > GetRankFromLevel(kMaxLevel)) return;
	const bool hasSuccessor = std::any_of(classCatalog_.OrderedIds().begin(), classCatalog_.OrderedIds().end(), [&](const std::string& id) {
		if (id == "Triple_Copy") return false; // Match the checkpoint candidate filter above.
		const PlayerClassConfig* config = GetClassConfig(id);
		return config && config->requiredRank == targetRank &&
			HasEvolutionEdge(current->id, id) && IsRunCompatibleClass(*config);
	});
	if (!hasSuccessor) return;
	// Grant only the checkpoint's required rank. HP, partial XP, and the build
	// remain untouched; normal level-up rewards and the legacy C popup do not run.
	while (level_ < kMaxLevel && GetRankFromLevel(level_) < targetRank) ++level_;
	nextLevelExp_ = GetNextLevelExp();
}

bool Player::ChooseRunEvolution(const std::string& id)
{
	if (!runModifiers_.enabled || isDead_) return false;
	const auto choices = GetRunEvolutionChoices();
	if (std::none_of(choices.begin(), choices.end(), [&](const RunEvolutionChoice& choice) { return choice.id == id; })) {
		return false;
	}
	if (runCheckpointEvolution_) {
		const auto* specialization = FindTankExpeditionSpecialization(id);
		const auto* starter = GetClassConfig(expeditionCombatStyleSelected_ && specialization ? specialization->starter : runStarterBranch_);
		if (!specialization || !starter || starter->barrels.empty()) return false;
		runEvolutionConfig_ = *starter;
		auto& config = runEvolutionConfig_;
		config.id = specialization->id;
		config.displayName = specialization->name;
		config.requiredRank = 3;
		config.usesDrone = specialization->drones > 0;
		config.maxDrones = specialization->drones;
		config.reloadScale *= specialization->reloadScale;
		config.bulletDamageScale *= specialization->damageScale;
		config.bulletSpeedScale *= specialization->speedScale;
		config.bulletCount = 1;
		config.spreadAngleDeg = specialization->randomSpread;
		config.randomSpread = specialization->randomSpread > 0.0f;
		config.reflect = starter->reflect || specialization->reflect;
		config.alternateBarrels = specialization->alternate;
		config.fireAllBarrels = !specialization->alternate;
		const WeaponMountConfig prototype = starter->barrels.front();
		config.barrels.assign(static_cast<size_t>(specialization->barrels), prototype);
		for (size_t i = 0; i < config.barrels.size(); ++i) {
			auto& barrel = config.barrels[i];
			const float position = static_cast<float>(i) - static_cast<float>(config.barrels.size() - 1) * 0.5f;
			barrel.offset = { 0.72f, position * 0.50f, 0.0f };
			barrel.angleDeg = position * specialization->fanAngle;
			barrel.fires = true;
			barrel.weaponType = WeaponType::Projectile;
			barrel.fireGroup = 0;
			barrel.reloadScale = 1.0f;
			barrel.damageScale = 1.0f;
			barrel.projectileSpeedScale = 1.0f;
			if (specialization->speedScale > 1.2f) barrel.scale.x *= 1.25f;
		}
		runEvolutionActive_ = true;
		runAuthoredEvolutionActive_ = false;
		runEvolutionPrepared_ = false;
		bulletCoolTime = 0.0f;
		shootBarrelIndex_ = 0;
		shootGroupIndex_ = 0;
		weaponGroupCooldowns_.clear();
		drones_.clear();
		runSupportDroneTimer_ = 0.0f;
		InitializeBarrels();
		UpdateBarrelLayout();
		isChangeMode = false;
		evolutionConfirmedEvent_ = true;
		return true;
	}
	return TryConfirmEvolutionById(id);
}

bool Player::AwardRunMaintenancePoint(int clearedRoom)
{
	return runModifiers_.enabled && runCheckpointEvolution_ && !isDead_ && runMaintenance_.AwardRoom(clearedRoom);
}

std::array<RunMaintenanceChoice, 3> Player::GetRunMaintenanceChoices() const
{
	std::array<RunMaintenanceChoice, 3> choices{{
		{ 0, "機動整備", "1段階ごとに移動速度 +12%\n敵の射線を外しやすくする" },
		{ 1, "装填整備", "1段階ごとに発射間隔 -14%\n主砲とドローンの両方に有効" },
		{ 2, "装甲整備", "1段階ごとに被ダメージ -8%\n最大24%・最低1ダメージ\nイベントのHP支払いは対象外" }
	}};
	for (auto& choice : choices) {
		choice.rank = runMaintenance_.Rank(choice.id);
		choice.canSpend = runModifiers_.enabled && runCheckpointEvolution_ && !isDead_ &&
			runMaintenance_.Points() > 0 && choice.rank < choice.maxRank;
	}
	return choices;
}

bool Player::SpendRunMaintenancePoint(int stat)
{
	if (!runModifiers_.enabled || !runCheckpointEvolution_ || isDead_ || !runMaintenance_.Spend(stat)) return false;
	const int previousHp = hp_;
	RecalculateStatsFromBase(false);
	// Maintenance changes never heal, including while the player is at full HP.
	hp_ = (std::min)(previousHp, GetMaxHp());
	return true;
}

bool Player::RefundRunMaintenancePoint(int stat)
{
	if (!runModifiers_.enabled || !runCheckpointEvolution_ || isDead_ || !runMaintenance_.Refund(stat)) return false;
	const int previousHp = hp_;
	RecalculateStatsFromBase(false);
	hp_ = (std::min)(previousHp, GetMaxHp());
	return true;
}

void Player::ResetRunRoomState(const cg2::Vector3& position)
{
	if (!runModifiers_.enabled || isDead_) return;
	ResetAdditionalAbilities();
	railCharge_.Reset();linkDamageClock_={};droneLaserLinks_.clear();pendingSpecialCombatEvents_.clear();
	specialMeleeElapsed_=-1;specialParriedBullets_.clear();
	velocity_ = {};
	move_ = {};
	inputDir_ = {};
	bulletCoolTime = 0.0f;
	shootBarrelIndex_ = 0;
	shootGroupIndex_ = 0;
	weaponGroupCooldowns_.clear();
	pendingLaserShots_.clear();
	pendingMineDrops_.clear();
	pendingMeleeSlashes_.clear();
	pendingDashImpacts_.clear();
	dashImpactTargets_.clear();
	drones_.clear();
	runHomingTargets_.clear();
	runSupportDroneTimer_ = 0.0f;
	runDashAttackTimer_ = 0.0f;
	runOverdriveTimer_ = 0.0f;
	runOverdriveCooldown_ = 0.0f;
	runDashBurstPending_ = false;
	runRoomAwaitInputRelease_ = true;
	isDashing_ = false;
	dashTimer_ = 0.0f;
	dashCooldown_ = 0.0f;
	isJustEvaded_ = false;
	isBuffActive_ = false;
	buffTimer_ = 0.0f;
	requestSlow_ = false;
	isSmash_ = false;
	smashCharge_ = 0.0f;
	smashDir_ = {};
	meleeComboStep_ = 0;
	meleeComboTimer_ = 0.0f;
	saberCounterTimer_ = 0.0f;
	isStealth_ = false;
	stealthTimer_ = 0.0f;
	stealthAlpha_ = 1.0f;
	summonTimer_ = 0.0f;
	stats_.stamina = stats_.maxStamina;
	invincibleTimer_ = 0.45f;
	damageFeedbackTimer_ = 0.0f;
	movementParticleTimer_ = 0.0f;
	primaryAttackPerformedEvent_ = false;
	dashStartedEvent_ = false;
	statUpgradePerformedEvent_ = false;
	evolutionConfirmedEvent_ = false;
	evolutionCancelledEvent_ = false;
	isChangeMode = false;
	upgradeHudMouseCaptured_ = false;
	for (auto& barrel : barrels_) {
		barrel.recoilOffset = 0.0f;
		barrel.muzzleFlashTimer = 0.0f;
	}
	SetWorldPosition(position);
	UpdateBarrelLayout();
	EnsureExpeditionDrones();
}

Player::RunCombatSnapshot Player::GetRunCombatSnapshot() const
{
    RunCombatSnapshot result{};
    const auto* config=GetCurrentClassConfig();
    if(!config) return result;
    result.style=expeditionCombatStyle_;result.hasStyle=expeditionCombatStyleSelected_;
    result.melee=IsMeleeBuild();result.droneLimit=GetExpeditionDroneLimit();
    result.classId=config->id;result.classDamageScale=config->bulletDamageScale;
    result.classReloadScale=config->reloadScale;result.classBulletSpeedScale=config->bulletSpeedScale;
    result.classReflects=config->reflect;result.classPenetrates=config->penetrate;
    result.isAuthored=runEvolutionActive_&&runAuthoredEvolutionActive_;
    result.classDroneCount=config->usesDrone||result.isAuthored?(std::max)(0,config->maxDrones):0;
    result.baseDroneCount=GetCombatStyleProfile(tankbuild::Style::Drone).droneCount;
    result.barrels=IsDroneBuild()||IsMeleeBuild()?0:static_cast<int>(config->barrels.size());
    result.activeDrones=static_cast<int>(drones_.size());
    AttackParam attack{};attack.bulletCount=config->bulletCount;attack.reflect=config->reflect;
    ApplyRunProjectileRules(attack);
    result.projectilesPerBarrel=attack.bulletCount;
    result.maxWallBounces=attack.maxWallBounces;
    result.actorPierceCount=attack.actorPierceCount;
    result.impactSplitCount=attack.impactSplitCount;
    result.reflects=attack.reflect || isBuffActive_;
    result.homing=runModifiers_.enabled && runModifiers_.homing;
    result.dashBurst=runModifiers_.enabled && runModifiers_.dashBurst;
    const auto synergy=MakeTankRunSynergy(runModifiers_,runOverdriveTimer_>0.0f);
    result.dashExplosion=synergy.dashExplosion;
    result.homingTurnRate=synergy.homingTurnRate;
    result.dashExplosionsEmitted=runDashExplosionsEmitted_;
    result.shotDamage=(std::max)(1.0f,std::round(stats_.bulletDamage*config->bulletDamageScale));
    if(!IsDroneBuild()&&!IsMeleeBuild()&&!config->barrels.empty())result.shotDamage=(std::max)(1.0f,std::round(result.shotDamage*config->barrels.front().damageScale));
    result.reloadSeconds=stats_.reloadSpeed/60.0f*config->reloadScale*GetRunFireIntervalScale()*(isBuffActive_?0.7f:1.0f);
    if(IsDroneBuild()) {
        const auto tuning=MakeTankDroneTuning(runModifiers_,true);
        result.shotDamage=(std::max)(1.0f,stats_.bulletDamage*config->bulletDamageScale*tuning.damageScale
            *(runModifiers_.autonomousSpread?(std::max)(.2f,1.0f-.18f*TankEffectPower(runModifiers_,38)):1.0f));
        result.reloadSeconds=(std::max)(.05f,GetCombatStyleProfile(tankbuild::Style::Drone).attackIntervalSeconds*tuning.reloadSeconds/.5f*stats_.reloadSpeed/GetRunBaseReloadFrames()
            *config->reloadScale*GetRunFireIntervalScale()*(isBuffActive_?.7f:1.0f)
            *(runModifiers_.core==TankRunCore::Drone?.8f:1.0f)*(empJammerTimer_>0?1.5f:1.0f));
    } else if(IsMeleeBuild()) {
        const auto combo=MakeTankMeleeCombo(0,runModifiers_);
        result.shotDamage=(std::max)(1.0f,std::round(stats_.bulletDamage*config->bulletDamageScale*3.8f*combo.damage));
        result.reloadSeconds=(std::max)(.05f,(combo.windup+combo.duration+combo.recovery)*stats_.reloadSpeed/GetRunBaseReloadFrames()
            *config->reloadScale*GetRunFireIntervalScale()*(expeditionCombatStyleSelected_?GetCombatStyleProfile(tankbuild::Style::Melee).attackIntervalSeconds/.33f:1.0f));
    }
    return result;
}

void Player::ApplyRunProjectileRules(AttackParam& param, bool applyFan) const
{
    (void)applyFan; // 旧APIの呼び出し互換用。現在はこの引数による扇状の複数弾を生成しない。
    // 遠征が無効でも、発射数と衝突時の分裂数はここで初期化する。
    param.bulletCount = 1;
    param.impactSplitCount = 0;
    if (!runModifiers_.enabled) {
        return;
    }
    const TankRunTuning tuning = MakeTankRunTuning(runModifiers_, runGrowth_);
    if (expeditionCombatStyleSelected_ && expeditionCombatStyle_ == tankbuild::Style::Shooter) {
        param.shooterChain = runModifiers_.chainLightning;
        param.shooterMark = runModifiers_.markDetonation;
        param.shooterBoomerang = runModifiers_.boomerangShell;
        param.shooterKillBurst = runModifiers_.killBurst;
        param.shooterChainPower = TankEffectPower(runModifiers_, 31);
        param.shooterMarkPower = TankEffectPower(runModifiers_, 32);
        param.shooterBoomerangPower = TankEffectPower(runModifiers_, 33);
        param.shooterKillBurstPower = TankEffectPower(runModifiers_, 34);
    }
    param.reflect = param.reflect || tuning.reflects;
    param.bulletHp = tuning.bulletHp;
    param.bulletPenetration = tuning.bulletInterception;
    if (runModifiers_.expedition) {
        param.maxWallBounces = tuning.maxWallBounces;
        const auto* active = GetCurrentClassConfig();
        if (active && active->reflect)
            param.maxWallBounces = (std::max)(3, param.maxWallBounces);
        if (isBuffActive_) {
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

float Player::GetRunFireIntervalScale() const
{
    if (!runModifiers_.enabled)
        return 1.0f;
    // 発射頻度の増加を間隔の逆数へ変換する。1未満の値ほど次の射撃までの待ち時間が短い。
    float scale = runOverdriveTimer_ > 0.0f ? 1.0f / (1.0f + TankEffectPower(runModifiers_, 11)) : 1.0f;
    if (runModifiers_.core == TankRunCore::Assault && runDashAttackTimer_ > 0.0f)
        scale *= 0.65f;
    return scale;
}

cg2::Vector3 Player::GetRailChargeMuzzle() const
{
    const cg2::Vector3 aim = cg2::Length(dir_) > .001f ? cg2::Normalize(dir_) : cg2::Vector3{1, 0, 0};
    const auto* config = GetCurrentClassConfig();
    if (config && !config->barrels.empty()) {
        const auto& mount = config->barrels.front();
        const cg2::Vector3 side{-aim.y, aim.x, 0};
        return GetWorldPosition() + aim * mount.offset.x + side * mount.offset.y +
               RotateDirection(aim, mount.angleDeg) * mount.muzzleForward;
    }
    return GetWorldPosition() + aim * 1.7f;
}

void Player::AttackRailCannon(BulletManager* bullets, bool pressed, float dt)
{
    if (!runModifiers_.enabled || !runModifiers_.railCannon || !expeditionCombatStyleSelected_ ||
        expeditionCombatStyle_ != tankbuild::Style::Shooter) {
        railCharge_.Reset();
        return;
    }
    // 戻り値は蓄積した秒数。負値は発射なしで、0秒の短い押下も有効な発射として扱う。
    const float charge = railCharge_.Step(pressed, dt, bulletCoolTime <= 0.0f && bullets != nullptr);
    if (charge < 0)
        return;
    const auto* config = GetCurrentClassConfig();
    const cg2::Vector3 aim = cg2::Length(dir_) > .001f ? cg2::Normalize(dir_) : cg2::Vector3{1, 0, 0};
    const cg2::Vector3 side{-aim.y, aim.x, 0};
    AttackParam param{};
    param.damage =
        static_cast<uint32_t>((std::max)(1.0f, std::round(stats_.bulletDamage * (config ? config->bulletDamageScale : 1.0f) *
                                                          tankspecial::RailDamageScale(charge, TankEffectPower(runModifiers_, 20)))));
    param.bulletSpeed = stats_.bulletSpeed * (config ? config->bulletSpeedScale : 1.0f) * tankspecial::RailSpeedScale(charge);
    param.reflect = config && config->reflect;
    param.penetrate = true;
    ApplyRunProjectileRules(param);
    param.actorPierceCount = (std::max)(3, param.actorPierceCount);
    param.bulletHp = (std::max)(2.0f + charge * 3.0f, param.bulletHp);
    const size_t barrelCount = config && !config->barrels.empty() ? (std::min)(config->barrels.size(), size_t{8}) : 1;
    const bool alternate = config && config->alternateBarrels && barrelCount > 1;
    const size_t count = alternate ? 1 : barrelCount;
    for (size_t shot = 0; shot < count; ++shot) {
        const size_t index = alternate ? static_cast<size_t>(shootBarrelIndex_) % barrelCount : shot;
        cg2::Vector3 fire = aim, muzzle = GetWorldPosition() + aim * 1.7f;
        float damageScale = 1, speedScale = 1;
        if (config && !config->barrels.empty()) {
            const auto& mount = config->barrels[index];
            if (!mount.fires)
                continue;
            fire = RotateDirection(aim, mount.angleDeg);
            muzzle = GetWorldPosition() + aim * mount.offset.x + side * mount.offset.y + fire * mount.muzzleForward;
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
        if (index < barrels_.size()) {
            barrels_[index].muzzleFlashTimer = kMuzzleFlashDuration;
            barrels_[index].recoilOffset = .32f;
        }
        if (pendingSpecialCombatEvents_.size() < 32)
            pendingSpecialCombatEvents_.push_back({SpecialEventKind::RailShot, muzzle, fire, charge});
    }
    if (alternate)
        shootBarrelIndex_ = static_cast<int>((static_cast<size_t>(shootBarrelIndex_) + 1) % barrelCount);
    bulletCoolTime =
        tankspecial::RailRecovery(stats_.reloadSpeed / 60.0f * (config ? config->reloadScale : 1.0f) * GetRunFireIntervalScale());
    velocity_ += aim * (-.04f - .035f * charge);
    primaryAttackPerformedEvent_ = true;
    ++primaryAttackCount_;
    ++specialCombatStats_.railShots;
}

std::vector<Player::SpecialCombatEvent> Player::ConsumeSpecialCombatEvents()
{
    auto events = std::move(pendingSpecialCombatEvents_);
    pendingSpecialCombatEvents_.clear();
    return events;
}

void Player::UpdateSpecialCombat(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt)
{
    // 表示用の接続線は毎回作り直す。更新対象外でも前回の接触表示を残さない。
    droneLaserLinks_.clear();
    if (!runModifiers_.enabled || isDead_ || !bullets || dt <= 0)
        return;
    UpdateAdditionalAbilities(stage, bullets, boss, enemies, dt);
    linkDamageClock_.Advance(dt);
    auto blocked = [&](const cg2::Vector3& a, const cg2::Vector3& b) {
        for (const auto& block : stage.GetMergedBlocks()) {
            if (tankspecial::SegmentCrossesBox(a.x, a.y, b.x, b.y, block.aabb.min.x, block.aabb.min.y, block.aabb.max.x, block.aabb.max.y))
                return true;
        }
        return false;
    };
    auto emit = [&](SpecialEventKind kind, const cg2::Vector3& origin, const cg2::Vector3& direction, float strength = 1.0f) {
        if (pendingSpecialCombatEvents_.size() < 32)
            pendingSpecialCombatEvents_.push_back({kind, origin, direction, strength});
    };
    // 再構築中・死亡済みのドローンを除き、2機なら1本、3機以上なら輪になるよう接続する。
    if (IsDroneBuild() && runModifiers_.droneLaserLink) {
        std::vector<cg2::Vector3> positions;
        positions.reserve(drones_.size());
        for (const auto& drone : drones_)
            if (drone && drone->IsRunAvailable())
                positions.push_back(drone->GetWorldPosition());
        const int count = tankspecial::LinkCount(static_cast<int>(positions.size()));
        for (int i = 0; i < count; ++i) {
            const auto& a = positions[static_cast<size_t>(i)];
            const auto& b = positions[(static_cast<size_t>(i) + 1) % positions.size()];
            if (cg2::Length(b - a) > .10f && !blocked(a, b))
                droneLaserLinks_.push_back({a, b, false});
        }
        const auto tuning = MakeTankDroneTuning(runModifiers_, true);
        const auto* config = GetCurrentClassConfig();
        const float baseDamage = stats_.bulletDamage * tuning.damageScale * (config ? config->bulletDamageScale : 1.0f) * .45f *
                                 TankEffectPower(runModifiers_, 21);
        auto contact = [&](Collider* target, bool bossTarget) {
            if (!target)
                return;
            const auto p = target->GetWorldPosition();
            for (auto& link : droneLaserLinks_) {
                if (!tankspecial::SegmentTouches(link.start.x, link.start.y, link.end.x, link.end.y, p.x, p.y, target->GetRadius() + .10f))
                    continue;
                const float t = tankspecial::SegmentClosestFraction(link.start.x, link.start.y, link.end.x, link.end.y, p.x, p.y);
                const cg2::Vector3 nearest = link.start + (link.end - link.start) * t;
                if (blocked(nearest, p))
                    continue;
                // 接触表示はダメージ間隔とは独立する。同じ対象への適用許可は全ての線で共有する。
                link.contact = true;
                if (linkDamageClock_.Claim(target->GetCollisionId())) {
                    const auto damage = static_cast<uint32_t>((std::max)(1.0f, std::round(baseDamage * (bossTarget ? .65f : 1.0f) *
                                                                                          GetDroneTargetDamageScale(target, bossTarget))));
                    if (bossTarget)
                        static_cast<Enemy*>(target)->TakeDamage(damage);
                    else
                        static_cast<ExpEnemy*>(target)->TakeDirectionalDamage(damage, nearest);
                    ++specialCombatStats_.linkTicks;
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
    if (!IsMeleeBuild() || specialMeleeElapsed_ < 0)
        return;
    specialMeleeElapsed_ += dt;
    const auto& swing = specialMeleeSwing_;
    // 準備時間を引いた秒数で斬撃の有効時間とジャストパリィの窓を判定する。
    const float active = specialMeleeElapsed_ - swing.windupDuration;
    if (active < 0)
        return;
    if (runModifiers_.slashWave && !specialWaveEmitted_ && tankspecial::EmitsSlashWave(swing.comboStep)) {
        // 壁で生成できない場合も、この振りでの生成試行は消費する。
        specialWaveEmitted_ = true;
        const float speed = .48f;
        const float radius = (std::clamp)(swing.range * .25f, .7f, 2.2f);
        const cg2::Vector3 origin = GetWorldPosition() + swing.direction * (swing.range * .5f);
        if (!blocked(GetWorldPosition(), origin)) {
            auto wave = std::make_unique<Bullet>();
            wave->Initialize(origin, swing.direction * speed, tankspecial::SlashDamage(swing.damage, TankEffectPower(runModifiers_, 22)),
                             kPlayer, false, 10.0f, tankspecial::kOrdinaryEnemyBulletHp);
            wave->ConfigureGrowth(0, 2, 0);
            // 60基準フレームの速度を秒単位へ直し、移動距離から寿命を求める。
            wave->ConfigureSpecial(Bullet::SpecialKind::SlashWave, radius, swing.range * 1.5f / (speed * 60.0f));
            bullets->Add(std::move(wave));
            ++specialCombatStats_.slashWaves;
        }
    }
    if (active > swing.duration) {
        specialMeleeElapsed_ = -1;
        return;
    }
    if (!runModifiers_.parryBlade)
        return;
    const bool perfect = swing.comboStep >= 0 && tankspecial::IsPerfectParry(active);
    const cg2::Vector3 origin = GetWorldPosition();
    const float minDot = std::cos(swing.arcDeg * .5f * 3.1415926535f / 180.0f);
    // ポインター配列のコピーを走査する。反射弾を追加しても今回の走査対象へ混ぜない。
    for (auto* bullet : bullets->GetBulletPtrs()) {
        if (!bullet || bullet->IsDead() || bullet->GetOwner() != kEnemy)
            continue;
        if (std::find(specialParriedBullets_.begin(), specialParriedBullets_.end(), bullet->GetCollisionId()) !=
            specialParriedBullets_.end())
            continue;
        const auto p = bullet->GetWorldPosition();
        const cg2::Vector3 delta = p - origin;
        const float distance = cg2::Length(delta);
        if (distance > swing.range + bullet->GetRadius() || (distance > .001f && cg2::Dot(delta / distance, swing.direction) < minDot) ||
            blocked(origin, p))
            continue;
        // 耐久ダメージが0の場合も履歴へ記録し、同じ振りでは再判定しない。
        specialParriedBullets_.push_back(bullet->GetCollisionId());
        const float durability = tankspecial::ParryDurabilityDamage(bullet->GetBulletHp(), perfect, TankEffectPower(runModifiers_, 23));
        if (durability <= 0)
            continue;
        const cg2::Vector3 incoming = bullet->GetMove();
        bullet->ApplyBulletDurabilityDamage(durability);
        ++specialCombatStats_.parries;
        if (perfect) {
            ++specialCombatStats_.perfectParries;
            if (bullet->IsDead()) {
                const cg2::Vector3 reflected = cg2::Length(incoming) > .001f ? cg2::Normalize(incoming) * -1.0f : swing.direction;
                auto shot = std::make_unique<Bullet>();
                const auto damage = static_cast<uint32_t>(
                    (std::max)(1.0f, (std::min)(static_cast<float>(bullet->GetDamage()), stats_.bulletDamage * 3.8f)));
                shot->Initialize(p, reflected * (std::max)(.25f, cg2::Length(incoming)), damage, kPlayer, false, 1, 1);
                shot->ConfigureGrowth(0, 0, 0);
                shot->ConfigureSpecial(Bullet::SpecialKind::ParryReflection, .35f, 1.4f);
                shot->SetArmorReflected(bullet->WasArmorReflected());
                bullets->Add(std::move(shot));
            }
            if (!specialPerfectFeedback_) {
                emit(SpecialEventKind::PerfectParry, p, swing.direction);
                specialPerfectFeedback_ = true;
            } else
                emit(SpecialEventKind::Parry, p, swing.direction, .5f);
        } else
            emit(SpecialEventKind::Parry, p, swing.direction);
    }
}

void Player::SetRunHomingTargets(const std::vector<cg2::Vector3>& targets)
{
	const size_t count = (std::min)(targets.size(), size_t{48});
	runHomingTargets_.assign(targets.begin(), targets.begin() + count);
}

void Player::UpdateRunProjectiles(BulletManager* bulletManager, float deltaTime)
{
    if (!runModifiers_.enabled || (!runModifiers_.homing && !runModifiers_.droneFocus && !runModifiers_.targetPainter) ||
        deltaTime <= 0.0f || runHomingTargets_.empty())
        return;
    const float homingTurnRate = (std::max)(MakeTankRunSynergy(runModifiers_, runOverdriveTimer_ > 0.0f).homingTurnRate,
                                            IsDroneBuild() && runModifiers_.droneFocus ? .70f : 0.0f) *
                                 (empJammerTimer_ > 0 ? .55f : 1.0f);
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
        for (const cg2::Vector3& target : runHomingTargets_) {
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
        if (IsDroneBuild() && runModifiers_.targetPainter && bullet->GetSourceDroneIndex() >= 0) {
            for (const auto& lock : targetLockVisuals_)
                if (lock.remaining > 0) {
                    const auto offset = lock.position - bullet->GetWorldPosition();
                    if (cg2::Length(offset) > .001f && cg2::Length(cg2::Normalize(offset) - targetDirection) < .1f)
                        targetTurn = (std::max)(targetTurn, 1.05f * (empJammerTimer_ > 0 ? .55f : 1.0f));
                }
        }
        // 回頭速度はラジアン/秒。目標を越えない角度に制限し、度へ直して元の速さを保った速度の向きを設定する。
        // ここでは位置を進めない。設定した速度による位置更新はBullet::Updateが行う。
        const float turn = (std::min)(std::acos(dot), targetTurn * deltaTime);
        const float cross = direction.x * targetDirection.y - direction.y * targetDirection.x;
        const float signedDegrees = turn * (cross < 0.0f ? -1.0f : 1.0f) * (180.0f / 3.1415926535f);
        bullet->SetVelocity(RotateDirection(direction, signedDegrees) * speed);
    }
}

void Player::ConfigureRunDrone(PlayerDrone& drone) const
{
    if (!runModifiers_.enabled) {
        return;
    }
    const PlayerClassConfig* config = GetCurrentClassConfig();
    const bool primaryStyle = IsDroneBuild();
    const bool isSwarm = config && config->usesDrone;
    auto droneTuning = MakeTankDroneTuning(runModifiers_, primaryStyle);
    if (!primaryStyle && isSwarm) {
        droneTuning.damageScale *= 0.6f / 0.35f;
        droneTuning.reloadSeconds *= 0.5f / 0.75f;
    }
    AttackParam param{};
    param.bulletSpeed = stats_.bulletSpeed * (config ? config->bulletSpeedScale : 1.0f);
    if (primaryStyle && runModifiers_.droneFocus)
        param.bulletSpeed *= 1.15f;
    param.bulletCount = 1;
    param.spreadAngleDeg = primaryStyle || runModifiers_.droneFocus ? droneTuning.spreadDegrees
                           : isSwarm && runEvolutionActive_         ? config->spreadAngleDeg
                                                                    : (isSwarm ? 10.0f : 6.0f);
    param.randomSpread = param.spreadAngleDeg > 0.0f;
    param.reflect = config && config->reflect;
    param.damage = static_cast<uint32_t>(
        (std::max)(1.0f, std::round(stats_.bulletDamage * (config ? config->bulletDamageScale : 1.0f) * droneTuning.damageScale *
                                    (primaryStyle && runModifiers_.autonomousSpread
                                         ? (std::max)(.2f, 1.0f - .18f * TankEffectPower(runModifiers_, 38))
                                         : 1.0f))));
    if (isBuffActive_) {
        param.reflect = true;
    }
    ApplyRunProjectileRules(param);
    param.bulletHp = (std::max)(param.bulletHp, droneTuning.bulletHp);
    param.bulletPenetration = (std::max)(param.bulletPenetration, droneTuning.interception);
    param.bulletVisualScale = droneTuning.sizeScale;
    param.bulletTrailScale = droneTuning.trailScale;
    // 基準フレーム同士の比率を秒単位のドローン発射間隔へ掛ける。
    const float reloadRatio = stats_.reloadSpeed / GetRunBaseReloadFrames();
    const float coreRate = runModifiers_.core == TankRunCore::Drone ? 0.8f : 1.0f;
    const auto* specialization = runEvolutionActive_ ? FindTankExpeditionSpecialization(runEvolutionConfig_.id) : nullptr;
    const float interval = droneTuning.reloadSeconds * reloadRatio * coreRate * (isBuffActive_ ? 0.7f : 1.0f) * GetRunFireIntervalScale() *
                           (primaryStyle && config      ? config->reloadScale
                            : isSwarm && specialization ? specialization->reloadScale
                                                        : 1.0f) *
                           (primaryStyle ? GetCombatStyleProfile(tankbuild::Style::Drone).attackIntervalSeconds / .5f : 1.0f) *
                           (empJammerTimer_ > 0 ? 1.5f : 1.0f);
    drone.ConfigureRunAttack(param, interval);
    // 主装備のドローンは小数の威力を別に保持し、射撃ごとの余りを繰り越す。
    // 例えば3×0.82を毎回整数へ切り捨てて2にする場合の、過剰な威力低下を避ける。
    drone.SetRunExactDamage(
        primaryStyle
            ? (std::max)(1.0f,
                         stats_.bulletDamage * (config ? config->bulletDamageScale : 1.0f) * droneTuning.damageScale *
                             (runModifiers_.autonomousSpread ? (std::max)(.2f, 1.0f - .18f * TankEffectPower(runModifiers_, 38)) : 1.0f))
            : 0.0f);
    if (primaryStyle) {
        const auto& profile = GetCombatStyleProfile(tankbuild::Style::Drone);
        drone.SetRunFollowTuning(profile.droneFollowSpeed, profile.droneCatchupSpeed, profile.droneResponse);
    }
}

void Player::Die()
{
	if (isDead_) return;

	isDead_ = true;
	isExploding_ = true;

	SpawnParticles();

	// 生成済みの弾はここでは消去しない。弾の消去はBulletManager側で行う。

}

bool Player::isFinished()
{
	if (isDead_ && !isExploding_) {
		return true;
	}
	return false;
}

void Player::UpdateDefeatPresentation(float deltaTime)
{
	if (isExploding_) {
		UpdateParticles((std::max)(0.0f, deltaTime));
	}
}

int Player::GetNextLevelExp() const
{
	// 簡易的な計算式（必要に応じて調整）
	return level_ * 10 + 20;
}

void Player::Evolve(ClassType newClass)
{
	EvolveById(ClassTypeToString(newClass));
}

bool Player::TryActivateSpecialAction()
{
    const PlayerClassConfig* config = GetCurrentClassConfig();
    if (!config || dashCooldown_ > 0.0f) {
        return false;
    }
    if (runModifiers_.enabled)
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

bool Player::ActivatePerfectDodge(const PlayerClassConfig& config)
{
    const float staminaCost = (std::max)(0.0f, config.specialActionStaminaCost);
    if (stats_.stamina < staminaCost) {
        return false;
    }

    cg2::Vector3 dashDir = inputDir_;
    if (cg2::Length(dashDir) < 0.01f) {
        dashDir = dir_;
    }
    if (cg2::Length(dashDir) < 0.01f) {
        return false;
    }

    const TankRunTuning runTuning = MakeTankRunTuning(runModifiers_, runGrowth_);
    const cg2::Vector3 direction = cg2::Normalize(dashDir);
    const float speed = kDashSpeed * runTuning.dashSpeed;
    velocity_ = {TankDashMomentum(velocity_.x, direction.x, speed), TankDashMomentum(velocity_.y, direction.y, speed), 0};
    // 新しいダッシュでだけ体当たり履歴を解除する。接触が続いても同じ対象へ繰り返し適用しない。
    dashImpactTargets_.clear();
    ++dashStartedCount_;
    isDashing_ = true;
    dashStartedEvent_ = true;
    dashTimer_ = kDashDuration;
    recentDashTimer_ = .30f;
    dashCooldown_ = kDashCooldown * (std::max)(0.05f, config.specialActionCooldownScale) * runTuning.dashCooldown;
    stats_.stamina = (std::max)(0.0f, stats_.stamina - staminaCost);
    if (runModifiers_.enabled) {
        runDashAttackTimer_ = 1.0f;
        // 追加射撃はPlayer::Updateの攻撃処理まで保留し、その時点の向きで生成する。
        runDashBurstPending_ = runModifiers_.dashBurst;
        if (runModifiers_.core == TankRunCore::Assault) {
            bulletCoolTime = 0.0f;
            std::fill(weaponGroupCooldowns_.begin(), weaponGroupCooldowns_.end(), 0.0f);
        }
        if (runModifiers_.overdrive && runOverdriveCooldown_ <= 0.0f) {
            runOverdriveTimer_ = 1.2f * TankEffectPower(runModifiers_, 11);
            runOverdriveCooldown_ = 4.0f;
        }
        if (runModifiers_.core == TankRunCore::Drone) {
            for (auto& drone : drones_)
                drone->RallyRunAttack();
        }
    }
    return true;
}

bool Player::ActivateSaberCounter(const PlayerClassConfig& config)
{
    const bool hasMeleeWeapon = std::any_of(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
        return mount.fires && mount.weaponType == WeaponType::Melee;
    });
    const float staminaCost = (std::max)(0.0f, config.specialActionStaminaCost);
    if (!hasMeleeWeapon || stats_.stamina < staminaCost) {
        return false;
    }

    saberCounterTimer_ = (std::max)(0.01f, config.saberCounterWindow);
    dashCooldown_ = 1.0f * (std::max)(0.05f, config.specialActionCooldownScale);
    stats_.stamina = (std::max)(0.0f, stats_.stamina - staminaCost);
    return true;
}

void Player::TriggerSaberCounter(const PlayerClassConfig& config)
{
    const auto mountIt = std::find_if(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
        return mount.fires && mount.weaponType == WeaponType::Melee;
    });
    if (mountIt == config.barrels.end()) {
        return;
    }

    const WeaponMountConfig& mount = *mountIt;
    const cg2::Vector3 forward = cg2::Length(dir_) > 0.0001f ? cg2::Normalize(dir_) : cg2::Vector3{1.0f, 0.0f, 0.0f};
    const cg2::Vector3 right = {-forward.y, forward.x, 0.0f};
    MeleeSlashEvent event{};
    event.origin = worldTransform_.translate + forward * mount.offset.x + right * mount.offset.y + cg2::Vector3{0.0f, 0.0f, mount.offset.z};
    event.direction = RotateDirection(forward, mount.angleDeg);
    event.range = mount.meleeRange * config.saberCounterRangeScale;
    event.arcDeg = (std::max)(180.0f, mount.meleeArcDeg);
    event.width = mount.meleeWidth * 1.35f;
    event.duration = (std::max)(0.08f, mount.meleeDuration * 0.85f);
    event.windupDuration = 0.0f;
    event.recoveryDuration = 0.22f;
    event.comboStep = 2;
    event.damage = static_cast<uint32_t>((std::max)(1.0f, stats_.bulletDamage * mount.damageScale * config.saberCounterDamageScale));
    event.color = {0.65f, 1.45f, 1.25f, 1.0f};
    // 命中判定と描画はシーンへ渡す斬撃イベントに予約し、ここでは受付を閉じて無敵とスローを要求する。
    pendingMeleeSlashes_.push_back(event);
    saberCounterTimer_ = 0.0f;
    invincibleTimer_ = 0.28f;
    requestSlow_ = true;
}

void Player::EvolveById(const std::string& classId)
{
	const PlayerClassConfig* config = GetClassConfig(classId);
	if (!config || !IsRunCompatibleClass(*config)) {
		return;
	}

	currentClassId_ = config->id;
	currentClass_ = config->type;
	bulletCoolTime = 0.0f;
	shootBarrelIndex_ = 0;
	shootGroupIndex_ = 0;
	weaponGroupCooldowns_.clear();
	if (runModifiers_.enabled) {
		drones_.clear();
		runSupportDroneTimer_ = 0.0f;
	}

	// 進化時に特殊状態をリセットする
	isSmash_ = false;
	smashCharge_ = 0.0f;
	isStealth_ = false;

	// 旧機体の切替用に残っている互換分岐。現在の砲塔・外観の反映は下の再構築で行う。
	switch (currentClass_) {
	case ClassType::Twin:

		break;
	}
	InitializeBarrels();
	UpdateBarrelLayout();

	isChangeMode = false;
}

bool Player::IsRunCompatibleClass(const PlayerClassConfig& config) const
{
	if (!runModifiers_.enabled) return true;
	// Smasher bypasses the configured guns and always performs a melee attack.
	if (config.type == ClassType::Smasher) return false;
	if (config.usesDrone) return true;
	return std::any_of(config.barrels.begin(), config.barrels.end(), [](const WeaponMountConfig& mount) {
		return mount.fires && mount.weaponType == WeaponType::Projectile;
	});
}

bool Player::IsEvolutionClassVisible(const std::string& classId) const
{
	// Keep the current node visible if an editor changes its weapon configuration.
	if (!runModifiers_.enabled || classId == currentClassId_) return true;
	const PlayerClassConfig* config = GetClassConfig(classId);
	return config && IsRunCompatibleClass(*config);
}

bool Player::CanEvolveTo(const std::string& classId) const
{
	const PlayerClassConfig* currentConfig = GetCurrentClassConfig();
	const PlayerClassConfig* targetConfig = GetClassConfig(classId);
	if (!currentConfig || !targetConfig || targetConfig->id == currentConfig->id ||
		!IsRunCompatibleClass(*targetConfig)) {
		return false;
	}
	if (!evolutionCircuitLoaded_ ||
		!HasEvolutionEdge(currentConfig->id, targetConfig->id) ||
		targetConfig->requiredRank != currentConfig->requiredRank + 1) {
		return false;
	}
	return GetRankFromLevel(level_) >= targetConfig->requiredRank;
}

bool Player::HasEvolutionEdge(const std::string& from, const std::string& to) const
{
	return std::any_of(
		evolutionCircuitEdges_.begin(),
		evolutionCircuitEdges_.end(),
		[&](const EvolutionCircuitEdgeDefinition& edge) {
			return edge.from == from && edge.to == to;
		});
}

bool Player::TryConfirmEvolutionById(const std::string& classId)
{
	if (!CanEvolveTo(classId)) {
		return false;
	}
	const std::string previousClassId = currentClassId_;
	EvolveById(classId);
	if (evolutionHistory_.empty() || evolutionHistory_.back() != previousClassId) {
		evolutionHistory_.clear();
		evolutionHistory_.push_back(previousClassId);
	}
	evolutionHistory_.push_back(classId);
	evolutionConfirmedEvent_ = true;
	return true;
}

bool Player::ConsumeEvolutionConfirmed()
{
	const bool confirmed = evolutionConfirmedEvent_;
	evolutionConfirmedEvent_ = false;
	return confirmed;
}

void Player::CloseEvolutionUiForTutorial()
{
	isChangeMode = false;
}

bool Player::ConsumeEvolutionCancelled()
{
	const bool cancelled = evolutionCancelledEvent_;
	evolutionCancelledEvent_ = false;
	return cancelled;
}

bool Player::LoadPlayerClassConfigs(const std::string& path)
{
	PlayerClassCatalog loadedCatalog;
	if (!loadedCatalog.Load(path)) return false;
	if (!loadedCatalog.Find(currentClassId_)) {
		std::cerr << "[PlayerClass] Reload failed: current class is missing: " << currentClassId_ << std::endl;
		return false;
	}
	// 現在の機体が残ることを確認してから確定する。失敗時はHP・装備・設定をそのまま保つ。
	classCatalog_.Swap(loadedCatalog);
	return true;
}

bool Player::ReloadPlayerClassConfigs(const std::string& path)
{
	const std::string activeClassId = currentClassId_;
	if (!LoadPlayerClassConfigs(path)) {
		return false;
	}

	const PlayerClassConfig* config = GetClassConfig(activeClassId);
	if (!config) {
		std::cerr << "[PlayerClass] Reload failed: current class is unavailable." << std::endl;
		return false;
	}
	currentClassId_ = activeClassId;
	currentClass_ = config->type;
	shootBarrelIndex_ = 0;
	shootGroupIndex_ = 0;
	weaponGroupCooldowns_.clear();
	InitializeBarrels();
	UpdateBarrelLayout();
	std::cerr << "[PlayerClass] Reload succeeded. Current class: "
		<< currentClassId_ << std::endl;
	return true;
}

Player::PlayerClassConfig Player::CreateDefaultClassConfig(ClassType type) const
{
	return PlayerClassCatalog::CreateDefaultConfig(type);
}

void Player::SavePlayerClassConfigs(const std::string& path) const
{
	classCatalog_.Save(path);
}

const Player::PlayerClassConfig* Player::GetClassConfig(ClassType type) const
{
	return classCatalog_.Find(type);
}

const Player::PlayerClassConfig* Player::GetClassConfig(const std::string& classId) const
{
	return classCatalog_.Find(classId);
}

const Player::PlayerClassConfig* Player::GetCurrentClassConfig() const
{
	if (runModifiers_.enabled && runEvolutionActive_) return &runEvolutionConfig_;
	if (runModifiers_.enabled && expeditionCombatStyleSelected_) return &runStarterConfig_;
    if (runModifiers_.enabled && runCheckpointEvolution_ && !runStarterConfig_.barrels.empty()) return &runStarterConfig_;
	return GetClassConfig(currentClassId_);
}

bool Player::GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const
{
	const PlayerClassConfig* config = GetClassConfig(classId);
	if (!config) {
		return false;
	}
	output = {};
	output.classId = config->id;
	output.rank = (std::clamp)(config->requiredRank, 1, 4);
	output.bodyScale = config->bodyScale;
	output.bodyFillColor = config->bodyFillColor;
	output.bodyOutlineColor = config->bodyOutlineColor;
	output.weaponMounts = config->barrels;
	output.usesDrone = config->usesDrone;
	switch (config->bodyShape) {
	case BodyShape::Box:
		output.bodyShape = TankButtonBodyShape::Box;
		break;
	case BodyShape::Triangle:
		output.bodyShape = TankButtonBodyShape::Triangle;
		break;
	case BodyShape::Pentagon:
		output.bodyShape = TankButtonBodyShape::Pentagon;
		break;
	case BodyShape::Circle:
	default:
		output.bodyShape = TankButtonBodyShape::Circle;
		break;
	}
	if (classId == "Twin") {
		output.hiraganaName = "ついん";
	} else if (classId == "MachineGun") {
		output.hiraganaName = "ましんがん";
	} else if (classId == "Overseer") {
		output.hiraganaName = "おーばーしあ";
	} else if (classId == "Basic") {
		output.hiraganaName = "べーしっく";
	} else {
		output.hiraganaName = config->displayName;
	}
	return true;
}

Player::PlayerClassConfig* Player::GetMutableClassConfig(const std::string& classId)
{
	return classCatalog_.FindMutable(classId);
}

std::vector<Player::LaserShotEvent> Player::ConsumeLaserShotEvents()
{
    std::vector<LaserShotEvent> events = std::move(pendingLaserShots_);
    pendingLaserShots_.clear();
    return events;
}

std::vector<Player::MineDropEvent> Player::ConsumeMineDropEvents()
{
    std::vector<MineDropEvent> events = std::move(pendingMineDrops_);
    pendingMineDrops_.clear();
    return events;
}

std::vector<Player::MeleeSlashEvent> Player::ConsumeMeleeSlashEvents()
{
    std::vector<MeleeSlashEvent> events = std::move(pendingMeleeSlashes_);
    pendingMeleeSlashes_.clear();
    return events;
}

bool Player::FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload, cg2::Vector3& recoilDir,
                                 float& recoilPower)
{
    // ドローン生成の成功判定は射撃処理の受付を表し、新しい弾が登録された保証ではない。
    if (config.usesDrone) {
        if (bulletCoolTime > 0.0f) {
            return false;
        }
        DroneShoot(bulletManager);
        recoilPower = 0.0f;
        bulletCoolTime = baseReload * config.reloadScale;
        return true;
    }
    // この内訳は削除待ちの死亡弾も含む。遠征の受付上限は生存数によるAddの上限とは別に判定する。
    if (runModifiers_.enabled && bulletManager->GetBulletCounts().player >= 240)
        return false;

    AttackParam param{};
    param.bulletSpeed = stats_.bulletSpeed * config.bulletSpeedScale;
    param.bulletCount = 1;
    param.spreadAngleDeg = config.spreadAngleDeg;
    param.randomSpread = config.randomSpread;
    param.reflect = config.reflect;
    param.penetrate = config.penetrate;
    param.cooldown = 1.0f;
    const float shotDamage = stats_.bulletDamage * config.bulletDamageScale;
    param.damage = static_cast<uint32_t>((std::max)(1.0f, runModifiers_.enabled ? std::round(shotDamage) : shotDamage));
    ApplyRunProjectileRules(param);

    if (isBuffActive_) {
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
            if (weaponGroupCooldowns_.size() != fireGroups.size()) {
                weaponGroupCooldowns_.assign(fireGroups.size(), 0.0f);
                shootGroupIndex_ = 0;
            }
            // 前回の次のグループから、待ち時間が終わったグループを探す。
            for (size_t attempt = 0; attempt < fireGroups.size(); ++attempt) {
                const size_t slot = (static_cast<size_t>(shootGroupIndex_) + attempt) % fireGroups.size();
                if (weaponGroupCooldowns_[slot] <= 0.0f) {
                    selectedGroupSlot = static_cast<int>(slot);
                    break;
                }
            }
            if (selectedGroupSlot < 0) {
                bulletCoolTime = 0.0f;
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
            shootGroupIndex_ = (selectedGroupSlot + 1) % static_cast<int>(fireGroups.size());
        } else {
            if (bulletCoolTime > 0.0f) {
                return false;
            }
            const size_t selectableCount = fireIndices.size();
            const size_t index = fireIndices[shootBarrelIndex_ % selectableCount];
            fireIndices = {index};
            shootBarrelIndex_ = static_cast<int>((shootBarrelIndex_ + 1) % selectableCount);
        }
    } else if (bulletCoolTime > 0.0f) {
        return false;
    }

    const cg2::Vector3 forward = cg2::Length(dir_) > 0.0001f ? cg2::Normalize(dir_) : cg2::Vector3{1.0f, 0.0f, 0.0f};
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
    if (firesMelee && meleeComboTimer_ <= 0.0f) {
        meleeComboStep_ = 0;
    }
    // 今回選ばれた全ての近接砲身でコンボ段階を共有し、次回用の段階は一度だけ進める。
    const int meleeComboStepForShot = (std::clamp)(meleeComboStep_, 0, 2);
    float meleeActionDuration = 0.0f;
    if (firesMelee) {
        meleeComboStep_ = (meleeComboStepForShot + 1) % 3;
        meleeComboTimer_ = (std::max)(0.05f, meleeComboResetTime);
    }

    cg2::Vector3 combinedRecoil{};
    float firedReloadScale = 1.0f;
    for (size_t index : fireIndices) {
        const WeaponMountConfig& barrelConfig = config.barrels[index];
        const cg2::Vector3 fireDir = RotateDirection(forward, barrelConfig.angleDeg);
        combinedRecoil = combinedRecoil + fireDir * (-(std::max)(0.0f, barrelConfig.recoilScale));
        firedReloadScale = (std::max)(firedReloadScale, barrelConfig.reloadScale);
        // 砲身の前後・横方向のオフセットを自機のXY軸へ変換し、Zオフセットはそのまま加える。
        const cg2::Vector3 mountBase = worldTransform_.translate + forward * barrelConfig.offset.x + right * barrelConfig.offset.y +
                                       cg2::Vector3{0.0f, 0.0f, barrelConfig.offset.z};
        const cg2::Vector3 muzzle = mountBase + fireDir * barrelConfig.muzzleForward;
        AttackParam mountParam = param;
        mountParam.damage = static_cast<uint32_t>(
            (std::max)(1.0f, runModifiers_.expedition ? std::round(static_cast<float>(param.damage) * barrelConfig.damageScale)
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
            pendingLaserShots_.push_back(event);
        } else if (barrelConfig.weaponType == WeaponType::Mine) {
            MineDropEvent event{};
            event.position = muzzle;
            event.radius = barrelConfig.mineRadius;
            event.fuseTime = barrelConfig.mineFuseTime;
            event.lifeTime = barrelConfig.mineLifeTime;
            event.damage = mountParam.damage;
            event.color = barrelConfig.effectColor;
            pendingMineDrops_.push_back(event);
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
            pendingMeleeSlashes_.push_back(event);
            meleeActionDuration = (std::max)(meleeActionDuration, meleeWindup + barrelConfig.meleeDuration + meleeRecovery);
        } else {
            if (runModifiers_.enabled) {
                const size_t active = bulletManager->GetBulletCounts().player;
                if (active >= 240)
                    break;
                mountParam.bulletCount = (std::min)(mountParam.bulletCount, static_cast<int>(240 - active));
            }
            mountParam.bulletSpeed *= barrelConfig.projectileSpeedScale;
            attackController_.FireFromMuzzle(muzzle, fireDir, mountParam, BulletOwner::kPlayer);
            if (index < barrels_.size()) {
                barrels_[index].muzzleFlashTimer = kMuzzleFlashDuration;
            }
            SpawnCasing();
        }
        if (index < barrels_.size()) {
            barrels_[index].recoilOffset = 0.22f;
        }
    }

    // 発射間隔は秒。今回の斬撃が終わる時間も含め、選択したグループだけ、または全体の待ち時間を更新する。
    const float reloadTime = (std::max)({baseReload * config.reloadScale * firedReloadScale, meleeActionDuration,
                                         expeditionCombatStyleSelected_ ? 0.05f : 0.0f});
    if (usesGroupCooldowns && selectedGroupSlot >= 0 && static_cast<size_t>(selectedGroupSlot) < weaponGroupCooldowns_.size()) {
        weaponGroupCooldowns_[static_cast<size_t>(selectedGroupSlot)] = reloadTime;
        bulletCoolTime = 0.0f;
    } else {
        bulletCoolTime = reloadTime;
    }
    const float combinedRecoilLength = cg2::Length(combinedRecoil);
    if (combinedRecoilLength > 0.0001f) {
        recoilDir = combinedRecoil * (1.0f / combinedRecoilLength);
        recoilPower = config.recoilPower * combinedRecoilLength;
    } else {
        recoilDir = cg2::Normalize(dir_) * -1.0f;
        recoilPower = 0.0f;
    }
    return true;
}

cg2::Vector3 Player::RotateDirection(const cg2::Vector3& direction, float angleDeg) const
{
	const float rad = angleDeg * 3.1415926535f / 180.0f;
	return {
		direction.x * cosf(rad) - direction.y * sinf(rad),
		direction.x * sinf(rad) + direction.y * cosf(rad),
		direction.z
	};
}

void Player::InitializeBarrels()
{
	barrels_.clear();
	const PlayerClassConfig* config = GetCurrentClassConfig();
	const size_t barrelCount = config ? config->barrels.size() : 1u;
	barrels_.reserve(barrelCount);
	for (size_t i = 0; i < barrelCount; ++i) {
		BarrelModel barrel{};
		barrel.object = std::make_unique<cg2::Object3d>();
		barrel.object->Initialize();
		barrel.object->SetModel(config ? config->barrels[i].model : "gunBarrel.obj");
		barrel.object->SetColor(cg2::Vector4(0.48f, 0.86f, 0.22f, 1.0f));
		baseBarrelColor_ = cg2::Vector4(0.48f, 0.86f, 0.22f, 1.0f);
		barrel.transform = cg2::InitWorldTransform();
		barrel.transform.scale = config ? config->barrels[i].scale : cg2::Vector3{ 1.25f, 0.24f, 0.24f };
		barrels_.push_back(std::move(barrel));
	}
}

void Player::UpdateBarrelLayout()
{
	if (barrels_.empty()) {
		return;
	}

	const cg2::Vector3 forward = cg2::Length(dir_) > 0.0001f ? cg2::Normalize(dir_) : cg2::Vector3{ 1.0f, 0.0f, 0.0f };
	const cg2::Vector3 right = { -forward.y, forward.x, 0.0f };
	const PlayerClassConfig* config = GetCurrentClassConfig();
	const float recoilReturn = 0.055f;

	for (size_t i = 0; i < barrels_.size(); ++i) {
		BarrelModel& barrel = barrels_[i];
		const WeaponMountConfig barrelConfig = (config && i < config->barrels.size()) ? config->barrels[i] : WeaponMountConfig{};
		const bool active = config ? i < config->barrels.size() : i == 0;

		barrel.recoilOffset = (std::max)(0.0f, barrel.recoilOffset - recoilReturn * dt_ * 60.0f);
		barrel.muzzleFlashTimer = (std::max)(0.0f, barrel.muzzleFlashTimer - dt_);
		barrel.localOffset = forward * (barrelConfig.offset.x - barrel.recoilOffset) + right * barrelConfig.offset.y + cg2::Vector3{ 0.0f, 0.0f, barrelConfig.offset.z };
		barrel.transform.translate = worldTransform_.translate + barrel.localOffset;
		barrel.transform.rotate = worldTransform_.rotate;
		barrel.transform.rotate.z += barrelConfig.angleDeg * 3.1415926535f / 180.0f;
		barrel.transform.scale = active ? barrelConfig.scale : cg2::Vector3{ 0.0f, 0.0f, 0.0f };
		barrel.object->SetTransform(barrel.transform);
		barrel.object->Update();
	}
}

void Player::DrawBarrels()
{
	const PlayerClassConfig* config = GetCurrentClassConfig();
	const size_t activeCount = config ? config->barrels.size() : 1u;
	for (size_t i = 0; i < barrels_.size() && i < activeCount; ++i) {
		barrels_[i].object->Draw();
	}
}

void Player::SetVehicleAlpha(float alpha)
{
	float flash = 0.0f;
	if (damageFeedbackTimer_ > 0.0f && damageFeedbackDuration_ > 0.0f) {
		const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
		flash = t * t * 0.75f;
	}
	cg2::Vector4 vehicleColor = LerpColor(baseVehicleColor_, { 1.0f, 1.0f, 1.0f, baseVehicleColor_.w }, flash);
	vehicleColor.w = alpha;
	object_->SetColor(vehicleColor);
	for (BarrelModel& barrel : barrels_) {
		if (barrel.object) {
			cg2::Vector4 barrelColor = LerpColor(baseBarrelColor_, { 1.0f, 1.0f, 1.0f, baseBarrelColor_.w }, flash);
			barrelColor.w = alpha;
			barrel.object->SetColor(barrelColor);
		}
	}
}

void Player::TriggerDamageFeedback()
{
	damageFeedbackTimer_ = damageFeedbackDuration_;
}

void Player::InitializeEncyclopedia()
{
    cg2::StartupTrace::Scope scope("Player.EncyclopediaUi");
	cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
	auto makePanel = [spriteCommon](const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) {
		auto panel = std::make_unique<cg2::Sprite>();
		panel->Initialize(spriteCommon, "resources/white512x512.png");
		panel->SetPosition(pos);
		panel->SetSize(size);
		panel->SetColor(color);
		return panel;
	};

	evolutionBackdropSprite_ = makePanel({ 0.0f, 0.0f }, { 1280.0f, 720.0f }, { 0.02f, 0.03f, 0.07f, 0.78f });
	evolutionPreviewPanelSprite_ = makePanel({ 28.0f, 84.0f }, { 360.0f, 560.0f }, { 0.05f, 0.12f, 0.17f, 0.86f });
	evolutionStatsPanelSprite_ = makePanel({ 910.0f, 84.0f }, { 342.0f, 560.0f }, { 0.07f, 0.08f, 0.12f, 0.88f });
	evolutionPreviewTankSprite_ = std::make_unique<cg2::Sprite>();
	evolutionPreviewTankSprite_->Initialize(spriteCommon, "resources/normalTank.png");
	evolutionPreviewTankSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	evolutionPreviewTankSprite_->SetSize({ 250.0f, 150.0f });
	evolutionPreviewTankSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	evolutionShotSprite_ = makePanel({ 0.0f, 0.0f }, { 170.0f, 9.0f }, { 1.0f, 0.88f, 0.28f, 0.0f });
	evolutionShotSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	evolutionChangeButtonSprite_ = makePanel({ 940.0f, 650.0f }, { 290.0f, 48.0f }, { 0.24f, 0.86f, 0.44f, 0.92f });

	cg2::TextStyle titleStyle{};
	titleStyle.fontFamily = "Meiryo";
	titleStyle.fontSize = 34.0f;
	titleStyle.color = { 0.75f, 1.0f, 0.92f, 1.0f };
	titleStyle.outlineColor = { 0.0f, 0.08f, 0.10f, 0.95f };
	titleStyle.outlineThickness = 3.0f;
	titleStyle.padding = 8.0f;
	SetLabel(evolutionTitleLabel_, spriteCommon, "戦車図鑑 / 進化ツリー", { 40.0f, 28.0f }, titleStyle);

	cg2::TextStyle smallStyle = titleStyle;
	smallStyle.fontSize = 20.0f;
	smallStyle.color = { 0.86f, 0.92f, 1.0f, 1.0f };
	smallStyle.outlineThickness = 2.0f;
	SetLabel(evolutionHintLabel_, spriteCommon, "C:閉じる / カード選択:詳細 / ボタン:機体変更", { 520.0f, 42.0f }, smallStyle);

	encyclopedia_.clear();
	const float cardWidth = 154.0f;
	const float cardHeight = 80.0f;
	const float cardGapX = 12.0f;
	const float cardGapY = 12.0f;
	const cg2::Vector2 cardBase = { 416.0f, 112.0f };

	for (int i = 0; i < static_cast<int>(classCatalog_.OrderedIds().size()); ++i) {
		const PlayerClassConfig* config = GetClassConfig(classCatalog_.OrderedIds()[i]);
		if (!config) {
			continue;
		}
		TankData data;
		data.type = config->type;
		data.classId = config->id;
		data.name = config->displayName;
		data.requiredRank = config->requiredRank;
		data.texturePath = ClassTexturePath(config->type);

		const int col = i % 3;
		const int row = i / 3;
		const cg2::Vector2 cardPos = { cardBase.x + static_cast<float>(col) * (cardWidth + cardGapX), cardBase.y + static_cast<float>(row) * (cardHeight + cardGapY) };

		data.cardSprite = makePanel(cardPos, { cardWidth, cardHeight }, { 0.10f, 0.15f, 0.22f, 0.82f });
		data.sprite = std::make_unique<cg2::Sprite>();
		data.sprite->Initialize(cg2::SpriteCommon::GetInstance(), data.texturePath);
		data.sprite->SetPosition({ cardPos.x + 14.0f, cardPos.y + 8.0f });
		data.sprite->SetSize({ 126.0f, 38.0f });

		cg2::TextStyle cardNameStyle = smallStyle;
		cardNameStyle.fontSize = 14.0f;
		cardNameStyle.color = { 0.92f, 1.0f, 0.95f, 1.0f };
		SetLabel(data.nameLabel, spriteCommon, data.name, { cardPos.x + 10.0f, cardPos.y + 50.0f }, cardNameStyle);

		cg2::TextStyle rankStyle = cardNameStyle;
		rankStyle.fontSize = 12.0f;
		rankStyle.color = { 0.72f, 0.86f, 1.0f, 1.0f };
		SetLabel(data.rankLabel, spriteCommon, "R" + std::to_string(data.requiredRank), { cardPos.x + 112.0f, cardPos.y + 54.0f }, rankStyle);

		encyclopedia_.push_back(std::move(data));
	}
}

void Player::InitializeUpgradeHud()
{
    cg2::StartupTrace::Scope scope("Player.UpgradeHud");
	cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
	auto makePanel = [spriteCommon](const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) {
		auto panel = std::make_unique<cg2::Sprite>();
		panel->Initialize(spriteCommon, "resources/white512x512.png");
		panel->SetPosition(pos);
		panel->SetSize(size);
		panel->SetColor(color);
		return panel;
	};
	auto makePill = [spriteCommon](const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) {
		auto pill = std::make_unique<cg2::Sprite>();
		pill->Initialize(spriteCommon, "resources/hpBarFillMask.png");
		pill->SetPosition(pos);
		pill->SetSize(size);
		pill->SetColor(color);
		return pill;
	};

	upgradeHudBackdropSprite_ = makePanel(upgradeHudPanelPos_, upgradeHudPanelSize_, { 0.03f, 0.04f, 0.06f, 0.58f });
	upgradeHudExpBackSprite_ = makePanel(upgradeHudExpBarPos_, upgradeHudExpBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
	upgradeHudExpFillSprite_ = makePanel(upgradeHudExpBarPos_, { 0.0f, upgradeHudExpBarSize_.y }, { 0.96f, 0.83f, 0.24f, 0.95f });
	upgradeHudLevelBackSprite_ = makePanel(upgradeHudLevelBarPos_, upgradeHudLevelBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
	upgradeHudLevelFillSprite_ = makePanel({ 785.0f, 786.0f }, { 0.0f, 18.0f }, { 0.36f, 1.0f, 0.56f, 0.92f });

	upgradeHudLevelProgressStyle_.backgroundColor = { 0.018f, 0.042f, 0.062f, 0.88f };
	upgradeHudLevelProgressStyle_.delayedFillColor = { 0.20f, 0.72f, 1.00f, 0.52f };
	upgradeHudLevelProgressStyle_.fillColor = { 0.36f, 1.00f, 0.56f, 0.94f };
	upgradeHudLevelProgressStyle_.outlineColor = { 0.40f, 0.94f, 1.00f, 0.92f };
	upgradeHudLevelProgressStyle_.outlineWidth = 1.5f;
	upgradeHudLevelProgressStyle_.roundedEnds = upgradeHudRoundedProgressBars_;
	upgradeHudExpProgressStyle_.backgroundColor = { 0.050f, 0.042f, 0.022f, 0.88f };
	upgradeHudExpProgressStyle_.delayedFillColor = { 0.36f, 0.96f, 0.82f, 0.52f };
	upgradeHudExpProgressStyle_.fillColor = { 1.00f, 0.82f, 0.22f, 0.96f };
	upgradeHudExpProgressStyle_.outlineColor = { 1.00f, 0.92f, 0.48f, 0.92f };
	upgradeHudExpProgressStyle_.outlineWidth = 1.5f;
	upgradeHudExpProgressStyle_.roundedEnds = upgradeHudRoundedProgressBars_;
	upgradeHudExpProgressBar_ = std::make_unique<NeonProgressBar>();
	upgradeHudExpProgressBar_->Initialize(spriteCommon);
	upgradeHudLevelProgressBar_ = std::make_unique<NeonProgressBar>();
	upgradeHudLevelProgressBar_->Initialize(spriteCommon);
	ApplyUpgradeHudProgressBarStyles();

	for (size_t i = 0; i < upgradeHudSegmentBars_.size(); ++i) {
		auto& segmentBar = upgradeHudSegmentBars_[i];
		segmentBar = std::make_unique<NeonSegmentedBar>();
		segmentBar->Initialize(spriteCommon);
		segmentBar->SetStyle(MakeUpgradeHudSegmentStyle(static_cast<int>(i)));
	}
	upgradeHudBarBloomEffect_ = std::make_unique<cg2::ObjectPostEffect>();
	upgradeHudBarBloomEffect_->Initialize(
		cg2::Object3dCommon::GetInstance()->GetDxCommon(),
		cg2::Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		0.75f);
	cg2::BloomParam& barBloomParam = upgradeHudBarBloomEffect_->GetParam();
	barBloomParam.threshold = 0.0f;
	barBloomParam.intensity = 0.78f;
	barBloomParam.outlineWidth = 0.0f;
	barBloomParam.outlineBloomIntensity = 0.0f;

	const cg2::TextStyle smallStyle = MakeUpgradeHudSmallTextStyle();
	const cg2::TextStyle overlayTextStyle = MakeUpgradeHudOverlayTextStyle();
	SetLabel(upgradeHudTitleLabel_, spriteCommon, "強化", upgradeHudTitlePos_, smallStyle);
	SetLabel(upgradeHudPointLabel_, spriteCommon, "x0", upgradeHudPointPos_, smallStyle);
	const cg2::TextStyle bottomBarTextStyle = MakeUpgradeHudBottomBarTextStyle();
	SetLabel(upgradeHudLevelLabel_, spriteCommon, "Lv ", upgradeHudLevelTextPos_, bottomBarTextStyle);
	SetLabel(upgradeHudLevelClassLabel_, spriteCommon, GetCurrentClassName(), upgradeHudLevelTextPos_, bottomBarTextStyle);
	for (auto& glyphLabel : upgradeHudExpGlyphLabels_) {
		glyphLabel = std::make_unique<cg2::TextLabel>();
		glyphLabel->Initialize(spriteCommon, " ", bottomBarTextStyle);
		glyphLabel->SetPosition(upgradeHudExpTextPos_);
		glyphLabel->SetAlpha(0.0f);
	}
	for (auto& glyphLabel : upgradeHudLevelGlyphLabels_) {
		glyphLabel = std::make_unique<cg2::TextLabel>();
		glyphLabel->Initialize(spriteCommon, " ", bottomBarTextStyle);
		glyphLabel->SetPosition(upgradeHudLevelTextPos_);
		glyphLabel->SetAlpha(0.0f);
	}

	const auto& names = UpgradeHudNames();
	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		SetLabel(upgradeHudNameLabels_[i], spriteCommon, std::to_string(i + 1) + " " + names[i],
			{ upgradeHudNameX_, y + upgradeHudNameTextOffsetY_ }, overlayTextStyle);
		SetLabel(upgradeHudLevelLabels_[i], spriteCommon, "Lv.0",
			{ upgradeHudLevelX_, y + upgradeHudLevelTextOffsetY_ }, smallStyle);
		SetLabel(upgradeHudMinusLabels_[i], spriteCommon, "-",
			{ upgradeHudMinusLabelX_, y + upgradeHudMinusTextOffsetY_ }, overlayTextStyle);
		SetLabel(upgradeHudPlusLabels_[i], spriteCommon, "+",
			{ upgradeHudPlusLabelX_, y + upgradeHudPlusTextOffsetY_ }, overlayTextStyle);
		upgradeHudNameLabels_[i]->SetAlpha(0.0f);
		upgradeHudLevelLabels_[i]->SetAlpha(0.0f);
		upgradeHudMinusLabels_[i]->SetAlpha(0.0f);
		upgradeHudPlusLabels_[i]->SetAlpha(0.0f);
	}

	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		upgradeHudButtonSprites_[i] = makePanel({ upgradeHudRowStart_.x, y }, upgradeHudButtonSize_, { 0.10f, 0.12f, 0.15f, 0.84f });
		upgradeHudMinusSprites_[i] = makePill({ upgradeHudMinusX_, y }, upgradeHudPlusSize_, { 0.26f, 0.42f, 0.86f, 0.68f });
		upgradeHudPlusSprites_[i] = makePill({ upgradeHudPlusX_, y }, upgradeHudPlusSize_, { 0.34f, 0.95f, 0.64f, 0.88f });
	}
	InitializeUpgradeHudBatch();
}

void Player::PrepareUpgradeHudTextTextures()
{
	if (!arenaUiEnabled_) return;
	cg2::TextRenderer* textRenderer = cg2::TextRenderer::GetInstance();
	if (!textRenderer ||
		(upgradeHudTextPrepared_ &&
		 upgradeHudTextFontRevision_ == textRenderer->GetFontRevision() &&
		 upgradeHudTextPreparedForSegmentedBars_ == upgradeHudUseSegmentedUpgradeBars_)) {
		return;
	}
	cg2::StartupTrace::Scope scope("Player.UpgradeHudTextPrewarm");

	const cg2::TextStyle bottomStyle = MakeUpgradeHudBottomBarTextStyle();
	const cg2::TextStyle smallStyle = MakeUpgradeHudSmallTextStyle();
	const cg2::TextStyle listControlStyle = upgradeHudUseSegmentedUpgradeBars_
		? MakeUpgradeHudOverlayTextStyle()
		: smallStyle;
	auto preload = [textRenderer](const std::string& text, const cg2::TextStyle& style) {
		const std::string texturePath = textRenderer->GetOrCreateTexture(text, style);
		cg2::TextureManager::GetInstance()->LoadTexture(texturePath);
	};

	constexpr char kGlyphs[] = "0123456789EXP /";
	for (const char glyph : kGlyphs) {
		if (glyph == '\0') {
			break;
		}
		preload(std::string(1, glyph), bottomStyle);
	}
	preload("Lv ", bottomStyle);
	preload(GetCurrentClassName(), bottomStyle);

	preload("強化", smallStyle);
	for (int point = 0; point <= kMaxLevel; ++point) {
		preload("x" + std::to_string(point), smallStyle);
	}
	for (int value = 0; value <= 10; ++value) {
		preload("Lv." + std::to_string(value), smallStyle);
	}
	const auto& names = UpgradeHudNames();
	for (int i = 0; i < 7; ++i) {
		preload(std::to_string(i + 1) + " " + names[i], listControlStyle);
	}
	preload("-", listControlStyle);
	preload("+", listControlStyle);
	preload("済", listControlStyle);

	if (upgradeHudTitleLabel_) {
		upgradeHudTitleLabel_->SetStyle(smallStyle);
		upgradeHudTitleLabel_->PrepareForDraw();
	}
	if (upgradeHudPointLabel_) {
		upgradeHudPointLabel_->SetStyle(smallStyle);
		upgradeHudPointLabel_->PrepareForDraw();
	}
	if (upgradeHudLevelLabel_) upgradeHudLevelLabel_->PrepareForDraw();
	if (upgradeHudLevelClassLabel_) upgradeHudLevelClassLabel_->PrepareForDraw();
	for (auto& label : upgradeHudExpGlyphLabels_) {
		if (label) label->PrepareForDraw();
	}
	for (auto& label : upgradeHudLevelGlyphLabels_) {
		if (label) label->PrepareForDraw();
	}
	for (int i = 0; i < 7; ++i) {
		if (upgradeHudNameLabels_[i]) {
			upgradeHudNameLabels_[i]->SetStyle(listControlStyle);
			upgradeHudNameLabels_[i]->PrepareForDraw();
		}
		if (upgradeHudLevelLabels_[i]) upgradeHudLevelLabels_[i]->PrepareForDraw();
		if (upgradeHudMinusLabels_[i]) {
			upgradeHudMinusLabels_[i]->SetStyle(listControlStyle);
			upgradeHudMinusLabels_[i]->PrepareForDraw();
		}
		if (upgradeHudPlusLabels_[i]) {
			upgradeHudPlusLabels_[i]->SetStyle(listControlStyle);
			upgradeHudPlusLabels_[i]->PrepareForDraw();
		}
	}

	upgradeHudTextFontRevision_ = textRenderer->GetFontRevision();
	upgradeHudTextPrepared_ = true;
	upgradeHudTextPreparedForSegmentedBars_ = upgradeHudUseSegmentedUpgradeBars_;
	// フォント上書きが変わった場合は、次のHUD描画でグリフ配置も更新する。
	cachedUpgradeHudExp_ = -1;
	cachedUpgradeHudLevel_ = -1;
	cachedUpgradeHudListVisible_ = false;
}

void Player::UpdateUpgradeHudExpGlyphs(const std::string& text, const cg2::TextStyle& style)
{
	const size_t glyphCount = (std::min)(text.size(), upgradeHudExpGlyphLabels_.size());
	for (size_t i = 0; i < glyphCount; ++i) {
		cg2::TextLabel* label = upgradeHudExpGlyphLabels_[i].get();
		if (!label) {
			continue;
		}
		label->SetStyle(style);
		label->SetText(std::string(1, text[i]));
		label->PrepareForDraw();
		label->SetAlpha(1.0f);
	}
	for (size_t i = glyphCount; i < upgradeHudExpGlyphCount_; ++i) {
		if (upgradeHudExpGlyphLabels_[i]) {
			upgradeHudExpGlyphLabels_[i]->SetAlpha(0.0f);
		}
	}
	upgradeHudExpGlyphCount_ = glyphCount;
	PositionUpgradeHudExpGlyphs();
}

void Player::PositionUpgradeHudExpGlyphs()
{
	const cg2::TextStyle style = MakeUpgradeHudBottomBarTextStyle();
	float x = upgradeHudExpTextPos_.x;
	for (size_t i = 0; i < upgradeHudExpGlyphCount_; ++i) {
		cg2::TextLabel* label = upgradeHudExpGlyphLabels_[i].get();
		if (!label || !label->GetSprite()) {
			continue;
		}
		label->SetPosition({ x, upgradeHudExpTextPos_.y });
		x += GetUpgradeHudTextAdvance(label, style);
	}
}

void Player::UpdateUpgradeHudLevelLabels(int level, const std::string& className, const cg2::TextStyle& style)
{
	const std::string levelText = std::to_string(level);
	const size_t glyphCount = (std::min)(levelText.size(), upgradeHudLevelGlyphLabels_.size());
	for (size_t i = 0; i < glyphCount; ++i) {
		cg2::TextLabel* label = upgradeHudLevelGlyphLabels_[i].get();
		if (!label) continue;
		label->SetStyle(style);
		label->SetText(std::string(1, levelText[i]));
		label->PrepareForDraw();
		label->SetAlpha(1.0f);
	}
	for (size_t i = glyphCount; i < upgradeHudLevelGlyphCount_; ++i) {
		if (upgradeHudLevelGlyphLabels_[i]) upgradeHudLevelGlyphLabels_[i]->SetAlpha(0.0f);
	}
	upgradeHudLevelGlyphCount_ = glyphCount;
	SetLabel(upgradeHudLevelLabel_, cg2::SpriteCommon::GetInstance(), "Lv ", upgradeHudLevelTextPos_, style);
	SetLabel(upgradeHudLevelClassLabel_, cg2::SpriteCommon::GetInstance(), className, upgradeHudLevelTextPos_, style);
	PositionUpgradeHudLevelLabels();
}

void Player::PositionUpgradeHudLevelLabels()
{
	const cg2::TextStyle style = MakeUpgradeHudBottomBarTextStyle();
	float x = upgradeHudLevelTextPos_.x;
	if (upgradeHudLevelLabel_) {
		upgradeHudLevelLabel_->SetPosition({ x, upgradeHudLevelTextPos_.y });
		x += GetUpgradeHudTextAdvance(upgradeHudLevelLabel_.get(), style);
	}
	for (size_t i = 0; i < upgradeHudLevelGlyphCount_; ++i) {
		cg2::TextLabel* label = upgradeHudLevelGlyphLabels_[i].get();
		if (!label) continue;
		label->SetPosition({ x, upgradeHudLevelTextPos_.y });
		x += GetUpgradeHudTextAdvance(label, style);
	}
	x += style.fontSize * 0.34f;
	if (upgradeHudLevelClassLabel_) {
		upgradeHudLevelClassLabel_->SetPosition({ x, upgradeHudLevelTextPos_.y });
	}
}

void Player::ApplyUpgradeHudProgressBarStyles()
{
	upgradeHudLevelProgressStyle_.roundedEnds = upgradeHudRoundedProgressBars_;
	upgradeHudExpProgressStyle_.roundedEnds = upgradeHudRoundedProgressBars_;
	if (upgradeHudLevelProgressBar_) {
		upgradeHudLevelProgressBar_->SetStyle(upgradeHudLevelProgressStyle_);
	}
	if (upgradeHudExpProgressBar_) {
		upgradeHudExpProgressBar_->SetStyle(upgradeHudExpProgressStyle_);
	}
}

void Player::PrepareUpgradeHudSegmentBars()
{
	for (size_t i = 0; i < upgradeHudSegmentBars_.size(); ++i) {
		NeonSegmentedBar* segmentBar = upgradeHudSegmentBars_[i].get();
		if (!segmentBar) {
			continue;
		}
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		segmentBar->SetBounds(
			{ upgradeHudRowStart_.x + upgradeHudSegmentBarOffset_.x,
			  y + upgradeHudSegmentBarOffset_.y },
			upgradeHudSegmentBarSize_);
		segmentBar->SetSegmentCount(maxEnhancePoint);
		segmentBar->SetFilledSegments(0);
		segmentBar->Update();
	}
}

void Player::InitializeUpgradeHudBatch()
{
	cg2::DirectXCommon* dxCommon = cg2::SpriteCommon::GetInstance()->GetDxCommon();
	if (!dxCommon) {
		return;
	}
	cg2::TextureManager::GetInstance()->LoadTexture("resources/white512x512.png");

	upgradeHudBatchVertexResource_ = dxCommon->CreateBufferResource(sizeof(cg2::TrailVertex) * kUpgradeHudBatchMaxVertices);
	upgradeHudBatchVertexBufferView_.BufferLocation = upgradeHudBatchVertexResource_->GetGPUVirtualAddress();
	upgradeHudBatchVertexBufferView_.SizeInBytes = sizeof(cg2::TrailVertex) * kUpgradeHudBatchMaxVertices;
	upgradeHudBatchVertexBufferView_.StrideInBytes = sizeof(cg2::TrailVertex);
	upgradeHudBatchVertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&upgradeHudBatchVertexData_));

	upgradeHudBatchTransformResource_ = dxCommon->CreateBufferResource(sizeof(cg2::Matrix4x4));
	upgradeHudBatchTransformResource_->Map(0, nullptr, reinterpret_cast<void**>(&upgradeHudBatchTransformData_));
	*upgradeHudBatchTransformData_ = cg2::MakeOrthographicMatrix(0.0f, 0.0f, float(cg2::WinApp::kClientWidth), float(cg2::WinApp::kClientHeight), 0.0f, 100.0f);

	upgradeHudBatchMaterialResource_ = dxCommon->CreateBufferResource(sizeof(cg2::Material));
	upgradeHudBatchMaterialResource_->Map(0, nullptr, reinterpret_cast<void**>(&upgradeHudBatchMaterialData_));
	*upgradeHudBatchMaterialData_ = cg2::MakeDefaultMaterial();
	upgradeHudBatchMaterialData_->shininess = 1.0f;
}

void Player::UpdateUpgradeHud(float uiDeltaTime)
{
	upgradeHudMouseCaptured_ = false;
	if (!arenaUiEnabled_ || !upgradeHudVisible_ || isChangeMode || isDead_) {
		return;
	}
	const float safeUiDeltaTime = (std::max)(0.0f, uiDeltaTime);

	for (float& timer : upgradeHudFlashTimers_) {
		timer = (std::max)(0.0f, timer - safeUiDeltaTime);
	}
	for (float& timer : upgradeHudRefundFlashTimers_) {
		timer = (std::max)(0.0f, timer - safeUiDeltaTime);
	}
	for (float& timer : upgradeHudMissFlashTimers_) {
		timer = (std::max)(0.0f, timer - safeUiDeltaTime);
	}

	ApplyUpgradeHudLayout();
	if (upgradeHudUseNeonProgressBars_) {
		const int safeNextExp = (std::max)(1, nextLevelExp_);
		const float expTarget = (std::clamp)(static_cast<float>(exp_) / static_cast<float>(safeNextExp), 0.0f, 1.0f);
		const float levelTarget = (std::clamp)(
			static_cast<float>(level_ - 1) / static_cast<float>((std::max)(1, kMaxLevel - 1)),
			0.0f,
			1.0f);
		const bool levelChanged = upgradeHudAnimatedLevel_ != level_;
		if (upgradeHudLevelProgressBar_) {
			upgradeHudLevelProgressBar_->SetBounds(upgradeHudLevelBarPos_, upgradeHudLevelBarSize_);
			upgradeHudLevelProgressBar_->SetTarget(levelTarget);
			upgradeHudLevelProgressBar_->Update(safeUiDeltaTime);
		}
		if (upgradeHudExpProgressBar_) {
			upgradeHudExpProgressBar_->SetBounds(upgradeHudExpBarPos_, upgradeHudExpBarSize_);
			if (levelChanged) {
				upgradeHudExpProgressBar_->BeginRollover(expTarget);
			} else {
				upgradeHudExpProgressBar_->SetTarget(expTarget);
			}
			upgradeHudExpProgressBar_->Update(safeUiDeltaTime);
		}
		upgradeHudAnimatedLevel_ = level_;
	}
	const bool wantsUpgradeList = !runModifiers_.enabled && (!upgradeHudHideListWithoutPoints_ || skillPoints_ > 0);
	const float targetListVisibility = wantsUpgradeList ? 1.0f : 0.0f;
	const float listStep = safeUiDeltaTime * upgradeHudListAnimSpeed_;
	if (upgradeHudListVisibility_ < targetListVisibility) {
		upgradeHudListVisibility_ = (std::min)(targetListVisibility, upgradeHudListVisibility_ + listStep);
	} else if (upgradeHudListVisibility_ > targetListVisibility) {
		upgradeHudListVisibility_ = (std::max)(targetListVisibility, upgradeHudListVisibility_ - listStep);
	}
	const bool showUpgradeList = upgradeHudListVisibility_ > 0.01f;
	const float listAlpha = (std::clamp)(upgradeHudListVisibility_, 0.0f, 1.0f);
	const float easedListAlpha = listAlpha * listAlpha * (3.0f - 2.0f * listAlpha);
	const float listOffsetX = -(1.0f - easedListAlpha) * upgradeHudListSlideDistance_;
	if (!showUpgradeList) {
		if (upgradeHudExpBackSprite_) upgradeHudExpBackSprite_->Update();
		if (upgradeHudExpFillSprite_) upgradeHudExpFillSprite_->Update();
		if (upgradeHudLevelBackSprite_) upgradeHudLevelBackSprite_->Update();
		if (upgradeHudLevelFillSprite_) upgradeHudLevelFillSprite_->Update();
		return;
	}
	if (upgradeHudUseSegmentedUpgradeBars_) {
		for (int i = 0; i < 7; ++i) {
			if (!upgradeHudSegmentBars_[i]) {
				continue;
			}
			const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
			upgradeHudSegmentBars_[i]->SetBounds(
				{ upgradeHudRowStart_.x + upgradeHudSegmentBarOffset_.x + listOffsetX,
				  y + upgradeHudSegmentBarOffset_.y },
				upgradeHudSegmentBarSize_);
			upgradeHudSegmentBars_[i]->SetSegmentCount(maxEnhancePoint);
			upgradeHudSegmentBars_[i]->SetFilledSegments(upgradeLevels_[i]);
			upgradeHudSegmentBars_[i]->Update();
		}
	}

	const bool canUpgrade = skillPoints_ > 0;
	const bool click = input_ && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0]);
	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		const float controlOffsetY = upgradeHudUseSegmentedUpgradeBars_
			? (std::max)(0.0f, (upgradeHudSegmentBarSize_.y - upgradeHudPlusSize_.y) * 0.5f)
			: 0.0f;
		cg2::Sprite* button = upgradeHudButtonSprites_[i].get();
		cg2::Sprite* plus = upgradeHudPlusSprites_[i].get();
		cg2::Sprite* minus = upgradeHudMinusSprites_[i].get();
		if (button) {
			button->SetPosition({ upgradeHudRowStart_.x + listOffsetX, y });
		}
		if (minus) {
			minus->SetPosition({ upgradeHudMinusX_ + listOffsetX, y + controlOffsetY });
		}
		if (plus) {
			plus->SetPosition({ upgradeHudPlusX_ + listOffsetX, y + controlOffsetY });
		}
		const bool plusHovered = plus && plus->IsHovered(mousePosition_);
		const bool minusHovered = minus && minus->IsHovered(mousePosition_);
		const bool rowHovered = button && button->IsHovered(mousePosition_);
		const bool hovered = rowHovered || plusHovered || minusHovered;
		upgradeHudMouseCaptured_ = upgradeHudMouseCaptured_ || hovered;
		const bool maxed = upgradeLevels_[i] >= maxEnhancePoint;
		if (wantsUpgradeList && click && plusHovered) {
			if (canUpgrade && !maxed && ApplyStatUpgrade(i)) {
				upgradeHudFlashTimers_[i] = 0.22f;
			} else {
				upgradeHudMissFlashTimers_[i] = 0.26f;
			}
		} else if (wantsUpgradeList && click && minusHovered) {
			if (RefundStatUpgrade(i)) {
				upgradeHudRefundFlashTimers_[i] = 0.22f;
			} else {
				upgradeHudMissFlashTimers_[i] = 0.26f;
			}
		}

		const float flash = (std::min)(1.0f, upgradeHudFlashTimers_[i] / 0.22f);
		const float refundFlash = (std::min)(1.0f, upgradeHudRefundFlashTimers_[i] / 0.22f);
		const float missFlash = (std::min)(1.0f, upgradeHudMissFlashTimers_[i] / 0.26f);
		const cg2::Vector4 rowColor = UpgradeHudRowColors()[i];
		if (button) {
			if (maxed) {
				button->SetColor({ 0.12f, 0.12f, 0.14f, 0.72f });
			} else if (rowHovered) {
				button->SetColor({ 0.18f + flash * 0.18f + missFlash * 0.22f, 0.28f + flash * 0.30f, 0.30f + refundFlash * 0.20f, 0.92f });
			} else {
				button->SetColor({ 0.10f + flash * 0.25f + missFlash * 0.20f, 0.12f + flash * 0.36f, 0.15f + refundFlash * 0.22f, 0.84f });
			}
			button->Update();
		}
		if (minus) {
			if (upgradeLevels_[i] <= 0) {
				minus->SetColor({ 0.12f + missFlash * 0.32f, 0.14f, 0.18f, 0.42f + missFlash * 0.32f });
			} else {
				minus->SetColor(LerpColor(
					{ rowColor.x * 0.45f, rowColor.y * 0.45f, rowColor.z * 0.45f, 0.72f },
					{ 0.88f, 0.94f, 1.0f, 0.98f },
					minusHovered ? 0.62f + refundFlash * 0.25f : refundFlash * 0.35f));
			}
			minus->Update();
		}
		if (plus) {
			if (maxed) {
				plus->SetColor({ 0.18f + missFlash * 0.30f, 0.18f, 0.20f, 0.72f + missFlash * 0.20f });
			} else if (canUpgrade) {
				plus->SetColor(LerpColor(
					{ rowColor.x * 0.82f, rowColor.y * 0.82f, rowColor.z * 0.82f, 0.90f },
					{ 1.0f, 1.0f, 1.0f, 1.0f },
					plusHovered ? 0.42f : flash * 0.35f));
			} else {
				plus->SetColor({ 0.15f + missFlash * 0.34f, 0.22f, 0.24f, 0.64f + missFlash * 0.25f });
			}
			plus->Update();
		}
	}

	if (upgradeHudBackdropSprite_) upgradeHudBackdropSprite_->Update();
	if (upgradeHudExpBackSprite_) upgradeHudExpBackSprite_->Update();
	if (upgradeHudExpFillSprite_) upgradeHudExpFillSprite_->Update();
	if (upgradeHudLevelBackSprite_) upgradeHudLevelBackSprite_->Update();
	if (upgradeHudLevelFillSprite_) upgradeHudLevelFillSprite_->Update();
}

void Player::DrawUpgradeHud()
{
	upgradeHudProfile_ = {};
	if (!arenaUiEnabled_ || !upgradeHudVisible_ || isChangeMode || isDead_) {
		return;
	}
	upgradeHudProfile_.visible = true;
	const auto totalStart = std::chrono::steady_clock::now();

	cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
	const cg2::TextStyle smallStyle = MakeUpgradeHudSmallTextStyle();
	const cg2::TextStyle upgradeOverlayTextStyle = MakeUpgradeHudOverlayTextStyle();
	const cg2::TextStyle bottomBarTextStyle = MakeUpgradeHudBottomBarTextStyle();

	const int safeNextExp = (std::max)(1, nextLevelExp_);
	const float expRatio = (std::clamp)(static_cast<float>(exp_) / static_cast<float>(safeNextExp), 0.0f, 1.0f);
	const float levelRatio = (std::clamp)(static_cast<float>(level_ - 1) / static_cast<float>((std::max)(1, kMaxLevel - 1)), 0.0f, 1.0f);
	if (upgradeHudExpFillSprite_) {
		upgradeHudExpFillSprite_->SetSize({ upgradeHudExpBarSize_.x * expRatio, upgradeHudExpBarSize_.y });
		upgradeHudExpFillSprite_->Update();
	}
	if (upgradeHudLevelFillSprite_) {
		upgradeHudLevelFillSprite_->SetSize({ upgradeHudLevelBarSize_.x * levelRatio, upgradeHudLevelBarSize_.y });
		upgradeHudLevelFillSprite_->Update();
	}

	const bool showUpgradeList = upgradeHudListVisibility_ > 0.01f;
	const float listAlpha = (std::clamp)(upgradeHudListVisibility_, 0.0f, 1.0f);
	const float easedListAlpha = listAlpha * listAlpha * (3.0f - 2.0f * listAlpha);
	const float listOffsetX = -(1.0f - easedListAlpha) * upgradeHudListSlideDistance_;
	const auto& names = UpgradeHudNames();
	const std::string className = GetCurrentClassName();
	const bool baseTextDirty =
		cachedUpgradeHudExp_ != exp_ ||
		cachedUpgradeHudNextExp_ != nextLevelExp_ ||
		cachedUpgradeHudLevel_ != level_ ||
		cachedUpgradeHudClassName_ != className;
	if (baseTextDirty) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
		upgradeHudProfile_.baseTextRefreshed = true;
		cg2::TextLabel::ResetProfileStats();
		const auto baseTextRefreshStart = std::chrono::steady_clock::now();
		const auto expLabelRefreshStart = baseTextRefreshStart;
#endif
		UpdateUpgradeHudExpGlyphs(
			"EXP " + std::to_string(exp_) + " / " + std::to_string(nextLevelExp_),
			bottomBarTextStyle);
#if defined(USE_IMGUI) && !defined(NDEBUG)
		const auto expLabelRefreshEnd = std::chrono::steady_clock::now();
		const auto levelLabelRefreshStart = expLabelRefreshEnd;
#endif
		UpdateUpgradeHudLevelLabels(level_, className, bottomBarTextStyle);
#if defined(USE_IMGUI) && !defined(NDEBUG)
		const auto levelLabelRefreshEnd = std::chrono::steady_clock::now();
#endif
		cachedUpgradeHudExp_ = exp_;
		cachedUpgradeHudNextExp_ = nextLevelExp_;
		cachedUpgradeHudLevel_ = level_;
		cachedUpgradeHudClassName_ = className;
#if defined(USE_IMGUI) && !defined(NDEBUG)
		const cg2::TextLabel::ProfileStats& textLabelStats = cg2::TextLabel::GetProfileStats();
		upgradeHudProfile_.baseTextRefreshMs = std::chrono::duration<float, std::milli>(
			std::chrono::steady_clock::now() - baseTextRefreshStart).count();
		upgradeHudProfile_.expLabelRefreshMs = std::chrono::duration<float, std::milli>(
			expLabelRefreshEnd - expLabelRefreshStart).count();
		upgradeHudProfile_.levelLabelRefreshMs = std::chrono::duration<float, std::milli>(
			levelLabelRefreshEnd - levelLabelRefreshStart).count();
		upgradeHudProfile_.baseTextSetStyleMs = textLabelStats.setStyleCpuMs;
		upgradeHudProfile_.baseTextSetTextMs = textLabelStats.setTextCpuMs;
		upgradeHudProfile_.baseTextSetTextRebuildMs = textLabelStats.setTextRebuildCpuMs;
		upgradeHudProfile_.baseTextRebuildTextureMs = textLabelStats.rebuildTextureCpuMs;
		upgradeHudProfile_.baseTextGetOrCreateTextureMs = textLabelStats.getOrCreateTextureCpuMs;
		upgradeHudProfile_.baseTextSpriteSetTextureMs = textLabelStats.spriteSetTextureCpuMs;
		upgradeHudProfile_.baseTextCacheFileExistedCount = static_cast<int>(textLabelStats.cacheFileExistedCount);
		upgradeHudProfile_.baseTextGeneratedPngCount = static_cast<int>(textLabelStats.generatedPngCount);
#endif
	} else {
		PositionUpgradeHudExpGlyphs();
		PositionUpgradeHudLevelLabels();
	}

	if (showUpgradeList) {
		const bool listDirty =
			!cachedUpgradeHudListVisible_ ||
			cachedUpgradeHudSkillPoints_ != skillPoints_ ||
			cachedUpgradeHudMaxEnhancePoint_ != maxEnhancePoint ||
			cachedUpgradeHudSegmentedBars_ != upgradeHudUseSegmentedUpgradeBars_ ||
			cachedUpgradeHudLevels_ != upgradeLevels_;
		if (listDirty) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
			upgradeHudProfile_.listTextRefreshed = true;
			cg2::TextLabel::ResetProfileStats();
			const auto listTextRefreshStart = std::chrono::steady_clock::now();
#endif
			SetLabel(upgradeHudPointLabel_, spriteCommon, "x" + std::to_string(skillPoints_), { upgradeHudPointPos_.x + listOffsetX, upgradeHudPointPos_.y }, smallStyle);
			SetLabel(upgradeHudTitleLabel_, spriteCommon, "強化", { upgradeHudTitlePos_.x + listOffsetX, upgradeHudTitlePos_.y }, smallStyle);
			for (int i = 0; i < 7; ++i) {
				const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
				const bool compactGauge = upgradeHudUseSegmentedUpgradeBars_;
				const float controlOffsetY = compactGauge
					? (std::max)(0.0f, (upgradeHudSegmentBarSize_.y - upgradeHudPlusSize_.y) * 0.5f)
					: 0.0f;
				const cg2::Vector2 namePosition = compactGauge
					? cg2::Vector2{ upgradeHudRowStart_.x + upgradeHudSegmentBarSize_.x * 0.5f + listOffsetX, y + upgradeHudSegmentBarSize_.y * 0.5f }
					: cg2::Vector2{ upgradeHudNameX_ + listOffsetX, y + upgradeHudNameTextOffsetY_ };
				SetLabel(upgradeHudNameLabels_[i], spriteCommon, std::to_string(i + 1) + " " + names[i],
					namePosition, compactGauge ? upgradeOverlayTextStyle : smallStyle);
				upgradeHudNameLabels_[i]->SetAnchorPoint(compactGauge ? cg2::Vector2{ 0.5f, 0.5f } : cg2::Vector2{ 0.0f, 0.0f });
				SetLabel(upgradeHudLevelLabels_[i], spriteCommon, "Lv." + std::to_string(upgradeLevels_[i]),
					{ upgradeHudLevelX_ + listOffsetX, y + upgradeHudLevelTextOffsetY_ }, smallStyle);
				SetLabel(upgradeHudMinusLabels_[i], spriteCommon, "-",
					compactGauge
						? cg2::Vector2{ upgradeHudMinusX_ + upgradeHudPlusSize_.x * 0.5f + listOffsetX, y + controlOffsetY + upgradeHudPlusSize_.y * 0.5f }
						: cg2::Vector2{ upgradeHudMinusLabelX_ + listOffsetX, y + upgradeHudMinusTextOffsetY_ }, compactGauge ? upgradeOverlayTextStyle : smallStyle);
				upgradeHudMinusLabels_[i]->SetAnchorPoint(compactGauge ? cg2::Vector2{ 0.5f, 0.5f } : cg2::Vector2{ 0.0f, 0.0f });
				SetLabel(upgradeHudPlusLabels_[i], spriteCommon, upgradeLevels_[i] >= maxEnhancePoint ? "済" : "+",
					compactGauge
						? cg2::Vector2{ upgradeHudPlusX_ + upgradeHudPlusSize_.x * 0.5f + listOffsetX, y + controlOffsetY + upgradeHudPlusSize_.y * 0.5f }
						: cg2::Vector2{ upgradeHudPlusLabelX_ + listOffsetX, y + upgradeHudPlusTextOffsetY_ }, compactGauge ? upgradeOverlayTextStyle : smallStyle);
				upgradeHudPlusLabels_[i]->SetAnchorPoint(compactGauge ? cg2::Vector2{ 0.5f, 0.5f } : cg2::Vector2{ 0.0f, 0.0f });
			}
			cachedUpgradeHudSkillPoints_ = skillPoints_;
			cachedUpgradeHudMaxEnhancePoint_ = maxEnhancePoint;
			cachedUpgradeHudSegmentedBars_ = upgradeHudUseSegmentedUpgradeBars_;
			cachedUpgradeHudLevels_ = upgradeLevels_;
#if defined(USE_IMGUI) && !defined(NDEBUG)
			const cg2::TextLabel::ProfileStats& textLabelStats = cg2::TextLabel::GetProfileStats();
			upgradeHudProfile_.listTextRefreshMs = std::chrono::duration<float, std::milli>(
				std::chrono::steady_clock::now() - listTextRefreshStart).count();
			upgradeHudProfile_.listTextSetStyleMs = textLabelStats.setStyleCpuMs;
			upgradeHudProfile_.listTextSetTextMs = textLabelStats.setTextCpuMs;
			upgradeHudProfile_.listTextSetTextRebuildMs = textLabelStats.setTextRebuildCpuMs;
			upgradeHudProfile_.listTextRebuildTextureMs = textLabelStats.rebuildTextureCpuMs;
			upgradeHudProfile_.listTextGetOrCreateTextureMs = textLabelStats.getOrCreateTextureCpuMs;
			upgradeHudProfile_.listTextSpriteSetTextureMs = textLabelStats.spriteSetTextureCpuMs;
			upgradeHudProfile_.listTextCacheFileExistedCount = static_cast<int>(textLabelStats.cacheFileExistedCount);
			upgradeHudProfile_.listTextGeneratedPngCount = static_cast<int>(textLabelStats.generatedPngCount);
#endif
		} else {
			if (upgradeHudPointLabel_) upgradeHudPointLabel_->SetPosition({ upgradeHudPointPos_.x + listOffsetX, upgradeHudPointPos_.y });
			if (upgradeHudTitleLabel_) upgradeHudTitleLabel_->SetPosition({ upgradeHudTitlePos_.x + listOffsetX, upgradeHudTitlePos_.y });
			for (int i = 0; i < 7; ++i) {
				const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
				const float controlOffsetY = upgradeHudUseSegmentedUpgradeBars_
					? (std::max)(0.0f, (upgradeHudSegmentBarSize_.y - upgradeHudPlusSize_.y) * 0.5f)
					: 0.0f;
				if (upgradeHudNameLabels_[i]) {
					upgradeHudNameLabels_[i]->SetPosition(upgradeHudUseSegmentedUpgradeBars_
						? cg2::Vector2{ upgradeHudRowStart_.x + upgradeHudSegmentBarSize_.x * 0.5f + listOffsetX, y + upgradeHudSegmentBarSize_.y * 0.5f }
						: cg2::Vector2{ upgradeHudNameX_ + listOffsetX, y + upgradeHudNameTextOffsetY_ });
				}
				if (upgradeHudLevelLabels_[i]) upgradeHudLevelLabels_[i]->SetPosition({ upgradeHudLevelX_ + listOffsetX, y + upgradeHudLevelTextOffsetY_ });
				if (upgradeHudMinusLabels_[i]) upgradeHudMinusLabels_[i]->SetPosition(upgradeHudUseSegmentedUpgradeBars_
					? cg2::Vector2{ upgradeHudMinusX_ + upgradeHudPlusSize_.x * 0.5f + listOffsetX, y + controlOffsetY + upgradeHudPlusSize_.y * 0.5f }
					: cg2::Vector2{ upgradeHudMinusLabelX_ + listOffsetX, y + upgradeHudMinusTextOffsetY_ });
				if (upgradeHudPlusLabels_[i]) upgradeHudPlusLabels_[i]->SetPosition(upgradeHudUseSegmentedUpgradeBars_
					? cg2::Vector2{ upgradeHudPlusX_ + upgradeHudPlusSize_.x * 0.5f + listOffsetX, y + controlOffsetY + upgradeHudPlusSize_.y * 0.5f }
					: cg2::Vector2{ upgradeHudPlusLabelX_ + listOffsetX, y + upgradeHudPlusTextOffsetY_ });
			}
		}
		if (upgradeHudPointLabel_) upgradeHudPointLabel_->SetAlpha(easedListAlpha);
		if (upgradeHudTitleLabel_) upgradeHudTitleLabel_->SetAlpha(easedListAlpha);
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudNameLabels_[i]) upgradeHudNameLabels_[i]->SetAlpha(easedListAlpha);
			if (upgradeHudLevelLabels_[i]) upgradeHudLevelLabels_[i]->SetAlpha(easedListAlpha);
			if (upgradeHudMinusLabels_[i]) upgradeHudMinusLabels_[i]->SetAlpha(easedListAlpha);
			if (upgradeHudPlusLabels_[i]) upgradeHudPlusLabels_[i]->SetAlpha(easedListAlpha);
		}
	}
	cachedUpgradeHudListVisible_ = showUpgradeList;

	const auto spriteStart = std::chrono::steady_clock::now();
	if (upgradeHudUseRectBatch_) {
		DrawUpgradeHudRectBatch(showUpgradeList, expRatio, levelRatio, easedListAlpha, listOffsetX);
	} else {
		cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
		if (showUpgradeList && upgradeHudDrawListPanels_) {
			if (!upgradeHudUseSegmentedUpgradeBars_ && upgradeHudBackdropSprite_) { upgradeHudBackdropSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			for (int i = 0; i < 7; ++i) {
				if (!upgradeHudUseSegmentedUpgradeBars_ && upgradeHudButtonSprites_[i]) { upgradeHudButtonSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
				if (!upgradeHudUseSegmentedUpgradeBars_ && upgradeHudMinusSprites_[i]) { upgradeHudMinusSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
				if (!upgradeHudUseSegmentedUpgradeBars_ && upgradeHudPlusSprites_[i]) { upgradeHudPlusSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
			}
		}
		if (upgradeHudDrawBottomBars_ && !upgradeHudUseNeonProgressBars_) {
			if (upgradeHudLevelBackSprite_) { upgradeHudLevelBackSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudLevelFillSprite_) { upgradeHudLevelFillSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudExpBackSprite_) { upgradeHudExpBackSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudExpFillSprite_) { upgradeHudExpFillSprite_->Draw(); ++upgradeHudProfile_.spriteDraws; }
		}
	}
	if (showUpgradeList && upgradeHudDrawListPanels_ && upgradeHudUseSegmentedUpgradeBars_) {
		cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudMinusSprites_[i]) { upgradeHudMinusSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
			if (upgradeHudPlusSprites_[i]) { upgradeHudPlusSprites_[i]->Draw(); ++upgradeHudProfile_.spriteDraws; }
		}
	}
	const auto spriteEnd = std::chrono::steady_clock::now();

	const auto textStart = std::chrono::steady_clock::now();
	if (showUpgradeList && upgradeHudUseSegmentedUpgradeBars_) {
		cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudSegmentBars_[i]) {
				upgradeHudSegmentBars_[i]->Draw();
			}
		}
		upgradeHudProfile_.spriteDraws += 7 * 11;
	}
	if (upgradeHudDrawBottomBars_ && upgradeHudUseNeonProgressBars_) {
		cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
		if (upgradeHudLevelProgressBar_) upgradeHudLevelProgressBar_->Draw();
		if (upgradeHudExpProgressBar_) upgradeHudExpProgressBar_->Draw();
		upgradeHudProfile_.spriteDraws += 10;
	}
	cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
	if (showUpgradeList && upgradeHudDrawListText_) {
		if (upgradeHudTitleLabel_) { upgradeHudTitleLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		if (upgradeHudPointLabel_) { upgradeHudPointLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudNameLabels_[i]) { upgradeHudNameLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
			if (!upgradeHudUseSegmentedUpgradeBars_ && upgradeHudLevelLabels_[i]) { upgradeHudLevelLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
			if (upgradeHudMinusLabels_[i]) { upgradeHudMinusLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
			if (upgradeHudPlusLabels_[i]) { upgradeHudPlusLabels_[i]->Draw(); ++upgradeHudProfile_.textDraws; }
		}
	}
	if (upgradeHudDrawBottomText_) {
		if (upgradeHudLevelLabel_) { upgradeHudLevelLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		for (size_t i = 0; i < upgradeHudLevelGlyphCount_; ++i) {
			if (upgradeHudLevelGlyphLabels_[i]) {
				upgradeHudLevelGlyphLabels_[i]->Draw();
				++upgradeHudProfile_.textDraws;
			}
		}
		if (upgradeHudLevelClassLabel_) { upgradeHudLevelClassLabel_->Draw(); ++upgradeHudProfile_.textDraws; }
		for (size_t i = 0; i < upgradeHudExpGlyphCount_; ++i) {
			if (upgradeHudExpGlyphLabels_[i]) {
				upgradeHudExpGlyphLabels_[i]->Draw();
				++upgradeHudProfile_.textDraws;
			}
		}
	}
	const auto textEnd = std::chrono::steady_clock::now();
	const auto totalEnd = std::chrono::steady_clock::now();

	upgradeHudProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
	upgradeHudProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
	upgradeHudProfile_.updateMs = std::chrono::duration<float, std::milli>(spriteStart - totalStart).count();
	upgradeHudProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
}

void Player::AppendGameplayNeonTextLabels(std::vector<cg2::TextLabel*>& labels) const
{
	if (isChangeMode) {
		return;
	}
	if (upgradeHudDrawBottomText_ && upgradeHudLevelLabel_) {
		labels.push_back(upgradeHudLevelLabel_.get());
	}
	if (upgradeHudDrawBottomText_) {
		for (size_t i = 0; i < upgradeHudLevelGlyphCount_; ++i) {
			if (upgradeHudLevelGlyphLabels_[i]) {
				labels.push_back(upgradeHudLevelGlyphLabels_[i].get());
			}
		}
		if (upgradeHudLevelClassLabel_) {
			labels.push_back(upgradeHudLevelClassLabel_.get());
		}
	}
	if (upgradeHudDrawBottomText_) {
		for (size_t i = 0; i < upgradeHudExpGlyphCount_; ++i) {
			if (upgradeHudExpGlyphLabels_[i]) {
				labels.push_back(upgradeHudExpGlyphLabels_[i].get());
			}
		}
	}
	if (upgradeHudListTextBloomEnabled_ && upgradeHudListVisibility_ > 0.01f && upgradeHudDrawListText_) {
		if (upgradeHudTitleLabel_) {
			labels.push_back(upgradeHudTitleLabel_.get());
		}
		if (upgradeHudPointLabel_) {
			labels.push_back(upgradeHudPointLabel_.get());
		}
		// 段数バーの上に重なる文字と操作記号だけは、黒アウトラインを保った
		// ままネオンBloomの対象にする。タイトルや通常説明文には影響しない。
		if (upgradeHudUseSegmentedUpgradeBars_) {
			for (int i = 0; i < 7; ++i) {
				if (upgradeHudNameLabels_[i]) {
					labels.push_back(upgradeHudNameLabels_[i].get());
				}
				if (upgradeHudMinusLabels_[i]) {
					labels.push_back(upgradeHudMinusLabels_[i].get());
				}
				if (upgradeHudPlusLabels_[i]) {
					labels.push_back(upgradeHudPlusLabels_[i].get());
				}
			}
		}
	}
}

void Player::QueueUpgradeHudRect(std::vector<cg2::TrailVertex>& vertices, const cg2::Vector2& pos, const cg2::Vector2& size, const cg2::Vector4& color) const
{
	const float left = pos.x;
	const float top = pos.y;
	const float right = pos.x + size.x;
	const float bottom = pos.y + size.y;
	const cg2::Vector3 p0{ left, bottom, 0.0f };
	const cg2::Vector3 p1{ left, top, 0.0f };
	const cg2::Vector3 p2{ right, bottom, 0.0f };
	const cg2::Vector3 p3{ right, top, 0.0f };

	vertices.push_back({ p0, color, { 0.0f, 1.0f } });
	vertices.push_back({ p1, color, { 0.0f, 0.0f } });
	vertices.push_back({ p2, color, { 1.0f, 1.0f } });
	vertices.push_back({ p1, color, { 0.0f, 0.0f } });
	vertices.push_back({ p3, color, { 1.0f, 0.0f } });
	vertices.push_back({ p2, color, { 1.0f, 1.0f } });
}

void Player::DrawUpgradeHudRectBatch(bool showUpgradeList, float expRatio, float levelRatio, float listAlpha, float listOffsetX)
{
	if (!upgradeHudBatchVertexData_ || !upgradeHudBatchTransformData_ || !upgradeHudBatchMaterialData_) {
		return;
	}

	std::vector<cg2::TrailVertex> vertices;
	vertices.reserve(kUpgradeHudBatchMaxVertices);
	auto withListAlpha = [listAlpha](const cg2::Vector4& color) {
		return cg2::Vector4{color.x, color.y, color.z, color.w * listAlpha};
	};
	auto offsetListPos = [listOffsetX](cg2::Vector2 pos) {
		pos.x += listOffsetX;
		return pos;
	};

	if (showUpgradeList && upgradeHudDrawListPanels_) {
		if (!upgradeHudUseSegmentedUpgradeBars_) {
			QueueUpgradeHudRect(vertices, offsetListPos(upgradeHudPanelPos_), upgradeHudPanelSize_, withListAlpha({ 0.03f, 0.04f, 0.06f, 0.58f }));
		}
		for (int i = 0; i < 7; ++i) {
			const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
			float flash = (std::min)(1.0f, upgradeHudFlashTimers_[i] / 0.22f);
			const float refundFlash = (std::min)(1.0f, upgradeHudRefundFlashTimers_[i] / 0.22f);
			const float missFlash = (std::min)(1.0f, upgradeHudMissFlashTimers_[i] / 0.26f);
			const cg2::Vector4 rowColor = UpgradeHudRowColors()[i];
			const cg2::Vector4 buttonColor = LerpColor({ 0.10f + missFlash * 0.20f, 0.12f, 0.15f + refundFlash * 0.16f, 0.84f }, { 0.35f, 0.90f, 0.72f, 0.96f }, flash);
			const cg2::Vector4 minusColor = upgradeLevels_[i] > 0
				? LerpColor({ rowColor.x * 0.45f, rowColor.y * 0.45f, rowColor.z * 0.45f, 0.72f }, { 0.88f, 0.94f, 1.0f, 0.98f }, refundFlash)
				: cg2::Vector4{ 0.12f + missFlash * 0.30f, 0.14f, 0.18f, 0.42f + missFlash * 0.28f };
			const cg2::Vector4 plusColor = skillPoints_ > 0
				? LerpColor({ rowColor.x * 0.82f, rowColor.y * 0.82f, rowColor.z * 0.82f, 0.90f }, { 1.0f, 1.0f, 1.0f, 1.0f }, flash)
				: cg2::Vector4{ 0.18f + missFlash * 0.32f, 0.22f, 0.24f, 0.48f + missFlash * 0.28f };
			if (!upgradeHudUseSegmentedUpgradeBars_) {
				QueueUpgradeHudRect(vertices, offsetListPos({ upgradeHudRowStart_.x, y }), upgradeHudButtonSize_, withListAlpha(buttonColor));
			}
			if (!upgradeHudUseSegmentedUpgradeBars_) {
				QueueUpgradeHudRect(vertices, offsetListPos({ upgradeHudMinusX_, y }), upgradeHudPlusSize_, withListAlpha(minusColor));
				QueueUpgradeHudRect(vertices, offsetListPos({ upgradeHudPlusX_, y }), upgradeHudPlusSize_, withListAlpha(plusColor));
			}
		}
	}

	if (upgradeHudDrawBottomBars_ && !upgradeHudUseNeonProgressBars_) {
		QueueUpgradeHudRect(vertices, upgradeHudLevelBarPos_, upgradeHudLevelBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
		QueueUpgradeHudRect(vertices, upgradeHudLevelBarPos_, { upgradeHudLevelBarSize_.x * levelRatio, upgradeHudLevelBarSize_.y }, { 0.36f, 1.0f, 0.56f, 0.92f });
		QueueUpgradeHudRect(vertices, upgradeHudExpBarPos_, upgradeHudExpBarSize_, { 0.04f, 0.04f, 0.05f, 0.82f });
		QueueUpgradeHudRect(vertices, upgradeHudExpBarPos_, { upgradeHudExpBarSize_.x * expRatio, upgradeHudExpBarSize_.y }, { 0.96f, 0.83f, 0.24f, 0.95f });
	}

	if (vertices.empty()) {
		return;
	}
	if (vertices.size() > kUpgradeHudBatchMaxVertices) {
		vertices.resize(kUpgradeHudBatchMaxVertices);
	}
	std::memcpy(upgradeHudBatchVertexData_, vertices.data(), sizeof(cg2::TrailVertex) * vertices.size());
	*upgradeHudBatchTransformData_ = cg2::MakeOrthographicMatrix(0.0f, 0.0f, float(cg2::WinApp::kClientWidth), float(cg2::WinApp::kClientHeight), 0.0f, 100.0f);

	cg2::DirectXCommon* dxCommon = cg2::SpriteCommon::GetInstance()->GetDxCommon();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetList().Get();
	cg2::TextureManager::GetInstance()->PreDraw();
	commandList->SetGraphicsRootSignature(dxCommon->GetPSOHudRect().root_.GetSignature().Get());
	commandList->SetPipelineState(dxCommon->GetPSOHudRect().graphicsState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &upgradeHudBatchVertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, upgradeHudBatchMaterialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, upgradeHudBatchTransformResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, cg2::TextureManager::GetInstance()->GetSrvHandleGPU("resources/white512x512.png"));
	commandList->DrawInstanced(static_cast<UINT>(vertices.size()), 1, 0, 0);
	upgradeHudProfile_.spriteDraws += 1;
}

void Player::ApplyUpgradeHudLayout()
{
	if (upgradeHudBackdropSprite_) {
		upgradeHudBackdropSprite_->SetPosition(upgradeHudPanelPos_);
		upgradeHudBackdropSprite_->SetSize(upgradeHudPanelSize_);
	}
	if (upgradeHudExpBackSprite_) {
		upgradeHudExpBackSprite_->SetPosition(upgradeHudExpBarPos_);
		upgradeHudExpBackSprite_->SetSize(upgradeHudExpBarSize_);
	}
	if (upgradeHudExpFillSprite_) {
		upgradeHudExpFillSprite_->SetPosition(upgradeHudExpBarPos_);
	}
	if (upgradeHudLevelBackSprite_) {
		upgradeHudLevelBackSprite_->SetPosition(upgradeHudLevelBarPos_);
		upgradeHudLevelBackSprite_->SetSize(upgradeHudLevelBarSize_);
	}
	if (upgradeHudLevelFillSprite_) {
		upgradeHudLevelFillSprite_->SetPosition(upgradeHudLevelBarPos_);
	}
	for (int i = 0; i < 7; ++i) {
		const float y = upgradeHudRowStart_.y + static_cast<float>(i) * upgradeHudRowGap_;
		if (upgradeHudButtonSprites_[i]) {
			upgradeHudButtonSprites_[i]->SetPosition({ upgradeHudRowStart_.x, y });
			upgradeHudButtonSprites_[i]->SetSize(upgradeHudButtonSize_);
		}
		if (upgradeHudMinusSprites_[i]) {
			upgradeHudMinusSprites_[i]->SetPosition({ upgradeHudMinusX_, y });
			upgradeHudMinusSprites_[i]->SetSize(upgradeHudPlusSize_);
		}
		if (upgradeHudPlusSprites_[i]) {
			upgradeHudPlusSprites_[i]->SetPosition({ upgradeHudPlusX_, y });
			upgradeHudPlusSprites_[i]->SetSize(upgradeHudPlusSize_);
		}
	}
}

bool Player::LoadUpgradeHudConfig(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		upgradeHudConfigStatus_ = "HUD設定ファイルが見つからないため初期値を使用します。";
		return false;
	}
	nlohmann::json json{};
	try {
		file >> json;
	} catch (...) {
		upgradeHudConfigStatus_ = "HUD設定JSONの読み込みに失敗しました。";
		return false;
	}
	if (!json.is_object()) {
		return false;
	}
	upgradeHudVisible_ = json.value("visible", upgradeHudVisible_);
	upgradeHudHideListWithoutPoints_ = json.value("hideListWithoutPoints", upgradeHudHideListWithoutPoints_);
	upgradeHudDrawListPanels_ = json.value("drawListPanels", upgradeHudDrawListPanels_);
	upgradeHudDrawListText_ = json.value("drawListText", upgradeHudDrawListText_);
	upgradeHudDrawBottomBars_ = json.value("drawBottomBars", upgradeHudDrawBottomBars_);
	upgradeHudDrawBottomText_ = json.value("drawBottomText", upgradeHudDrawBottomText_);
	upgradeHudUseRectBatch_ = json.value("useRectBatch", upgradeHudUseRectBatch_);
	upgradeHudRoundedProgressBars_ = json.value("roundedProgressBars", upgradeHudRoundedProgressBars_);
	upgradeHudUseSegmentedUpgradeBars_ = json.value("segmentedUpgradeBars", upgradeHudUseSegmentedUpgradeBars_);
	upgradeHudListTextBloomEnabled_ = json.value("listTextBloom", false);
	maxEnhancePoint = (std::clamp)(json.value("maxEnhancePoint", maxEnhancePoint), 1, 10);
	upgradeHudListAnimSpeed_ = json.value("listAnimSpeed", upgradeHudListAnimSpeed_);
	upgradeHudListSlideDistance_ = json.value("listSlideDistance", upgradeHudListSlideDistance_);
	upgradeHudPanelPos_ = ReadVector2Object(json.value("panelPos", nlohmann::json::object()), upgradeHudPanelPos_);
	upgradeHudPanelSize_ = ReadVector2Object(json.value("panelSize", nlohmann::json::object()), upgradeHudPanelSize_);
	upgradeHudRowStart_ = ReadVector2Object(json.value("rowStart", nlohmann::json::object()), upgradeHudRowStart_);
	upgradeHudButtonSize_ = ReadVector2Object(json.value("buttonSize", nlohmann::json::object()), upgradeHudButtonSize_);
	upgradeHudPlusSize_ = ReadVector2Object(json.value("plusSize", nlohmann::json::object()), upgradeHudPlusSize_);
	upgradeHudSegmentBarOffset_ = ReadVector2Object(json.value("segmentBarOffset", nlohmann::json::object()), upgradeHudSegmentBarOffset_);
	upgradeHudSegmentBarSize_ = ReadVector2Object(json.value("segmentBarSize", nlohmann::json::object()), upgradeHudSegmentBarSize_);
	upgradeHudRowGap_ = json.value("rowGap", upgradeHudRowGap_);
	upgradeHudNameX_ = json.value("nameX", upgradeHudNameX_);
	upgradeHudLevelX_ = json.value("levelX", upgradeHudLevelX_);
	upgradeHudMinusX_ = json.value("minusX", upgradeHudMinusX_);
	upgradeHudPlusX_ = json.value("plusX", upgradeHudPlusX_);
	upgradeHudMinusLabelX_ = json.value("minusLabelX", upgradeHudMinusLabelX_);
	upgradeHudPlusLabelX_ = json.value("plusLabelX", upgradeHudPlusLabelX_);
	upgradeHudNameTextOffsetY_ = json.value("nameTextOffsetY", upgradeHudNameTextOffsetY_);
	upgradeHudLevelTextOffsetY_ = json.value("levelTextOffsetY", upgradeHudLevelTextOffsetY_);
	upgradeHudMinusTextOffsetY_ = json.value("minusTextOffsetY", upgradeHudMinusTextOffsetY_);
	upgradeHudPlusTextOffsetY_ = json.value("plusTextOffsetY", upgradeHudPlusTextOffsetY_);
	upgradeHudTitlePos_ = ReadVector2Object(json.value("titlePos", nlohmann::json::object()), upgradeHudTitlePos_);
	upgradeHudPointPos_ = ReadVector2Object(json.value("pointPos", nlohmann::json::object()), upgradeHudPointPos_);
	upgradeHudLevelBarPos_ = ReadVector2Object(json.value("levelBarPos", nlohmann::json::object()), upgradeHudLevelBarPos_);
	upgradeHudLevelBarSize_ = ReadVector2Object(json.value("levelBarSize", nlohmann::json::object()), upgradeHudLevelBarSize_);
	upgradeHudLevelTextPos_ = ReadVector2Object(json.value("levelTextPos", nlohmann::json::object()), upgradeHudLevelTextPos_);
	upgradeHudExpBarPos_ = ReadVector2Object(json.value("expBarPos", nlohmann::json::object()), upgradeHudExpBarPos_);
	upgradeHudExpBarSize_ = ReadVector2Object(json.value("expBarSize", nlohmann::json::object()), upgradeHudExpBarSize_);
	upgradeHudExpTextPos_ = ReadVector2Object(json.value("expTextPos", nlohmann::json::object()), upgradeHudExpTextPos_);
	ApplyUpgradeHudLayout();
	ApplyUpgradeHudProgressBarStyles();
	upgradeHudConfigStatus_ = "HUD設定を読み込みました: " + path;
	return true;
}

bool Player::SaveUpgradeHudConfig(const std::string& path) const
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	std::ofstream file(path);
	if (!file.is_open()) {
		return false;
	}
	nlohmann::json json = {
		{ "version", 1 },
		{ "visible", upgradeHudVisible_ },
		{ "hideListWithoutPoints", upgradeHudHideListWithoutPoints_ },
		{ "drawListPanels", upgradeHudDrawListPanels_ },
		{ "drawListText", upgradeHudDrawListText_ },
		{ "drawBottomBars", upgradeHudDrawBottomBars_ },
		{ "drawBottomText", upgradeHudDrawBottomText_ },
		{ "useRectBatch", upgradeHudUseRectBatch_ },
		{ "roundedProgressBars", upgradeHudRoundedProgressBars_ },
		{ "segmentedUpgradeBars", upgradeHudUseSegmentedUpgradeBars_ },
		{ "listTextBloom", upgradeHudListTextBloomEnabled_ },
		{ "maxEnhancePoint", maxEnhancePoint },
		{ "listAnimSpeed", upgradeHudListAnimSpeed_ },
		{ "listSlideDistance", upgradeHudListSlideDistance_ },
		{ "panelPos", WriteVector2Object(upgradeHudPanelPos_) },
		{ "panelSize", WriteVector2Object(upgradeHudPanelSize_) },
		{ "rowStart", WriteVector2Object(upgradeHudRowStart_) },
		{ "buttonSize", WriteVector2Object(upgradeHudButtonSize_) },
		{ "plusSize", WriteVector2Object(upgradeHudPlusSize_) },
		{ "segmentBarOffset", WriteVector2Object(upgradeHudSegmentBarOffset_) },
		{ "segmentBarSize", WriteVector2Object(upgradeHudSegmentBarSize_) },
		{ "rowGap", upgradeHudRowGap_ },
		{ "nameX", upgradeHudNameX_ },
		{ "levelX", upgradeHudLevelX_ },
		{ "minusX", upgradeHudMinusX_ },
		{ "plusX", upgradeHudPlusX_ },
		{ "minusLabelX", upgradeHudMinusLabelX_ },
		{ "plusLabelX", upgradeHudPlusLabelX_ },
		{ "nameTextOffsetY", upgradeHudNameTextOffsetY_ },
		{ "levelTextOffsetY", upgradeHudLevelTextOffsetY_ },
		{ "minusTextOffsetY", upgradeHudMinusTextOffsetY_ },
		{ "plusTextOffsetY", upgradeHudPlusTextOffsetY_ },
		{ "titlePos", WriteVector2Object(upgradeHudTitlePos_) },
		{ "pointPos", WriteVector2Object(upgradeHudPointPos_) },
		{ "levelBarPos", WriteVector2Object(upgradeHudLevelBarPos_) },
		{ "levelBarSize", WriteVector2Object(upgradeHudLevelBarSize_) },
		{ "levelTextPos", WriteVector2Object(upgradeHudLevelTextPos_) },
		{ "expBarPos", WriteVector2Object(upgradeHudExpBarPos_) },
		{ "expBarSize", WriteVector2Object(upgradeHudExpBarSize_) },
		{ "expTextPos", WriteVector2Object(upgradeHudExpTextPos_) }
	};
	file << json.dump(2);
	return true;
}

void Player::DrawUpgradeHudDebugImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("プレイヤー強化HUD", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	ImGui::Checkbox("HUDを表示", &upgradeHudVisible_);
	ImGui::Checkbox("スキルポイントがない時は強化リストを隠す", &upgradeHudHideListWithoutPoints_);
	ImGui::Checkbox("強化リスト背景/ボタンを描画", &upgradeHudDrawListPanels_);
	ImGui::Checkbox("強化リスト文字を描画", &upgradeHudDrawListText_);
#if !defined(NDEBUG)
	ImGui::Checkbox("強化段数ゲージ Bloom を描画", &upgradeHudSegmentedBarBloomEnabled_);
	ImGui::Checkbox("強化リスト文字 Bloom を描画", &upgradeHudListTextBloomEnabled_);
#endif
	ImGui::Checkbox("下部EXP/Levelバーを描画", &upgradeHudDrawBottomBars_);
	ImGui::Checkbox("下部EXP/Level文字を描画", &upgradeHudDrawBottomText_);
	ImGui::Checkbox("背景/バーを矩形バッチで描画", &upgradeHudUseRectBatch_);
	if (ImGui::Checkbox("下部EXP/Levelバーを丸端にする", &upgradeHudRoundedProgressBars_)) {
		ApplyUpgradeHudProgressBarStyles();
	}
	ImGui::Checkbox("強化段数ゲージを描画", &upgradeHudUseSegmentedUpgradeBars_);
	ImGui::DragInt("1項目の最大強化段数", &maxEnhancePoint, 1.0f, 1, 10);
	ImGui::Text("矩形Draw数: %d", upgradeHudProfile_.spriteDraws);
	ImGui::DragFloat("リスト表示アニメ速度", &upgradeHudListAnimSpeed_, 0.1f, 1.0f, 30.0f);
	ImGui::DragFloat("リストスライド距離", &upgradeHudListSlideDistance_, 1.0f, 0.0f, 500.0f);
	if (ImGui::Button("強化HUD設定を保存")) {
		upgradeHudConfigStatus_ = SaveUpgradeHudConfig() ? "強化HUD設定を保存しました。" : "強化HUD設定の保存に失敗しました。";
	}
	ImGui::SameLine();
	if (ImGui::Button("強化HUD設定を再読み込み")) {
		LoadUpgradeHudConfig();
	}
	if (!upgradeHudConfigStatus_.empty()) {
		ImGui::TextWrapped("%s", upgradeHudConfigStatus_.c_str());
	}
	ImGui::DragFloat2("左パネル位置", &upgradeHudPanelPos_.x, 1.0f);
	ImGui::DragFloat2("左パネルサイズ", &upgradeHudPanelSize_.x, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat2("強化行 開始位置", &upgradeHudRowStart_.x, 1.0f);
	ImGui::DragFloat2("強化ボタンサイズ", &upgradeHudButtonSize_.x, 1.0f, 0.0f, 1000.0f);
	ImGui::DragFloat2("プラスボタンサイズ", &upgradeHudPlusSize_.x, 1.0f, 0.0f, 300.0f);
	ImGui::DragFloat2("段数ゲージ オフセット", &upgradeHudSegmentBarOffset_.x, 1.0f);
	ImGui::DragFloat2("段数ゲージ サイズ", &upgradeHudSegmentBarSize_.x, 1.0f, 0.0f, 1000.0f);
	ImGui::DragFloat("強化行 間隔", &upgradeHudRowGap_, 1.0f, 10.0f, 100.0f);
	ImGui::DragFloat("項目名X", &upgradeHudNameX_, 1.0f);
	ImGui::DragFloat("Lv表示X", &upgradeHudLevelX_, 1.0f);
	ImGui::DragFloat("マイナスボタンX", &upgradeHudMinusX_, 1.0f);
	ImGui::DragFloat("プラスボタンX", &upgradeHudPlusX_, 1.0f);
	ImGui::DragFloat("マイナス文字X", &upgradeHudMinusLabelX_, 1.0f);
	ImGui::DragFloat("プラス文字X", &upgradeHudPlusLabelX_, 1.0f);
	ImGui::DragFloat("項目名文字Yオフセット", &upgradeHudNameTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat("Lv文字Yオフセット", &upgradeHudLevelTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat("マイナス文字Yオフセット", &upgradeHudMinusTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat("プラス文字Yオフセット", &upgradeHudPlusTextOffsetY_, 0.25f, -20.0f, 40.0f);
	ImGui::DragFloat2("タイトル位置", &upgradeHudTitlePos_.x, 1.0f);
	ImGui::DragFloat2("ポイント表示位置", &upgradeHudPointPos_.x, 1.0f);
	ImGui::Separator();
	ImGui::DragFloat2("レベルバー位置", &upgradeHudLevelBarPos_.x, 1.0f);
	ImGui::DragFloat2("レベルバーサイズ", &upgradeHudLevelBarSize_.x, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat2("レベル文字位置", &upgradeHudLevelTextPos_.x, 1.0f);
	ImGui::DragFloat2("経験値バー位置", &upgradeHudExpBarPos_.x, 1.0f);
	ImGui::DragFloat2("経験値バーサイズ", &upgradeHudExpBarSize_.x, 1.0f, 0.0f, 2000.0f);
	ImGui::DragFloat2("経験値文字位置", &upgradeHudExpTextPos_.x, 1.0f);
	ApplyUpgradeHudLayout();
#endif
}

void Player::DrawUpgradeHudAfterPostEffects()
{
	if (!arenaUiEnabled_) return;
	const bool drawBottomBars = upgradeHudDrawBottomBars_ && upgradeHudUseNeonProgressBars_;
	bool drawSegmentBars = upgradeHudUseSegmentedUpgradeBars_ && upgradeHudListVisibility_ > 0.01f;
#if defined(USE_IMGUI) && !defined(NDEBUG)
	drawSegmentBars = drawSegmentBars && upgradeHudSegmentedBarBloomEnabled_;
#endif
	if (!upgradeHudVisible_ || isChangeMode || isDead_ || !upgradeHudBarBloomEffect_ ||
		(!drawBottomBars && !drawSegmentBars)) {
		return;
	}

	upgradeHudBarBloomEffect_->BeginCapture();
	cg2::SpriteCommon::GetInstance()->PreDrawForScene(cg2::kNormal);
	if (drawBottomBars) {
		if (upgradeHudLevelProgressBar_) {
			upgradeHudLevelProgressBar_->DrawBloomSource();
		}
		if (upgradeHudExpProgressBar_) {
			upgradeHudExpProgressBar_->DrawBloomSource();
		}
	}
	if (drawSegmentBars) {
		for (int i = 0; i < 7; ++i) {
			if (upgradeHudSegmentBars_[i]) {
				upgradeHudSegmentBars_[i]->DrawBloomSource();
			}
		}
	}
	upgradeHudBarBloomEffect_->EndCaptureBloomOnlyToBackBuffer();
}

void Player::SpawnParticles()
{
	cg2::Vector3 center = GetWorldPosition();
	cg2::ParticleManager::GetInstance()->EmitNeonDeathEffect(
		center,
		{ 1.45f, 1.30f, 0.72f, 1.0f },
		{ 0.08f, 1.25f, 1.55f, 0.0f },
		1.0f);
	deathChargeTimer_ = deathChargeDuration_;
	deathEffectTimer_ = 1.85f;
}

void Player::UpdateParticles(float deltaTime)
{
	deathChargeTimer_ = (std::max)(0.0f, deathChargeTimer_ - deltaTime);
	deathEffectTimer_ -= deltaTime;
	if (deathEffectTimer_ <= 0.0f) {
		isExploding_ = false;
	}
}

void Player::UpdateP(float deltaTime)
{
	(void)deltaTime;
}

void Player::UpdateEncyclopedia(float uiDeltaTime)
{
	if (!arenaUiEnabled_ || !isChangeMode) {
		return;
	}
	if (encyclopedia_.size() != classCatalog_.OrderedIds().size()) {
		InitializeEncyclopedia();
	}
	if (encyclopedia_.empty()) {
		return;
	}
	evolutionUiTimer_ += (std::max)(0.0f, uiDeltaTime);
	if (ShouldUseEvolutionCircuitPrototype()) {
		UpdateEvolutionCircuitPrototype();
		return;
	}
	if (ShouldUseStaticEvolutionPrototype()) {
		UpdateStaticEvolutionPrototype();
		return;
	}
	int currentRank = GetRankFromLevel(this->level_);

	for (int i = 0; i < static_cast<int>(encyclopedia_.size()); ++i) {
		auto& tank = encyclopedia_[i];
		if (!IsEvolutionClassVisible(tank.classId)) continue;
		bool isAvailable = (currentRank >= tank.requiredRank);
		const bool selected = (i == evolutionSelectedIndex_);
		const bool hovered = tank.cardSprite && tank.cardSprite->IsHovered(mousePosition_);

		if (hovered && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
			evolutionSelectedIndex_ = i;
			codexSelectedClassIndex_ = i;
		}

		if (tank.cardSprite) {
			if (selected) {
				tank.cardSprite->SetColor({ 0.18f, 0.48f, 0.42f, 0.94f });
			} else if (hovered) {
				tank.cardSprite->SetColor({ 0.16f, 0.30f, 0.38f, 0.92f });
			} else if (isAvailable) {
				tank.cardSprite->SetColor({ 0.10f, 0.15f, 0.22f, 0.82f });
			} else {
				tank.cardSprite->SetColor({ 0.05f, 0.05f, 0.07f, 0.72f });
			}
			tank.cardSprite->Update();
		}

		if (isAvailable) {
			tank.sprite->SetColor({ 1.0f, 1.0f, 1.0f, selected ? 1.0f : 0.86f });
		} else {
			tank.sprite->SetColor({ 0.20f, 0.24f, 0.28f, 0.45f });
		}

		tank.sprite->Update();
	}

	evolutionSelectedIndex_ = (std::clamp)(evolutionSelectedIndex_, 0, static_cast<int>(encyclopedia_.size()) - 1);
	if (!IsEvolutionClassVisible(encyclopedia_[evolutionSelectedIndex_].classId)) {
		for (size_t i = 0; i < encyclopedia_.size(); ++i) {
			if (encyclopedia_[i].classId == currentClassId_) {
				evolutionSelectedIndex_ = static_cast<int>(i);
				break;
			}
		}
	}
	const TankData& selectedTank = encyclopedia_[evolutionSelectedIndex_];
	const PlayerClassConfig* selectedConfig = GetClassConfig(selectedTank.classId);
	if (!selectedConfig) {
		return;
	}

	const bool locked = currentRank < selectedTank.requiredRank;
	if (evolutionPreviewTankSprite_) {
		evolutionPreviewTankSprite_->SetTexture(selectedTank.texturePath);
		const float bob = std::sin(evolutionUiTimer_ * 2.0f) * 12.0f;
		evolutionPreviewTankSprite_->SetPosition({ 210.0f + bob, 300.0f });
		evolutionPreviewTankSprite_->SetRotation(std::sin(evolutionUiTimer_ * 1.35f) * 0.08f);
		evolutionPreviewTankSprite_->SetColor(locked ? cg2::Vector4{ 0.35f, 0.40f, 0.45f, 0.65f } : cg2::Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		evolutionPreviewTankSprite_->Update();
	}

	if (evolutionShotSprite_) {
		const float shotPhase = std::fmod(evolutionUiTimer_ * 1.8f, 1.0f);
		evolutionShotSprite_->SetPosition({ 300.0f + shotPhase * 52.0f, 296.0f });
		evolutionShotSprite_->SetRotation(std::sin(evolutionUiTimer_ * 1.2f) * 0.12f);
		evolutionShotSprite_->SetSize({ 85.0f + shotPhase * 95.0f, 7.0f });
		evolutionShotSprite_->SetColor(locked ? cg2::Vector4{ 0.55f, 0.55f, 0.60f, 0.18f } : cg2::Vector4{ 1.0f, 0.90f, 0.32f, 0.82f * (1.0f - shotPhase * 0.55f) });
		evolutionShotSprite_->Update();
	}

	if (evolutionChangeButtonSprite_) {
		const bool buttonHovered = evolutionChangeButtonSprite_->IsHovered(mousePosition_);
		if (locked) {
			evolutionChangeButtonSprite_->SetColor({ 0.16f, 0.16f, 0.18f, 0.82f });
		} else if (buttonHovered) {
			evolutionChangeButtonSprite_->SetColor({ 0.36f, 1.0f, 0.58f, 0.96f });
			if (input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
				TryConfirmEvolutionById(selectedTank.classId);
			}
		} else {
			evolutionChangeButtonSprite_->SetColor({ 0.24f, 0.86f, 0.44f, 0.92f });
		}
		evolutionChangeButtonSprite_->Update();
	}
}

void Player::DrawEncyclopedia() {
	if (!arenaUiEnabled_) return;

	evolutionUiProfile_ = {};
	if (isChangeMode && ShouldUseEvolutionCircuitPrototype()) {
		DrawEvolutionCircuitPrototype();
		return;
	}
	if (isChangeMode && ShouldUseStaticEvolutionPrototype()) {
		DrawStaticEvolutionPrototype();
		return;
	}
	if (isChangeMode) {
		evolutionUiProfile_.visible = true;
		const auto totalStart = std::chrono::steady_clock::now();
		if (encyclopedia_.empty()) {
			return;
		}
		evolutionSelectedIndex_ = (std::clamp)(evolutionSelectedIndex_, 0, static_cast<int>(encyclopedia_.size()) - 1);
		const TankData& selectedTank = encyclopedia_[evolutionSelectedIndex_];
		const PlayerClassConfig* selectedConfig = GetClassConfig(selectedTank.classId);
		if (!selectedConfig) {
			return;
		}

		auto roleText = [](const PlayerClassConfig& config) {
			if (config.usesDrone) {
				return std::string("ドローンで周囲を制圧する支援型タンク。");
			}
			if (config.reflect) {
				return std::string("反射弾で壁越しにも圧をかける技巧型タンク。");
			}
			if (config.bulletSpeedScale > 1.2f) {
				return std::string("高速弾で遠距離から狙う狙撃型タンク。");
			}
			if (config.reloadScale < 0.75f) {
				return std::string("連射力で押し切る近中距離向けタンク。");
			}
			if (config.bulletCount > 1 || config.barrels.size() >= 3) {
				return std::string("複数の砲身で広い範囲を抑える制圧型タンク。");
			}
			return std::string("扱いやすい基本性能を持つバランス型タンク。");
		};

		cg2::TextStyle headingStyle{};
		headingStyle.fontFamily = "Meiryo";
		headingStyle.fontSize = 28.0f;
		headingStyle.color = { 0.82f, 1.0f, 0.92f, 1.0f };
		headingStyle.outlineColor = { 0.0f, 0.05f, 0.08f, 0.95f };
		headingStyle.outlineThickness = 3.0f;
		headingStyle.padding = 8.0f;

		cg2::TextStyle bodyStyle = headingStyle;
		bodyStyle.fontSize = 21.0f;
		bodyStyle.color = { 0.92f, 0.96f, 1.0f, 1.0f };
		bodyStyle.outlineThickness = 2.0f;

		cg2::TextStyle smallStyle = bodyStyle;
		smallStyle.fontSize = 18.0f;

		cg2::SpriteCommon* spriteCommon = cg2::SpriteCommon::GetInstance();
		SetLabel(evolutionPreviewNameLabel_, spriteCommon, selectedConfig->displayName, { 58.0f, 112.0f }, headingStyle);
		SetLabel(evolutionRoleLabel_, spriteCommon, roleText(*selectedConfig), { 58.0f, 560.0f }, bodyStyle);

		const int currentRank = GetRankFromLevel(level_);
		const bool locked = currentRank < selectedConfig->requiredRank;
		std::array<std::string, 9> statLines = {
			"必要ランク: " + std::to_string(selectedConfig->requiredRank) + (locked ? "  (未解放)" : "  (使用可能)"),
			"砲塔数: " + std::to_string(selectedConfig->barrels.size()),
			"発射方式: " + std::string(selectedConfig->fireAllBarrels ? "全砲門" : (selectedConfig->alternateBarrels ? "交互発射" : "単発")),
			"弾数: " + std::to_string(selectedConfig->bulletCount),
			"リロード倍率: " + std::to_string(selectedConfig->reloadScale).substr(0, 4),
			"弾速倍率: " + std::to_string(selectedConfig->bulletSpeedScale).substr(0, 4),
			"ダメージ倍率: " + std::to_string(selectedConfig->bulletDamageScale).substr(0, 4),
			"拡散角: " + std::to_string(selectedConfig->spreadAngleDeg).substr(0, 4),
			std::string("特殊: ") + (selectedConfig->usesDrone ? "ドローン" : selectedConfig->reflect ? "反射" : selectedConfig->penetrate ? "貫通" : "なし")
		};
		for (int i = 0; i < static_cast<int>(statLines.size()); ++i) {
			SetLabel(evolutionStatLabels_[i], spriteCommon, statLines[i], { 932.0f, 136.0f + static_cast<float>(i) * 40.0f }, smallStyle);
		}

		cg2::TextStyle buttonStyle = headingStyle;
		buttonStyle.fontSize = 24.0f;
		buttonStyle.color = locked ? cg2::Vector4{ 0.68f, 0.68f, 0.72f, 1.0f } : cg2::Vector4{ 0.02f, 0.09f, 0.04f, 1.0f };
		buttonStyle.outlineColor = locked ? cg2::Vector4{ 0.0f, 0.0f, 0.0f, 0.70f } : cg2::Vector4{ 0.78f, 1.0f, 0.82f, 0.65f };
		SetLabel(evolutionChangeButtonLabel_, spriteCommon, locked ? "ランク不足" : "この戦車に変更", { 990.0f, 660.0f }, buttonStyle);

		const auto spriteStart = std::chrono::steady_clock::now();
		cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
		if (evolutionBackdropSprite_) { evolutionBackdropSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionPreviewPanelSprite_) { evolutionPreviewPanelSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionStatsPanelSprite_) { evolutionStatsPanelSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionShotSprite_) { evolutionShotSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }
		if (evolutionPreviewTankSprite_) { evolutionPreviewTankSprite_->Draw(); ++evolutionUiProfile_.spriteDraws; }

		for (auto& tank : encyclopedia_) {
			if (!IsEvolutionClassVisible(tank.classId)) continue;
			if (tank.cardSprite) { tank.cardSprite->Draw(); ++evolutionUiProfile_.spriteDraws; }
			tank.sprite->Draw(); // 各スプライトが持つ位置で描画
			++evolutionUiProfile_.spriteDraws;
		}
		if (evolutionChangeButtonSprite_) {
			evolutionChangeButtonSprite_->Draw();
			++evolutionUiProfile_.spriteDraws;
		}
		const auto spriteEnd = std::chrono::steady_clock::now();

		const auto textStart = std::chrono::steady_clock::now();
		if (evolutionTitleLabel_) { evolutionTitleLabel_->Draw(); ++evolutionUiProfile_.textDraws; }
		if (evolutionHintLabel_) { evolutionHintLabel_->Draw(); ++evolutionUiProfile_.textDraws; }
		if (evolutionPreviewNameLabel_) { evolutionPreviewNameLabel_->Draw(); ++evolutionUiProfile_.textDraws; }
		if (evolutionRoleLabel_) { evolutionRoleLabel_->Draw(); ++evolutionUiProfile_.textDraws; }

		for (auto& tank : encyclopedia_) {
			if (!IsEvolutionClassVisible(tank.classId)) continue;
			if (tank.nameLabel) { tank.nameLabel->Draw(); ++evolutionUiProfile_.textDraws; }
			if (tank.rankLabel) { tank.rankLabel->Draw(); ++evolutionUiProfile_.textDraws; }
		}

		for (auto& label : evolutionStatLabels_) {
			if (label) {
				label->Draw();
				++evolutionUiProfile_.textDraws;
			}
		}
		if (evolutionChangeButtonLabel_) {
			evolutionChangeButtonLabel_->Draw();
			++evolutionUiProfile_.textDraws;
		}
		const auto textEnd = std::chrono::steady_clock::now();
		const auto totalEnd = std::chrono::steady_clock::now();

		evolutionUiProfile_.spriteMs = std::chrono::duration<float, std::milli>(spriteEnd - spriteStart).count();
		evolutionUiProfile_.textMs = std::chrono::duration<float, std::milli>(textEnd - textStart).count();
		evolutionUiProfile_.updateMs = std::chrono::duration<float, std::milli>(spriteStart - totalStart).count();
		evolutionUiProfile_.totalMs = std::chrono::duration<float, std::milli>(totalEnd - totalStart).count();
	}
}

void Player::DrawTankCodex()
{
#ifdef USE_IMGUI
	if (classCatalog_.OrderedIds().empty()) {
		LoadPlayerClassConfigs();
	}
	if (classCatalog_.OrderedIds().empty()) {
		return;
	}

	codexSelectedClassIndex_ = (std::clamp)(codexSelectedClassIndex_, 0, static_cast<int>(classCatalog_.OrderedIds().size()) - 1);
	const std::string selectedId = classCatalog_.OrderedIds()[codexSelectedClassIndex_];
	const PlayerClassConfig* selectedConfig = GetClassConfig(selectedId);
	if (!selectedConfig) {
		return;
	}

	if (!ImGui::Begin("Tank Codex / Evolution Tree")) {
		ImGui::End();
		return;
	}

	const int currentRank = GetRankFromLevel(level_);
	codexPreviewTimer_ += dt_;
	ImGui::Text("Level %d  Rank %d  Current: %s", level_, currentRank, GetCurrentClassName());
	ImGui::Text("Click a tank to inspect it. Locked tanks can be previewed, but not equipped.");
	ImGui::Separator();

	ImGui::Columns(3, "TankCodexColumns", true);
	ImGui::SetColumnWidth(0, 230.0f);
	ImGui::SetColumnWidth(1, 420.0f);

	ImGui::Text("Evolution Tree");
	ImGui::BeginChild("TankCodexTree", ImVec2(0.0f, 430.0f), true);
	for (int rank = 1; rank <= 4; ++rank) {
		ImGui::Text("Rank %d", rank);
		ImGui::Indent(14.0f);
		for (int i = 0; i < static_cast<int>(classCatalog_.OrderedIds().size()); ++i) {
			const PlayerClassConfig* config = GetClassConfig(classCatalog_.OrderedIds()[i]);
			if (!config || config->requiredRank != rank) {
				continue;
			}
			const bool selected = i == codexSelectedClassIndex_;
			const bool locked = currentRank < config->requiredRank;
			ImGui::PushID(i);
			if (locked) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.62f, 1.0f));
			}
			std::string label = config->displayName;
			if (config->id == currentClassId_) {
				label += "  [Current]";
			} else if (locked) {
				label += "  [Locked]";
			}
			if (ImGui::Selectable(label.c_str(), selected)) {
				codexSelectedClassIndex_ = i;
			}
			if (locked) {
				ImGui::PopStyleColor();
			}
			ImGui::PopID();
		}
		ImGui::Unindent(14.0f);
		ImGui::Spacing();
		if (rank < 4) {
			ImGui::Text("  v");
		}
	}
	ImGui::EndChild();

	ImGui::NextColumn();

	ImGui::Text("Preview");
	ImGui::BeginChild("TankCodexPreview", ImVec2(0.0f, 430.0f), true);
	ImGui::Checkbox("Auto Move", &codexPreviewAutoMove_);
	ImGui::SameLine();
	ImGui::Checkbox("Auto Fire", &codexPreviewAutoFire_);
	ImGui::SliderFloat("Aim Deg", &codexPreviewAimDeg_, -180.0f, 180.0f);
	if (ImGui::Button("Test Shot")) {
		codexPreviewTimer_ = 0.0f;
	}

	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImVec2((std::max)(360.0f, ImGui::GetContentRegionAvail().x), 300.0f);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(8, 12, 20, 255));
	drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(90, 160, 190, 150));

	const float grid = 28.0f;
	for (float x = std::fmod(codexPreviewTimer_ * 18.0f, grid); x < canvasSize.x; x += grid) {
		drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), IM_COL32(45, 105, 155, 80), 1.0f);
	}
	for (float y = std::fmod(codexPreviewTimer_ * 10.0f, grid); y < canvasSize.y; y += grid) {
		drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), IM_COL32(45, 105, 155, 80), 1.0f);
	}

	const float moveWave = codexPreviewAutoMove_ ? std::sin(codexPreviewTimer_ * 1.7f) : 0.0f;
	const ImVec2 center = ImVec2(canvasPos.x + canvasSize.x * 0.48f + moveWave * 42.0f, canvasPos.y + canvasSize.y * 0.55f);
	const float aimRad = codexPreviewAimDeg_ * 3.1415926535f / 180.0f;
	const ImVec2 forward = ImVec2(std::cos(aimRad), std::sin(aimRad));
	const ImVec2 right = ImVec2(-forward.y, forward.x);
	const float bodyRadius = 38.0f;
	auto toImColor = [](const cg2::Vector4& color) {
		return IM_COL32(
			static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.w, 0.0f, 1.0f) * 255.0f));
	};
	const ImU32 bodyFill = toImColor(selectedConfig->bodyFillColor);
	const ImU32 bodyOutline = toImColor(selectedConfig->bodyOutlineColor);

	auto addRotatedRect = [&](ImVec2 origin, ImVec2 axisX, ImVec2 axisY, float halfX, float halfY, ImU32 fill, ImU32 outline) {
		const ImVec2 p0 = ImVec2(origin.x - axisX.x * halfX - axisY.x * halfY, origin.y - axisX.y * halfX - axisY.y * halfY);
		const ImVec2 p1 = ImVec2(origin.x + axisX.x * halfX - axisY.x * halfY, origin.y + axisX.y * halfX - axisY.y * halfY);
		const ImVec2 p2 = ImVec2(origin.x + axisX.x * halfX + axisY.x * halfY, origin.y + axisX.y * halfX + axisY.y * halfY);
		const ImVec2 p3 = ImVec2(origin.x - axisX.x * halfX + axisY.x * halfY, origin.y - axisX.y * halfX + axisY.y * halfY);
		drawList->AddQuadFilled(p0, p1, p2, p3, fill);
		drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
	};
	auto addPolygonBody = [&](int segments, float rotationRad, ImU32 fill, ImU32 outline) {
		segments = (std::clamp)(segments, 3, 48);
		std::vector<ImVec2> points;
		points.reserve(static_cast<size_t>(segments));
		for (int i = 0; i < segments; ++i) {
			const float angle = rotationRad + static_cast<float>(i) * 6.283185307f / static_cast<float>(segments);
			points.push_back(ImVec2(
				center.x + std::cos(angle) * bodyRadius * selectedConfig->bodyScale.x,
				center.y + std::sin(angle) * bodyRadius * selectedConfig->bodyScale.y));
		}
		drawList->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), fill);
		drawList->AddPolyline(points.data(), static_cast<int>(points.size()), outline, ImDrawFlags_Closed, 3.0f);
	};
	auto groupColor = [](int group) {
		static const ImU32 colors[] = {
			IM_COL32(160, 255, 120, 230),
			IM_COL32(255, 220, 80, 230),
			IM_COL32(90, 230, 255, 230),
			IM_COL32(255, 110, 190, 230),
			IM_COL32(190, 140, 255, 230),
			IM_COL32(255, 150, 95, 230)
		};
		return colors[static_cast<size_t>((std::max)(0, group)) % (sizeof(colors) / sizeof(colors[0]))];
	};

	std::vector<ImVec2> muzzlePoints;
	for (const WeaponMountConfig& barrel : selectedConfig->barrels) {
		const float localAngleRad = (codexPreviewAimDeg_ + barrel.angleDeg) * 3.1415926535f / 180.0f;
		const ImVec2 barrelForward = ImVec2(std::cos(localAngleRad), std::sin(localAngleRad));
		const ImVec2 barrelRight = ImVec2(-barrelForward.y, barrelForward.x);
		const ImVec2 offset = ImVec2(
			forward.x * barrel.offset.x * 42.0f + right.x * barrel.offset.y * 42.0f,
			forward.y * barrel.offset.x * 42.0f + right.y * barrel.offset.y * 42.0f
		);
		float length = (std::max)(26.0f, barrel.scale.x * 34.0f);
		float width = (std::max)(8.0f, barrel.scale.y * 38.0f);
		if (barrel.barrelShape == BarrelShape::Heavy) {
			length *= 1.12f;
			width *= 1.45f;
		} else if (barrel.barrelShape == BarrelShape::Short) {
			length *= 0.58f;
			width *= 1.08f;
		} else if (barrel.barrelShape == BarrelShape::Wide) {
			length *= 0.86f;
			width *= 1.80f;
		}
		const ImVec2 base = ImVec2(center.x + offset.x + barrelForward.x * length * 0.32f, center.y + offset.y + barrelForward.y * length * 0.32f);
		const ImU32 barrelFill = toImColor(barrel.barrelColor);
		const ImU32 outline = groupColor(barrel.fireGroup);
		if (barrel.barrelShape == BarrelShape::Trapezoid) {
			const float halfBase = width * 0.64f;
			const float halfTip = width * 0.36f;
			const float halfLength = length * 0.5f;
			const ImVec2 p0 = ImVec2(base.x - barrelForward.x * halfLength - barrelRight.x * halfBase, base.y - barrelForward.y * halfLength - barrelRight.y * halfBase);
			const ImVec2 p1 = ImVec2(base.x + barrelForward.x * halfLength - barrelRight.x * halfTip, base.y + barrelForward.y * halfLength - barrelRight.y * halfTip);
			const ImVec2 p2 = ImVec2(base.x + barrelForward.x * halfLength + barrelRight.x * halfTip, base.y + barrelForward.y * halfLength + barrelRight.y * halfTip);
			const ImVec2 p3 = ImVec2(base.x - barrelForward.x * halfLength + barrelRight.x * halfBase, base.y - barrelForward.y * halfLength + barrelRight.y * halfBase);
			drawList->AddQuadFilled(p0, p1, p2, p3, barrelFill);
			drawList->AddQuad(p0, p1, p2, p3, outline, 2.0f);
		} else {
			addRotatedRect(base, barrelForward, barrelRight, length * 0.5f, width * 0.5f, barrelFill, outline);
		}
		muzzlePoints.push_back(ImVec2(base.x + barrelForward.x * length * 0.56f, base.y + barrelForward.y * length * 0.56f));
	}

	switch (selectedConfig->bodyShape) {
	case BodyShape::Box:
		addPolygonBody(4, aimRad + 6.283185307f * 0.125f, bodyFill, bodyOutline);
		break;
	case BodyShape::Triangle:
		addPolygonBody(3, aimRad - 6.283185307f * 0.25f, bodyFill, bodyOutline);
		break;
	case BodyShape::Pentagon:
		addPolygonBody(5, aimRad - 6.283185307f * 0.25f, bodyFill, bodyOutline);
		break;
	case BodyShape::Circle:
	default:
		drawList->AddCircleFilled(center, bodyRadius + 7.0f, IM_COL32(95, 255, 135, 45), 48);
		drawList->AddCircleFilled(center, bodyRadius, bodyFill, 48);
		drawList->AddCircle(center, bodyRadius, bodyOutline, 48, 3.0f);
		break;
	}

	const bool showShot = codexPreviewAutoFire_ || std::fmod(codexPreviewTimer_, 1.0f) < 0.18f;
	if (showShot) {
		const float shotPhase = std::fmod(codexPreviewTimer_ * 1.8f, 1.0f);
		for (const ImVec2& muzzle : muzzlePoints) {
			const ImVec2 head = ImVec2(muzzle.x + forward.x * (50.0f + shotPhase * 130.0f), muzzle.y + forward.y * (50.0f + shotPhase * 130.0f));
			drawList->AddLine(muzzle, head, IM_COL32(255, 245, 175, 190), 8.0f);
			drawList->AddLine(muzzle, head, IM_COL32(255, 105, 130, 210), 3.0f);
			drawList->AddCircleFilled(head, 8.0f, IM_COL32(255, 245, 185, 220), 20);
		}
	}

	if (selectedConfig->usesDrone) {
		for (int i = 0; i < (std::min)(selectedConfig->maxDrones, 7); ++i) {
			const float a = codexPreviewTimer_ * 1.8f + static_cast<float>(i) * 6.283185307f / (std::max)(1, (std::min)(selectedConfig->maxDrones, 7));
			const ImVec2 drone = ImVec2(center.x + std::cos(a) * 78.0f, center.y + std::sin(a) * 78.0f);
			drawList->AddCircleFilled(drone, 8.0f, IM_COL32(120, 230, 255, 210), 20);
			drawList->AddCircle(drone, 8.0f, IM_COL32(215, 250, 255, 230), 20, 2.0f);
		}
	}

	ImGui::Dummy(canvasSize);
	ImGui::EndChild();

	ImGui::NextColumn();

	ImGui::Text("Tank Data");
	ImGui::BeginChild("TankCodexStats", ImVec2(0.0f, 430.0f), true);
	const bool locked = currentRank < selectedConfig->requiredRank;
	ImGui::Text("Name: %s", selectedConfig->displayName.c_str());
	ImGui::Text("ID: %s", selectedConfig->id.c_str());
	ImGui::Text("Required Rank: %d  %s", selectedConfig->requiredRank, locked ? "(locked)" : "(available)");
	ImGui::Separator();
	ImGui::Text("Barrels: %zu", selectedConfig->barrels.size());
	ImGui::Text("Fire Mode: %s%s",
		selectedConfig->fireAllBarrels ? "All Barrels" : (selectedConfig->alternateBarrels ? "Alternate" : "Single"),
		selectedConfig->usesDrone ? " + Drone" : "");
	ImGui::Text("Bullet Count: %d", selectedConfig->bulletCount);
	ImGui::Text("Reload Scale: %.2f", selectedConfig->reloadScale);
	ImGui::Text("Bullet Speed Scale: %.2f", selectedConfig->bulletSpeedScale);
	ImGui::Text("Bullet Damage Scale: %.2f", selectedConfig->bulletDamageScale);
	ImGui::Text("Spread: %.1f deg  %s", selectedConfig->spreadAngleDeg, selectedConfig->randomSpread ? "random" : "fixed");
	ImGui::Text("Reflect: %s", selectedConfig->reflect ? "yes" : "no");
	ImGui::Text("Penetrate: %s", selectedConfig->penetrate ? "yes" : "no");
	ImGui::Text("Drones: %s  max %d", selectedConfig->usesDrone ? "yes" : "no", selectedConfig->maxDrones);
	ImGui::Separator();
	ImGui::TextWrapped("Role: %s",
		selectedConfig->usesDrone ? "Controls drones and keeps pressure while repositioning." :
		selectedConfig->reflect ? "Uses bounce shots to fight around cover." :
		selectedConfig->bulletSpeedScale > 1.2f ? "Long range, fast projectile style." :
		selectedConfig->reloadScale < 0.75f ? "Rapid fire tank for close and mid range pressure." :
		selectedConfig->bulletCount > 1 || selectedConfig->barrels.size() >= 3 ? "Wide multi-shot tank that controls space." :
		"Balanced starter tank.");
	ImGui::Spacing();
	if (locked) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
	}
	const bool changeClicked = ImGui::Button(locked ? "Locked" : "Change To This Tank", ImVec2(-1.0f, 32.0f));
	if (locked) {
		ImGui::PopStyleColor(3);
	}
	if (!locked && changeClicked) {
		EvolveById(selectedConfig->id);
	}
	if (locked) {
		ImGui::TextWrapped("Need Rank %d. Use debug exp or play to unlock it.", selectedConfig->requiredRank);
	}
	if (ImGui::Button("Edit This In Class Editor", ImVec2(-1.0f, 28.0f))) {
		editorSelectedClassIndex_ = codexSelectedClassIndex_;
	}
	ImGui::EndChild();

	ImGui::Columns(1);
	ImGui::End();
#endif
}

int Player::GetRankFromLevel(int level) const {
	if (level >= 15) return 4;
	if (level >= 10) return 3;
	if (level >= 5) return 2;
	return 1;
}

int Player::GetUpgradeLevel(int index) const
{
	if (index < 0 || index >= static_cast<int>(upgradeLevels_.size())) {
		return 0;
	}
	return upgradeLevels_[index];
}

const char* Player::GetCurrentClassName() const
{
	if (const PlayerClassConfig* config = GetCurrentClassConfig()) {
		return config->displayName.c_str();
	}
	switch (currentClass_) {
	case ClassType::Basic: return "Basic";
	case ClassType::Twin: return "Twin";
	case ClassType::MachineGun: return "MachineGun";
	case ClassType::Overseer: return "Overseer";
	case ClassType::Triple: return "Triple";
	case ClassType::Assassin: return "Assassin";
	case ClassType::Bounder: return "Bounder";
	case ClassType::Ninja: return "Ninja";
	case ClassType::Smasher: return "Smasher";
	case ClassType::Summoner: return "Summoner";
	}
	return "Unknown";
}

bool Player::ApplyStatUpgrade(int index)
{
	if (runModifiers_.enabled) {
		return false;
	}
	if (skillPoints_ <= 0 || index < 0 || index >= static_cast<int>(upgradeLevels_.size())) {
		return false;
	}
	if (upgradeLevels_[index] >= maxEnhancePoint) {
		return false;
	}

	upgradeLevels_[index]++;
	skillPoints_--;

	switch (index) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	case 5:
		break;
	case 6:
		break;
	}

	RecalculateStatsFromBase(false);
	statUpgradePerformedEvent_ = true;
	return true;
}

bool Player::ConsumeStatUpgradePerformedEvent()
{
	const bool performed = statUpgradePerformedEvent_;
	statUpgradePerformedEvent_ = false;
	return performed;
}

bool Player::RefundStatUpgrade(int index)
{
	if (runModifiers_.enabled) {
		return false;
	}
	if (index < 0 || index >= static_cast<int>(upgradeLevels_.size())) {
		return false;
	}
	if (upgradeLevels_[index] <= 0) {
		return false;
	}

	upgradeLevels_[index]--;
	skillPoints_++;
	RecalculateStatsFromBase(false);
	return true;
}

void Player::RecalculateStatsFromBase(bool healToFull)
{
	const int oldMaxHp = GetMaxHp();
	const bool wasFullHp = oldMaxHp > 0 && hp_ >= oldMaxHp;
	const float previousStamina = stats_.stamina;

	stats_ = baseStats_;
	if(runModifiers_.enabled&&expeditionCombatStyleSelected_) {
		const auto& profile=GetCombatStyleProfile(expeditionCombatStyle_);
		stats_.maxHp=profile.maxHp;stats_.moveSpeed=profile.moveSpeed;
		stats_.maxStamina=profile.maxStamina;stats_.staminaRecovery=profile.staminaRecovery;
		stats_.bodyDamage=profile.bodyDamage;stats_.bulletSpeed=profile.bulletSpeed;
		stats_.bulletDamage=profile.attackDamage/(expeditionCombatStyle_==tankbuild::Style::Melee?3.8f:1.0f);
		stats_.reloadSpeed=profile.attackIntervalSeconds*60.0f;
	}
	stats_.staminaRecovery *= 1.0f + healthRegenUpgradeRate_ * static_cast<float>(upgradeLevels_[0]);
	stats_.maxHp *= 1.0f + maxHpUpgradeRate_ * static_cast<float>(upgradeLevels_[1]);
	stats_.bodyDamage *= 1.0f + bodyDamageUpgradeRate_ * static_cast<float>(upgradeLevels_[2]);
	stats_.bulletSpeed *= 1.0f + bulletSpeedUpgradeRate_ * static_cast<float>(upgradeLevels_[3]);
	stats_.bulletDamage *= 1.0f + bulletDamageUpgradeRate_ * static_cast<float>(upgradeLevels_[4]);
	stats_.reloadSpeed *= (std::max)(0.05f, 1.0f - reloadUpgradeRate_ * static_cast<float>(upgradeLevels_[5]));
	if(!expeditionCombatStyleSelected_)stats_.reloadSpeed = (std::max)(minReloadSpeed_, stats_.reloadSpeed);
	stats_.moveSpeed *= 1.0f + moveSpeedUpgradeRate_ * static_cast<float>(upgradeLevels_[6]);
	const TankRunTuning runTuning = MakeTankRunTuning(runModifiers_, runGrowth_);
	stats_.bulletDamage *= runTuning.damage;
	stats_.bulletSpeed *= runTuning.bulletSpeed;
	stats_.reloadSpeed *= runTuning.reloadInterval;
	stats_.moveSpeed *= runTuning.moveSpeed;
	stats_.staminaRecovery *= runTuning.staminaRecovery;
	stats_.maxHp *= runTuning.maxHp;
	if (runModifiers_.enabled) {
		stats_.moveSpeed *= runMaintenance_.MoveScale();
		stats_.reloadSpeed *= runMaintenance_.ReloadScale();
		stats_.stamina = previousStamina;
	}
	stats_.maxHp = (std::max)(1.0f, stats_.maxHp);
	stats_.reloadSpeed = (std::max)(0.05f, stats_.reloadSpeed);
	stats_.bulletDamage = (std::max)(0.1f, stats_.bulletDamage);
	stats_.bulletSpeed = (std::max)(0.01f, stats_.bulletSpeed);
	stats_.moveSpeed = (std::max)(0.01f, stats_.moveSpeed);
	stats_.maxStamina = (std::max)(0.0f, stats_.maxStamina);
	stats_.stamina = (std::min)(stats_.stamina, stats_.maxStamina);

	SetDamage(static_cast<uint32_t>((std::max)(1.0f, stats_.bodyDamage)));

	const int newMaxHp = GetMaxHp();
	if (healToFull || wasFullHp) {
		hp_ = newMaxHp;
	} else {
		hp_ = (std::clamp)(hp_, 0, newMaxHp);
	}
}

void Player::UpdateStealth(float deltaTime) {
	// Assassinの仕様: しばらく経つとステルス
	if (currentClass_ == ClassType::Assassin) {
		// 移動入力があるか、攻撃しているかをチェック
		if (cg2::Length(velocity_) > 0.1f || input_->IsPress(input_->GetMouseState().rgbButtons[0])) {
			stealthTimer_ = 0.0f;
			isStealth_ = false;
		} else {
			stealthTimer_ += deltaTime;
			if (stealthTimer_ >= 2.0f) { // 2秒静止でステルス
				isStealth_ = true;
			}
		}
	}

	// Ninjaの仕様: ダッシュまたはジャスト回避でステルス
	if (currentClass_ == ClassType::Ninja) {
		if (isDashing_ || isJustEvaded_) {
			isStealth_ = true;
			stealthTimer_ = 1.5f; // 1.5秒間持続
		}

		if (stealthTimer_ > 0) {
			stealthTimer_ -= deltaTime;
		} else {
			isStealth_ = false;
		}
	}

	// アルファ値の適用 (描画時に反映させる)
	float targetAlpha = isStealth_ ? 0.2f : 1.0f;
	stealthAlpha_ = cg2::Lerp(stealthAlpha_, targetAlpha, 0.1f);
	// 実際のモデルのカラーに適用
	object_->SetAlpha(stealthAlpha_);
	for (BarrelModel& barrel : barrels_) {
		if (barrel.object) {
			barrel.object->SetAlpha(stealthAlpha_);
		}
	}
}

void Player::UpdateSummoner(float deltaTime) {
	if (currentClass_ != ClassType::Summoner) return;

	// ドローンの数が足りなければ生成
	if (drones_.size() < kMaxSummonerDrones) {
		summonTimer_ += deltaTime;
		if (summonTimer_ > 2.0f) {
			// この旧召喚タイマーでは生成しない。遠征の召喚は現在のドローン更新処理で行う。

			summonTimer_ = 0.0f;
		}
	}
}

void Player::SpawnCasing() {
	cg2::ParticleManager::GetInstance()->Emit("CasingSpark", GetWorldPosition() + dir_ * 1.0f, 2);
}

void Player::SpawnAfterimage() {
	cg2::Vector3 position = GetWorldPosition();
	cg2::Vector3 moveDirection = velocity_;
	moveDirection.z = 0.0f;
	if (cg2::Length(moveDirection) > 0.001f) {
		position -= cg2::Normalize(moveDirection) * 0.65f;
	}
	cg2::ParticleManager::GetInstance()->Emit("DashDust", position, 1);
	cg2::ParticleManager::GetInstance()->EmitNeonMovementEffect(position, moveDirection);
}

std::vector<Player::NeonBarrelLayout> Player::GetNeonBarrelLayouts() const
{
	std::vector<NeonBarrelLayout> layouts;
	if (IsDroneBuild()) return layouts;
	if (IsMeleeBuild()) {
		NeonBarrelLayout blade{};
		blade.isMelee = true;
		blade.offset = {0.60f,-0.40f,0.0f};
		blade.scale = {runModifiers_.bladeReach ? 2.4f : 1.85f,0.22f,0.22f};
		blade.angleRad = -0.22f;
		blade.barrelColor = {0.18f,1.35f,1.70f,1.0f};
		blade.outlineColor = {0.65f,1.6f,1.75f,1.0f};
		layouts.push_back(blade);
		return layouts;
	}
	const PlayerClassConfig* config = GetCurrentClassConfig();
	if (!config) {
		NeonBarrelLayout layout{};
		if (!barrels_.empty()) {
			layout.muzzleFlashRatio = kMuzzleFlashDuration > 0.0f
				? (std::clamp)(barrels_[0].muzzleFlashTimer / kMuzzleFlashDuration, 0.0f, 1.0f)
				: 0.0f;
		}
		layouts.push_back(layout);
		return layouts;
	}

	layouts.reserve(config->barrels.size());
	for (size_t i = 0; i < config->barrels.size(); ++i) {
		const WeaponMountConfig& barrel = config->barrels[i];
		NeonBarrelLayout layout{};
		layout.offset = barrel.offset;
		layout.scale = barrel.scale;
		layout.angleRad = barrel.angleDeg * 3.1415926535f / 180.0f;
		layout.isMelee = barrel.weaponType == WeaponType::Melee;
		layout.shape = barrel.barrelShape;
		layout.fireGroup = barrel.fireGroup;
		layout.barrelColor = barrel.barrelColor;
		layout.outlineColor = barrel.outlineColor;
		if (i < barrels_.size()) {
			layout.recoilOffset = barrels_[i].recoilOffset;
			layout.muzzleFlashRatio = kMuzzleFlashDuration > 0.0f
				? (std::clamp)(barrels_[i].muzzleFlashTimer / kMuzzleFlashDuration, 0.0f, 1.0f)
				: 0.0f;
		}
		layouts.push_back(layout);
	}
	return layouts;
}

Player::NeonBodyLayout Player::GetNeonBodyLayout() const
{
	NeonBodyLayout layout{};
	const PlayerClassConfig* config = GetCurrentClassConfig();
	if (!config) {
		return layout;
	}
	layout.shape = config->bodyShape;
	layout.scale = config->bodyScale;
	layout.fillColor = config->bodyFillColor;
	layout.outlineColor = config->bodyOutlineColor;
	return layout;
}

float Player::GetDamageFeedbackRatio() const
{
	if (damageFeedbackDuration_ <= 0.0f) {
		return 0.0f;
	}
	return (std::clamp)(damageFeedbackTimer_ / damageFeedbackDuration_, 0.0f, 1.0f);
}

void Player::SpawnBuffParticle() {
	cg2::ParticleManager::GetInstance()->Emit("DashDust", GetWorldPosition(), 1);
}
cg2::Vector2 Player::WorldToScreen(const cg2::Vector3& worldPos, cg2::Camera* camera) {
	// 1. ビュープロジェクション行列で変換

	cg2::Matrix4x4 matVP = camera->GetViewMatrix() * camera->GetProjectionMatrix();
	cg2::Vector3 ndcPos = cg2::TransformMatrix(worldPos, matVP);

	// 2. NDC座標 (-1.0 ~ 1.0) をスクリーン座標 (0 ~ ウィンドウ幅/高) に変換
	// ※ WinAppなどのシングルトンから画面サイズを取得してください
	float screenX = (ndcPos.x + 1.0f) * 0.5f * cg2::WinApp::kClientWidth;
	float screenY = (1.0f - ndcPos.y) * 0.5f * cg2::WinApp::kClientHeight;

	return { screenX, screenY };
}
