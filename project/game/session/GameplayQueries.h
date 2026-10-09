#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayQueriesの処理を担当し、同じプレイの共有状態を借用する。
class GameplayQueries {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayQueries(GameWorld& world) : world_(world) {}
    /// @brief 終了済みであるか判定する。
    bool IsFinished() const;

    /// @brief BallOBJ形式を返す。
    cg2::Object3d* GetBallObj();

    /// @brief 最終差分時間を返す。
    float GetFinalDeltaTime() const;

    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const;

    /// @brief 後処理演出パルスを返す。
    PostEffectPulse GetPostEffectPulse() const;

    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const;

    bool IsNeonShowcaseActive() const;

    DeveloperShowcaseState GetDeveloperShowcaseState();

    void RecordDeveloperPostParameters(const cg2::BloomParam& param);

    void RecordDeveloperFrame(cg2::DirectXCommon& dx);

    /// @brief 次のシーン名前を返す。
    std::string GetNextSceneName() const;

    /// @brief 遠征以外、または遠征でライバルが有効な場合true。死亡判定は別に必要。
    bool IsRunRivalActive() const;

    /// @brief 通常チュートリアルが有効で、敵AI・特殊戦闘・通常衝突を抑制する場合true。
    bool IsTutorialCombatSuppressed() const;

    /// @brief ワールド座標を画面上の正規化座標へ変換する。
    cg2::Vector2 WorldToScreenUv(const cg2::Vector3& worldPos) const;

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json MakeDeveloperGameCaptureMetadata(cg2::DirectXCommon& dx) const;
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
