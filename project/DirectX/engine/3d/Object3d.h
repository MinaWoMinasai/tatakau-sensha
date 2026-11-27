#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "ModelManager.h"
#include <array>
#include <vector>

namespace myEngine {

class SkinnedModel;

/// @brief モデルの配置・姿勢・マテリアルを持つ、ワールド上の描画オブジェクトを表す。
class Object3d {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize();

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    // Skeleton Jointなど、親側で完成したワールド行列をそのまま利用する。
    // 装備品・アクセサリ・子オブジェクトの追従用途を想定。
    void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 骨格変形を描画する。
    void DrawSkinned(SkinnedModel& model);
    /// @brief 骨格変形影を描画する。
    void DrawSkinnedShadow(SkinnedModel& model);
    /// @brief 影を描画する。
    void DrawShadow();

    /// @brief モデルを設定する。
    void SetModel(Model* model);

    /// @brief モデルを設定する。
    void SetModel(const std::string& filePath);

    /// @brief 拡大率・回転角・平行移動を返す。
    const Transform& GetTransform() const
    {
        return transform_;
    }

    /// @brief 姿勢を設定する。
    void SetTransform(const Transform& transform)
    {
        transform_ = transform;
        useQuaternionRotate_ = false;
    }

    /// @brief 拡大率を設定する。
    void SetScale(const Vector3& scale)
    {
        transform_.scale = scale;
    }

    /// @brief 回転角を設定する。
    void SetRotate(const Vector3& rotate)
    {
        transform_.rotate = rotate;
        useQuaternionRotate_ = false;
    }

    /// @brief オブジェクトの回転表現をオイラー角へ切り替える。
    void UseEulerRotate()
    {
        useQuaternionRotate_ = false;
    }

    /// @brief 平行移動を設定する。
    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
    }

    /// @brief カメラを設定する。
    void SetCamera(Camera* camera)
    {
        camera_ = camera;
    }

    /// @brief デバッグカメラを設定する。
    void SetDebugCamera(DebugCamera* debugCamera)
    {
        debugCamera_ = debugCamera;
    }

    /// @brief 色を返す。
    const Vector4& GetColor() const
    {
        return materialData_->color;
    }

    /// @brief 色を設定する。
    void SetColor(const Vector4& color)
    {
        materialData_->color = color;
        userColorOverride_ = true;
    }

    /// @brief 透明度を設定する。
    void SetAlpha(float color)
    {
        materialData_->color.w = color;
        userColorOverride_ = true;
    }

    /// @brief ライティングを設定する。
    void SetLighting(bool enable)
    {
        materialData_->enableLighting = enable;
    }

    /// @brief モデルの基本色のテクスチャの代わりに、指定したテクスチャ（DynamicTexture など）を貼る。
    /// テクスチャの寿命は呼び出し側で管理し、描画する間は有効にしておく。
    /// スキニングのモデル（DrawSkinned）には効かない。
    void SetTextureOverride(D3D12_GPU_DESCRIPTOR_HANDLE texture)
    {
        textureOverride_ = texture;
        hasTextureOverride_ = true;
    }

    /// @brief SetTextureOverride をやめ、モデルのテクスチャに戻す。
    void ClearTextureOverride()
    {
        hasTextureOverride_ = false;
    }

    /// @brief 物理ベースの面の設定（ライティング方式・金属度・粗さ・AO・自己発光・映り込み・IBL・影・ディテール法線）。
    struct SurfaceMaterial {
        int32_t lightingMode = 0;
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ambientOcclusion = 1.0f;
        Vector3 emissiveColor{0.0f, 0.0f, 0.0f};
        float emissiveIntensity = 0.0f;
        float environmentCoefficient = 0.0f;
        float iblDiffuseIntensity = 1.0f;
        float iblSpecularIntensity = 1.0f;
        float shadowReceiveStrength = 1.0f;
        float normalDetailStrength = 0.0f;
        float normalDetailScale = 1.0f;
    };

    /// @brief 物理ベースの面の設定をまとめて決める。金属度・粗さ・AO・自己発光は、モデルの値の代わりにこの値を使う。
    void SetSurfaceMaterial(const SurfaceMaterial& surface)
    {
        materialData_->lightingMode = surface.lightingMode;
        materialData_->metallic = surface.metallic;
        materialData_->roughness = surface.roughness;
        materialData_->ambientOcclusion = surface.ambientOcclusion;
        materialData_->emissiveColor = surface.emissiveColor;
        materialData_->emissiveIntensity = surface.emissiveIntensity;
        materialData_->environmentCoefficient = surface.environmentCoefficient;
        materialData_->iblDiffuseIntensity = surface.iblDiffuseIntensity;
        materialData_->iblSpecularIntensity = surface.iblSpecularIntensity;
        materialData_->shadowReceiveStrength = surface.shadowReceiveStrength;
        materialData_->normalDetailStrength = surface.normalDetailStrength;
        materialData_->normalDetailScale = surface.normalDetailScale;
        userMetallicOverride_ = true;
        userRoughnessOverride_ = true;
        userAmbientOcclusionOverride_ = true;
        userEmissiveOverride_ = true;
    }

    /// @brief ライティング有効であるか判定する。
    bool IsLightingEnabled() const
    {
        return materialData_->enableLighting != 0;
    }

    /// @brief この描画物だけの平行光の向きと強さを決める。Update で全体のライトの向きに戻るので、Update の後に呼ぶ。
    void SetDirectionalLight(const Vector3& direction, float intensity)
    {
        directionalLightData->direction = Normalize(direction);
        directionalLightData->intensity = intensity;
    }

    /// @brief Shininessを設定する。
    void SetShininess(float shininess)
    {
        materialData_->shininess = shininess;
    }

    /// @brief 局所光を、どこにも届かない遠くへ置いて消す。
    void ClearPointLight()
    {
        pointLightData->position = {-10000.0f, -10000.0f, -10000.0f};
    }

    /// @brief この描画物だけに適用する局所光。ゲーム全体のライトは変更しない。
    void SetPointLight(const Vector3& position, const Vector3& color, float intensity, float radius, float decay)
    {
        pointLightData->position = position;
        pointLightData->color = {color.x, color.y, color.z, 1.0f};
        pointLightData->intensity = intensity;
        pointLightData->radius = radius;
        pointLightData->decay = decay;
    }

    /// @brief 材質の定数バッファの GPU アドレスを返す（自前で描くとき、ルートの CBV に渡す）。
    D3D12_GPU_VIRTUAL_ADDRESS GetMaterialAddress() const
    {
        return materialResource_->GetGPUVirtualAddress();
    }

    /// @brief 行列の定数バッファの GPU アドレスを返す（自前で描くとき、ルートの CBV に渡す）。
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformationAddress() const
    {
        return transformationMatrixResource->GetGPUVirtualAddress();
    }

    /// @brief モデルのメッシュだけを描く（ルートと PSO は呼び出し側で決める）。モデルが無ければ何もしない。
    void DrawMeshOnly()
    {
        if (model_) {
            model_->DrawOnlyMesh();
        }
    }

    /// @brief 環境マップを設定する。
    void SetEnvironmentMap(uint32_t srvIndex)
    {
        environmentMapIndex_ = srvIndex;
    }

    /// @brief 環境マップの映り込みの強さを設定する。
    void SetEnvironmentCoefficient(float coefficient)
    {
        materialData_->environmentCoefficient = coefficient;
    }

    /// @brief 環境マップの映り込みの強さを返す。
    float GetEnvironmentCoefficient() const
    {
        return materialData_->environmentCoefficient;
    }

    /// @brief 材質を Initialize の直後の既定値へ戻し、材質の上書きの印も消す。GPU の資源は作り直さない。
    /// @note 使い回す Object3d に、前の持ち主の色・光の設定を残さないために使う。モデルは次の SetModel で決め直す。
    void ResetMaterialToDefault();

private:
    /// @brief ワールド行列から、シェーダーへ渡す行列の定数を更新する。
    void UpdateMatrixConstants(const Matrix4x4& worldMatrix);
    /// @brief モデル材質データを現在の状態へ適用する。
    void ApplyModelMaterialData();
    /// @brief 材質上書き設定フラグを初期状態へ戻す。
    void ResetMaterialOverrideFlags();
    /// @brief 材質インスタンスリソースを必要な状態を用意する。
    void EnsureMaterialInstanceResources(size_t materialCount);
    /// @brief 材質インスタンスデータを更新する。
    void UpdateMaterialInstanceData();
    /// @brief 材質インスタンスデータを更新する。
    void UpdateMaterialInstanceData(const ModelData& modelData);
    /// @brief 材質用のモデル材質を組み立てる。
    Material BuildMaterialForModelMaterial(const MaterialData& materialData) const;

    Object3dCommon* object3dCommon_;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;

    TransformationMatrixWithShadow* transformationMatrixData;
    DirectionalLight* directionalLightData;
    PointLightData* pointLightData;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> oceanWakeResource_;
    OceanWakeData* oceanWakeData_ = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> materialInstanceResources_;
    std::vector<Material*> materialInstanceData_;

    Texture texture;
    Resource resource;

    Transform transform_;
    Quaternion quaternionRotate_ = {0.0f, 0.0f, 0.0f, 1.0f};
    bool useQuaternionRotate_ = false;

    Model* model_ = nullptr;

    Camera* camera_ = nullptr;
    DebugCamera* debugCamera_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraData* cameraData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowDataResource;
    ShadowData* shadowData;

    Matrix4x4 lightViewProjection_;

    uint32_t environmentMapIndex_ = 0; // デフォルトのSRVインデックス
    uint32_t causticsTextureIndex_ = 0;
    // SetTextureOverride で指定した、基本色のテクスチャの代わり。
    D3D12_GPU_DESCRIPTOR_HANDLE textureOverride_{};
    bool hasTextureOverride_ = false;
    bool hasCausticsTexture_ = false;
    bool causticsRequestedEnabled_ = false;

    bool userColorOverride_ = false;
    bool userMetallicOverride_ = false;
    bool userRoughnessOverride_ = false;
    bool userAmbientOcclusionOverride_ = false;
    bool userEmissiveOverride_ = false;
};

} // namespace myEngine
