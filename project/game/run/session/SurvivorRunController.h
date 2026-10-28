#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief 育つ戦車の試作（RunMode::Survivor）の進行を担当し、同じプレイの共有状態を借用する。
/// @note 今は空のアリーナを用意するだけ。湧き方・経験値・部品は後のカードで足す。
class SurvivorRunController {
public:
    explicit SurvivorRunController(GameWorld& world) : world_(world) {}

    /// @brief 試作の見た目と自機の設定を整え、レベルデータが置いた敵を片付けて空のアリーナにする。
    void Initialize();
    /// @brief 試作の進行を1ステップ進める。
    /// @param dt 基準時間（秒）。演出の減速を含まない。
    void Update(float dt);
    /// @brief 自機が敵を倒したときに呼ばれる。
    void OnEnemyDefeated(const myEngine::Vector3& position);
    /// @brief 試作の画面表示を描く。
    void DrawUi();

private:
    /// @brief MYENGINE_SURVIVOR_AUTOTEST=1 のとき、決まった入力で動かして画面を保存し、結果を書いて終了する。
    void UpdateAutoTest(float dt);

    GameWorld& world_;
    bool autoTest_ = false;
    float autoTestTime_ = 0.0f;
    int autoTestStep_ = 0;
    myEngine::Vector3 autoTestStart_{};
};
} // namespace gameplay
