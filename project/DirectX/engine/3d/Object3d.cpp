#include "Object3d.h"

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
	object3dCommon_ = object3dCommon;
	// モデル読み込み
	modelData_ = LoadObjFile("resources", "teapot.obj");

	// 用の頂点リソースを作る
	vertexResource = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(VertexData) * modelData_.vertices.size());

	// リソースの先端のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// 頂点データをリソースにコピー
	std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = texture.CreateBufferResource(object3dCommon_->GetDxCommon()->GetDevice(), sizeof(Material));
	// マテリアルにデータを書き込む
	materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 赤を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// ライティングを有効にする
	materialData->enableLighting = false;
	materialData->lightingMode = false;
	materialData->uvTransform = MakeIdentity4x4();
	materialData->shininess = 10.0f;

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

	// objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData_.material.textureFilePath);
	// 読み込んだテクスチャの番号を取得
	modelData_.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexbyFilePath(modelData_.material.textureFilePath);

	transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

}

void Object3d::Update(const Matrix4x4& cameraMatrix) {

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = cameraMatrix;
	Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;

}

void Object3d::Draw() {

	// スプライトの描画
	object3dCommon_->GetDxCommon()->GetList()->IASetVertexBuffers(0, 1, &vertexBufferView); //VBVを設定
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(modelData_.material.textureIndex));
	object3dCommon_->GetDxCommon()->GetList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());
	
	object3dCommon_->GetDxCommon()->GetList()->DrawInstanced(UINT(modelData_.vertices.size()), 1, 0, 0);

}

MaterialData Object3d::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{

	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ一行を格納するもの
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // 開けなかったらエラー

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	if (materialData.textureFilePath.empty()) {
		materialData.textureFilePath = "resources/white512x512.png";
	}

	return materialData;
}

ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{

	ModelData modelData;
	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標
	std::string line; // ファイルから読んだ一行を格納するもの
	std::string currentMaterialName;

	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // 開けなかったらエラー

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; // 先端の識別子を読む

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			position.x *= 1.0f;
			position.y *= 1.0f;
			position.z *= -1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			//texcoord.x = 1.0f - texcoord.x;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {

			VertexData triangle[3];
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDef;
				s >> vertexDef;
				std::istringstream v(vertexDef);

				std::string indexStr;
				uint32_t elementIndices[3] = { 0, 0, 0 };
				int i = 0;
				while (std::getline(v, indexStr, '/') && i < 3) {
					if (!indexStr.empty()) {
						elementIndices[i] = std::stoi(indexStr);
					} else {
						elementIndices[i] = 0;
					}
					++i;
				}

				Vector4 position = (elementIndices[0] > 0) ? positions[elementIndices[0] - 1] : Vector4{};
				Vector2 texcoord = (elementIndices[1] > 0) ? texcoords[elementIndices[1] - 1] : Vector2{};
				Vector3 normal = (elementIndices[2] > 0) ? normals[elementIndices[2] - 1] : Vector3{};

				triangle[faceVertex] = { position, texcoord, normal };
			}
			// 頂点を逆順に登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		} else if (identifier == "mtllib") {
			// materialTemplateLiblaryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		} else if (identifier == "newmtl") {
			s >> currentMaterialName;
		}
	}
	return modelData;
}
