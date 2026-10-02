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

	float GetDistance() const { return distance; }

	Vector3 GetEyePosition() { return eye_; }

	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	const Matrix4x4& GetUnjitteredProjectionMatrix() const { return unjitteredProjectionMatrix_; }
	const Matrix4x4& GetUnjitteredViewProjectionMatrix() const { return unjitteredViewProjectionMatrix_; }
	const Vector2& GetProjectionJitter() const { return projectionJitter_; }
	float GetNearClip() const { return nearClip_; }
	float GetFarClip() const { return farClip_; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }
	void SetProjectionJitter(const Vector2& jitter);

private:
	void UpdateProjectionMatrices();

	// X,Y,Z軸回りのローカル回転角
	Vector3 rotation_ = {0,0,0};
	// ローカル座標
	Vector3 translation_ = {19.0f,12.3f, 0.15f};
	// ビュー行列
	Matrix4x4 viewMatrix_ = MakeIdentity4x4();
	// 射影行列
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	Matrix4x4 unjitteredProjectionMatrix_;
	Matrix4x4 unjitteredViewProjectionMatrix_;
	Vector2 projectionJitter_ = { 0.0f, 0.0f };
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

