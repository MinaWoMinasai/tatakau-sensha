#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayHudの処理を担当し、同じプレイの共有状態を借用する。
class GameplayHud {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayHud(GameWorld& world) : world_(world) {}
    /// @brief 描画
    void DrawSprite();

    /// @brief 追従HPBarsを初期化する。
    void InitializeFollowHpBars(size_t count);

    /// @brief 追従HPバー一括処理を初期化する。
    void InitializeFollowHpBarBatch();

    /// @brief 追従HPバーを描画する。
    void DrawFollowHpBar(const void* ownerKey, const cg2::Vector3& worldPos, int hp, int maxHp, float width, float yOffset);

    /// @brief 追従スタミナバーを描画する。
    void DrawFollowStaminaBar(const cg2::Vector3& worldPos, float stamina, float maxStamina, float width, float yOffset);

    /// @brief HPバー四角形を後で処理するために予約する。
    void QueueHpBarQuad(std::vector<cg2::VertexData>& vertices, const cg2::Vector2& center, const cg2::Vector2& size);

    /// @brief HPバー一括処理を描画する。
    void DrawHpBarBatch(uint32_t startVertex, uint32_t vertexCount, const std::string& textureFilePath,
                        const HpBarMaterialBuffer& material);

    /// @brief HPバーBatchesを描画する。
    void DrawHpBarBatches();

    /// @brief ワールド座標をカメラの画面座標へ変換する。
    cg2::Vector2 WorldToScreen(const cg2::Vector3& worldPos) const;

    /// @brief ゲーム文字外観を現在の状態へ適用する。
    void ApplyGameTextAppearance();

    /// @brief ゲーム文字ブルームを描画する。
    void DrawGameTextBloom();

private:
    GameWorld& world_;
};
} // namespace gameplay
