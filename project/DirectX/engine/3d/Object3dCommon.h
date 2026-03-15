#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "SrvManager.h"
#include "ShadowMap.h"

class Object3dCommon
{
public:

	// シングルトン
	static Object3dCommon* GetInstance();

	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShadowMap* shadowMap);

	void Update();

	// 共通描画設定
	void PreDraw(BlendMode blendMode);

	DirectXCommon* GetDxCommon() { return dxCommon_; }

	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
	Camera* GetDefaultCamera() { return defaultCamera_; }
	void SetDebugDefaultCamera(DebugCamera* debugCamera) { debugDefaultCamera_ = debugCamera; }
	DebugCamera* GetDebugCamera() { return debugDefaultCamera_; }

	bool& GetIsDebugCamera() { return isDebugCamera_; }
	void SetIsDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

	Matrix4x4& GetLightViewProjection() { return lightViewProjection_; }

	SrvManager* GetSrvManager() { return srvManager_; }

	BlendMode GetBlendMode() { return blendMode_; }

	ShadowMap* GetShadowMap() { return shadowMap_; }

	Vector3& GetLightDir() { return lightDir_; }

private:

	DirectXCommon* dxCommon_ = nullptr;

	Camera* defaultCamera_ = nullptr;
	DebugCamera* debugDefaultCamera_ = nullptr;

	bool isDebugCamera_ = false;
	Matrix4x4 lightViewProjection_;
	SrvManager* srvManager_ = nullptr;
	ShadowMap* shadowMap_ = nullptr;

	BlendMode blendMode_;

	Vector3 lightDir_;
};

