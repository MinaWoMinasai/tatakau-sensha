#pragma once
#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "Fade.h"
#include "IScene.h"
#include "game/ui/NeonTextEffect.h"
#include "game/ui/session/NeonMenuKit.h"
#include <array>
#include <string_view>

class GameScene;

/// @brief タイトルの操作・背景デモ・ゲーム開始への遷移を管理する。
class TitleScene : public IScene {
public:
    TitleScene();
    ~TitleScene() override;
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize() override;

    /// @brief エンジンの初期化完了を受け取る。
    void OnEngineReady() override
    {
        engineReady_ = true;
    }

    /// @brief タイトル文字を出せるようになるまでは簡易描画で描画する。デモの準備は待たない。
    bool UsesSimpleRendering() const override
    {
        return !titleShown_ || finished_;
    }

    /// @brief タイトルを表示しながら背景デモを組み立てている間はtrueを返す。
    bool IsLoadingInBackground() const override
    {
        return titleShown_ && !demoReady_;
    }

    /// @brief 非同期起動の間は、シーン切り替え中もタイトルを表示し続ける。
    bool KeepTitleDuringTransition() const override
    {
        return asyncStartup_;
    }

    /// @brief シーン切り替えの前に、デモのコールバックやカメラを切り離す。
    void PrepareForTransition() override;
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update() override;
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw() override;
    /// @brief 影を描画する。
    void DrawShadow() override;
    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D() override;
    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D() override;
    /// @brief スプライトを描画する。
    void DrawSprite() override;
    /// @brief 開発者用にフレームを記録する。
    void RecordDeveloperFrame(myEngine::DirectXCommon&) override;
    /// @brief タイトルとデモが使うファイルの読み込みを裏で開始する。何度呼んでもよい。
    static void StartAssetPreloads();

    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const override
    {
        // 遠征の見本は後ろをぼかす。無限の場の見本は、本番のまま見せる。
        return expeditionDemo_ ? 0.88f : 0.0f;
    }

    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const override;
    /// @brief ベクター管モニター風のアリーナ表示設定を返す。
    ArenaLookSettings GetArenaLookSettings() const override;

    /// @brief 終了済みであるか判定する。
    bool IsFinished() const override
    {
        return finished_ && engineReady_;
    }

    /// @brief 次のシーン名前を返す。
    std::string GetNextSceneName() const override
    {
        return nextSceneName_;
    }

private:
    /// @brief シーン利用可能であるか判定する。
    bool IsSceneAvailable(std::string_view name) const;
    /// @brief 指定したシーンが使えるなら、そこへの遷移を始める。始めたらtrueを返す。
    bool StartTransitionIfAvailable(std::string_view name, float duration);
    /// @brief タイトルの項目（計画書 p11 の R1）。
    enum class MenuAction { Start, Quit, Expedition };
    /// @brief 項目を並べる。
    void OpenMenu();
    /// @brief 入力を受けて、決めた項目を動かす。
    void UpdateMenu(float dt);
    /// @brief 項目と操作の案内を積む。
    void QueueMenu();
    /// @brief デモ検証を書き込む。
    void WriteDemoValidation(bool fadeComplete);
    /// @return 先読み待ち以外で処理が進んだ場合true。
    bool InitializeDemoStep();
    /// @brief デモと同じ字形の設定を先に適用し、タイトル文字を生成する。
    void CreateLabels();
    /// @brief エンジンの準備ができた最初の更新で、タイトル文字の表示を始める。
    void ShowTitle();
    bool engineReady_ = false, demoReady_ = false, loadingGame_ = false, asyncStartup_ = true, revealed_ = false;
    bool titleShown_ = false;
    // 0から1へ進み、タイトルの後ろの暗幕を通常の濃さまで薄くしてデモを見せる。
    float demoReveal_ = 0;
    unsigned titleFrames_ = 0;
    std::unique_ptr<GameScene> demo_;
    std::unique_ptr<Fade> fade_;
    std::unique_ptr<myEngine::Sprite> backgroundVeil_;
    myEngine::Input* input_ = nullptr;
    std::unique_ptr<myEngine::TextLabel> title_, demoCaption_;
    // 後ろの場面を、左だけ暗くする板（文字を読みやすくする）。
    std::unique_ptr<myEngine::Sprite> titleShade_;
    gameplay::NeonMenuKit menuKit_;
    gameplay::NeonMenu menu_;
    std::vector<MenuAction> menuActions_;
    myEngine::Vector2 menuMouse_{};
    bool padPrompt_ = false;

    /// @brief タイトルデモの1時点の状態と表示用の計測値を表す。
    struct DemoSample {
        std::string build;
        int shots = 0, kills = 0, dashes = 0;
        float maxHomingTurnRate = 0;
        uint32_t maxDashExplosionsEmitted = 0;
        uint64_t wallBouncesObserved = 0, actorPiercesObserved = 0, splitChildrenObserved = 0;
    };

    std::array<DemoSample, 4> stageSamples_{};
    std::unique_ptr<NeonTextEffect> titleTextNeonEffect_;
    Phase phase_ = Phase::kFadeIn;
    std::string nextSceneName_;
    bool finished_ = false, demoFrozen_ = false, autoTest_ = false, startupAutoTest_ = false, startupAutoTestEarly_ = false;
    bool submissionCaptured_ = false;
    bool capturedSceneFadeOut_ = false, capturedSceneFadeIn_ = false;
    float maxObservedBurstAge_ = 0;
    uint32_t capturedStages_ = 0;
    float frozenAt_ = 0, blinkTimer_ = 0;
    // 後ろで動かす場面が、遠征の見本か（遠征の自動確認のときだけ）。ふだんは無限の場を戦車が自動で戦う。
    bool expeditionDemo_ = false;
};
