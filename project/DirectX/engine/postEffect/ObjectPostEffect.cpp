#include "ObjectPostEffect.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace myEngine {

void ObjectPostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager, float renderScale,
                                  float qualityCaptureScale)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    if (rtvManager) {
        rtvManager_ = rtvManager;
    } else {
        ownedRtvManager_ = std::make_unique<RtvManager>();
        ownedRtvManager_->Initialize(dxCommon_);
        rtvManager_ = ownedRtvManager_.get();
    }

    const std::array<float, 4> transparent = {0.0f, 0.0f, 0.0f, 0.0f};
    const float safeScale = (std::clamp)(renderScale, 0.25f, 1.0f);
    const uint32_t clientWidth = static_cast<uint32_t>(WinApp::kRenderWidth);
    const uint32_t clientHeight = static_cast<uint32_t>(WinApp::kRenderHeight);
    renderWidth_ = (std::max)(1u, static_cast<uint32_t>(static_cast<float>(clientWidth) * safeScale));
    renderHeight_ = (std::max)(1u, static_cast<uint32_t>(static_cast<float>(clientHeight) * safeScale));
    halfWidth_ = (std::max)(1u, renderWidth_ / 2);
    halfHeight_ = (std::max)(1u, renderHeight_ / 2);
    bloomWidth_ = halfWidth_;
    bloomHeight_ = halfHeight_;
    const DXGI_FORMAT hdrFormat = DirectXCommon::kSceneRenderTargetFormat;

    objectRT_ = std::make_unique<RenderTexture>();
    objectRT_->Initialize(dxCommon_, srvManager_, rtvManager_, renderWidth_, renderHeight_, transparent, false, hdrFormat);

    bloomRT_A_ = std::make_unique<RenderTexture>();
    bloomRT_A_->Initialize(dxCommon_, srvManager_, rtvManager_, bloomWidth_, bloomHeight_, transparent, false, hdrFormat);

    bloomRT_B_ = std::make_unique<RenderTexture>();
    bloomRT_B_->Initialize(dxCommon_, srvManager_, rtvManager_, bloomWidth_, bloomHeight_, transparent, false, hdrFormat);

    bloomRT_Half_ = std::make_unique<RenderTexture>();
    bloomRT_Half_->Initialize(dxCommon_, srvManager_, rtvManager_, halfWidth_, halfHeight_, transparent, false, hdrFormat);

    bloomPyramid_ = std::make_unique<BloomPyramid>();
    bloomPyramid_->Initialize(dxCommon_, srvManager_, renderWidth_, renderHeight_);
    const float qualityScale =
        std::isfinite(qualityCaptureScale) && qualityCaptureScale > 0 ? (std::clamp)(qualityCaptureScale, 0.25f, 1.0f) : safeScale;
    qualityWidth_ = (std::max)(1u, static_cast<uint32_t>(clientWidth * qualityScale));
    qualityHeight_ = (std::max)(1u, static_cast<uint32_t>(clientHeight * qualityScale));
    if (qualityWidth_ != renderWidth_ || qualityHeight_ != renderHeight_) {
        qualityObjectRT_ = std::make_unique<RenderTexture>();
        qualityObjectRT_->Initialize(dxCommon_, srvManager_, rtvManager_, qualityWidth_, qualityHeight_, transparent, false, hdrFormat);
        qualityBloomPyramid_ = std::make_unique<BloomPyramid>();
        qualityBloomPyramid_->Initialize(dxCommon_, srvManager_, qualityWidth_, qualityHeight_);
    }
    activeCaptureRT_ = objectRT_.get();
    activeWidth_ = renderWidth_;
    activeHeight_ = renderHeight_;

    cb_ = std::make_unique<BloomConstantBuffer>();
    cb_->Initialize(dxCommon_);

    postEffect_ = std::make_unique<PostEffect>();
    postEffect_->Initialize(dxCommon_, cb_.get());

    param_.threshold = 0.0f;
    param_.intensity = 1.0f;
    param_.vignetteIntensity = 0.0f;
    param_.vignetteScale = 0.0f;
    param_.distortionAmount = 0.0f;
    param_.chromAbAmount = 0.0f;
    param_.noiseIntensity = 0.0f;
    param_.scanlineIntensity = 0.0f;
    param_.scanlineFrequency = 100.0f;
    param_.curvature = 0.0f;
    param_.borderSharp = 0.0f;
    param_.glitchAmount = 0.0f;
    param_.fullScreenBoxBlurBlend = 0.0f;
    param_.dissolveThreshold = 0.0f;
    param_.outlineWidth = 0.0f;
    param_.outlineThreshold = 0.5f;
    param_.outlineColor = {1.0f, 1.0f, 1.0f};
    param_.outlineBloomIntensity = 0.0f;
    param_.outlineBloomWidth = 6.0f;
    param_.radialBlurCenter = {0.5f, 0.5f};
    param_.radialBlurWidth = 0.01f;
    param_.radialBlurIntensity = 0.0f;
    param_.dissolveEdgeColor = {1.0f, 0.4f, 0.3f};
    param_.dissolveEdgeWidth = 0.03f;
    param_.dissolveNoiseScale = 100.0f;
    param_.dissolveNoiseSpeed = 0.0f;
    param_.exposure = 1.0f;
    param_.toneMappingMode = 1.0f;
    param_.hdrWhitePoint = 11.2f;

    cb_->Update(param_);
}

void ObjectPostEffect::Update(float deltaTime)
{
    timer_ += deltaTime;
    param_.timer = timer_;
    cb_->Update(param_);
}

void ObjectPostEffect::BeginCapture()
{
    preserveFullResolutionCapture_ = false;
    restoreTargets_ = dxCommon_->GetRenderTargetState();
    restoreRtvHandle_ = dxCommon_->GetCurrentRTVHandle();
    restoreDsvHandle_ = dxCommon_->GetCurrentDSVHandle();
    restoreHasDsv_ = dxCommon_->HasCurrentDSV();

    SelectCaptureTarget();
    Transition(activeCaptureRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    dxCommon_->SetRenderTargetNoDepth(activeCaptureRT_->GetRTVHandle());
    ClearTransparent(activeCaptureRT_->GetRTVHandle());
    dxCommon_->SetViewport(activeWidth_, activeHeight_);
}

void ObjectPostEffect::BeginCaptureWithCurrentDepth(bool preserveFullResolution)
{
    preserveFullResolutionCapture_ = preserveFullResolution;
    restoreTargets_ = dxCommon_->GetRenderTargetState();
    restoreRtvHandle_ = dxCommon_->GetCurrentRTVHandle();
    restoreDsvHandle_ = dxCommon_->GetCurrentDSVHandle();
    restoreHasDsv_ = dxCommon_->HasCurrentDSV();

    SelectCaptureTarget(preserveFullResolution);
    Transition(activeCaptureRT_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    if (restoreHasDsv_ && activeWidth_ == static_cast<uint32_t>(restoreTargets_.viewport.Width) &&
        activeHeight_ == static_cast<uint32_t>(restoreTargets_.viewport.Height)) {
        dxCommon_->SetRenderTarget(activeCaptureRT_->GetRTVHandle(), restoreDsvHandle_);
    } else {
        dxCommon_->SetRenderTargetNoDepth(activeCaptureRT_->GetRTVHandle());
    }
    ClearTransparent(activeCaptureRT_->GetRTVHandle());
    dxCommon_->SetViewport(activeWidth_, activeHeight_);
}

void ObjectPostEffect::SelectCaptureTarget(bool preserveFullResolution)
{
    const bool useQualitySource = qualityObjectRT_ && (preserveFullResolution || BloomPyramid::EffectiveMode(param_) != 1);
    activeCaptureRT_ = useQualitySource ? qualityObjectRT_.get() : objectRT_.get();
    activeWidth_ = useQualitySource ? qualityWidth_ : renderWidth_;
    activeHeight_ = useQualitySource ? qualityHeight_ : renderHeight_;
}

void ObjectPostEffect::EndCapture()
{
    FinishCapture(FinishMode::CompositeAndAdd, true);
}

void ObjectPostEffect::EndCaptureToBackBuffer()
{
    FinishCapture(FinishMode::CompositeAndAdd, false);
}

void ObjectPostEffect::EndCaptureAdditiveOnly()
{
    FinishCapture(FinishMode::AdditiveOnly, true);
}

void ObjectPostEffect::EndCaptureBloomOnly()
{
    FinishCapture(FinishMode::BloomOnly, true);
}

void ObjectPostEffect::EndCaptureBloomOnlyToBackBuffer()
{
    FinishCapture(FinishMode::BloomOnly, false);
}

void ObjectPostEffect::EndCaptureBloomOnlyToCache()
{
    FinishCapture(FinishMode::BloomOnlyCache, true);
}

void ObjectPostEffect::DrawCachedBloom(const Vector2& uvOffset)
{
    const auto savedTargets = dxCommon_->GetRenderTargetState();

    struct TargetRestore {
        DirectXCommon* dx;
        DirectXCommon::RenderTargetState state;

        ~TargetRestore()
        {
            dx->RestoreRenderTargetState(state);
        }
    } targetRestore{dxCommon_, savedTargets};

    BloomParam drawParam = MakeDrawParam(true);
    if (cachedBloomSrv_ == 0) {
        return;
    }
    if (qualityObjectRT_ && ((drawParam.bloomMode == 1) != (cachedFilterParam_.bloomMode == 1))) {
        throw std::logic_error("A Bloom capture with separate quality resolution must be recaptured after a Legacy mode switch.");
    }
    // 前後の切り替えで、もう一方のモードでフィルタしたテクスチャを使い回してはならない。
    // 元のキャプチャは保持したまま、ここではBloomだけを作り直す。
    if (cachedPresentationGeneration_ != BloomPyramid::GetPresentationGeneration() || drawParam.bloomMode != cachedFilterParam_.bloomMode ||
        drawParam.threshold != cachedFilterParam_.threshold || drawParam.bloomSoftKnee != cachedFilterParam_.bloomSoftKnee ||
        drawParam.bloomScatter != cachedFilterParam_.bloomScatter || drawParam.bloomRadius != cachedFilterParam_.bloomRadius ||
        (drawParam.bloomMode == 1 && drawParam.intensity != cachedFilterParam_.intensity)) {
        const auto currentRtv = dxCommon_->GetCurrentRTVHandle();
        const auto currentDsv = dxCommon_->GetCurrentDSVHandle();
        const bool hasDsv = dxCommon_->HasCurrentDSV();
        cachedBloomSrv_ = RenderBloom(drawParam);
        cachedPresentationGeneration_ = BloomPyramid::GetPresentationGeneration();
        if (hasDsv) {
            dxCommon_->SetRenderTarget(currentRtv, currentDsv);
        } else {
            dxCommon_->SetRenderTargetNoDepth(currentRtv);
        }
        dxCommon_->SetViewport(static_cast<uint32_t>(WinApp::kRenderWidth), static_cast<uint32_t>(WinApp::kRenderHeight));
    }
    drawParam.boxBlurRadius = uvOffset.x;
    drawParam.fullScreenBoxBlurBlend = uvOffset.y;
    if (savedTargets.hasDepth) {
        dxCommon_->SetRenderTarget(savedTargets.rtvs[0], savedTargets.dsv);
    } else {
        dxCommon_->SetRenderTargetNoDepth(savedTargets.rtvs[0]);
    }
    dxCommon_->SetViewport(static_cast<uint32_t>(savedTargets.viewport.Width), static_cast<uint32_t>(savedTargets.viewport.Height));
    cb_->Update(drawParam);
    postEffect_->DrawObjectBloomAdd(srvManager_->GetGPUDescriptorHandle(cachedBloomSrv_));
}

void ObjectPostEffect::FinishCapture(FinishMode mode, bool outputToHdr)
{
    // 全画面の合成は色だけを扱う。後続の不透明描画が法線やマテリアルを書き込めるよう、
    // 合成後にMRTをすべて元に戻す。
    struct TargetRestore {
        DirectXCommon* dx;
        DirectXCommon::RenderTargetState state;

        ~TargetRestore()
        {
            dx->RestoreRenderTargetState(state);
        }
    } targetRestore{dxCommon_, restoreTargets_};

    Transition(activeCaptureRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    const BloomParam drawParam = MakeDrawParam(outputToHdr);
    cachedBloomSrv_ = RenderBloom(drawParam);

    if (restoreHasDsv_) {
        dxCommon_->SetRenderTarget(restoreRtvHandle_, restoreDsvHandle_);
    } else {
        dxCommon_->SetRenderTargetNoDepth(restoreRtvHandle_);
    }
    dxCommon_->SetViewport(static_cast<uint32_t>(restoreTargets_.viewport.Width), static_cast<uint32_t>(restoreTargets_.viewport.Height));
    if (mode == FinishMode::BloomOnlyCache) {
        return;
    }
    // Updateはこの描画用の不変なGPUスナップショット（LDR/HDR出力や実際の比較モードを含む）を
    // 用意する。公開している設定値はそのまま保つ。
    cb_->Update(drawParam);
    const auto bloomHandle = srvManager_->GetGPUDescriptorHandle(cachedBloomSrv_);
    const auto drawUnshiftedBloom = [&]() {
        // これらのフィールドを明示的なUVオフセットとして使うのはDrawCachedBloomだけ。
        // 通常の合成ではフィルタ設定は保ったまま、オフセットを0にする。
        BloomParam bloomAddParam = drawParam;
        bloomAddParam.boxBlurRadius = 0.0f;
        bloomAddParam.fullScreenBoxBlurBlend = 0.0f;
        cb_->Update(bloomAddParam);
        postEffect_->DrawObjectBloomAdd(bloomHandle, outputToHdr);
    };
    if (mode == FinishMode::BloomOnly) {
        drawUnshiftedBloom();
        return;
    }
    if (mode == FinishMode::CompositeAndAdd) {
        postEffect_->DrawObjectComposite(activeCaptureRT_->GetGPUHandle(), bloomHandle, outputToHdr);
        if (drawParam.bloomMode != 1 && drawParam.outlineWidth <= 0 && drawParam.outlineBloomIntensity <= 0) {
            // 鮮明な元画像はCompositeで既に描かれている。OutlineAddの輪郭なし時の代替処理では
            // それが二重に加算されてしまう（特にOFFでハローを消した後）。
            // 従来のLegacy経路は残したまま、新しくフィルタした発光だけを加える。
            drawUnshiftedBloom();
            return;
        }
    }
    postEffect_->DrawObjectOutlineAdd(activeCaptureRT_->GetGPUHandle(), bloomHandle, outputToHdr);
}

BloomParam ObjectPostEffect::MakeDrawParam(bool outputToHdr) const
{
    BloomParam drawParam = param_;
    drawParam.bloomMode = BloomPyramid::EffectiveMode(param_);
    drawParam.bloomGain = (std::min)(4.0f, drawParam.bloomGain * BloomPyramid::GetPresentationGainScale(drawParam.bloomScatter));
    drawParam.bloomOutputToHdr = outputToHdr ? 1u : 0u;
    if (drawParam.bloomMode == 0) {
        drawParam.outlineBloomIntensity = 0;
    }
    return drawParam;
}

uint32_t ObjectPostEffect::RenderBloom(const BloomParam& drawParam)
{
    cachedFilterParam_ = drawParam;
    cb_->Update(drawParam);
    if (drawParam.bloomMode != 1) {
        BloomPyramid& pyramid = activeCaptureRT_ == qualityObjectRT_.get() ? *qualityBloomPyramid_ : *bloomPyramid_;
        return pyramid.Render(activeCaptureRT_->GetSrvIndex(), drawParam);
    }
    if (activeCaptureRT_ != objectRT_.get() && !preserveFullResolutionCapture_) {
        throw std::logic_error("Bloom mode must remain fixed between BeginCapture and EndCapture.");
    }

    // これらの不透明な全画面フィルタはアルファも含めて全ピクセルを置き換える。
    // BeginCaptureでのキャプチャのクリアは残すが、フィルタの出力はクリアしない。
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_Half_->GetRTVHandle());
    dxCommon_->SetViewport(halfWidth_, halfHeight_);
    postEffect_->Draw(activeCaptureRT_->GetGPUHandle(), kAdd_Bloom_Extract);
    Transition(bloomRT_Half_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(bloomWidth_, bloomHeight_);
    postEffect_->Draw(bloomRT_Half_->GetGPUHandle(), kAdd_Bloom_Downsample);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_B_->GetRTVHandle());
    dxCommon_->SetViewport(bloomWidth_, bloomHeight_);
    postEffect_->Draw(bloomRT_A_->GetGPUHandle(), kAdd_Bloom_BlurH);
    Transition(bloomRT_B_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(bloomRT_A_->GetRTVHandle());
    dxCommon_->SetViewport(bloomWidth_, bloomHeight_);
    postEffect_->Draw(bloomRT_B_->GetGPUHandle(), kAdd_Bloom_BlurV);
    Transition(bloomRT_A_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return bloomRT_A_->GetSrvIndex();
}

void ObjectPostEffect::SetParam(const BloomParam& param)
{
    param_ = param;
    cb_->Update(param_);
}

void ObjectPostEffect::Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    assert(resource != nullptr);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);
}

void ObjectPostEffect::ClearTransparent(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    const float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    dxCommon_->GetList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

} // namespace myEngine
