#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "SrvManager.h"
#include "ShadowMap.h"
#include <algorithm>

namespace cg2 {

/// @brief 3Dオブジェクトの描画で共有するカメラ・パイプライン・ライトを管理する。
class Object3dCommon {
public:
    // シングルトン
    static Object3dCommon* GetInstance();

    // 初期化
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShadowMap* shadowMap);

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    /// @brief 開発表示UI有効を設定する。
    void SetDebugUiEnabled(bool enabled)
    {
        debugUiEnabled_ = enabled;
    }
    /// @brief 開発表示UI有効を返す。
    bool GetDebugUiEnabled() const
    {
        return debugUiEnabled_;
    }

    // 共通描画設定
    void PreDraw(BlendMode blendMode);

    /// @brief DirectXの共通基盤を返す。
    DirectXCommon* GetDxCommon()
    {
        return dxCommon_;
    }

    /// @brief 既定値カメラを設定する。
    void SetDefaultCamera(Camera* camera)
    {
        defaultCamera_ = camera;
    }
    /// @brief 既定値カメラを返す。
    Camera* GetDefaultCamera()
    {
        return defaultCamera_;
    }
    /// @brief 開発表示既定値カメラを設定する。
    void SetDebugDefaultCamera(DebugCamera* debugCamera)
    {
        debugDefaultCamera_ = debugCamera;
    }
    /// @brief 開発表示カメラを返す。
    DebugCamera* GetDebugCamera()
    {
        return debugDefaultCamera_;
    }

    /// @brief 状態開発表示カメラを返す。
    bool& GetIsDebugCamera()
    {
        return isDebugCamera_;
    }
    /// @brief 状態開発表示カメラを設定する。
    void SetIsDebugCamera(bool isDebugCamera)
    {
        isDebugCamera_ = isDebugCamera;
    }

    /// @brief ライト視点射影を返す。
    Matrix4x4& GetLightViewProjection()
    {
        return lightViewProjection_;
    }

    /// @brief SRV管理を返す。
    SrvManager* GetSrvManager()
    {
        return srvManager_;
    }

    /// @brief 合成方式を返す。
    BlendMode GetBlendMode()
    {
        return blendMode_;
    }

    /// @brief 影マップを返す。
    ShadowMap* GetShadowMap()
    {
        return shadowMap_;
    }

    /// @brief ライトDirを返す。
    Vector3& GetLightDir()
    {
        return lightDir_;
    }
    /// @brief 影注目点を設定する。
    void SetShadowFocus(const Vector3& focus)
    {
        shadowFocus_ = focus;
    }
    /// @brief 影範囲を設定する。
    void SetShadowRange(float range)
    {
        shadowRange_ = (std::max)(range, 1.0f);
    }
    /// @brief 影注目点を返す。
    const Vector3& GetShadowFocus() const
    {
        return shadowFocus_;
    }
    /// @brief 影範囲を返す。
    float GetShadowRange() const
    {
        return shadowRange_;
    }

private:
    DirectXCommon* dxCommon_ = nullptr;

    Camera* defaultCamera_ = nullptr;
    DebugCamera* debugDefaultCamera_ = nullptr;

    bool isDebugCamera_ = false;
    bool debugUiEnabled_ = true;
    Matrix4x4 lightViewProjection_;
    SrvManager* srvManager_ = nullptr;
    ShadowMap* shadowMap_ = nullptr;

    BlendMode blendMode_;

    Vector3 lightDir_;
    Vector3 shadowFocus_ = {0.0f, 0.0f, 0.0f};
    float shadowRange_ = 500.0f;
};

} // namespace cg2
