#pragma once
#include "Calculation.h"
#include "Model.h"
#include "DebugCamera.h"

class ObjectDraw
{
public:


	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="model">モデル</param>
	/// <param name="camera">カメラ</param>
	void Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& device, Descriptor descriptor, Command command);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(Renderer renderer, DebugCamera debugcamera, Microsoft::WRL::ComPtr<ID3D12Resource> materialResource, Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource);

private:

	// モデル
	Model suzanneModel_;
	Model teapotModel_;
	Model bunnyModel_;
	Model multiMeshModel_;
	Model planeModel_;

	// ワールドトランスフォーム
	Transform worldTransformSuzanne_;
	Transform worldTransformTeapot_;
	Transform worldTransformBunny_;
	Transform worldTransformMultiMesh_;
	Transform worldTransformPlane_;
};

