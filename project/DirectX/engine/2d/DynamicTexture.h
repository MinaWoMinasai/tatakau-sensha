#pragma once
#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <span>
#include <vector>

namespace myEngine {

class DirectXCommon;
class SrvManager;

/// @brief CPU で書いた RGBA8 の画像を、何度でも GPU へ送り直せるテクスチャ（地図の塗り分け、ミニマップなど）。
/// Update は送る内容を書き込み、コピーのコマンドを今のフレームのコマンドリストへ積む。
/// フレームの終わりに GPU の処理を待つエンジンの作りに合わせ、送り用のバッファは1つだけ持つ。
/// 1フレームに何度 Update しても、そのフレームでは最後に送った内容になる。
/// Update と ReadBack は、描画の途中（PreDraw〜PostDraw の間）ではなく、シーンの Update などから呼ぶ。
/// 破棄したときは、積んだコマンドが GPU で終わるまで資源を DirectXCommon に預けてから解放する。
class DynamicTexture {
public:
    DynamicTexture() = default;
    ~DynamicTexture();
    DynamicTexture(const DynamicTexture&) = delete;
    DynamicTexture& operator=(const DynamicTexture&) = delete;

    /// @brief 指定した大きさのテクスチャを作り、透明な黒で埋める。
    /// @param srgb true なら画素の値を sRGB の色として扱う（色の指定をそのまま見せたいとき）。false なら値をそのまま使う。
    /// @return 作れたらtrue。
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height, bool srgb = true);

    /// @brief 画像の内容を送る。rgba は上の行から順に、1画素4バイト（R, G, B, A）で width × height 画素分。
    /// @return 大きさが合わなければ何もせずfalse。
    bool Update(std::span<const uint8_t> rgba);

    /// @brief GPU 上の内容を読み戻す（確認・テスト用。GPU の処理を待つので遅い）。
    /// @return 読めたらtrue。rgba は width × height × 4 バイトになる。
    bool ReadBack(std::vector<uint8_t>& rgba);

    uint32_t GetWidth() const
    {
        return width_;
    }

    uint32_t GetHeight() const
    {
        return height_;
    }

    /// @brief SrvManager の中の番号（SetGraphicsRootDescriptorTable などに使う）。
    uint32_t GetSrvIndex() const
    {
        return srvIndex_;
    }

    /// @brief シェーダーから読むためのハンドル（Sprite::SetSrvHandleGPU、Object3d::SetTextureOverride に渡す）。
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const;

    ID3D12Resource* GetResource() const
    {
        return texture_.Get();
    }

private:
    /// @brief テクスチャの状態を変えるバリアを積む。
    void Transition(D3D12_RESOURCE_STATES after);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t srvIndex_ = 0;
    bool hasSrv_ = false;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer_;
    uint8_t* uploadData_ = nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT64 totalBytes_ = 0;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COPY_DEST;
};

} // namespace myEngine
