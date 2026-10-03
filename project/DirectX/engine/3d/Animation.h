#pragma once

#include "Struct.h"
#include <map>
#include <string>
#include <vector>

namespace cg2 {

/// @brief 時刻と値の組を表す。アニメーション補間の入力に使う。
template <typename TValue> struct Keyframe {
    float time = 0.0f;
    TValue value{};
};

/// @brief 同じ属性のキーフレーム列を保持し、時間に対応する値の取得に使う。
template <typename TValue> struct AnimationCurve {
    std::vector<Keyframe<TValue>> keyframes;
};

/// @brief 拡大率・クォータニオン回転・平行移動で姿勢を表す。
struct QuaternionTransform {
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    Quaternion rotate = {0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 translate = {0.0f, 0.0f, 0.0f};
};

/// @brief 1ノードの移動・回転・拡大率のアニメーション曲線を保持する。
struct NodeAnimation {
    AnimationCurve<Vector3> translate;
    AnimationCurve<Quaternion> rotate;
    AnimationCurve<Vector3> scale;
};

/// @brief 再生時間とノードごとのアニメーション曲線を保持する。
struct Animation {
    std::string name;
    float duration = 0.0f;
    std::map<std::string, NodeAnimation> nodeAnimations;
};

/// @brief 値を計算して返す。
Vector3 CalculateValue(const AnimationCurve<Vector3>& curve, float time, const Vector3& fallback);
/// @brief 値を計算して返す。
Quaternion CalculateValue(const AnimationCurve<Quaternion>& curve, float time, const Quaternion& fallback);

/// @brief 再生時刻を進め、指定したノードのアニメーション姿勢を求める。
class AnimationPlayer {
public:
    /// @brief アニメーションを設定する。
    void SetAnimation(const Animation* animation, bool restart = true);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    /// @brief アニメーションの再生位置を指定した時刻へ移す。
    void Seek(float time);
    /// @brief ノードをサンプリングする。
    QuaternionTransform SampleNode(const std::string& nodeName, const QuaternionTransform& fallback = {}) const;

    /// @brief ループを設定する。
    void SetLoop(bool loop)
    {
        loop_ = loop;
    }
    /// @brief 再生中を設定する。
    void SetPlaying(bool playing)
    {
        playing_ = playing;
    }
    /// @brief Playback速度を設定する。
    void SetPlaybackSpeed(float speed)
    {
        playbackSpeed_ = speed;
    }
    float GetPlaybackSpeed() const { return playbackSpeed_; }
    /// @brief 再生中であるか判定する。
    bool IsPlaying() const
    {
        return playing_;
    }
    /// @brief Loopingであるか判定する。
    bool IsLooping() const
    {
        return loop_;
    }
    /// @brief 時間を返す。
    float GetTime() const
    {
        return time_;
    }
    /// @brief アニメーションを返す。
    const Animation* GetAnimation() const
    {
        return animation_;
    }

private:
    const Animation* animation_ = nullptr;
    float time_ = 0.0f;
    float playbackSpeed_ = 1.0f;
    bool playing_ = true;
    bool loop_ = true;
};

/// @brief Assimpのアニメーションをエンジンのキーフレーム表現へ変換する。
class AnimationLoader {
public:
    /// @brief からのファイルを読み込む。
    static Animation LoadFromFile(const std::string& filePath, uint32_t animationIndex = 0);
    /// @brief 全体からのファイルを読み込む。
    static std::vector<Animation> LoadAllFromFile(const std::string& filePath);
};

} // namespace cg2
