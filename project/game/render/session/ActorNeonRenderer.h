#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ActorNeonRendererの処理を担当し、同じプレイの共有状態を借用する。
class ActorNeonRenderer {
public:
    explicit ActorNeonRenderer(GameWorld& world) : world_(world) {}

    /// @brief 自機と敵のネオンのビルボードを描画待ちに積む。
    void QueueActorNeonBillboards(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp, bool drawBodies = true,
                                  bool drawBarrels = true);

    /// @brief アクターネオン機体塗りつぶし描画パスを描画する。
    /// @brief 描く本体の範囲。丸い機体（自機・ボス・ドローン）は線のブルーム後にシーンへ直接描く。
    enum class BodySet {
        All,
        Enemies,
        RoundActors
    };
    /// @brief アクター本体の塗りつぶしを描画する。
    void DrawActorNeonBodyFillPass(bool restoreOutlines = true, BodySet bodies = BodySet::All);

    /// @brief 自機を履帯戦車で描く（計画書 p10 の16章の M1）。黒い面と線を部品の奥から順に、今の描画先へ直接描く。
    /// @note 敵の線を描き終えてから呼ぶ（敵が重なっても、自機の中は黒いままにする）。
    /// @param overlay 光を重ねた後にもう一度描くとき：近いにじみを描かず、黒い面と線だけを重ねる。
    void DrawPlayerTank(const myEngine::Vector3& cameraForward, const myEngine::Matrix4x4& viewProjection, bool noDepth, bool overlay = false);

private:
    /// @brief 武器の切り替えの間なら、自機の線と塗りへディゾルブを掛け始める（EndLineDissolve で外す）。
    void BeginPlayerSwapDissolve(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp);

    GameWorld& world_;
};
} // namespace gameplay
