#pragma once
#include "Input.h"
#include "Struct.h"
#include <array>

/// @brief 強化HUD（PlayerHud）が自機に対して行う操作だけをまとめた窓口。
/// @note HUDにPlayerの非公開メンバを直接触らせないために使う。実装はPlayerが持ち、寿命もPlayerと同じ。
class PlayerHudTarget {
public:
    /// @brief 現在のレベルを返す。
    virtual int GetLevel() const = 0;
    /// @brief レベルの上限を返す。
    virtual int GetMaxLevel() const = 0;
    /// @brief 現在の経験値を返す。
    virtual int GetExp() const = 0;
    /// @brief 次のレベルアップに必要な経験値を返す。
    virtual int GetNextLevelExpValue() const = 0;
    /// @brief 未使用の性能強化ポイント数を返す。
    virtual int GetSkillPoints() const = 0;
    /// @brief 能力ごとの強化段階を返す。
    virtual const std::array<int, 7>& GetUpgradeLevels() const = 0;
    /// @brief 1つの能力を強化できる上限段階を返す。
    virtual int GetMaxEnhancePoint() const = 0;
    /// @brief 1つの能力を強化できる上限段階を設定する。HUDの設定ファイルから読む。
    virtual void SetMaxEnhancePoint(int maxEnhancePoint) = 0;
    /// @brief 進化画面を開いているか判定する。
    virtual bool IsChangeMode() const = 0;
    /// @brief 撃破されているか判定する。
    virtual bool IsDead() const = 0;
    /// @brief 遠征の強化補正が有効か判定する。
    virtual bool IsRunModifiersEnabled() const = 0;
    /// @brief 自機が読む入力を返す。初期化前はnullptr。
    virtual myEngine::Input* GetInput() const = 0;
    /// @brief 論理座標に直したマウス位置を返す。
    virtual const myEngine::Vector2& GetMousePosition() const = 0;
    /// @brief 現在の機体の表示名を返す。
    virtual const char* GetCurrentClassName() const = 0;
    /// @brief 指定した能力を1段階強化する。強化できた場合true。
    virtual bool ApplyStatUpgrade(int index) = 0;
    /// @brief 指定した能力の強化を1段階戻す。戻せた場合true。
    virtual bool RefundStatUpgrade(int index) = 0;

protected:
    virtual ~PlayerHudTarget() = default;
};
