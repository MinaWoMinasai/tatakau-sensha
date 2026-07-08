#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "Object3d.h"

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
		std::unique_ptr<Object3d> hull;
		bool alive = true;
	};

	struct Projectile {
		Vector3 position = {};
		Vector3 velocity = {};
		float life = 0.0f;
		float damage = 20.0f;
		float radius = 3.0f;
		std::unique_ptr<Object3d> object;
	};

	struct ImpactEffect {
		Vector3 position = {};
		float life = 0.0f;
		float duration = 1.0f;
		float baseScale = 1.0f;
		bool waterSplash = false;
		std::unique_ptr<Object3d> object;
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
	void SpawnImpactEffect(const Vector3& position, bool waterSplash);
	void UpdateCamera();
	void FireMainGun();
	void DrawDebugWindow();
	void DrawBattleHud();

	Vector3 ForwardFromYaw(float yaw) const;
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

	std::unique_ptr<Object3d> sea_;
	std::unique_ptr<Object3d> playerHull_;
	std::unique_ptr<Object3d> playerTurret_;
	std::unique_ptr<Object3d> playerMarker_;
	std::vector<EnemyShip> enemies_;
	std::vector<Projectile> projectiles_;
	std::vector<ImpactEffect> impactEffects_;

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
};
