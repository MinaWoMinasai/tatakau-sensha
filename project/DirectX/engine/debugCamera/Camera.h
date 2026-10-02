#pragma once
#include "Struct.h"
#include "Calculation.h"

namespace cg2 {

class Camera
{

public:

	Camera();

	// 更新
	void Update();

	// setter
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }
	void SetProjectionJitter(const Vector2& jitter);

	// getter
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	const Matrix4x4& GetUnjitteredProjectionMatrix() const { return unjitteredProjectionMatrix_; }
	const Matrix4x4& GetUnjitteredViewProjectionMatrix() const { return unjitteredViewProjectionMatrix_; }
	const Vector2& GetProjectionJitter() const { return projectionJitter_; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	float GetNearClip() const { return nearClip_; }
	float GetFarClip() const { return farClip_; }

private:
	void UpdateProjectionMatrices();

	Transform transform_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;
	Matrix4x4 unjitteredProjectionMatrix_;
	Matrix4x4 unjitteredViewProjectionMatrix_;
	Vector2 projectionJitter_ = { 0.0f, 0.0f };
	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;
};

} // namespace cg2
