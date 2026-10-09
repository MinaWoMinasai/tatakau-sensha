#pragma once
#include "DirectXCommon.h"
#include "Struct.h"
#include <array>
#include <string>
#include <cstddef>

/// @brief 風車だけに適用する発光源の独立設定。HLSL b0の64byte配置と一致する。
struct WindmillNeonSettings {
    float baseBrightness = .48f, coreWidthPixels = 1.35f, coreIntensity = 3.2f, innerThreshold = .24f;
    float innerIntensity = .45f, surfaceEmission = .08f, haloWidthPixels = 4.0f, haloIntensity = .85f;
    cg2::Vector3 coreColor{1, .88f, .48f};
    uint32_t diagnostic = 0;
    cg2::Vector3 haloColor{1, .38f, .025f};
    float recognitionAccent = 0;
};
static_assert(sizeof(WindmillNeonSettings) == 64);
static_assert(offsetof(WindmillNeonSettings, coreColor) == 32);
static_assert(offsetof(WindmillNeonSettings, haloColor) == 48);
/// @brief 比較時も姿勢と時刻を変更しない描画モード。
enum class WindmillRenderMode {
    Legacy,
    LineArt,
    Hybrid
};

/// @brief カメラへ追従しないワールドXY平面の絵文字板を、HDR描画先へ一括描画する。
class WindmillEmojiRenderer {
public:
    /// @brief アトラステクスチャと深度判定付きのAlpha cutoutパイプラインを初期化する。
    void Initialize(cg2::DirectXCommon* dx, const std::string& texture);
    /// @brief GPUフェンス完了後、新しいフレームの板リストを開始する。
    void BeginFrame();
    /// @brief 次のQueue/Drawで使う風車専用モードと発光源を指定する。
    void SetAppearance(WindmillRenderMode mode, const WindmillNeonSettings& settings);
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
    std::string qualityTexture_;
    WindmillRenderMode mode_ = WindmillRenderMode::Legacy;
    WindmillNeonSettings settings_{};
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_, qualityPipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_, matrixBuffer_, materialBuffer_;
    D3D12_VERTEX_BUFFER_VIEW view_{};
    cg2::TrailVertex* data_ = nullptr;
    cg2::Matrix4x4* matrix_ = nullptr;
    WindmillNeonSettings* material_ = nullptr;
    uint32_t vertices_ = 0;
};
