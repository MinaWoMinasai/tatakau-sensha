#pragma once
#include <memory>
#include "../runtime/IApplicationHooks.h"

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
class NeonShowcaseCapture;
#endif

/// @brief 『たたかうせんしゃ』の実行ファイル固有の処理をApplicationへ差し込む。
/// ゲームモジュールの登録、タイトル素材の先読み、共通の効果音、開発用の自動検証を扱う。
class Game final : public IApplicationHooks {
public:
    Game();
    ~Game() override;

    bool RegisterGameModules(GameModuleRegistry& registry) override;
    std::string_view GetFallbackGameModuleId() const override;
    std::string_view GetFallbackSceneName() const override;
    std::string_view GetProgressiveStartupSceneName() const override;
    std::string_view GetStartupValidationSceneName() const override;

    void OnBeforeEngineInitialize(const std::string& startupScene) override;
    void OnLoadSharedAssets() override;

    bool IsBackgroundValidationActive() const override;
    void OnMainLoopStarting() override;
    void OnMainLoopFinished() override;
    void OnFrameInputReady(myEngine::Input& input) override;
    void OnBeforeSceneUpdate() override;
    void OnAfterSceneUpdate() override;
    void AdjustGrayscale(float deltaTime, const myEngine::DeveloperShowcaseState& showcase, bool& grayscale) override;
    void OnStartupFrameRecorded(myEngine::DirectXCommon& dx, bool firstFrame) override;
    void OnStartupFramePresented(myEngine::DirectXCommon& dx) override;

private:
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    // 起動直後のタイトルを画像に残す開発用キャプチャ（MYENGINE_STARTUP_CAPTURE=1）。メインループの間だけ持つ。
    std::unique_ptr<NeonShowcaseCapture> startupCapture_;
    bool captureStartup_ = false;
    // 深度シナリオで、シーンの切り替わりを検出するための更新前の状態。
    unsigned depthSceneEpochBefore_ = 0;
    std::string depthSceneNameBefore_;
#endif
};
