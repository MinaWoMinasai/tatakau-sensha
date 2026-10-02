#include "Camera.h"
#include "WinApp.h"

namespace cg2 {

Camera::Camera() {
	transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f,0.0f}, {0.0f, 0.0f, -20.0f} };
	fovY_ = 0.45f;
	aspectRatio_ = (float(WinApp::kClientWidth) / float(WinApp::kClientHeight));
	nearClip_ = 0.1f;
	farClip_ = 5000.0f;
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	viewMatrix_ = Inverse(worldMatrix_);
	UpdateProjectionMatrices();
}

void Camera::Update() {
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	viewMatrix_ = Inverse(worldMatrix_);
	UpdateProjectionMatrices();
}

void Camera::SetProjectionJitter(const Vector2& jitter) {
	projectionJitter_ = jitter;
	UpdateProjectionMatrices();
}

void Camera::UpdateProjectionMatrices() {
	unjitteredProjectionMatrix_ = MakePerspectiveForMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	unjitteredViewProjectionMatrix_ = Multiply(viewMatrix_, unjitteredProjectionMatrix_);
	projectionMatrix_ = unjitteredProjectionMatrix_;
	projectionMatrix_.m[2][0] += projectionJitter_.x;
	projectionMatrix_.m[2][1] += projectionJitter_.y;
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}

} // namespace cg2
