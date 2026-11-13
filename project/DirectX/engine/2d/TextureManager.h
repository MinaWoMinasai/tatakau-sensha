#pragma once
#include <string>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <future>
#include <memory>

namespace myEngine {

/// @brief 画像の読み込みとSRVの割り当てを管理し、読み込み済み画像を共有する。
class TextureManager {
public:
    /// @brief テクスチャをsRGBとして扱うか、リニアなデータとして扱うかを表す。
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

    /// @brief テクスチャを読み込む。ここから読み込んだテクスチャは、文字画像でも解放しない。
    void LoadTexture(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief テクスチャを読み込み、使っている数を1つ増やす。ReleaseTextureと組にして使う。
    void AcquireTexture(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief 使っている数を1つ減らす。解放してよい画像は、0になりGPUが使い終わってからSRVとリソースを返す。
    void ReleaseTexture(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief 使われなくなったら解放してよい画像（文字画像など、ファイルから作り直せるもの）として登録する。
    void MarkTextureReleasable(const std::string& filePath);
    /// @brief 解放を待っているテクスチャのうち、GPUが使い終わったものを解放する。
    void CollectReleasedTextures();
    // CPUでのデコードはBackgroundLoaderで行い、登録とアップロードはメインスレッドで行う。
    void PreloadTexture(const std::string& filePath, TextureColorSpace colorSpace = TextureColorSpace::SRGB);
    /// @brief 先読み済みのテクスチャを、1回の呼び出しで最大maxUploads枚だけGPUへ登録する。メインスレッドで呼ぶ。
    void PumpPreloads(unsigned maxUploads = 1);
    /// @brief 要求した先読みがすべてGPUへ登録済みかを返す。
    bool ArePreloadsReady() const;
    /// @brief メモリ上の画像データをデコードし、ミップマップ付きのScratchImageを作る。どのスレッドからでも呼べる。
    static std::shared_ptr<DirectX::ScratchImage> DecodeTextureMemory(const void* data, size_t size, TextureColorSpace colorSpace);
    /// @brief デコード済みの画像を、指定したキーのテクスチャとして登録する。
    void RegisterPreparedTexture(const std::string& key, TextureColorSpace colorSpace, const DirectX::ScratchImage& image);
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

    /// @brief ファイルパスからテクスチャの番号を返す。
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
    /// @brief バックグラウンドでデコード中のテクスチャの要求内容と結果を保持する。
    struct PendingTexture {
        std::string path;
        TextureColorSpace colorSpace;
        std::shared_future<std::shared_ptr<DirectX::ScratchImage>> result;
    };

    std::unordered_map<std::string, PendingTexture> pendingTextures_;

    /// @brief 画像のGPUリソース・メタデータ・SRVハンドルをまとめる。
    struct TextureData {
        DirectX::TexMetadata metaData;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
        // AcquireTextureで使っている数。
        uint32_t refCount = 0;
        // LoadTextureで直接読み込まれた。使っている数が0でも解放しない。
        bool pinned = false;
        // 解放を待っている間、このフェンス値までGPUが進めば解放できる。0なら待っていない。
        uint64_t releaseFence = 0;
    };

    /// @brief テクスチャを読み込む。読み込み済みなら何もしない。
    void LoadTextureInternal(const std::string& filePath, TextureColorSpace colorSpace);

    /// @brief テクスチャキーを作成して返す。
    static std::string MakeTextureKey(const std::string& filePath, TextureColorSpace colorSpace);
    /// @brief 生成した画像をGPUテクスチャとして登録し、参照用の識別子を返す。
    void StoreGeneratedTexture(const std::string& filePath, TextureColorSpace colorSpace, const DirectX::ScratchImage& image,
                               bool textureCube);

    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // テクスチャデータ
    std::unordered_map<std::string, TextureData> textureDatas;
    // 使われなくなったら解放してよい画像のファイルパス。
    std::unordered_set<std::string> releasablePaths_;
    /// @brief 解放を待っているテクスチャ。
    struct PendingRelease {
        std::string key;
        std::string filePath;
    };
    std::vector<PendingRelease> pendingReleases_;

    Texture texture;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::string pbrEnvironmentSourcePath_;
    bool pbrEnvironmentBuilt_ = false;
    bool pbrEnvironmentLoadedFromSource_ = false;
};

/// @brief TextureManagerのテクスチャを1つ使っている間、その使用数を持つ。コピーすると使用数も増える。
class TextureReference {
public:
    TextureReference() = default;
    ~TextureReference()
    {
        Reset();
    }
    TextureReference(const TextureReference& other)
    {
        Reset(other.filePath_);
    }
    TextureReference& operator=(const TextureReference& other)
    {
        if (this != &other) {
            Reset(other.filePath_);
        }
        return *this;
    }

    /// @brief 使うテクスチャを替える。新しいほうを先に読み込んでから古いほうを手放す。空文字なら手放すだけ。
    void Reset(const std::string& filePath = {})
    {
        if (!filePath.empty()) {
            TextureManager::GetInstance()->AcquireTexture(filePath);
        }
        if (!filePath_.empty()) {
            TextureManager::GetInstance()->ReleaseTexture(filePath_);
        }
        filePath_ = filePath;
    }

private:
    std::string filePath_;
};

} // namespace myEngine
