#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayHudの処理を担当し、同じプレイの共有状態を借用する。
class GameplayHud {
public:
    explicit GameplayHud(GameWorld& world) : world_(world) {}

    /// @brief 描画
    void DrawSprite();
    /// @brief 最終ボスの登場・撃破の黒帯とタイトルを描く。
    void DrawBossCinematic();

    /// @brief 追従HPBarsを初期化する。
    void InitializeFollowHpBars(size_t count);

    /// @brief 追従HPバー一括処理を初期化する。
    void InitializeFollowHpBarBatch();

    /// @brief 追従HPバーを描画する。
    void DrawFollowHpBar(const void* ownerKey, const myEngine::Vector3& worldPos, int hp, int maxHp, float width, float yOffset);

    /// @brief 追従スタミナバーを描画する。
    void DrawFollowStaminaBar(const myEngine::Vector3& worldPos, float stamina, float maxStamina, float width, float yOffset);

    /// @brief HPバーの四角形を描画待ちに積む。
    void QueueHpBarQuad(std::vector<myEngine::VertexData>& vertices, const myEngine::Vector2& center, const myEngine::Vector2& size);

    /// @brief HPバー一括処理を描画する。
    void DrawHpBarBatch(uint32_t startVertex, uint32_t vertexCount, const std::string& textureFilePath,
                        const HpBarMaterialBuffer& material);

    /// @brief 積んでおいたHPバーをまとめて描画する。
    void DrawHpBarBatches();

    /// @brief ワールド座標をカメラの画面座標へ変換する。
    myEngine::Vector2 WorldToScreen(const myEngine::Vector3& worldPos) const;

    /// @brief ゲーム文字外観を現在の状態へ適用する。
    void ApplyGameTextAppearance();

    /// @brief ゲーム文字ブルームを描画する。
    void DrawGameTextBloom();

private:
    GameWorld& world_;
};
} // namespace gameplay
