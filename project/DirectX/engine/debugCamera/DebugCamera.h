#pragma once
#define NOMINMAX
#include "Calculation.h"
#include <dinput.h>
#include "Input.h"
#include "algorithm"
#include <DirectXMath.h>
#include "externals/imgui/imgui.h"
#include "WinApp.h"

class DebugCamera
{

public:

	DebugCamera();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update(const DIMOUSESTATE& mousestate, std::span<const BYTE> key, Vector2 leftStick);

	Matrix4x4 GetViewMatrix() { return viewMatrix_; }

	float& GetDistance() { return distance; }

	Vector3 GetEyePosition() { return eye_; }

	Matrix4x4& GetProjectionMatrix() { return projectionMatrix_; }
	Matrix4x4& GetViewProjectionMatrix() { return viewProjectionMatrix_; }

private:
	// X,Y,Z軸回りのローカル回転角
	Vector3 rotation_ = {0,0,0};
	// ローカル座標
	Vector3 translation_ = {19.0f,12.3f, 0.15f};
	// ビュー行列
	Matrix4x4 viewMatrix_ = MakeIdentity4x4();
	// 射影行列
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;
	// カメラの移動速度
	// カメラ
	Matrix4x4 WorldMatrix_;
	
	Input input_;

	// 移動・回転速度調整用
	Vector3 velocity_ = { 0.01f, 0.01f, 1.0f }; // x=横回転速度, y=縦回転速度, z=ズーム速度
	float distance = 100.0f;             // 注視点との距離（ズーム）
	Vector3 target = {0.01f, 0.01f, 0.01f};             // 注視点のワールド座標
	Vector3 eye_; // カメラのワールド位置
};

