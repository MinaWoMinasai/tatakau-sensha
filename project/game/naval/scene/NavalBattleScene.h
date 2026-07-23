#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "Object3d.h"
#include "Skybox.h"
#include "game/naval/rendering/NavalOceanRenderer.h"

// 海戦ゲーム用の最小プロトタイプシーン。
// 既存の2D/3Dアクション実装と衝突しないよう、naval配下だけで操作・砲撃・敵艦の骨組みを試す。
class NavalBattleScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override {}
	void DrawShadow() override;
	void DrawPostEffect3D() override;
	void DrawSprite() override {}

	bool IsFinished() const override { return finished_; }
	float GetFinalDeltaTime() const override { return finalDeltaTime_; }
	std::string GetNextSceneName() const override { return nextSceneName_; }

private:
	struct ShipState {
		Vector3 position = {};
		float yaw = 0.0f;
		float speed = 0.0f;
		float hp = 100.0f;
		float maxHp = 100.0f;
		float reload = 0.0f;
	};

	struct EnemyShip {
		ShipState ship;
		Vector3 formationOffset = {};
		std::unique_ptr<Object3d> hull;
		bool alive = true;
		bool sinking = false;
		bool detectedPlayer = false;
		float aimYaw = 0.0f;
		float desiredRange = 145.0f;
		float sinkTimer = 0.0f;
		float deathFlash = 0.0f;
		int broadsideDirection = 1;
	};

	struct Projectile {
		Vector3 position = {};
		Vector3 velocity = {};
		float life = 0.0f;
		float damage = 20.0f;
		float radius = 3.0f;
		bool fromPlayer = true;
		std::unique_ptr<Object3d> object;
	};

	struct ImpactEffect {
		Vector3 position = {};
		Vector3 velocity = {};
		float life = 0.0f;
		float duration = 1.0f;
		float baseScale = 1.0f;
		float verticalScale = 1.0f;
		bool waterSplash = false;
		std::unique_ptr<Object3d> object;
	};

	struct WakeTrail {
		Vector3 position = {};
		Vector3 velocity = {};
		float yaw = 0.0f;
		float life = 0.0f;
		float duration = 1.0f;
		float baseWidth = 1.0f;
		float baseLength = 1.0f;
		float baseAlpha = 0.25f;
		std::unique_ptr<Object3d> object;
	};

	struct OceanWakeSource {
		Vector3 position = {};
		Vector3 direction = {};
		float age = 0.0f;
		float strength = 0.0f;
		float type = 0.0f;
	};

	struct GunMount {
		Vector3 localOffset = {};
		float centerYaw = 0.0f;
		float halfArc = 0.0f;
	};

	void ResetBattle();
	void InitializeObject(Object3d& object, const std::string& modelPath, const Vector4& color, bool lighting);
	void UpdatePlayer();
	void UpdateEnemies();
	void UpdateProjectiles();
	void SpawnEnemyShell(EnemyShip& enemy);
	void SpawnImpactEffect(const Vector3& position, bool waterSplash);
	void AddCameraShake(float intensity, float duration);
	int CountAliveEnemies() const;
	void UpdateWakeTrails();
	void UpdateOceanWakeSources();
	void ApplyOceanWakeToOcean();
	void SpawnWakeTrail();
	void SpawnHullFoamTrail();
	void AddWaterFoamTrail(const Vector3& position, const Vector3& velocity, float yaw, float duration, float width, float length, float alpha);
	void UpdateCamera();
	void FireMainGun();
	void DrawDebugWindow();
	void DrawBattleHud();

	Vector3 ForwardFromYaw(float yaw) const;
	Vector3 VelocityFromShip(const ShipState& ship) const;
	Vector3 PredictBallisticTargetPosition(const Vector3& muzzlePosition, const Vector3& targetPosition, const Vector3& targetVelocity, float horizontalSpeed) const;
	float SampleOceanHeight(const Vector3& position, float time) const;
	Vector3 GetReticleRayDirection() const;
	bool TryGetReticleWaterTarget(Vector3& targetPosition) const;
	float GetAimYaw() const;
	float GetEffectiveAimYaw() const;
	float GetAimVerticalVelocity() const;
	float CalculateBallisticVerticalVelocity(const Vector3& muzzlePosition, const Vector3& targetPosition, float horizontalSpeed) const;
	int GetAssistedTargetIndex() const;
	std::vector<GunMount> GetBearableGunMounts(float aimYaw) const;
	bool HasFiringSolution() const;
	float DistanceXZ(const Vector3& a, const Vector3& b) const;
	int FindPrecisionTargetFromAim() const;
	int FindNextPrecisionTarget(int direction) const;
	int FindLockTarget() const;
	float NormalizeAngle(float angle) const;

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;

	std::unique_ptr<NavalOceanRenderer> ocean_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<Object3d> playerHull_;
	std::unique_ptr<Object3d> playerTurret_;
	std::unique_ptr<Object3d> playerMarker_;
	std::vector<EnemyShip> enemies_;
	std::vector<Projectile> projectiles_;
	std::vector<ImpactEffect> impactEffects_;
	std::vector<WakeTrail> wakeTrails_;
	std::vector<OceanWakeSource> oceanWakeSources_;
	Vector3 lastOceanWakePosition_ = {};

	ShipState player_;
	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;
	float battleTimer_ = 0.0f;
	float cameraYawOffset_ = 0.0f;
	float cameraPitch_ = 0.46f;
	float cameraDistance_ = 70.0f;
	Vector2 aimOffset_ = {};
	int throttleStep_ = 0;
	bool precisionAimMode_ = false;
	int lockTargetIndex_ = -1;
	float precisionSwitchCooldown_ = 0.0f;
	float wakeSpawnTimer_ = 0.0f;
	float hullFoamSpawnTimer_ = 0.0f;
	float oceanWakeSpawnTimer_ = 0.0f;
	float oceanWakeDistanceAccumulator_ = 0.0f;
	bool hasLastOceanWakePosition_ = false;
	float playerVisualWaterHeight_ = 1.1f;
	float playerVisualPitch_ = 0.0f;
	float playerVisualRoll_ = 0.0f;
	float playerDraftOffset_ = -0.68f;
	float playerModelScale_ = 1.12f;
	float cameraShakeTime_ = 0.0f;
	float cameraShakeDuration_ = 0.0f;
	float cameraShakeIntensity_ = 0.0f;
	float playerDamageFlash_ = 0.0f;
	bool missionComplete_ = false;
	bool gameOver_ = false;
	bool showBattleHud_ = true;
	bool showHorizonFog_ = false;
	bool showFoamPlates_ = false;
	bool playerInvincible_ = false;
	Vector3 enemyFleetAnchor_ = {};
	float enemyFleetYaw_ = 0.0f;
	float enemyFleetSpeed_ = 0.0f;
	bool enemyFleetDetected_ = false;
	int enemyFleetBroadsideDirection_ = 1;
};
