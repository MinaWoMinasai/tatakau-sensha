#pragma once
#include "TankGuidedCombatTutorial.h"

namespace tankexp {
/// @brief 導入手順に表示する説明文と見出しを表す。
struct TutorialCopy {
    const char* heading;
    const char* detail;
};
// One current action per panel. Later systems are explained when they are used.
/// @brief 戦闘中の操作課題に対応する説明文を返す。
inline constexpr TutorialCopy GuidedTutorialCopy(GuidedCombatTutorial::Stage stage, bool /*retry*/ = false)
{
    using S = GuidedCombatTutorial::Stage;
    switch (stage) {
    case S::Briefing:
        return {"マウスで敵を狙おう", "狙う方向を決めたら、左クリックで攻撃します。"};
    case S::ShootKill:
        return {"左クリックで敵を倒そう", "移動はWASD。敵を狙って撃ってみよう。"};
    case S::Collect:
        return {"光る通貨を回収しよう", "近づくと自動回収。工房で強化を買うときに使います。"};
    case S::Vitals:
        return {"緑はHP、黄色はスタミナ", "HPが0になると終了。ダッシュ用のスタミナは時間で回復します。"};
    case S::Dash:
        return {"右クリックで3回ダッシュしよう", "スタミナが回復したら、もう一度ダッシュしよう。"};
    case S::Upgrade:
        return {"強化を1つ選ぼう", "集めた通貨で戦車を強化します。効果はこの遠征中ずっと有効です。"};
    default:
        return {"操作練習は完了です", "自分に合う強化を選んで、遠征を進めよう。"};
    }
}
} // namespace tankexp
