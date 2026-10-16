#pragma once

#include "RenderTexture.h"
#include "RtvManager.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace myEngine {

// シーンのHDRピクセルを、鮮明な元画像をぼかしたり置き換えたりせずに再利用する。
// Renderはコマンドを記録するだけ。呼び出し側は後で描画先・ビューポート・パイプラインを戻し、
// GPUがアイドルになるまで共有のSrvManagerを生かしておく。
class BloomPyramid {
public:
    static constexpr uint32_t kMaxLevels = 5;

    struct LevelDimensions {
        uint32_t width = 1, height = 1;
    };

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t sourceWidth, uint32_t sourceHeight);
    uint32_t Render(uint32_t sourceSrv, const BloomParam& param, uint32_t materialSrv = (std::numeric_limits<uint32_t>::max)());
    uint32_t Render(D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv, const BloomParam& param, D3D12_GPU_DESCRIPTOR_HANDLE materialSrv = {});

    // フレーム全体での比較用の上書き値。-1なら各カテゴリのパラメータに従う。
    static void SetModeOverride(int mode);
    // すべてのピラミッド（全体用とカテゴリ別の発光）で共有する表示比較の設定。
    // offはBloomをすべて消す。isolatedLevel >= 0なら、混ぜ合わせたアップサンプル連鎖の代わりに
    // その縮小レベルだけを出力する（0 = 1/2解像度 ... kMaxLevels-1 = 1/32）。
    static void SetPresentationView(bool off, int isolatedLevel);

    static int GetPresentationIsolatedLevel()
    {
        return presentationLevel_;
    }

    /// @brief 1段だけ表示するときの明るさ補正（全段合成でその段が受け持つ割合の逆数）。
    static float GetPresentationGainScale(float scatter);

    static uint32_t GetPresentationGeneration()
    {
        return presentationGeneration_;
    }

    static uint32_t EffectiveMode(const BloomParam& param);

    uint32_t GetExtractSrvIndex() const;

    uint32_t GetOutputSrvIndex() const
    {
        return outputSrv_;
    }

    uint32_t GetActiveLevelCount() const
    {
        return activeLevelCount_;
    }

    uint32_t GetSourceWidth() const
    {
        return sourceWidth_;
    }

    uint32_t GetSourceHeight() const
    {
        return sourceHeight_;
    }

    const std::array<LevelDimensions, kMaxLevels>& GetLevelDimensions() const
    {
        return dimensions_;
    }

private:
    struct PipelineBundle;

    // BloomPyramidCommon.hlsliと対応させる。ルート定数はパスごとにコマンドストリームへ
    // コピーされるため、後の描画で値が上書きされることはない。
    struct PassConstants {
        uint32_t sourceWidth, sourceHeight;
        float threshold, softKnee;
        float scatter, radius;
        uint32_t materialEnabled;
        float waterBloomEnabled;
        float waterDiagnosticsEnabled;
        uint32_t padding[3]{};
    };

    static_assert(sizeof(PassConstants) == 48);
    static_assert(offsetof(PassConstants, threshold) == 8);
    static_assert(offsetof(PassConstants, scatter) == 16);
    static_assert(offsetof(PassConstants, materialEnabled) == 24);
    static_assert(offsetof(PassConstants, waterDiagnosticsEnabled) == 32);
    static_assert(offsetof(PassConstants, padding) == 36);

    void CreatePipeline();
    void DrawPass(RenderTexture& output, const LevelDimensions& dimensions, ID3D12PipelineState* pipeline,
                  D3D12_GPU_DESCRIPTOR_HANDLE source, D3D12_GPU_DESCRIPTOR_HANDLE auxiliary, const PassConstants& constants);
    void Transition(RenderTexture& target, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    static int modeOverride_;
    static bool presentationOff_;
    static int presentationLevel_;
    static uint32_t presentationGeneration_;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    // 専用のヒープを使い、シーン側で既に埋まっているRTVヒープを使い切らないようにする。
    std::unique_ptr<RtvManager> rtvManager_;
    std::array<std::unique_ptr<RenderTexture>, kMaxLevels> down_;
    std::array<std::unique_ptr<RenderTexture>, kMaxLevels - 1> up_;
    std::array<LevelDimensions, kMaxLevels> dimensions_{};
    std::shared_ptr<PipelineBundle> pipelines_;
    uint32_t sourceWidth_ = 1, sourceHeight_ = 1;
    uint32_t outputSrv_ = 0, activeLevelCount_ = 0;
};

} // namespace myEngine
