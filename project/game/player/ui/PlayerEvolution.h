#pragma once
#include "game/player/ui/PlayerUiState.h"

/// @brief PlayerEvolutionの表示と入力を担当し、必要な自機の状態を借用する。
class PlayerEvolution {
public:
    using BodyShape = Player::BodyShape;
    /// @brief 自機と画面状態を接続する。
    PlayerEvolution(Player& player, PlayerUiState& ui) : player_(player), ui_(ui) {}
    using UiProfileStats = Player::UiProfileStats;
    /// @brief 進化画面の背景と発光演出を、シーンのポストエフェクト後に描画する。
    void DrawEvolutionAfterPostEffects();

    /// @brief 図鑑を初期化する。
    void InitializeEncyclopedia();

    /// @brief 図鑑を更新する。
    /// @param uiDeltaTime UI用の経過秒。戦闘の時間倍率と分けて渡す。
    void UpdateEncyclopedia(float uiDeltaTime);

    /// @brief 図鑑を描画する。
    void DrawEncyclopedia();

    /// @brief 制作用の機体図鑑で機体プレビューと発射デモを表示する。USE_IMGUIが有効な構成で使う。
    void DrawTankCodex();

    /// @brief 進化UI外観編集画面を描画する。
    void DrawEvolutionUiStyleEditor();

    /// @brief 進化UIの処理時間と描画件数への読み取り専用参照を返す。
    const UiProfileStats& GetEvolutionUiProfileStats() const;

    /// @brief 現在の機体と進化候補を表示する固定配置のUI資源を用意し、初回更新する。
    void InitializeStaticEvolutionPrototype();

    /// @brief 固定配置の進化UIの候補・配置・表示を更新し、選択・取消・確定入力を処理する。
    void UpdateStaticEvolutionPrototype();

    /// @brief 固定配置の進化UIの回路・ノード・詳細パネル・文字を描画し、所要時間を記録する。
    void DrawStaticEvolutionPrototype();

    /// @brief JSONから進化経路のノードと接続を読み込む。使用可能なノードが残ればtrue。
    /// @note 先に既存の経路を消去する。読み込み失敗時は経路なしになる。
    bool LoadEvolutionCircuitTree(const std::string& path = "resources/configs/evolutionTree.json");

    /// @brief 進化経路を読み込み、経路図のUI資源と選択状態を準備する。読み込み失敗なら資源生成を行わない。
    void InitializeEvolutionCircuitPrototype();

    /// @brief 進化経路図の配置・履歴・詳細表示を更新し、選択・取消・確定入力を処理する。
    void UpdateEvolutionCircuitPrototype();

    /// @brief 進化経路図の接続線・機体ボタン・詳細パネル・文字を描画する。
    void DrawEvolutionCircuitPrototype();

    /// @brief 進化経路図の背景と発光演出を、シーンのポストエフェクト後に描画する。
    void DrawEvolutionCircuitAfterPostEffects();

    /// @brief 進化経路図文字テクスチャを利用前に準備する。
    void PrepareEvolutionCircuitTextTextures();

    /// @brief 新しい進化UIが有効で、経路図の読み込みが成功しノードが存在する場合trueを返す。
    bool ShouldUseEvolutionCircuitPrototype() const;

    /// @brief 進化UI外観を読み込む。
    bool LoadEvolutionUiStyle(const std::string& path = "resources/configs/evolutionUiStyle.json");

    /// @brief 進化UI外観を保存する。
    /// @return 保存先を開き、JSONの書き込みを行えばtrue。書き込み後のストリームエラーは検査しない。
    bool SaveEvolutionUiStyle(const std::string& path = "resources/configs/evolutionUiStyle.json") const;

    /// @brief 新しい進化UIが有効で、次ランクに使用可能な機体候補がある場合trueを返す。
    bool ShouldUseStaticEvolutionPrototype() const;

    /// @brief 進化画面の配置基準を仮想画面座標へ変換する。
    cg2::Vector2 EvolutionAnchorToVirtual(const cg2::Vector2& normalizedAnchor) const;

    /// @brief 進化画面の仮想座標を描画座標へ変換する。
    cg2::Vector2 EvolutionVirtualToRender(const cg2::Vector2& virtualPosition) const;

    /// @brief ウィンドウ座標を進化画面の仮想座標へ変換する。
    cg2::Vector2 EvolutionClientToVirtual(const cg2::Vector2& clientPosition) const;

    /// @brief 仮想画面全体がクライアント領域に収まる、縦横共通の描画倍率を返す。
    float GetEvolutionRenderScale() const;

    /// @brief 仮想画面をクライアント領域の中央へ配置するオフセット（ピクセル）を返す。
    cg2::Vector2 GetEvolutionRenderOffset() const;

    /// @brief 固定配置の進化UIで、ノード間の回路線の制御点とスプライトを更新する。
    void UpdateStaticEvolutionCircuit();

    /// @brief 固定配置の進化ノードの外枠を、解放・選択状態に応じて更新する。
    void UpdateStaticEvolutionNodeFrames();

    /// @brief 固定配置の進化ノードに、機体形状・砲塔から組み立てたシルエットを配置する。
    void UpdateStaticEvolutionSilhouettes();

    /// @brief 固定進化文字を更新する。
    void UpdateStaticEvolutionText();

    /// @brief 固定進化文字テクスチャを利用前に準備する。
    void PrepareStaticEvolutionTextTextures();

    /// @brief 固定配置の進化UIの座標・領域・解像度を、USE_IMGUIの開発表示へ重ねて描く。
    void DrawStaticEvolutionDebugOverlay();

    /// @brief 固定配置の進化UI用に次ランクの候補IDを集め、件数と選択添字を更新する。
    void RefreshStaticEvolutionCandidates();

    /// @brief 進化画面用の英字機体名を値で返す。未登録IDは下線を空白へ変え、大文字化する。
    std::string GetEvolutionClassName(const std::string& classId) const;

    /// @brief 機体設定から、進化画面用の短い英語の役割表示を値で返す。
    std::string GetEvolutionShortRole(const PlayerClassConfig& config) const;

    /// @brief 機体設定から、進化画面用の役割説明を値で返す。
    std::string GetEvolutionRole(const PlayerClassConfig& config) const;

    /// @brief 現在と進化先の砲身数・発射間隔倍率・拡散角の比較文を3件返す。
    std::array<std::string, 3> GetEvolutionDeltas(const PlayerClassConfig& current, const PlayerClassConfig& target) const;

    /// @brief 機体設定から、進化画面用の固有能力の説明文を値で返す。
    std::string GetEvolutionAbility(const PlayerClassConfig& config) const;

private:
    Player& player_;
    PlayerUiState& ui_;
};
