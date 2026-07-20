#include "Object3d.h"
#include "SkinCluster.h"

#include <algorithm>

void Object3d::Initialize()
{
	object3dCommon_ = Object3dCommon::GetInstance();
	
	// 用のTransformationMatrixWithShadow用のリソースを作る。Matrix4x41つ分のサイズを用意する
	transformationMatrixResource = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(TransformationMatrixWithShadow));
	// 書き込むためのアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
	// 単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();

	// 平行光源用のリソースを作る
	directionalLightResource = resource.CreatedirectionalLight(texture, object3dCommon_->GetDxCommon()->GetDevice());
	// マテリアルにデータを書き込む
	directionalLightData = nullptr;
	// 書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// デフォルト値
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = object3dCommon_->GetLightDir();
	directionalLightData->intensity = 1.0f;

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource_ = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	materialData_ = nullptr;
	// 書き込むためのアドレスを取得
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	*materialData_ = MakeDefaultMaterial();
	
	// ポイントライトリソース作成
	pointLightResource = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(PointLightData));
	pointLightResource->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData));

	// デフォルト値
	pointLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData->position = { 0.0f, 5.0f, 0.0f };
	pointLightData->intensity = 1.0f;
	pointLightData->radius = 10.0f;
	pointLightData->decay = 1.0f;
	transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	
	// カメラ用 CBV を作成
	cameraResource_ = texture.CreateBufferResource(
		object3dCommon_->GetDxCommon()->GetDevice(),
		sizeof(CameraData)
	);

	// マップ
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

	// 初期値（とりあえず原点）
	cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
	cameraData_->padding = 0.0f;

	camera_ = object3dCommon_->GetDefaultCamera();
	debugCamera_ = object3dCommon_->GetDebugCamera();

	shadowDataResource = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(ShadowData));
	shadowDataResource->Map(0, nullptr, reinterpret_cast<void**>(&shadowData));

}

void Object3d::DrawSkinned(SkinnedModel& model) {
	auto commandList = object3dCommon_->GetDxCommon()->GetList();
	auto& pso = object3dCommon_->GetDxCommon()->GetPSOSkinningForScene();
	commandList->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
	commandList->SetPipelineState(pso.graphicsState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(6, shadowDataResource->GetGPUVirtualAddress());
	object3dCommon_->GetSrvManager()->SetGraphicsRootDescriptorTable(7, object3dCommon_->GetShadowMap()->GetSrvIndex());
	object3dCommon_->GetSrvManager()->SetGraphicsRootDescriptorTable(8, environmentMapIndex_);
	commandList->SetGraphicsRootConstantBufferView(10, materialResource_->GetGPUVirtualAddress());
	UpdateMaterialInstanceData(model.GetAsset().modelData);
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS> materialCbvAddresses;
	materialCbvAddresses.reserve(materialInstanceResources_.size());
	for (const auto& materialResource : materialInstanceResources_) {
		materialCbvAddresses.push_back(materialResource ? materialResource->GetGPUVirtualAddress() : 0);
	}
	model.Draw(materialCbvAddresses);
	object3dCommon_->PreDraw(kNone);
}

void Object3d::DrawSkinnedShadow(SkinnedModel& model) {
	auto commandList = object3dCommon_->GetDxCommon()->GetList();
	auto& pso = object3dCommon_->GetDxCommon()->GetPSOSkinningShadow();
	commandList->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
	commandList->SetPipelineState(pso.graphicsState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, transformationMatrixResource->GetGPUVirtualAddress());
	model.DrawShadow();
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootSignature(
		object3dCommon_->GetDxCommon()->GetPSOObject(kShadow).root_.GetSignature().Get());
	object3dCommon_->GetDxCommon()->GetList()->SetPipelineState(
		object3dCommon_->GetDxCommon()->GetPSOObject(kShadow).graphicsState_.Get());
}

void Object3d::Update() {

	Matrix4x4 worldMatrix = useQuaternionRotate_
		? MakeAffineMatrix(transform_.scale, quaternionRotate_, transform_.translate)
		: MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	UpdateMatrixConstants(worldMatrix);
}

void Object3d::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix) {
	UpdateMatrixConstants(worldMatrix);
}

void Object3d::UpdateMatrixConstants(const Matrix4x4& worldMatrix) {
	// ワールド行列の逆行列を計算
	Matrix4x4 inverseWorldMatrix = Inverse(worldMatrix);
	// 逆行列を転置する
	Matrix4x4 worldInverseTransposeMatrix = Transpose(inverseWorldMatrix);
	Matrix4x4 worldViewProjectionMatrix;
	if (object3dCommon_->GetIsDebugCamera()) {
		const Matrix4x4& viewProjectionMatrix = debugCamera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
		cameraData_->worldPosition = debugCamera_->GetEyePosition();
	} else {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
		cameraData_->worldPosition = camera_->GetTranslate();
	}
	
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;
	transformationMatrixData->WorldInverseTranspose = worldInverseTransposeMatrix;

	directionalLightData->direction = object3dCommon_->GetLightDir();
	lightViewProjection_ = object3dCommon_->GetLightViewProjection();
	shadowData->lightViewProjection = lightViewProjection_;
	transformationMatrixData->LightWVP = Multiply(worldMatrix, lightViewProjection_);
}

void Object3d::Draw() {

	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(5, pointLightResource->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(6, shadowDataResource->GetGPUVirtualAddress());
	object3dCommon_->GetSrvManager()->SetGraphicsRootDescriptorTable(7, object3dCommon_->GetShadowMap()->GetSrvIndex());
	object3dCommon_->GetSrvManager()->SetGraphicsRootDescriptorTable(8, environmentMapIndex_);
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(10, materialResource_->GetGPUVirtualAddress());

	if (model_) {
		UpdateMaterialInstanceData();
		std::vector<D3D12_GPU_VIRTUAL_ADDRESS> materialCbvAddresses;
		materialCbvAddresses.reserve(materialInstanceResources_.size());
		for (const auto& materialResource : materialInstanceResources_) {
			materialCbvAddresses.push_back(materialResource ? materialResource->GetGPUVirtualAddress() : 0);
		}
		model_->Draw(materialCbvAddresses);
	}
}

void Object3d::DrawShadow() {
	auto list = object3dCommon_->GetDxCommon()->GetList();

	list->SetGraphicsRootConstantBufferView(0, transformationMatrixResource->GetGPUVirtualAddress());

	if (model_) {
		// Model側もテクスチャセットを含まない DrawMesh 等の関数を呼ぶか
		// 頂点バッファのセットと描画だけを行うようにします
		model_->DrawOnlyMesh();
	}
}

void Object3d::SetModel(Model* model)
{
	ResetMaterialOverrideFlags();
	model_ = model;
	ApplyModelMaterialData();
	if (model_) {
		EnsureMaterialInstanceResources((std::max)(size_t{ 1 }, model_->GetModelData().materials.size()));
	}
}

void Object3d::SetModel(const std::string& filePath)
{
	ResetMaterialOverrideFlags();
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	ApplyModelMaterialData();
	if (model_) {
		EnsureMaterialInstanceResources((std::max)(size_t{ 1 }, model_->GetModelData().materials.size()));
	}
}

void Object3d::ApplyModelMaterialData()
{
	if (!materialData_ || !model_) {
		return;
	}

	const ModelData& modelData = model_->GetModelData();
	const MaterialData& material = modelData.materials.empty()
		? modelData.material
		: modelData.materials.front();
	materialData_->normalMapStrength = material.hasNormalTexture ? 1.0f : 0.0f;
	materialData_->metallicMapStrength = material.hasMetallicTexture ? 1.0f : 0.0f;
	materialData_->roughnessMapStrength = material.hasRoughnessTexture ? 1.0f : 0.0f;
	materialData_->occlusionMapStrength = material.hasOcclusionTexture ? 1.0f : 0.0f;
	materialData_->metallicMapChannel = material.metallicMapChannel;
	materialData_->roughnessMapChannel = material.roughnessMapChannel;
	materialData_->occlusionMapChannel = material.occlusionMapChannel;

	if (material.hasBaseColorFactor) {
		materialData_->color = material.baseColorFactor;
	}
	if (material.hasPbrFactors) {
		materialData_->metallic = material.metallicFactor;
		materialData_->roughness = material.roughnessFactor;
		materialData_->ambientOcclusion = material.ambientOcclusionFactor;
	}
	if (material.hasEmissive) {
		materialData_->emissiveColor = material.emissiveColor;
		materialData_->emissiveIntensity = material.emissiveIntensity;
	}
}

void Object3d::ResetMaterialOverrideFlags()
{
	userColorOverride_ = false;
	userMetallicOverride_ = false;
	userRoughnessOverride_ = false;
	userAmbientOcclusionOverride_ = false;
	userEmissiveOverride_ = false;
}

void Object3d::EnsureMaterialInstanceResources(size_t materialCount)
{
	materialCount = (std::max)(materialCount, size_t{ 1 });
	if (materialInstanceResources_.size() == materialCount &&
		materialInstanceData_.size() == materialCount) {
		return;
	}

	materialInstanceResources_.clear();
	materialInstanceData_.clear();
	materialInstanceResources_.resize(materialCount);
	materialInstanceData_.resize(materialCount, nullptr);

	for (size_t index = 0; index < materialCount; ++index) {
		materialInstanceResources_[index] = texture.CreateBufferResource(
			object3dCommon_->GetDxCommon()->GetDevice(),
			sizeof(Material));
		materialInstanceResources_[index]->Map(0, nullptr, reinterpret_cast<void**>(&materialInstanceData_[index]));
		if (materialInstanceData_[index]) {
			*materialInstanceData_[index] = materialData_ ? *materialData_ : MakeDefaultMaterial();
		}
	}
}

Material Object3d::BuildMaterialForModelMaterial(const MaterialData& materialData) const
{
	Material material = materialData_ ? *materialData_ : MakeDefaultMaterial();
	material.normalMapStrength = materialData.hasNormalTexture ? 1.0f : 0.0f;
	material.metallicMapStrength = materialData.hasMetallicTexture ? 1.0f : 0.0f;
	material.roughnessMapStrength = materialData.hasRoughnessTexture ? 1.0f : 0.0f;
	material.occlusionMapStrength = materialData.hasOcclusionTexture ? 1.0f : 0.0f;
	material.metallicMapChannel = materialData.metallicMapChannel;
	material.roughnessMapChannel = materialData.roughnessMapChannel;
	material.occlusionMapChannel = materialData.occlusionMapChannel;

	if (!userColorOverride_ && materialData.hasBaseColorFactor) {
		material.color = materialData.baseColorFactor;
	}
	if (materialData.hasPbrFactors) {
		if (!userMetallicOverride_) {
			material.metallic = materialData.metallicFactor;
		}
		if (!userRoughnessOverride_) {
			material.roughness = materialData.roughnessFactor;
		}
		if (!userAmbientOcclusionOverride_) {
			material.ambientOcclusion = materialData.ambientOcclusionFactor;
		}
	}
	if (!userEmissiveOverride_ && materialData.hasEmissive) {
		material.emissiveColor = materialData.emissiveColor;
		material.emissiveIntensity = materialData.emissiveIntensity;
	}
	return material;
}

void Object3d::UpdateMaterialInstanceData()
{
	if (!model_) {
		return;
	}

	UpdateMaterialInstanceData(model_->GetModelData());
}

void Object3d::UpdateMaterialInstanceData(const ModelData& modelData)
{
	const size_t materialCount = (std::max)(size_t{ 1 }, modelData.materials.size());
	EnsureMaterialInstanceResources(materialCount);

	for (size_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
		if (!materialInstanceData_[materialIndex]) {
			continue;
		}
		const MaterialData& sourceMaterial = modelData.materials.empty()
			? modelData.material
			: modelData.materials[materialIndex];
		*materialInstanceData_[materialIndex] = BuildMaterialForModelMaterial(sourceMaterial);
	}
}
