#pragma once

#include "Skeleton.h"
#include "Struct.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

namespace cg2 {

class DirectXCommon;
class SrvManager;

/// @brief 1頂点に対する関節の重みを表す。
struct VertexWeightData {
    float weight = 0.0f;
    uint32_t vertexIndex = 0;
};

/// @brief 1関節の逆バインド行列と影響を受ける頂点の重みを保持する。
struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix{};
    std::vector<VertexWeightData> vertexWeights;
};

/// @brief スキニングモデルのCPU側データを保持する。描画インスタンスの再生状態とは分けて扱う。
struct SkinningModelAsset {
    /// @brief 同じマテリアルで描画する頂点・インデックスの範囲を表す。
    struct Submesh {
        uint32_t indexStart = 0;
        uint32_t indexCount = 0;
        uint32_t materialIndex = 0;
        std::string materialName;
        std::string textureKey;
        bool doubleSided = false;
    };

    /// @brief モデルファイル内に埋め込まれた画像データを保持する。
    struct EmbeddedTexture {
        std::string textureKey;
        std::vector<uint8_t> encodedData;
        bool linearData = false;
    };

    ModelData modelData;
    SkeletonNode rootNode;
    std::map<std::string, JointWeightData> jointWeights;
    std::vector<Submesh> submeshes;
    std::vector<EmbeddedTexture> embeddedTextures;
};

/// @brief 頂点を変形する関節番号と重みをシェーダーへ渡す。
struct VertexInfluence {
    std::array<float, 4> weights{};
    std::array<int32_t, 4> jointIndices{};
};

/// @brief 関節ごとの変形行列と法線変換行列をGPUへ渡す。
struct SkinningPaletteEntry {
    Matrix4x4 skeletonSpaceMatrix{};
    Matrix4x4 skeletonSpaceInverseTransposeMatrix{};
};

/// @brief モデル・骨格・関節の重みを読み込み、スキニング用のアセットへ変換する。
class SkinningModelLoader {
public:
    /// @brief からのファイルを読み込む。
    static SkinningModelAsset LoadFromFile(const std::string& filePath);
};

/// @brief 頂点の関節情報と変形パレットのGPUリソースを管理する。
class SkinCluster {
public:
    /// @brief 指定条件でインスタンスまたはリソースを生成する。
    static SkinCluster Create(const Skeleton& skeleton, const SkinningModelAsset& asset);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update(const Skeleton& skeleton);

    /// @brief 関節の影響を返す。
    const std::vector<VertexInfluence>& GetInfluences() const
    {
        return influences_;
    }
    /// @brief 変形パレットを返す。
    const std::vector<SkinningPaletteEntry>& GetPalette() const
    {
        return palette_;
    }
    /// @brief 逆行列結合姿勢行列を返す。
    const std::vector<Matrix4x4>& GetInverseBindPoseMatrices() const
    {
        return inverseBindPoseMatrices_;
    }
    /// @brief 割り当て済みInfluence件数を返す。
    uint32_t GetAssignedInfluenceCount() const
    {
        return assignedInfluenceCount_;
    }

private:
    std::vector<Matrix4x4> inverseBindPoseMatrices_;
    std::vector<VertexInfluence> influences_;
    std::vector<SkinningPaletteEntry> palette_;
    uint32_t assignedInfluenceCount_ = 0;
};

/// @brief 骨格とアニメーションの再生状態を持ち、スキニングモデルを描画する。
class SkinnedModel {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& filePath);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    /// @brief Blendedを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateBlended(float deltaTime, AnimationPlayer& animationA, AnimationPlayer& animationB, float blendFactor);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses = {});
    /// @brief 影を描画する。
    void DrawShadow();

    // Initialize時のDirectXCommonのCommand Listへ記録する低レベル描画API。
    // Root Signature / PSO / Material / Texture / Topology / Descriptor Heapは変更しない。
    // 呼び出し側で互換性のある描画設定を行い、モデルをGPU実行完了まで保持すること。
    // Slot 0: VertexData、Slot 1: VertexInfluence、Index: R32_UINT。
    void BindGeometry() const;
    // SrvManagerの共通Heapを事前にBindし、指定Root Parameterに単一SRVの
    // Descriptor Tableを用意すること。先頭にSkinningPaletteEntryのStructuredBufferを設定する。
    void BindSkinningPalette(uint32_t rootParameterIndex) const;
    /// @brief サブメッシュ件数を返す。
    size_t GetSubmeshCount() const
    {
        return asset_.submeshes.size();
    }
    // 範囲外のindexはstd::out_of_range。返す参照はモデルの再Initializeまで有効。
    const SkinningModelAsset::Submesh& GetSubmesh(size_t index) const
    {
        return asset_.submeshes.at(index);
    }
    // Geometry / Paletteと各パスの設定をBind済みであること。範囲外はstd::out_of_range。
    void DrawSubmesh(size_t index) const;

    /// @brief アニメーションを設定する。
    bool SetAnimation(const std::string& name, bool restart = true);
    /// @brief アニメーションを設定する。
    bool SetAnimation(size_t index, bool restart = true);
    // Initialize後に追加するCPUクリップをモデルが所有する。検証失敗時は再生・遷移状態を保持する。
    // vectorを確定してからAnimationPlayerの参照を結び直す。GPU Paletteは次のUpdateで更新する。
    bool RegisterAnimations(std::vector<Animation> animations, std::string* error = nullptr);
    /// @brief へのアニメーションを状態を遷移させる。
    bool TransitionToAnimation(const std::string& name, float duration, bool synchronizeNormalizedTime = false);
    /// @brief へのアニメーションを状態を遷移させる。
    bool TransitionToAnimation(size_t index, float duration, bool synchronizeNormalizedTime = false);

    /// @brief 骨格を返す。
    const Skeleton& GetSkeleton() const
    {
        return skeleton_;
    }
    /// @brief アニメーションループを設定する。
    void SetAnimationLoop(bool loop)
    {
        animationPlayer_.SetLoop(loop);
    }
    /// @brief アニメーションPlayback速度を設定する。
    void SetAnimationPlaybackSpeed(float speed)
    {
        animationPlayer_.SetPlaybackSpeed(speed);
    }
    /// @brief 現在アニメーションを指定した再生位置へ移す。
    // 遷移を終了し、次のUpdateで指定時刻の姿勢を直接適用する。Pause状態は維持する。
    void SeekCurrentAnimation(float time);
    // 明示的なPauseは再生時刻と進行中のポーズブレンドの両方を止める。
    void SetAnimationPlaying(bool playing);
    bool IsAnimationPaused() const { return animationPaused_; }
    /// @brief 現在アニメーション時間を返す。
    float GetCurrentAnimationTime() const
    {
        return animationPlayer_.GetTime();
    }
    /// @brief 現在アニメーション継続時間を返す。
    float GetCurrentAnimationDuration() const
    {
        return animationPlayer_.GetAnimation() ? animationPlayer_.GetAnimation()->duration : 0.0f;
    }
    /// @brief 現在アニメーション再生中であるか判定する。
    bool IsCurrentAnimationPlaying() const
    {
        return animationPlayer_.IsPlaying();
    }
    /// @brief 現在アニメーションLoopingであるか判定する。
    bool IsCurrentAnimationLooping() const
    {
        return animationPlayer_.IsLooping();
    }
    /// @brief スキン変形情報を返す。
    const SkinCluster& GetSkinCluster() const
    {
        return skinCluster_;
    }
    /// @brief アニメーション自機を返す。
    const AnimationPlayer& GetAnimationPlayer() const
    {
        return animationPlayer_;
    }
    /// @brief アニメーションを返す。
    const Animation& GetAnimation() const
    {
        return animations_[currentAnimationIndex_];
    }
    /// @brief アニメーションを返す。
    const std::vector<Animation>& GetAnimations() const
    {
        return animations_;
    }
    /// @brief 現在アニメーション添字を返す。
    size_t GetCurrentAnimationIndex() const
    {
        return currentAnimationIndex_;
    }
    /// @brief アセットを返す。
    const SkinningModelAsset& GetAsset() const
    {
        return asset_;
    }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SkinningModelAsset asset_;
    Skeleton skeleton_;
    SkinCluster skinCluster_;
    std::vector<Animation> animations_;
    size_t currentAnimationIndex_ = 0;
    AnimationPlayer animationPlayer_;
    std::vector<QuaternionTransform> animationTransitionStartPose_;
    float animationTransitionDuration_ = 0.0f;
    float animationTransitionElapsed_ = 0.0f;
    bool animationTransitionActive_ = false;
    bool animationPaused_ = false;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViews_[2]{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    SkinningPaletteEntry* mappedPalette_ = nullptr;
    uint32_t paletteSrvIndex_ = 0;
};

} // namespace cg2
