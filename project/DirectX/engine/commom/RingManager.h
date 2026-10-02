#pragma once
#include <vector>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "Calculation.h"
#include "Struct.h"

namespace cg2 {

/// @brief 輪状エフェクトの色・半径・寿命・動作を指定する。
struct RingEffectConfig {
    float lifeTime = 0.7f;
    float startRadius = 1.0f;
    float endRadius = 16.0f;
    float startWidth = 0.8f;
    float endWidth = 2.2f;
    Vector3 rotate = {pi * 0.5f, 0.0f, 0.0f};
    Vector4 startColor = {0.2f, 0.85f, 1.0f, 1.0f};
    Vector4 endColor = {0.2f, 0.85f, 1.0f, 0.0f};
    uint32_t divisions = 96;
};

/// @brief 発生中の輪状エフェクトを更新し、まとめて描画する。
class RingManager {
public:
    static const uint32_t kMaxVertices = 12288;

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    /// @brief 設定に従って演出を発生させる。
    void Emit(const Vector3& position, const RingEffectConfig& config = {});
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    /// @brief 全体を描画する。
    void DrawAll(const Matrix4x4& viewProjection);
    /// @brief 保持している要素を消去する。
    void Clear()
    {
        rings_.clear();
    }

private:
    /// @brief 発生済みの輪状エフェクトの位置と進行時間を保持する。
    struct ActiveRing {
        Vector3 position = {};
        RingEffectConfig config = {};
        float currentTime = 0.0f;
    };

    /// @brief 補間係数にEaseOutのイージング曲線を適用する。動きの加減速を調整するために使う。
    static float EaseOut(float t);
    /// @brief 始点と終点を係数tで線形補間した値を返す。tの範囲外の扱いは呼び出し先の実装に従う。
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
    /// @brief 点を座標変換する。
    static Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix);

    DirectXCommon* dxCommon_ = nullptr;
    std::string textureFilePath_;
    std::vector<ActiveRing> rings_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    Matrix4x4* viewProjectionData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};

} // namespace cg2
