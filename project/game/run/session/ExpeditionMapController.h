#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionMapControllerの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionMapController {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExpeditionMapController(GameWorld& world) : world_(world) {}
    /// @brief 遠征マップを初期化する。
    void InitializeExpeditionMap();

    /// @brief 遠征マップを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionMap(float dt);

    /// @brief 遠征制作データを更新する。
    void UpdateExpeditionAuthoring();

    /// @brief 遠征マップUIを最新の内容へ更新する。
    void RefreshExpeditionMapUi();

    /// @brief 遠征マップUIを描画する。
    void DrawExpeditionMapUi();

    /// @brief 遠征マップノードを開始する。
    void EnterExpeditionMapNode(const std::string& id);

    /// @brief 遠征マップノードを要求を予約する。
    void RequestExpeditionMapNode(const std::string& id);

    /// @brief 停止を伴う遠征遷移を開始し、反映時点で実行するactionを予約する。既に遷移中なら何もしない。
    void BeginExpeditionPresentation(int action, const std::string& title, const std::string& detail, const cg2::Vector4& color);

    /// @brief 遠征遷移と表示を進め、反映時点で予約済みactionを一度消費する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionPresentation(float dt);

    /// @brief 遠征画面演出を描画する。
    void DrawExpeditionPresentation();

    /// @brief 遠征修理・整備を選択する。
    void SelectExpeditionService(int option);

    /// @brief 生存中の非ボス戦を完了し、地図選択へ戻す。敵・弾の実体は次の部屋開始時まで保持する。
    void CompleteExpeditionMapCombat();

    /// @brief 制作部屋の地形を読み込み、旧戦闘を消去して自機・敵・資源を配置する。
    /// @return 有効な部屋の地形読み込みと配置処理を終えた場合true。敵1体ごとの生成成功は保証しない。
    /// @note 自機の部屋状態リセットは、遠征成長が有効で生存中の場合にだけ行われる。
    bool StartAuthoredExpeditionRoom();

    /// @brief 遠征修理・整備候補を最新の内容へ更新する。
    void RefreshExpeditionServiceOffers();

    /// @brief Intro遠征修理・整備であるか判定する。
    bool IsIntroExpeditionService() const;

    /// @brief 遠征の修理や整備に必要な価格を計算する。
    int ExpeditionServicePrice(const std::string& id) const;

    /// @brief 遠征Blueprintを設定する。
    void SetExpeditionBlueprint(int index);

    /// @brief 遠征マップ検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionMapValidation(float dt);

private:
    GameWorld& world_;
};
} // namespace gameplay
