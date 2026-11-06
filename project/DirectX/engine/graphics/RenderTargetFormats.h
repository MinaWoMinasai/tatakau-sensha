#pragma once
#include <dxgi1_6.h>

namespace myEngine {

/// @brief 描画先の形式。DirectXCommonとPipelineLibraryが同じ値を使う。
struct RenderTargetFormats {
    static constexpr DXGI_FORMAT kBackBufferRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    static constexpr DXGI_FORMAT kSceneRenderTargetFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT kNormalBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static constexpr DXGI_FORMAT kMaterialBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kAmbientOcclusionBufferFormat = DXGI_FORMAT_R8_UNORM;
    static constexpr DXGI_FORMAT kMotionVectorBufferFormat = DXGI_FORMAT_R16G16_FLOAT;
};

} // namespace myEngine
