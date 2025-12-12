#pragma once
#include "Struct.h"
#include "Calculation.h"

class Camera
{

public:

	Camera();

	// 更新
	void Update();

	// setter
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(const float& fovY) { fovY_ = fovY; }
	void SetAspectRatio(const float& aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(const float& nearClip) { nearClip_ = nearClip; }
	void SetFarClip(const float& farClip) { farClip_ = farClip; }

	// getter
	Matrix4x4& GetWorldMatrix() { return worldMatrix_; }
	Matrix4x4& GetViewMatrix() { return viewMatrix_; }
	Matrix4x4& GetProjectionMatrix() { return projectionMatrix_; }
	Matrix4x4& GetViewProjectionMatrix() { return viewProjectionMatrix_; }
	Vector3& GetRotate() { return transform_.rotate; }
	Vector3& GetTranslate() { return transform_.translate; }

private:
	Transform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;
};

