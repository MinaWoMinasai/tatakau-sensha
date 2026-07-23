#pragma once

#include <array>
#include <memory>
#include <string>

#include "Object3d.h"

class NavalOceanRenderer {
public:
	void Initialize(const std::string& environmentTexturePath);
	void Update(float time);
	void Draw();

	void SetWakeData(
		const std::array<Vector4, 16>& wakePoints,
		const std::array<Vector4, 16>& wakeDirections,
		const Vector4& parameters);
	void SetShipReflection(const Vector3& position, float yaw, float strength);

	float SampleHeight(const Vector3& worldPosition, float time) const;

private:
	static constexpr float kSeaScaleXZ = 0.50f;

	std::unique_ptr<Object3d> sea_;
};
