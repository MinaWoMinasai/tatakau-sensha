#pragma once
#include "DeveloperTools.h"

// Releaseでは明示的なDeveloperTools overrideがあってもPreviewをコンパイルしない。
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Object3d.h"
#include "SkinCluster.h"
#include "DirectX/engine/3d/neon/NeonSkinnedRenderer.h"
#include <memory>
#include <string>
#include <vector>

// GameSceneのDeveloper UIからのみ使用。初期状態は無効、GPU資源は有効化時に作成する。
class NeonSkinnedPreview {
public:
	void Initialize(cg2::Camera* camera, cg2::DebugCamera* debugCamera);
	// 前フレームのFence完了後、Camera更新後に1フレーム1回呼ぶ。
	void Update(float deltaTime);
	// Bloom::PreDraw直後のScene HDR / Normal / Material + D24S8内でのみ呼ぶ。
	void Draw();
	void DrawImGui();

private:
	struct SourceMaterial {
		std::string meshName;
		std::string materialName;
		std::string alphaMode;
		float alphaCutoff = 0.0f;
	};
	void Load();
	void PlaceInFrontOfCamera();
	cg2::Vector3 GetCameraPosition() const;

	cg2::Camera* camera_ = nullptr;
	cg2::DebugCamera* debugCamera_ = nullptr;
	bool enabled_ = false;
	bool neonMode_ = true;
	bool ready_ = false;
	bool alphaCutoutEnabled_ = true;
	cg2::Transform transform_{ { 8.0f, 8.0f, 8.0f }, {}, {} }; // 正面から顔の内部線を比較する。
	cg2::NeonSkinnedParams params_;
	std::unique_ptr<cg2::SkinnedModel> model_;
	std::unique_ptr<cg2::Object3d> object_;
	cg2::NeonSkinnedRenderer renderer_;
	std::vector<SourceMaterial> sourceMaterials_;
	std::vector<cg2::NeonSkinnedSubmeshParams> submeshParams_;
	size_t sourceAnimationCount_ = 0;
	std::string loadError_;
};
#endif
