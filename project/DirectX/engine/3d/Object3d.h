#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "ModelManager.h"
#include <array>
#include <vector>

namespace cg2 {

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

    /// @brief 拡大率を返す。
    const Vector3& GetScale() const
    {
        return transform_.scale;
    }
    /// @brief 回転角を返す。
    const Vector3& GetRotate() const
    {
        return transform_.rotate;
    }
    /// @brief 平行移動を返す。
    const Vector3& GetTranslate() const
    {
        return transform_.translate;
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
    /// @brief クォータニオン回転を設定する。
    void SetQuaternionRotate(const Quaternion& rotate)
    {
        quaternionRotate_ = rotate;
        useQuaternionRotate_ = true;
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
    /// @brief 開発表示カメラを設定する。
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
    /// @brief ライティング方式を設定する。
    void SetLightingMode(int32_t mode)
    {
        materialData_->lightingMode = mode;
    }
    /// @brief ライティング有効であるか判定する。
    bool IsLightingEnabled() const
    {
        return materialData_->enableLighting != 0;
    }
    /// @brief 平行光のライト方向を設定する。
    void SetDirectionalLightDirection(const Vector3& direction)
    {
        directionalLightData->direction = Normalize(direction);
    }
    /// @brief Shininessを設定する。
    void SetShininess(float shininess)
    {
        materialData_->shininess = shininess;
    }
    /// @brief Insensityを設定する。
    void SetInsensity(float insensity)
    {
        directionalLightData->intensity = insensity;
    }

    /// @brief 点ライト方向を返す。
    Vector3 GetPointLightDirection()
    {
        return directionalLightData->direction;
    }

    /// @brief 点ライト位置を返す。
    Vector3 GetPointLightPosition()
    {
        return pointLightData->position;
    }

    /// @brief 点ライト方向を設定する。
    void SetPointLightDirection(const Vector3& direction)
    {
        directionalLightData->direction = Normalize(direction);
    }

    /// @brief 点ライト位置を設定する。
    void SetPointLightPosition(const Vector3& position)
    {
        pointLightData->position = position;
    }

    /// @brief 材質リソースを返す。
    ID3D12Resource* GetMaterialResource()
    {
        return materialResource_.Get();
    }
    /// @brief Transformationリソースを返す。
    ID3D12Resource* GetTransformationResource()
    {
        return transformationMatrixResource.Get();
    }
    /// @brief モデルを返す。
    Model* GetModel()
    {
        return model_;
    }

    /// @brief 環境マップを設定する。
    void SetEnvironmentMap(uint32_t srvIndex)
    {
        environmentMapIndex_ = srvIndex;
    }
    /// @brief 環境Coefficientを設定する。
    void SetEnvironmentCoefficient(float coefficient)
    {
        materialData_->environmentCoefficient = coefficient;
    }
    /// @brief 環境Coefficientを返す。
    float GetEnvironmentCoefficient() const
    {
        return materialData_->environmentCoefficient;
    }
    /// @brief 金属度を設定する。
    void SetMetallic(float metallic)
    {
        materialData_->metallic = metallic;
        userMetallicOverride_ = true;
    }
    /// @brief 粗さを設定する。
    void SetRoughness(float roughness)
    {
        materialData_->roughness = roughness;
        userRoughnessOverride_ = true;
    }
    /// @brief AmbientOcclusionを設定する。
    void SetAmbientOcclusion(float ambientOcclusion)
    {
        materialData_->ambientOcclusion = ambientOcclusion;
        userAmbientOcclusionOverride_ = true;
    }
    /// @brief 自己発光を設定する。
    void SetEmissive(const Vector3& color, float intensity)
    {
        materialData_->emissiveColor = color;
        materialData_->emissiveIntensity = intensity;
        userEmissiveOverride_ = true;
    }
    /// @brief 結晶材質を設定する。
    void SetCrystalMaterial(const CrystalMaterialSettings& settings);
    /// @brief コースティクステクスチャを設定する。
    void SetCausticsTexture(const std::string& filePath);
    /// @brief コースティクス設定を設定する。
    void SetCausticsSettings(bool enabled, float scale, float intensity, const Vector3& color);
    /// @brief コースティクスアニメーション設定を設定する。
    void SetCausticsAnimationSettings(bool enabled, float playbackTime, float loopDuration, uint32_t frameCount, uint32_t atlasColumns,
                                      uint32_t atlasRows);
    /// @brief IBL強度を設定する。
    void SetIBLIntensity(float diffuseIntensity, float specularIntensity)
    {
        materialData_->iblDiffuseIntensity = diffuseIntensity;
        materialData_->iblSpecularIntensity = specularIntensity;
    }
    /// @brief IBL最大値Mipレベルを設定する。
    void SetIBLMaxMipLevel(float maxMipLevel)
    {
        materialData_->iblMaxMipLevel = maxMipLevel;
    }
    /// @brief PBR環境方式を設定する。
    void SetPBREnvironmentMode(float mode)
    {
        materialData_->pbrEnvironmentMode = mode;
    }
    /// @brief 影Receive強度を設定する。
    void SetShadowReceiveStrength(float strength)
    {
        materialData_->shadowReceiveStrength = strength;
    }
    /// @brief 法線Detailを設定する。
    void SetNormalDetail(float strength, float scale)
    {
        materialData_->normalDetailStrength = strength;
        materialData_->normalDetailScale = scale;
    }
    /// @brief 法線マップ強度を設定する。
    void SetNormalMapStrength(float strength)
    {
        materialData_->normalMapStrength = strength;
    }
    /// @brief 金属度マップ強度を設定する。
    void SetMetallicMapStrength(float strength)
    {
        materialData_->metallicMapStrength = strength;
    }
    /// @brief 粗さマップ強度を設定する。
    void SetRoughnessMapStrength(float strength)
    {
        materialData_->roughnessMapStrength = strength;
    }
    /// @brief 金属度粗さマップ強度を設定する。
    void SetMetallicRoughnessMapStrength(float strength)
    {
        materialData_->metallicMapStrength = strength;
        materialData_->roughnessMapStrength = strength;
    }
    /// @brief Occlusionマップ強度を設定する。
    void SetOcclusionMapStrength(float strength)
    {
        materialData_->occlusionMapStrength = strength;
    }
    /// @brief 複数成分を格納した材質マップチャンネルを設定する。
    void SetPackedMaterialMapChannels(float metallicChannel, float roughnessChannel, float occlusionChannel)
    {
        materialData_->metallicMapChannel = metallicChannel;
        materialData_->roughnessMapChannel = roughnessChannel;
        materialData_->occlusionMapChannel = occlusionChannel;
    }
    /// @brief 影Filterを設定する。
    void SetShadowFilter(float depthBias, float slopeBias, float pcfRadius)
    {
        materialData_->shadowDepthBias = depthBias;
        materialData_->shadowSlopeBias = slopeBias;
        materialData_->shadowPcfRadius = pcfRadius;
    }
    /// @brief 材質開発表示方式を設定する。
    void SetMaterialDebugMode(int32_t mode)
    {
        materialData_->materialDebugMode = static_cast<float>(mode);
    }
    /// @brief 水面診断値を設定する。
    void SetWaterDiagnostics(bool enabled, bool sunPathEnabled, bool atmosphereEnabled, bool farFlattenEnabled,
                             bool proceduralCloudReflectionEnabled, int32_t debugMode, float atmosphereStrength, float farFlattenStrength)
    {
        materialData_->waterDiagnosticsEnabled = enabled ? 1.0f : 0.0f;
        materialData_->waterSunPathEnabled = sunPathEnabled ? 1.0f : 0.0f;
        materialData_->waterAtmosphereEnabled = atmosphereEnabled ? 1.0f : 0.0f;
        materialData_->waterFarFlattenEnabled = farFlattenEnabled ? 1.0f : 0.0f;
        materialData_->waterProceduralCloudReflectionEnabled = proceduralCloudReflectionEnabled ? 1.0f : 0.0f;
        materialData_->waterDebugMode = static_cast<float>(debugMode);
        materialData_->waterAtmosphereStrength = atmosphereStrength;
        materialData_->waterFarFlattenStrength = farFlattenStrength;
    }
    /// @brief 海面Wakeデータを設定する。
    void SetOceanWakeData(const std::array<Vector4, 16>& wakePoints, const std::array<Vector4, 16>& wakeDirections,
                          const Vector4& parameters);
    /// @brief キャラクターShadingを設定する。
    void SetCharacterShading(float lightWrap, float shadowSoftness, float shadowStrength, float rimStrength, float rimPower,
                             float specularStrength, float specularPower)
    {
        materialData_->characterLightWrap = lightWrap;
        materialData_->characterShadowSoftness = shadowSoftness;
        materialData_->characterShadowStrength = shadowStrength;
        materialData_->characterRimStrength = rimStrength;
        materialData_->characterRimPower = rimPower;
        materialData_->characterSpecularStrength = specularStrength;
        materialData_->characterSpecularPower = specularPower;
    }

private:
    /// @brief 行列Constantsを更新する。
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
    bool hasCausticsTexture_ = false;
    bool causticsRequestedEnabled_ = false;

    bool userColorOverride_ = false;
    bool userMetallicOverride_ = false;
    bool userRoughnessOverride_ = false;
    bool userAmbientOcclusionOverride_ = false;
    bool userEmissiveOverride_ = false;
};

} // namespace cg2
