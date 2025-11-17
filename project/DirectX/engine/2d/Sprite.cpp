#include "Sprite.h"

void Sprite::Initialize(SpriteCommon* spriteCommon) {
	spriteCommon_ = spriteCommon;

	// 用の頂点リソースを作る
	vertexResource = texture.CreateBufferResource(spriteCommon_->GetDxCommon()->GetDevice(), sizeof(VertexData) * 4);

	// リソースの先端のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// 一枚目の三角形
	vertexData[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };// 左下
	vertexData[0].texcoord = { 0.0f, 1.0f };
	vertexData[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };// 左上
	vertexData[1].texcoord = { 0.0f, 0.0f };
	vertexData[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };// 右下
	vertexData[2].texcoord = { 1.0f, 1.0f };
	// 二枚目の三角形
	vertexData[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };// 右上
	vertexData[3].texcoord = { 1.0f, 0.0f };

	// Index用の頂点リソースを作る
	indexResource = texture.CreateBufferResource(spriteCommon_->GetDxCommon()->GetDevice(), sizeof(uint32_t) * 6);

	// リソースの先端のアドレスから使う
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	// 1頂点あたりのサイズ
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// インデックスリソースにデータを書きこむ
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	indexData[0] = 0; indexData[1] = 1; indexData[2] = 2;
	indexData[3] = 1; indexData[4] = 3; indexData[5] = 2;

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = texture.CreateBufferResource(spriteCommon_->GetDxCommon()->GetDevice(), sizeof(Material));
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 白を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// SptiteはLightingを使わないのでfalse
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();

	// 用のTransformationMatrix用のリソースを作る。Matrix4x41つ分のサイズを用意する
	transformationMatrixResource = texture.CreateBufferResource(spriteCommon_->GetDxCommon()->GetDevice(), sizeof(TransformationMatrix));
	// 書き込むためのアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));
	// 単位行列を書き込んでおく
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();

}

void Sprite::Update()
{
	// 用のWorldViewProjectionMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;
	// パラメータからUVTransform用の行列を作成する
	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransform.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransform.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransform.translate));
	materialData->uvTransform = uvTransformMatrix;

}

void Sprite::Draw()
{
	// スプライトの描画
	spriteCommon_->GetDxCommon()->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
	spriteCommon_->GetDxCommon()->GetList()->IASetIndexBuffer(&indexBufferView);// IBVを設定
	spriteCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	spriteCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	spriteCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(2, srvHandleGPU_);
	// 描画!(DrawCall/ドローコール) 。6個のインデックスを使用しで1つのインスタンスを描画。その他は当面0 
	spriteCommon_->GetDxCommon()->GetList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

}
