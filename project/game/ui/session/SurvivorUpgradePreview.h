#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "game/ui/session/SurvivorQuadBatch.h"

namespace gameplay {
/// @brief 強化の画面（計画書 p9 の L1）の左側で、選んでいる強化が動く様子を見せる小さな場。
/// @note 本物の戦闘とは別の、見せるためだけの簡単な計算（数値は合わせていない）。絵は resources/endless_ui の光の絵を足して描く。
class SurvivorUpgradePreview {
public:
    /// @brief 場の置き場所（論理座標）。
    struct Area {
        float x = 24.0f, y = 104.0f, width = 600.0f, height = 480.0f;
    };
    /// @brief 絵を読み込んでおく。
    void Preload(SurvivorQuadBatch& batch);
    /// @brief 見せる強化を替える。key はアイコンの名前（sub_mortar など）、tier は選んだ後の段、awaken は覚醒するか。
    void Reset(const std::string& key, int tier, bool awaken);
    void Update(float dt);
    /// @brief 今の様子を batch に積む。fade は明るさ（0〜1。画面が閉じるときに消していく）。
    void Queue(SurvivorQuadBatch& batch, float fade = 1.0f);
    const Area& GetArea() const
    {
        return area_;
    }

private:
    enum Texture { Square, Triangle, Pentagon, Tank, Drone, Ring, Dash, Hexagon, Wrench, Shard, Dot, Bolt, Flame, kTextures };
    struct Enemy {
        int type = 0;
        float x = 0.0f, y = 0.0f, rotation = 0.0f, spin = 0.0f;
        float hp = 24.0f, flash = 0.0f, burn = 0.0f, touch = 0.0f;
    };
    struct Bullet {
        float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f, damage = 8.0f, size = 1.0f, life = 2.0f;
        int pierce = 0;
        bool critical = false, hostile = false, explode = false;
        myEngine::Vector4 color{};
        std::vector<int> hit;
    };
    struct Effect {
        /// @brief 0 広がる輪、1 予告の印（終わると爆発）、2 自機へ飛ぶ光、3 自機へ飛ぶスパナ、4 自機へ飛ぶ欠片、5 置いた地雷。
        int kind = 0;
        float x = 0.0f, y = 0.0f, age = 0.0f, life = 0.4f, radius = 40.0f, damage = 0.0f, push = 0.0f;
        myEngine::Vector4 color{};
    };
    struct Number {
        float x = 0.0f, y = 0.0f, age = 0.0f;
        std::string text;
        float size = 1.0f;
        myEngine::Vector4 color{};
    };

    float Random(float a, float b);
    void SpawnEnemy(bool outside);
    int Nearest(float x, float y, float maxDistance = 1.0e9f) const;
    void Damage(int index, float amount, bool critical);
    void Explode(float x, float y, float radius, float damage, float push, const myEngine::Vector4& color);
    void Shoot(float x, float y, float angle, float speed, float damage, const myEngine::Vector4& color, float size = 1.0f, int pierce = 0);
    void Add(int kind, float x, float y, float life, float radius, const myEngine::Vector4& color, float damage = 0.0f, float push = 0.0f);
    void Say(float x, float y, const std::string& text, float size, const myEngine::Vector4& color);
    bool Is(const char* key) const
    {
        return key_ == key;
    }

    void Quad(Texture texture, float x, float y, float width, float height, float rotation, const myEngine::Vector4& color);
    void Bar(float x, float y, float width, float height, const myEngine::Vector4& color);

    Area area_{};
    /// @brief Queue の間だけの積み先。
    SurvivorQuadBatch* batch_ = nullptr;
    float fade_ = 1.0f;
    std::string key_;
    int tier_ = 1;
    bool awaken_ = false;
    uint32_t seed_ = 1u;
    float time_ = 0.0f, tankX_ = 0.0f, tankY_ = 0.0f, aim_ = 0.0f, recoil_ = 0.0f;
    float shotTimer_ = 0.0f, actionTimer_ = 0.0f, sideTimer_ = 0.0f, haste_ = 0.0f, gauge_ = 0.0f, hp_ = 1.0f, flash_ = 0.0f;
    int burstLeft_ = 0, stacks_ = 0, phase_ = 0;
    std::array<std::array<float, 2>, 3> drones_{};
    std::vector<Enemy> enemies_;
    std::vector<Bullet> bullets_;
    std::vector<Effect> effects_;
    std::vector<Number> numbers_;
};
} // namespace gameplay
