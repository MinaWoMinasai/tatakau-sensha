#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <array>
#include <string>
#include "InitializationTask.h"
#include "InputDesc.h"
#include "RenderTargetFormats.h"
#include "Root.h"
#include "Struct.h"

namespace myEngine {

class DirectXCommon;

/// @brief PipelineLibrary が持つパイプラインの名前。Hdr はシーン（HDR）向け。作る順に並べている。
enum class PipelineId {
    ObjectNone,
    ObjectAlpha,
    ObjectAdd,
    ObjectNoneHdr,
    ObjectAlphaHdr,
    ObjectAddHdr,
    Particle,
    ModelParticle,
    ModelParticleHdr,
    ParticleInitialize,
    ParticleEmit,
    ParticleEmitBatch,
    ParticleUpdate,
    ParticleScan,
    ParticleCompact,
    BloomExtract,
    BloomExtractHdr,
    BloomBlurH,
    BloomBlurHHdr,
    BloomBlurV,
    BloomBlurVHdr,
    GaussianFilter,
    BloomComposite,
    ObjectPostComposite,
    ObjectPostOutlineAdd,
    ObjectPostBloomAdd,
    ObjectPostCompositeHdr,
    ObjectPostOutlineAddHdr,
    ObjectPostBloomAddHdr,
    Random,
    RandomHdr,
    SsaoResolve,
    SsaoDenoise,
    SsrResolve,
    SsrResolveHdr,
    SsrDenoise,
    SsrDenoiseHdr,
    MotionVectorResolve,
    TemporalResolve,
    PhosphorPersistence,
    CrtPresent,
    BloomDownsample,
    BloomDownsampleHdr,
    Shadow,
    Trail,
    TrailHdr,
    HudRect,
    Skybox,
    SkyboxHdr,
    Skinning,
    SkinningDoubleSided,
    SkinningHdr,
    SkinningDoubleSidedHdr,
    SkinningShadow,
    Count,
};

/// @brief 描画とコンピュートのパイプライン（PSO）を作って持つ。作成は起動中に少しずつ進められる。
class PipelineLibrary : public RenderTargetFormats {
public:
    /// @brief グラフィックスパイプラインを作るシェーダーの種類。
    enum ShaderType {
        Object,
        Particle,
        ModelParticle,
        ComputeParticle,
        PostEffect,
        Shadow,
        Trail,
        Skybox,
        Skinning,
        SkinningShadow,
    };

    /// @brief ポストエフェクト用パイプラインの種類。
    enum PostEffectType {
        Bloom_Extract,
        Bloom_Downsample,
        Bloom_BlurH,
        Bloom_BlurV,
        Gaussian_Filter,
        Bloom_Composite,
        ObjectPost_Composite,
        ObjectPost_OutlineAdd,
        ObjectPost_BloomAdd,
        Random,
        SSAO_Resolve,
        SSAO_Denoise,
        SSR_Resolve,
        SSR_Denoise,
        MotionVector_Resolve,
        Temporal_Resolve,
        Phosphor_Persistence,
        Crt_Present,
    };

    /// @brief 1パイプラインの状態・シェーダー・入力配置・ルートシグネチャをまとめる。
    struct PSO {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc_{};
        D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc_{};
        Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsState_ = nullptr;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> computeState_ = nullptr;
        InputDesc inputDesc_;
        Root root_;
        // Shaderをコンパイルする
        Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
        Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;
        Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob_;
        ShaderType shaderType_ = Object;
        std::wstring vsFilePath_;
        std::wstring psFilePath_;
        PostEffectType postEffectType_ = Bloom_Extract;
    };

    /// @brief 作成に使うデバイスとシェーダーのコンパイラを借りる。dxCommonは作成が終わるまで有効に保つ。
    void Initialize(DirectXCommon* dxCommon);
    /// @brief パイプラインの作成を1ステップ進める。すべて完了したらtrueを返す。
    bool InitializeStep();
    /// @brief 残っているパイプラインをすべて作る。
    void CreateAll();

    /// @brief すべてのパイプラインの作成が完了したかを返す。
    bool IsReady() const
    {
        return pipelinesReady_;
    }

    /// @brief 作成途中のコルーチンを捨てる。
    void Release();

    /// @brief 名前を指定してPSOを返す。
    PSO& GetPipeline(PipelineId id)
    {
        return pipelines_[static_cast<size_t>(id)];
    }

    /// @brief ブレンドモードに対応する、バックバッファ（SDR）向けのPSOを返す。
    PSO& GetPSOObject(BlendMode blendMode = kAdd);
    /// @brief ブレンドモードに対応する、シーン（HDR）向けのPSOを返す。
    PSO& GetPSOObjectForScene(BlendMode blendMode = kAdd);

    /// @brief 深度を持たないHDRキャプチャにはDSVなしの互換PSOを再利用する。
    ID3D12PipelineState* GetObjectScenePipeline(BlendMode blendMode, bool hasDepth);

    /// @brief 深度バッファなしでシーン（HDR）へ軌跡を描くパイプラインを作る。成功したらtrueを返す。
    bool InitializeTrailSceneNoDepthPipeline();

    /// @brief InitializeTrailSceneNoDepthPipelineで作ったパイプラインを返す。
    ID3D12PipelineState* GetTrailSceneNoDepthPipeline() const
    {
        return trailSceneNoDepthPipeline_.Get();
    }

private:
    /// @brief PSOの種類とブレンドモードから、グラフィックスパイプラインを作る。
    void CreateShaderCommon(PSO& pso, BlendMode blendMode = kAdd, bool doubleSided = false,
                            DXGI_FORMAT renderTargetFormat = kBackBufferRenderTargetFormat);
    /// @brief コンピュートパイプラインを作る。
    void CreateComputeShaderCommon(PSO& pso, const std::wstring& shaderPath);
    /// @brief パイプラインを順番に作成するコルーチンを作る。
    InitializationTask BuildPipelines();
    InitializationTask pipelineTask_;
    bool pipelinesReady_ = false;

    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    std::array<PSO, static_cast<size_t>(PipelineId::Count)> pipelines_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> trailSceneNoDepthPipeline_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> noDepthObjectPipelines_;
};

} // namespace myEngine
