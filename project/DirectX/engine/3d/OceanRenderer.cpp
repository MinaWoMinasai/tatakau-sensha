#include "OceanRenderer.h"

#include <algorithm>
#include <cmath>

#include "Camera.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "Model.h"
#include "Object3dCommon.h"

namespace {
constexpr size_t AlignConstantBufferSize(size_t size)
{
	return (size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
		~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}
}

void OceanRenderer::Initialize(Model* gridModel, uint32_t environmentSrvIndex)
{
	gridModel_ = gridModel;
	environmentSrvIndex_ = environmentSrvIndex;

	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	parameterResource_ = dxCommon->CreateBufferResource(
		AlignConstantBufferSize(sizeof(OceanParameters)));
	parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
	UploadParameters();
}

void OceanRenderer::Update(
	float time,
	Camera& camera,
	DebugCamera& debugCamera,
	bool useDebugCamera)
{
	parameters_.time = time;
	if (useDebugCamera) {
		parameters_.viewProjection = debugCamera.GetViewProjectionMatrix();
		parameters_.cameraPosition = debugCamera.GetEyePosition();
	} else {
		parameters_.viewProjection = camera.GetViewProjectionMatrix();
		parameters_.cameraPosition = camera.GetTranslate();
	}

	// Only the finite grid follows the camera. Ocean.VS evaluates every wave
	// from the resulting world position, so this movement cannot drag the pattern.
	parameters_.gridOrigin.x = parameters_.cameraPosition.x;
	parameters_.gridOrigin.y = parameters_.baseHeight;
	parameters_.gridOrigin.z = parameters_.cameraPosition.z;
	parameters_.sunDirection = Object3dCommon::GetInstance()->GetLightDir();
	UploadParameters();
}

void OceanRenderer::Draw() const
{
	if (!gridModel_ || !parameterResource_) {
		return;
	}

	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	auto commandList = dxCommon->GetList();
	auto& pso = dxCommon->GetPSOOceanForScene();
	commandList->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
	commandList->SetPipelineState(pso.graphicsState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(
		0,
		parameterResource_->GetGPUVirtualAddress());
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		1,
		environmentSrvIndex_);
	gridModel_->DrawOnlyMesh();
}

void OceanRenderer::SetMode(Mode mode)
{
	parameters_.mode = static_cast<float>(mode);
}

void OceanRenderer::SetTint(const Vector4& tint)
{
	parameters_.tint = tint;
}

void OceanRenderer::SetBaseHeight(float height)
{
	parameters_.baseHeight = height;
}

void OceanRenderer::SetWind(
	const Vector2& direction,
	float speed,
	float choppiness)
{
	const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
	parameters_.windDirection =
		length > 0.0001f
		? Vector2{ direction.x / length, direction.y / length }
		: Vector2{ 0.18f, 0.98f };
	parameters_.windSpeed = (std::max)(speed, 0.0f);
	parameters_.choppiness = (std::max)(choppiness, 0.0f);
}

void OceanRenderer::SetSun(
	float intensity,
	float sunSpecularStrength,
	float artisticSunLaneStrength)
{
	parameters_.sunIntensity = (std::max)(intensity, 0.0f);
	parameters_.sunSpecularStrength = (std::max)(sunSpecularStrength, 0.0f);
	parameters_.artisticSunLaneStrength = (std::max)(artisticSunLaneStrength, 0.0f);
}

void OceanRenderer::SetDiagnostics(
	bool sunPathEnabled,
	bool atmosphereEnabled,
	bool farFlattenEnabled,
	bool proceduralCloudReflectionEnabled,
	int debugMode,
	float atmosphereStrength,
	float farFlattenStrength)
{
	parameters_.diagnosticsEnabled = 1.0f;
	parameters_.sunPathEnabled = sunPathEnabled ? 1.0f : 0.0f;
	parameters_.atmosphereEnabled = atmosphereEnabled ? 1.0f : 0.0f;
	parameters_.farFlattenEnabled = farFlattenEnabled ? 1.0f : 0.0f;
	parameters_.proceduralCloudReflectionEnabled =
		proceduralCloudReflectionEnabled ? 1.0f : 0.0f;
	parameters_.debugMode = static_cast<float>((std::clamp)(debugMode, 0, 5));
	parameters_.atmosphereStrength = (std::max)(atmosphereStrength, 0.0f);
	parameters_.farFlattenStrength = (std::clamp)(farFlattenStrength, 0.0f, 1.0f);
}

void OceanRenderer::SetEnvironmentSrvIndex(uint32_t environmentSrvIndex)
{
	environmentSrvIndex_ = environmentSrvIndex;
}

void OceanRenderer::UploadParameters()
{
	if (parameterData_) {
		*parameterData_ = parameters_;
	}
}
