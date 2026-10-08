#pragma once
#include "DirectXCommon.h"
#include "Struct.h"
#include <array>
#include <string>

/// @brief カメラへ追従しないワールドXY平面の絵文字板を、HDR描画先へ一括描画する。
class WindmillEmojiRenderer {
public:
    /// @brief アトラステクスチャと深度判定付きのAlpha cutoutパイプラインを初期化する。
    void Initialize(cg2::DirectXCommon* dx, const std::string& texture);
    /// @brief GPUフェンス完了後、新しいフレームの板リストを開始する。
    void BeginFrame();
    /// @brief XY平面の板を追加する。angleはZ軸回転のラジアン、sizeはワールド単位。
    void Queue(const cg2::Vector3& center, float size, float angle, const std::array<float, 4>& uv, float visibility);
    /// @brief すべての板を一つの描画コマンドで描画する。
    void Draw(const cg2::Matrix4x4& viewProjection);
    /// @brief 今フレームに追加した板の数を返す。
    uint32_t GetQuadCount() const
    {
        return vertices_ / 6;
    }

private:
    cg2::DirectXCommon* dx_ = nullptr;
    std::string texture_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_, matrixBuffer_, materialBuffer_;
    D3D12_VERTEX_BUFFER_VIEW view_{};
    cg2::TrailVertex* data_ = nullptr;
    cg2::Matrix4x4* matrix_ = nullptr;
    uint32_t vertices_ = 0;
};
