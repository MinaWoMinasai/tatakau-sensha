#pragma once
#include "ParticleManager.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace cg2 {

class Object3d;
class Object3dCommon;
class DirectXCommon;
class Camera;
class TrailManager;
class TrailInstance;

/// @brief 演出として表示する投射物の移動と外観を指定する。
struct ProjectileProfile {
    std::string modelPath = "ball.obj";
    Vector3 scale = {0.3f, 0.3f, 0.3f};
    Vector3 rotationSpeed = {0.0f, 5.0f, 0.0f};

    /// @brief 保存する設定をJSON形式へ変換して返す。
    nlohmann::json ToJson() const;
    /// @brief JSONの値を対応する設定項目へ読み取る。省略時の扱いは各項目の既定値に従う。
    void FromJson(const nlohmann::json& j);
};

/// @brief 演出に付属する軌跡の外観と動作を指定する。
struct TrailProfile {
    Vector4 startColor = {0.5f, 0.0f, 1.0f, 1.0f};
    Vector4 endColor = {0.5f, 0.0f, 1.0f, 0.0f};
    uint32_t maxPoints = 100;
    uint32_t interpolationSteps = 6;
    Vector3 tipOffset = {0.0f, 0.3f, 0.0f};
    Vector3 baseOffset = {0.0f, -0.3f, 0.0f};
    float lifetime = 0.5f;

    /// @brief 保存する設定をJSON形式へ変換して返す。
    nlohmann::json ToJson() const;
    /// @brief JSONの値を対応する設定項目へ読み取る。省略時の扱いは各項目の既定値に従う。
    void FromJson(const nlohmann::json& j);
};

/// @brief 発射・飛行・命中を組み合わせた演出設定を表す。
struct EffectProfile {
    ProjectileProfile projectile;
    std::string flyParticle = "HitSpark";
    std::string hitParticle = "HitSpark";
    TrailProfile trail;
    float duration = 1.0f;
    float hitDuration = 0.5f;
    uint32_t flyParticleCount = 1;
    uint32_t hitParticleCount = 1;
    bool enableTrail = true;

    /// @brief 保存する設定をJSON形式へ変換して返す。
    nlohmann::json ToJson() const;
    /// @brief JSONの値を対応する設定項目へ読み取る。省略時の扱いは各項目の既定値に従う。
    void FromJson(const nlohmann::json& j);
};

/// @brief 登録した演出を時間順に発生させ、発射・飛行・命中の進行を管理する。
class EffectSequencer {
public:
    enum class State {
        Idle,
        Firing,
        Flying,
        Hit,
        Finished,
    };

    /// @brief cg2::EffectSequencerが扱う演出の発生時刻・内容を表す。
    struct Event {
        std::string effectName;
        float delay = 0.0f;
        uint32_t count = 1;
        Vector3 offset = {0.0f, 0.0f, 0.0f};
    };

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(ParticleManager* particleManager = ParticleManager::GetInstance());
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(Object3dCommon* objCommon, DirectXCommon* dx, Camera* camera, ParticleManager* particleManager,
                    TrailManager* trailManager);
    /// @brief イベントを追加する。
    void AddEvent(const Event& event);
    /// @brief イベントを消去する。
    void ClearEvents();
    /// @brief 指定した演出または音源の再生を開始する。
    void Play(const Vector3& position);
    /// @brief 命中演出を再生する。
    void PlayHitEffect(const Vector3& position);
    /// @brief 指定した攻撃または演出の発射を開始する。
    void Fire(const EffectProfile& profile, const Vector3& startPos, const Vector3& targetPos);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief ImGUIを描画する。
    void DrawImGui();
#ifdef USE_IMGUI
    /// @brief ImGUI編集画面を描画する。
    void DrawImGuiEditor(const Vector3& defaultStartPos, const Vector3& defaultTargetPos);
#endif

    /// @brief 終了済みであるか判定する。
    bool IsFinished() const
    {
        return state_ == State::Finished || state_ == State::Idle;
    }
    /// @brief 有効であるか判定する。
    bool IsActive() const
    {
        return state_ != State::Idle && state_ != State::Finished;
    }
    /// @brief 状態を返す。
    State GetState() const
    {
        return state_;
    }
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset();
    /// @brief On命中通知関数を設定する。
    void SetOnHitCallback(std::function<void()> callback)
    {
        onHitCallback_ = std::move(callback);
    }

    /// @brief 演出設定を保存する。
    static void SaveProfile(const std::string& path, const EffectProfile& profile);
    /// @brief 演出設定を読み込む。
    static bool LoadProfile(const std::string& path, EffectProfile& profile);

    /// @brief 演出設定を返す。
    const EffectProfile& GetProfile() const
    {
        return profile_;
    }
    /// @brief 演出設定を設定する。
    void SetProfile(const EffectProfile& profile)
    {
        profile_ = profile;
    }

private:
    /// @brief cg2::EffectSequencerが扱う演出の発生時刻・内容と発生後の進行状態を保持する。
    struct ActiveEvent {
        Event event;
        Vector3 origin = {0.0f, 0.0f, 0.0f};
        float timer = 0.0f;
        bool emitted = false;
    };

    /// @brief 発射中を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateFiring(float deltaTime);
    /// @brief 飛行中を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateFlying(float deltaTime);
    /// @brief 命中を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateHit(float deltaTime);
    /// @brief 2つの3Dベクトルを指定した係数で線形補間して返す。
    static Vector3 LerpVec3(const Vector3& a, const Vector3& b, float t);

    ParticleManager* particleManager_ = nullptr;
    Object3dCommon* objCommon_ = nullptr;
    DirectXCommon* dx_ = nullptr;
    Camera* camera_ = nullptr;
    TrailManager* trailManager_ = nullptr;

    std::vector<Event> events_;
    std::vector<ActiveEvent> activeEvents_;

    State state_ = State::Idle;
    EffectProfile profile_;
    EffectProfile editingProfile_;
    Vector3 startPos_ = {};
    Vector3 targetPos_ = {};
    Vector3 currentPos_ = {};
    Vector3 projectileRotation_ = {};
    float elapsedTime_ = 0.0f;

    std::unique_ptr<Object3d> projectile_;
    TrailInstance* trail_ = nullptr;
    std::function<void()> onHitCallback_;
    char profileFilename_[128] = "effect_default.json";
};

} // namespace cg2
