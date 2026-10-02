#pragma once
#include <string>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace cg2 {

/// @brief 画像の読み込みとSRVの割り当てを管理し、読み込み済み画像を共有する。
class TextureManager {

public:
    enum class TextureColorSpace {
        SRGB,
        LinearData,
    };

    /// @brief 共有インスタンスを返す。呼び出し側は取得したポインターをdeleteしない。
    static TextureManager* GetInstance();
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    /// @brief 利用終了時の資源と状態を解放する。
    void Finalize();

    /// @brief テクスチャを読み込む。
    void LoadTexture(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief Blackコースティクステクスチャを生成する。
    void CreateBlackCausticsTexture();
    /// @brief 平面法線テクスチャを生成する。
    void CreateFlatNormalTexture();
    /// @brief BrdfLutテクスチャを生成する。
    void CreateBrdfLutTexture();
    /// @brief PBRIrradianceテクスチャを生成する。
    void CreatePbrIrradianceTexture();
    /// @brief PBRPrefiltered環境テクスチャを生成する。
    void CreatePbrPrefilteredEnvironmentTexture();
    /// @brief PBR環境テクスチャからのキューブマップを生成する。
    bool CreatePbrEnvironmentTexturesFromCubeMap(const std::string& sourceCubePath);
    /// @brief PBR単色色環境テクスチャを生成する。
    bool CreatePbrSolidColorEnvironmentTextures(float red, float green, float blue);
    /// @brief 複数成分を格納したPBR材質テクスチャを生成する。
    std::string CreatePackedPbrMaterialTexture(const std::string& metallicPath, float metallicChannel, const std::string& roughnessPath,
                                               float roughnessChannel, const std::string& occlusionPath, float occlusionChannel,
                                               float metallicFallback, float roughnessFallback, float occlusionFallback);
    // GLBなどに埋め込まれたPNG/JPEGを仮想キー付きで直接読み込む。
    bool LoadTextureFromMemory(const std::string& textureKey, const void* data, size_t size,
                               TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief 後続の描画で使う描画先・パイプラインを準備する。対応するPostDrawと組にして使う。
    void PreDraw();

    /// @brief テクスチャ添字Byファイルパスを返す。
    uint32_t GetTextureIndexByFilePath(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

    /// @brief GPU側のSRVハンドルを返す。
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

    // SRVインデックスの開始番号
    static uint32_t kSRVIndexTop;

    // メタデータを取得
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);

    // SRVインデックスを取得
    uint32_t GetSrvIndex(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief Blackコースティクステクスチャパスを返す。
    static const std::string& GetBlackCausticsTexturePath();
    /// @brief 平面法線テクスチャパスを返す。
    static const std::string& GetFlatNormalTexturePath();
    /// @brief BrdfLutテクスチャパスを返す。
    static const std::string& GetBrdfLutTexturePath();
    /// @brief PBR環境テクスチャパスを返す。
    static const std::string& GetPbrEnvironmentTexturePath();
    /// @brief PBRIrradianceテクスチャパスを返す。
    static const std::string& GetPbrIrradianceTexturePath();
    /// @brief PBRPrefiltered環境テクスチャパスを返す。
    static const std::string& GetPbrPrefilteredEnvironmentTexturePath();

private:
    /// @brief 画像のGPUリソース・メタデータ・SRVハンドルをまとめる。
    struct TextureData {
        DirectX::TexMetadata metaData;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
    };

    /// @brief テクスチャキーを作成して返す。
    static std::string MakeTextureKey(const std::string& filePath, TextureColorSpace colorSpace);
    /// @brief 生成した画像をGPUテクスチャとして登録し、参照用の識別子を返す。
    void StoreGeneratedTexture(const std::string& filePath, TextureColorSpace colorSpace, const DirectX::ScratchImage& image,
                               bool textureCube);

    /// @brief インスタンスの初期値と利用先を設定する。
    TextureManager() = default;
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~TextureManager() = default;
    TextureManager(TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // テクスチャデータ
    std::unordered_map<std::string, TextureData> textureDatas;

    Texture texture;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::string pbrEnvironmentSourcePath_;
    bool pbrEnvironmentBuilt_ = false;
    bool pbrEnvironmentLoadedFromSource_ = false;
};

} // namespace cg2
