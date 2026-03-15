#include "Object3d.h"

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
	// 赤を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// ライティングを有効にする
	materialData_->enableLighting = false;
	materialData_->lightingMode = false;
	materialData_->uvTransform = MakeIdentity4x4();
	materialData_->shininess = 32.0f;
	
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

void Object3d::Update() {

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
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

	if (model_) {
		model_->Draw();
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

void Object3d::SetModel(const std::string& filePath)
{
	// モデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
}
