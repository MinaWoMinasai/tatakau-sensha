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
        ShaderType shaderType_;
        std::wstring vsFilePath_;
        std::wstring psFilePath_;
        PostEffectType postEffectType_;
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

    /// @brief ブレンドモードに対応する、バックバッファ（SDR）向けのPSOを返す。
    PSO& GetPSOObject(BlendMode blendMode = kAdd)
    {
        switch (blendMode) {
        case kNone:
            return objectPSO_None;
            break;
        case kNormal:
            return objectPSO_Alpha;
            break;
        case kAdd:
            return objectPSO_Add;
            break;
        case kShadow:
            return shadowPSO;
            break;
        case kAdd_Bloom_Extract:
            return bloomPSO;
            break;

        case kAdd_Bloom_Downsample:
            return downsamplePSO;
            break;

        case kAdd_Bloom_BlurH:
            return blurHPSO;
            break;

        case kAdd_Bloom_BlurV:
            return blurVPSO;
            break;

        case kAdd_Bloom_Composite:
            return compositePSO;
            break;
        case kAdd_ObjectPost_Composite:
            return objectPostCompositePSO;
            break;
        case kAdd_ObjectPost_OutlineAdd:
            return objectPostOutlineAddPSO;
            break;
        case kAdd_ObjectPost_BloomAdd:
            return objectPostBloomAddPSO;
            break;
        case kRandom:
            return randomPSO;
            break;
        case kAdd_SSAO_Resolve:
            return ssaoResolvePSO;
            break;
        case kAdd_SSAO_Denoise:
            return ssaoDenoisePSO;
            break;
        case kAdd_SSR_Resolve:
            return ssrResolvePSO;
            break;
        case kAdd_SSR_Denoise:
            return ssrDenoisePSO;
            break;
        case kAdd_MotionVector_Resolve:
            return motionVectorResolvePSO;
            break;
        case kAdd_Temporal_Resolve:
            return temporalResolvePSO;
            break;
        case kAdd_Phosphor_Persistence:
            return phosphorPersistencePSO;
            break;
        case kAdd_Crt_Present:
            return crtPresentPSO;
            break;
        default:
            return objectPSO_None;
            break;
        }
    }

    /// @brief ブレンドモードに対応する、シーン（HDR）向けのPSOを返す。
    PSO& GetPSOObjectForScene(BlendMode blendMode = kAdd)
    {
        switch (blendMode) {
        case kNone:
            return objectPSO_None_HDR;
        case kNormal:
            return objectPSO_Alpha_HDR;
        case kAdd:
            return objectPSO_Add_HDR;
        case kShadow:
            return shadowPSO;
        case kAdd_Bloom_Extract:
            return bloomPSO_HDR;
        case kAdd_Bloom_Downsample:
            return downsamplePSO_HDR;
        case kAdd_Bloom_BlurH:
            return blurHPSO_HDR;
        case kAdd_Bloom_BlurV:
            return blurVPSO_HDR;
        case kAdd_ObjectPost_Composite:
            return objectPostCompositePSO_HDR;
        case kAdd_ObjectPost_OutlineAdd:
            return objectPostOutlineAddPSO_HDR;
        case kAdd_ObjectPost_BloomAdd:
            return objectPostBloomAddPSO_HDR;
        case kRandom:
            return randomPSO_HDR;
        case kAdd_SSAO_Resolve:
            return ssaoResolvePSO;
        case kAdd_SSAO_Denoise:
            return ssaoDenoisePSO;
        case kAdd_SSR_Resolve:
            return ssrResolvePSO_HDR;
        case kAdd_SSR_Denoise:
            return ssrDenoisePSO_HDR;
        case kAdd_MotionVector_Resolve:
            return motionVectorResolvePSO;
        case kAdd_Temporal_Resolve:
            return temporalResolvePSO;
        case kAdd_Phosphor_Persistence:
            return phosphorPersistencePSO;
        default:
            return objectPSO_None_HDR;
        }
    }

    /// @brief 深度を持たないHDRキャプチャにはDSVなしの互換PSOを再利用する。
    ID3D12PipelineState* GetObjectScenePipeline(BlendMode blendMode, bool hasDepth);

    /// @brief パーティクル用のPSOを返す。
    PSO& GetPSOParticle()
    {
        return psoParticle_;
    }

    /// @brief モデルパーティクル用のPSOを返す。
    PSO& GetPSOModelParticle()
    {
        return psoModelParticle_;
    }

    /// @brief モデルパーティクルのシーン（HDR）用PSOを返す。
    PSO& GetPSOModelParticleForScene()
    {
        return psoModelParticle_HDR;
    }

    /// @brief ガウシアンフィルター用のPSOを返す。
    const PSO& GetGaussianFilterPSO() const
    {
        return gaussianFilterPSO;
    }

    /// @brief GPUパーティクルを更新するPSOを返す。
    PSO& GetPSOComputeParticle()
    {
        return psoComputeParticle_;
    }

    /// @brief GPUパーティクルの詰め先を数えるPSOを返す。
    PSO& GetPSOScanParticle()
    {
        return psoScanParticle_;
    }

    /// @brief GPUパーティクルを詰め直すPSOを返す。
    PSO& GetPSOCompactParticle()
    {
        return psoCompactParticle_;
    }

    /// @brief GPUパーティクルを初期化するPSOを返す。
    PSO& GetPSOInitializeParticle()
    {
        return psoInitializeParticle_;
    }

    /// @brief GPUパーティクルを発生させるPSOを返す。
    PSO& GetPSOEmitParticle()
    {
        return psoEmitParticle_;
    }

    /// @brief GPUパーティクルをまとめて発生させるPSOを返す。
    PSO& GetPSOEmitBatchParticle()
    {
        return psoEmitBatchParticle_;
    }

    /// @brief 軌跡用のPSOを返す。
    PSO& GetPSOTrail()
    {
        return trailPSO;
    }

    /// @brief 軌跡のシーン（HDR）用PSOを返す。
    PSO& GetPSOTrailForScene()
    {
        return trailPSO_HDR;
    }

    /// @brief 深度バッファなしでシーン（HDR）へ軌跡を描くパイプラインを作る。成功したらtrueを返す。
    bool InitializeTrailSceneNoDepthPipeline();

    /// @brief InitializeTrailSceneNoDepthPipelineで作ったパイプラインを返す。
    ID3D12PipelineState* GetTrailSceneNoDepthPipeline() const
    {
        return trailSceneNoDepthPipeline_.Get();
    }

    /// @brief HUDの矩形用のPSOを返す。
    PSO& GetPSOHudRect()
    {
        return hudRectPSO;
    }

    /// @brief スカイボックス用のPSOを返す。
    PSO& GetPSOSkybox()
    {
        return skyboxPSO_HDR;
    }

    /// @brief スキニング用のPSOを返す。
    PSO& GetPSOSkinning()
    {
        return skinningPSO;
    }

    /// @brief スキニングのシーン（HDR）用PSOを返す。
    PSO& GetPSOSkinningForScene()
    {
        return skinningPSO_HDR;
    }

    /// @brief 両面を描くスキニング用のPSOを返す。
    PSO& GetPSOSkinningDoubleSided()
    {
        return skinningDoubleSidedPSO;
    }

    /// @brief 両面を描くスキニングのシーン（HDR）用PSOを返す。
    PSO& GetPSOSkinningDoubleSidedForScene()
    {
        return skinningDoubleSidedPSO_HDR;
    }

    /// @brief スキニングの影用のPSOを返す。
    PSO& GetPSOSkinningShadow()
    {
        return skinningShadowPSO;
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

    PSO objectPSO_None;
    PSO objectPSO_Alpha;
    PSO objectPSO_Add;
    PSO objectPSO_None_HDR;
    PSO objectPSO_Alpha_HDR;
    PSO objectPSO_Add_HDR;
    PSO psoParticle_;
    PSO psoModelParticle_;
    PSO psoModelParticle_HDR;
    PSO psoComputeParticle_;
    PSO psoScanParticle_;
    PSO psoCompactParticle_;
    PSO psoInitializeParticle_;
    PSO psoEmitParticle_;
    PSO psoEmitBatchParticle_;
    PSO bloomPSO;
    PSO downsamplePSO;
    PSO blurHPSO;
    PSO blurVPSO;
    PSO bloomPSO_HDR;
    PSO downsamplePSO_HDR;
    PSO blurHPSO_HDR;
    PSO blurVPSO_HDR;
    PSO gaussianFilterPSO;
    PSO compositePSO;
    PSO objectPostCompositePSO;
    PSO objectPostOutlineAddPSO;
    PSO objectPostBloomAddPSO;
    PSO randomPSO;
    PSO ssaoResolvePSO;
    PSO ssaoDenoisePSO;
    PSO ssrResolvePSO;
    PSO ssrDenoisePSO;
    PSO motionVectorResolvePSO;
    PSO temporalResolvePSO;
    PSO phosphorPersistencePSO;
    PSO crtPresentPSO;
    PSO objectPostCompositePSO_HDR;
    PSO objectPostOutlineAddPSO_HDR;
    PSO objectPostBloomAddPSO_HDR;
    PSO randomPSO_HDR;
    PSO ssrResolvePSO_HDR;
    PSO ssrDenoisePSO_HDR;
    PSO shadowPSO;
    PSO trailPSO;
    PSO trailPSO_HDR;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> trailSceneNoDepthPipeline_;
    PSO hudRectPSO;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> noDepthObjectPipelines_;
    PSO skyboxPSO;
    PSO skyboxPSO_HDR;
    PSO skinningPSO;
    PSO skinningDoubleSidedPSO;
    PSO skinningPSO_HDR;
    PSO skinningDoubleSidedPSO_HDR;
    PSO skinningShadowPSO;
};

} // namespace myEngine
