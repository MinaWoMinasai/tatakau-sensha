#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ActorNeonRendererの処理を担当し、同じプレイの共有状態を借用する。
class ActorNeonRenderer {
public:
    /// @brief 借用するワールドを設定する。
    explicit ActorNeonRenderer(GameWorld& world) : world_(world) {}
    /// @brief アクターネオンBillboardsを後で処理するために予約する。
    void QueueActorNeonBillboards(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, bool drawBodies = true,
                                  bool drawBarrels = true);

    /// @brief アクターネオン機体塗りつぶし描画パスを描画する。
    /// @brief 描く本体の範囲。丸い機体（自機・ボス・ドローン）は線のブルーム後にシーンへ直接描く。
    enum class BodySet { All, Enemies, RoundActors };
    void DrawActorNeonBodyFillPass(bool restoreOutlines = true, BodySet bodies = BodySet::All);

private:
    GameWorld& world_;
};
} // namespace gameplay
