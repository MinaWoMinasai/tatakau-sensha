#include "VfxLabScene.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <d3d12.h>
#include <wrl.h>

#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

using Microsoft::WRL::ComPtr;

class VfxLabBackgroundRenderer {
public:
	void Initialize(DirectXCommon* dxCommon)
	{
		assert(dxCommon);
		dxCommon_ = dxCommon;
		CreatePipeline();
		constantBuffer_ = dxCommon_->CreateBufferResource(sizeof(Parameters));
		constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedParameters_));
		*mappedParameters_ = {};
	}

	void Draw(
		int mode,
		float checkerScale,
		const Vector4& darkColor,
		const Vector4& lightColor)
	{
		if (!dxCommon_ || !pipelineState_ || !mappedParameters_) {
			return;
		}

		mappedParameters_->darkColor = darkColor;
		mappedParameters_->lightColor = lightColor;
		mappedParameters_->mode = static_cast<float>(mode);
		mappedParameters_->checkerScale = checkerScale;
		mappedParameters_->aspectRatio =
			static_cast<float>(WinApp::GetInstance()->GetClientWidth()) /
			static_cast<float>((std::max)(WinApp::GetInstance()->GetClientHeight(), 1));

		auto commandList = dxCommon_->GetList();
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(pipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(
			0,
			constantBuffer_->GetGPUVirtualAddress());
		commandList->DrawInstanced(3, 1, 0, 0);
	}

private:
	struct Parameters {
		Vector4 darkColor{};
		Vector4 lightColor{};
		float mode = 0.0f;
		float checkerScale = 14.0f;
		float aspectRatio = 1.0f;
		float padding = 0.0f;
	};

	void CreatePipeline()
	{
		D3D12_ROOT_PARAMETER rootParameter{};
		rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameter.Descriptor.ShaderRegister = 0;

		D3D12_ROOT_SIGNATURE_DESC rootDesc{};
		rootDesc.NumParameters = 1;
		rootDesc.pParameters = &rootParameter;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob);
		assert(SUCCEEDED(hr));
		hr = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature_));
		assert(SUCCEEDED(hr));

		IDxcBlob* vertexShader = dxCommon_->CompileShader(
			L"resources/shaders/VfxLabBackground.VS.hlsl",
			L"vs_6_0");
		IDxcBlob* pixelShader = dxCommon_->CompileShader(
			L"resources/shaders/VfxLabBackground.PS.hlsl",
			L"ps_6_0");
		assert(vertexShader && pixelShader);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature_.Get();
		desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		desc.SampleDesc.Count = 1;
		desc.NumRenderTargets = 3;
		desc.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
		desc.RTVFormats[1] = DirectXCommon::kNormalBufferFormat;
		desc.RTVFormats[2] = DirectXCommon::kMaterialBufferFormat;
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.RasterizerState.DepthClipEnable = TRUE;
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
			D3D12_COLOR_WRITE_ENABLE_ALL;

		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
			&desc,
			IID_PPV_ARGS(&pipelineState_));
		assert(SUCCEEDED(hr));
		vertexShader->Release();
		pixelShader->Release();
	}

	DirectXCommon* dxCommon_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12PipelineState> pipelineState_;
	ComPtr<ID3D12Resource> constantBuffer_;
	Parameters* mappedParameters_ = nullptr;
};

VfxLabScene::VfxLabScene() = default;
VfxLabScene::~VfxLabScene() = default;

void VfxLabScene::Initialize()
{
	input_ = Input::GetInstance();

	camera_ = std::make_unique<Camera>();
	camera_->SetNearClip(0.1f);
	camera_->SetFarClip(500.0f);
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();
	debugCamera_->GetDistance() = cameraDistance_;
	debugCamera_->SetNearClip(0.1f);
	debugCamera_->SetFarClip(500.0f);

	Object3dCommon::GetInstance()->SetDefaultCamera(camera_.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera_.get());
	Object3dCommon::GetInstance()->SetIsDebugCamera(false);
	UpdateCamera();

	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	backgroundRenderer_ = std::make_unique<VfxLabBackgroundRenderer>();
	backgroundRenderer_->Initialize(dxCommon);
	proceduralFlame_ = std::make_unique<ProceduralFlameRenderer>();
	proceduralFlame_->Initialize(dxCommon);
	proceduralFlame_->SetParameters(proceduralFlameParameters_);
}

void VfxLabScene::Update()
{
	if (input_->IsTrigger(input_->GetKey()[DIK_ESCAPE], input_->GetPreKey()[DIK_ESCAPE])) {
		finished_ = true;
		nextSceneName_ = "TITLE";
		return;
	}

	if (!pauseProceduralFlame_) {
		flameTime_ += finalDeltaTime_;
	}
	proceduralFlameParameters_.time = flameTime_;
	if (proceduralFlame_) {
		proceduralFlame_->SetParameters(proceduralFlameParameters_);
		if (!pauseProceduralFlame_) {
			proceduralFlame_->Update(finalDeltaTime_);
		}
	}

	if (!Object3dCommon::GetInstance()->GetIsDebugCamera()) {
		UpdateCamera();
	}
	debugCamera_->Update(
		input_->GetMouseState(),
		input_->GetKey(),
		input_->GetLeftStick());
	DrawDebugWindow();
}

void VfxLabScene::DrawPostEffect3D()
{
	if (backgroundRenderer_) {
		backgroundRenderer_->Draw(
			static_cast<int>(backgroundMode_),
			checkerScale_,
			checkerDarkColor_,
			checkerLightColor_);
	}

	if (!showProceduralFlame_ || !proceduralFlame_) {
		return;
	}

	const bool useDebugCamera = Object3dCommon::GetInstance()->GetIsDebugCamera();
	const Matrix4x4 cameraWorld = useDebugCamera
		? Inverse(debugCamera_->GetViewMatrix())
		: camera_->GetWorldMatrix();
	const Matrix4x4 viewProjection = useDebugCamera
		? debugCamera_->GetViewProjectionMatrix()
		: camera_->GetViewProjectionMatrix();
	proceduralFlame_->Draw(
		proceduralFlamePosition_,
		proceduralFlameSize_,
		cameraWorld,
		viewProjection);
}

void VfxLabScene::UpdateCamera()
{
	cameraYaw_ += (input_->IsPress(input_->GetKey()[DIK_D]) ? 1.0f : 0.0f)
		* finalDeltaTime_ * 0.9f;
	cameraYaw_ -= (input_->IsPress(input_->GetKey()[DIK_A]) ? 1.0f : 0.0f)
		* finalDeltaTime_ * 0.9f;
	cameraPitch_ += (input_->IsPress(input_->GetKey()[DIK_W]) ? 1.0f : 0.0f)
		* finalDeltaTime_ * 0.55f;
	cameraPitch_ -= (input_->IsPress(input_->GetKey()[DIK_S]) ? 1.0f : 0.0f)
		* finalDeltaTime_ * 0.55f;
	cameraPitch_ = std::clamp(cameraPitch_, -0.35f, 0.75f);

	const float wheel = static_cast<float>(input_->GetMouseState().lZ);
	if (std::abs(wheel) > 0.0f) {
		cameraDistance_ = std::clamp(cameraDistance_ - wheel * 0.012f, 12.0f, 80.0f);
	}

	const float cosPitch = std::cos(cameraPitch_);
	const Vector3 eye = {
		cameraTarget_.x + std::sin(cameraYaw_) * cosPitch * cameraDistance_,
		cameraTarget_.y + std::sin(cameraPitch_) * cameraDistance_,
		cameraTarget_.z - std::cos(cameraYaw_) * cosPitch * cameraDistance_,
	};
	const Vector3 diff = {
		cameraTarget_.x - eye.x,
		cameraTarget_.y - eye.y,
		cameraTarget_.z - eye.z,
	};
	const float yaw = std::atan2(diff.x, diff.z);
	const float horizontal = std::sqrt(diff.x * diff.x + diff.z * diff.z);
	const float pitch = -std::atan2(diff.y, horizontal);

	camera_->SetTranslate(eye);
	camera_->SetRotate({ pitch, yaw, 0.0f });
	camera_->Update();
}

void VfxLabScene::DrawDebugWindow()
{
#ifdef USE_IMGUI
	ImGui::Begin("VFX Lab");
	ImGui::Text("A,D: Orbit  W,S: Pitch  Mouse wheel: Zoom  Esc: Title");
	ImGui::Text("Shift+D: Debug camera  MMB: Orbit  Shift+MMB: Pan  Wheel: Zoom");

	const char* backgroundModes[] = { "Black", "Neutral Dark Gray", "Checker" };
	int backgroundMode = static_cast<int>(backgroundMode_);
	if (ImGui::Combo(
		"Background",
		&backgroundMode,
		backgroundModes,
		IM_ARRAYSIZE(backgroundModes))) {
		backgroundMode_ = static_cast<BackgroundMode>(backgroundMode);
	}
	if (backgroundMode_ == BackgroundMode::Checker) {
		ImGui::DragFloat("Checker scale", &checkerScale_, 0.25f, 2.0f, 64.0f);
		ImGui::ColorEdit3("Checker dark", &checkerDarkColor_.x);
		ImGui::ColorEdit3("Checker light", &checkerLightColor_.x);
	}

	if (ImGui::CollapsingHeader("Effect", ImGuiTreeNodeFlags_DefaultOpen) &&
		ImGui::TreeNodeEx("Procedural Flame", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Show", &showProceduralFlame_);
		ImGui::SameLine();
		ImGui::Checkbox("Pause", &pauseProceduralFlame_);

		const char* displayModes[] = {
			"Rainbow Outer Contour",
			"Scalar Field",
			"Filled Mask",
			"Contour Mask",
		};
		int displayMode = static_cast<int>(proceduralFlameParameters_.displayMode);
		if (ImGui::Combo(
			"Display Mode",
			&displayMode,
			displayModes,
			IM_ARRAYSIZE(displayModes))) {
			proceduralFlameParameters_.displayMode =
				static_cast<ProceduralFlameRenderer::DisplayMode>(displayMode);
		}
		if (ImGui::Button("Reset Metaballs") && proceduralFlame_) {
			proceduralFlame_->ResetMetaballs();
		}

		ImGui::DragFloat3("Position", &proceduralFlamePosition_.x, 0.1f);
		ImGui::DragFloat2("Billboard Size", &proceduralFlameSize_.x, 0.1f, 0.1f, 100.0f);
		int activeMetaballs =
			static_cast<int>(proceduralFlameParameters_.activeMetaballCount);
		if (ImGui::SliderInt("Active Metaballs", &activeMetaballs, 6, 12)) {
			proceduralFlameParameters_.activeMetaballCount =
				static_cast<uint32_t>(activeMetaballs);
		}
		ImGui::DragFloat("Flow Speed", &proceduralFlameParameters_.flowSpeed, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Radius Scale", &proceduralFlameParameters_.radiusScale, 0.01f, 0.35f, 2.0f);
		ImGui::DragFloat("Lateral Sway", &proceduralFlameParameters_.swayStrength, 0.01f, 0.0f, 3.0f);
		ImGui::DragFloat("Spawn Spread", &proceduralFlameParameters_.spawnSpread, 0.005f, 0.0f, 0.45f);
		ImGui::DragFloat(
			"Compact Support Scale",
			&proceduralFlameParameters_.compactSupportScale,
			0.01f,
			1.25f,
			4.0f);
		ImGui::DragFloat(
			"Satellite Separation",
			&proceduralFlameParameters_.satelliteSeparation,
			0.005f,
			0.0f,
			0.35f);
		ImGui::DragFloat("Noise Scale", &proceduralFlameParameters_.noiseScale, 0.05f, 0.1f, 16.0f);
		ImGui::DragFloat("Noise Speed", &proceduralFlameParameters_.noiseSpeed, 0.01f, 0.0f, 6.0f);
		ImGui::DragFloat(
			"Domain Warp / Distortion",
			&proceduralFlameParameters_.distortionStrength,
			0.002f,
			0.0f,
			0.45f);
		ImGui::DragFloat(
			"Contour Threshold",
			&proceduralFlameParameters_.contourThreshold,
			0.01f,
			0.1f,
			4.0f);
		ImGui::DragFloat(
			"Contour Width",
			&proceduralFlameParameters_.contourWidth,
			0.001f,
			0.002f,
			0.35f);
		ImGui::DragFloat(
			"Contour Softness",
			&proceduralFlameParameters_.contourSoftness,
			0.001f,
			0.001f,
			0.2f);
		ImGui::DragFloat("Field Gain", &proceduralFlameParameters_.fieldGain, 0.01f, 0.05f, 4.0f);
		ImGui::ColorEdit4("Contour Tint", &proceduralFlameParameters_.color.x);
		ImGui::DragFloat(
			"Contour Emissive Intensity",
			&proceduralFlameParameters_.contourEmissiveIntensity,
			0.05f,
			0.0f,
			16.0f);
		ImGui::Text("Time: %.2f s", flameTime_);
		ImGui::TreePop();
	}
	ImGui::End();
#endif
}
