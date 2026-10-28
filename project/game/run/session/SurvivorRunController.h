#pragma once
#include "game/session/GameWorld.h"
#include <sstream>

namespace gameplay {
/// @brief 育つ戦車の試作（RunMode::Survivor）の進行を担当し、同じプレイの共有状態を借用する。
/// @note 今は広いアリーナを用意するだけ。湧き方・経験値・部品は後のカードで足す。
class SurvivorRunController {
public:
    explicit SurvivorRunController(GameWorld& world) : world_(world) {}

    /// @brief 試作の見た目と自機の設定を整え、試作用の部屋を読み込み、敵のいないアリーナにする。
    void Initialize();
    /// @brief 試作の進行を1ステップ進める。
    /// @param dt 基準時間（秒）。演出の減速を含まない。
    void Update(float dt);
    /// @brief 自機が敵を倒したときに呼ばれる。
    void OnEnemyDefeated(const myEngine::Vector3& position);
    /// @brief 試作の画面表示を描く。
    void DrawUi();

private:
    /// @brief MYENGINE_SURVIVOR_AUTOTEST=1 のとき、決まった順に部屋の四隅を回って画面を保存し、結果を書いて終了する。
    void UpdateAutoTest(float dt);
    /// @brief 自機の近くの点を画面へ写し、照準と同じ方法でゲームの平面へ戻したときのずれ（ワールド単位）を返す。
    float MeasureAimRoundTripError() const;

    GameWorld& world_;
    bool autoTest_ = false;
    float autoTestTime_ = 0.0f;
    float autoTestLegTime_ = 0.0f;
    size_t autoTestWaypoint_ = 0;
    bool autoTestSettling_ = false;
    bool autoTestCaptureQueued_ = false;
    std::ostringstream autoTestLog_;
};
} // namespace gameplay
