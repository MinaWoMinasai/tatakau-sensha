#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include "DebugCamera.h"

class Object3dCommon
{
public:

	// シングルトン
	static Object3dCommon* GetInstance();

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void PreDraw(BlendMode blendMode);

	DirectXCommon* GetDxCommon() { return dxCommon_; }

	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
	Camera* GetDefaultCamera() { return defaultCamera_; }
	void SetDebugDefaultCamera(DebugCamera* debugCamera) { debugDefaultCamera_ = debugCamera; }
	DebugCamera* GetDebugCamera() { return debugDefaultCamera_; }

	bool& GetIsDebugCamera() { return isDebugCamera_; }
	void SetIsDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

private:

	DirectXCommon* dxCommon_ = nullptr;

	Camera* defaultCamera_ = nullptr;
	DebugCamera* debugDefaultCamera_ = nullptr;

	bool isDebugCamera_ = false;

};

