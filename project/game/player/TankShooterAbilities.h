#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// 戦闘と描画資源を使わないテストで共有する、固定容量の状態と数値計算。
namespace tankshooter {
inline constexpr int kChainTargets = 2;
inline constexpr int kBurstChildren = 6;
inline constexpr float kChainRadius = 7.0f;
/// @brief 帰還開始を判定するための累積時間と、帰還開始済みかを保持する。方向や位置は保持しない。
struct ReturnFlight {
    float age = 0;
    bool returning = false;
    /// @brief 負でないdtを累積し、初めて0.60秒以上になったときに帰還開始済みへ切り替える。
    /// @param dt この時計へ加える秒数。負値は0として扱う。
    /// @return 今回の呼び出しで帰還開始済みへ切り替えた場合だけtrue。既に帰還中ならfalseでも時間は加算する。
    /// @note Bullet::Updateは時間倍率で補正した秒数を渡すため、0.60秒は常にゲーム内の経過秒数と一致するわけではない。
    /// 弾の帰還処理の開始はBullet::BeginReturn、速度の向きと位置の更新はBullet::Updateが担当する。
    bool Step(float dt)
    {
        age += (std::max)(0.0f, dt);
        if (!returning && age >= .60f) {
            returning = true;
            return true;
        }
        return false;
    }
};
/// @brief 衝突IDごとのマーキングの蓄積数と残り寿命を、最大128対象分保持する。対象の実体は所有しない。
struct MarkLedger {
    /// @brief 1対象の衝突ID・命中蓄積数・残り秒数を保持する。idが0なら空き枠。
    struct Mark {
        uint64_t id = 0;
        int stacks = 0;
        float remaining = 0;
    };
    std::array<Mark, 128> entries{};
    /// @brief 記録済みの対象の寿命を減らし、残りが0以下になった枠を空にする。
    /// @param dt 減らす秒数。負値は0として扱う。
    void Update(float dt)
    {
        for (auto& m : entries)
            if (m.id) {
                m.remaining -= (std::max)(0.0f, dt);
                if (m.remaining <= 0)
                    m = {};
            }
    }
    /// @brief 対象の記録を探すか空き枠を使い、寿命を4秒へ戻して命中蓄積を1増やす。
    /// @param id Collider::GetCollisionIdで取得する非0のID。0は空き枠用であり、入力検査は行わない。
    /// @param boss trueなら6回、falseなら4回の蓄積で起爆条件を満たす。
    /// @return 条件を満たして記録を消費した場合true。ダメージ適用や演出生成は呼び出し側が行う。
    /// @note falseでも蓄積数と寿命は変わり得る。新しい対象を記録する空き枠がなければ、記録を変更せずfalseを返す。
    bool Hit(uint64_t id, bool boss)
    {
        Mark* slot = nullptr;
        // 一致するIDを優先する。一致がなければ、走査中に見つけた最初の空き枠を使う。
        for (auto& m : entries) {
            if (m.id == id) {
                slot = &m;
                break;
            }
            if (!slot && !m.id)
                slot = &m;
        }
        if (!slot)
            return false;
        if (slot->id != id)
            *slot = {id, 0, 4};
        slot->remaining = 4;
        if (++slot->stacks >= (boss ? 6 : 4)) {
            // 起爆条件を満たした記録はここで消費する。次の命中は蓄積1から始まる。
            *slot = {};
            return true;
        }
        return false;
    }
};
/// @brief 基礎ダメージにscaleとpowerを掛け、四捨五入して最低1のダメージ値を返す。対象のHPは変更しない。
/// @param source 基礎ダメージ。0でも戻り値の最低値は1。
/// @param scale ダメージ倍率。この関数では範囲を制限しない。
/// @param power 能力の強さの倍率。0.1～5に制限して計算する。
/// @return 計算した整数ダメージ。負の積や0も最低1に補正する。
/// @note 呼び出し側は有限の倍率を渡し、計算結果がuint32_tで表せる範囲になるようにする。上限の補正は行わない。
inline uint32_t Damage(uint32_t source, float scale, float power = 1)
{
    return static_cast<uint32_t>((std::max)(1.0f, std::round(static_cast<float>(source) * scale * (std::clamp)(power, .1f, 5.0f))));
}
} // namespace tankshooter
