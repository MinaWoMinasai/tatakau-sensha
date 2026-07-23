#include "NavalOceanRenderer.h"

#include <algorithm>
#include <cmath>

#include "ModelManager.h"
#include "Object3dCommon.h"
#include "TextureManager.h"

namespace {
const char* kArcBlancOceanGridModelName = "__arc_blanc_ocean_grid";
}

void NavalOceanRenderer::Initialize(const std::string& environmentTexturePath)
{
	ModelManager::GetInstance()->CreateGridModel(kArcBlancOceanGridModelName, 1800.0f, 1800.0f, 256, 256);

	sea_ = std::make_unique<Object3d>();
	sea_->Initialize();
	sea_->SetModel(kArcBlancOceanGridModelName);
	sea_->SetColor({ 0.72f, 0.82f, 0.92f, 1.0f });
	sea_->SetLighting(false);
	sea_->SetInsensity(1.35f);
	sea_->SetEnvironmentMap(TextureManager::GetInstance()->GetSrvIndex(environmentTexturePath));
	sea_->SetEnvironmentCoefficient(3.15f);
	sea_->SetScale({ kSeaScaleXZ, 1.0f, kSeaScaleXZ });
	sea_->SetTranslate({ 0.0f, -1.2f, 0.0f });
}

void NavalOceanRenderer::Update(float time)
{
	if (!sea_) {
		return;
	}

	sea_->SetShininess(time);
	sea_->Update();
}

void NavalOceanRenderer::Draw()
{
	if (!sea_) {
		return;
	}

	Object3dCommon::GetInstance()->PreDraw(kNone);
	sea_->Draw();
}

void NavalOceanRenderer::SetWakeData(
	const std::array<Vector4, 16>& wakePoints,
	const std::array<Vector4, 16>& wakeDirections,
	const Vector4& parameters)
{
	if (!sea_) {
		return;
	}

	sea_->SetOceanWakeData(wakePoints, wakeDirections, parameters);
}

void NavalOceanRenderer::SetShipReflection(const Vector3& position, float yaw, float strength)
{
	if (!sea_) {
		return;
	}

	sea_->SetEmissive(position, yaw);
	sea_->SetNormalDetail(std::clamp(strength, 0.0f, 1.0f), 1.0f);
}

float NavalOceanRenderer::SampleHeight(const Vector3& worldPosition, float time) const
{
	const float x = worldPosition.x / kSeaScaleXZ;
	const float z = worldPosition.z / kSeaScaleXZ;

	constexpr float dirALen = 0.9963935f;
	constexpr float dirBLen = 0.9984488f;
	constexpr float dirCLen = 0.9976472f;
	constexpr float dirFLen = 1.0001945f;
	constexpr float dirGLen = 0.9992497f;
	const float dirAx = 0.18f / dirALen;
	const float dirAz = 0.98f / dirALen;
	const float dirBx = -0.42f / dirBLen;
	const float dirBz = 0.91f / dirBLen;
	const float dirCx = 0.72f / dirCLen;
	const float dirCz = 0.69f / dirCLen;
	const float dirFx = 0.08f / dirFLen;
	const float dirFz = 0.997f / dirFLen;
	const float dirGx = -0.31f / dirGLen;
	const float dirGz = 0.95f / dirGLen;

	const float flow = z * 0.030f + time * 0.86f;
	const float cross = x * 0.095f;
	const float phaseA = (x * dirAx + z * dirAz) * 0.040f + time * 0.92f;
	const float phaseB = (x * dirBx + z * dirBz) * 0.078f - time * 1.34f;
	const float phaseC = (x * dirCx + z * dirCz) * 0.150f + time * 2.05f;
	const float phaseD = flow + std::sin(cross + time * 0.35f) * 0.55f;
	constexpr float dirDLen = 0.9976472f;
	constexpr float dirELen = 0.9976472f;
	const float dirDx = -0.88f / dirDLen;
	const float dirDz = 0.47f / dirDLen;
	const float dirEx = 0.36f / dirELen;
	const float dirEz = 0.93f / dirELen;
	const float spectrumPatch = 0.72f + 0.28f * std::sin((x * 0.53f + z * 0.85f) * 0.010f + time * 0.16f);
	const float phaseE = (x * dirDx + z * dirDz) * 0.245f - time * 2.90f;
	const float phaseF = (x * dirEx + z * dirEz) * 0.360f + time * 3.70f + std::sin((x * dirBx + z * dirBz) * 0.018f + time * 0.28f) * 0.85f;
	const float swellPatch = 0.68f + 0.32f * std::sin((x * -0.18f + z * 0.98f) * 0.0042f + time * 0.035f);
	const float phaseG = (x * dirFx + z * dirFz) * 0.0155f + std::sin((x * dirGx + z * dirGz) * 0.0065f + time * 0.055f) * 1.55f + time * 0.30f;
	const float phaseH = (x * dirGx + z * dirGz) * 0.0260f - std::sin((x * dirFx + z * dirFz) * 0.0080f - time * 0.045f) * 1.10f - time * 0.42f;

	return (
		std::sin(phaseA) * 0.34f +
		std::sin(phaseB) * 0.16f +
		std::sin(phaseC) * 0.11f +
		std::sin(phaseD) * 0.09f +
		spectrumPatch * (std::sin(phaseE) * 0.070f + std::sin(phaseF) * 0.044f) +
		swellPatch * (std::sin(phaseG) * 0.165f + std::sin(phaseH) * 0.095f)
		) * 1.32f;
}
