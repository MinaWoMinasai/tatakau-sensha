#include "Object3d.h"

void Object3d::Initialize()
{
	object3dCommon_ = Object3dCommon::GetInstance();
	
	// 用のTransformationMatrix用のリソースを作る。Matrix4x41つ分のサイズを用意する
	transformationMatrixResource = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(TransformationMatrix));
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
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->direction = Normalize(directionalLightData->direction);
	directionalLightData->intensity = 1.0f;

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource_ = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	materialData_ = nullptr;
	// 書き込むためのアドレスを取得
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	// 赤を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// ライティングを有効にする
	materialData_->enableLighting = false;
	materialData_->lightingMode = false;
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->shininess = 10.0f;

	transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	camera_ = object3dCommon_->GetDefaultCamera();
	debugCamera_ = object3dCommon_->GetDebugCamera();
}

void Object3d::Update() {

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 worldViewProjectionMatrix;
	if (object3dCommon_->GetIsDebugCamera()) {
		const Matrix4x4& viewProjectionMatrix = debugCamera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Multiply(worldMatrix, viewProjectionMatrix);
	}
	
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;

}

void Object3d::Draw() {

	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	
	if (model_) {
		model_->Draw();
	}
}

void Object3d::SetModel(const std::string& filePath)
{
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}
