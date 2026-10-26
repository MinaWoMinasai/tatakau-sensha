#pragma once

namespace myEngine {

/// @brief ブレンドモード。
enum BlendMode {
    kNone,
    kNormal,
    kAdd,
    kShadow,

    kAdd_Bloom_Extract,
    kAdd_Bloom_BlurH,
    kAdd_Bloom_BlurV,
    kAdd_Bloom_Composite,
    kAdd_Bloom_Downsample,
    kAdd_ObjectPost_Composite,
    kAdd_ObjectPost_OutlineAdd,
    kAdd_ObjectPost_BloomAdd,
    kRandom,
    kAdd_SSAO_Resolve,
    kAdd_SSAO_Denoise,
    kAdd_SSR_Resolve,
    kAdd_SSR_Denoise,
    kAdd_MotionVector_Resolve,
    kAdd_Temporal_Resolve,
    kAdd_Phosphor_Persistence,
    kAdd_Crt_Present,
};

} // namespace myEngine
