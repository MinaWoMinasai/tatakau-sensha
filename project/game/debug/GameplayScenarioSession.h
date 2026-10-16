#pragma once
#include "DeveloperTools.h"
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "GameplayScenario.h"
#include "externals/nlohmann/json.hpp"
#include <string>
#include <vector>

namespace gameplaytest {
/// @brief スナップショットをJSONへ変換する。
nlohmann::json ToJson(const Snapshot& snapshot);
} // namespace gameplaytest

// プロセスが所有するDeveloper用の検証セッション。シーンを破棄・再生成しても
// スクリプトのフレームや記録は失われない。ゲームプレイや描画へのポインタは持たない。
class GameplayScenarioSession {
public:
    static GameplayScenarioSession& Get();

    bool IsActive() const
    {
        return enabled_;
    }

    bool IsFinished() const
    {
        return finished_;
    }

    const gameplaytest::Settings& GetSettings() const
    {
        return settings_;
    }

    unsigned GetFrame() const
    {
        return frame_;
    }

    unsigned GetSceneEpoch() const
    {
        return sceneEpoch_;
    }

    const std::string& GetOutputDirectory() const
    {
        return outputDirectory_;
    }

    const std::vector<std::string>& GetErrors() const
    {
        return errors_;
    }

    /// @brief 新しいシーンの開始を記録する。
    void BeginScene();
    // シミュレーションの更新ごとに、その後で1回呼ぶ。入力は更新前のGetFrame()を使い、
    // 記録・キャプチャのフレームは1から始まる完了済みの更新回数で数える。
    void Record(gameplaytest::Snapshot snapshot, float gameplayDt = 0.0f, float presentationDt = 0.0f, float effectiveTimeScale = 1.0f);
    /// @brief 連続記録を開始する。
    bool BeginRecording(unsigned width, unsigned height);
    /// @brief 直前に完了したフレームを記録するかを返す。
    bool ShouldRecordCompletedFrame() const;
    unsigned GetRecordingSequenceFrame() const;
    /// @brief 連続記録全体のメタデータを作る。
    nlohmann::json MakeRecordingMetadata() const;
    /// @brief 記録したフレームの内容をレポートに追加する。
    void ReportRecordedFrame(unsigned sequenceFrame, const std::string& name, const gameplaytest::Snapshot& snapshot, uint64_t pngBytes,
                             uint64_t metadataBytes);
    /// @brief Depthボス検証の1フレーム分の結果を追加する。
    void ReportDepthFrame(const nlohmann::json& row);
    /// @brief Depthボスの描画の確認結果を追加する。
    void ReportDepthDraw(const nlohmann::json& proof);
    /// @brief Depthボス検証用のシーンに入ったことを通知する。初回だけtrueを返す。
    bool NotifyDepthSceneEntered(const std::string& name);
    /// @brief Depthボス検証中のシーン遷移を記録する。タイトルへ戻って検証を終えた場合はtrueを返す。
    void Fail(const std::string& error);
    /// @brief 検証を終了するべきかを返す。
    bool ShouldFinish() const;
    // アダプタはFinishの前に最後のGPUキャプチャを解決し、終了処理も担当する。
    bool Finish();
    /// @brief 検証全体のメタデータを作る。
    nlohmann::json MakeMetadata() const;
    /// @brief キャプチャした画像の情報をレポートに追加する。
    void ReportCapture(const std::string& name, const gameplaytest::Snapshot& snapshot);
    /// @brief 任意の詳細情報をレポートに追加する。
    void ReportDetail(const std::string& name, const nlohmann::json& detail);

private:
    GameplayScenarioSession();
    gameplaytest::Settings settings_;
    gameplaytest::InvariantValidator validator_;
    std::vector<gameplaytest::Snapshot> snapshots_;
    std::vector<std::string> errors_;
    nlohmann::json captures_ = nlohmann::json::array();
    nlohmann::json details_ = nlohmann::json::object();
    nlohmann::json depthRows_ = nlohmann::json::array(), depthScenes_ = nlohmann::json::array();
    uint64_t depthBytes_ = 0;
    bool depthTitleEntered_ = false;
    nlohmann::json recordingFrames_ = nlohmann::json::array();
    unsigned recordingWidth_ = 0, recordingHeight_ = 0;
    uint64_t recordingBytes_ = 0, recordingPreflightBytes_ = 0, recordingAvailableBytes_ = 0;
    double gameplayElapsed_ = 0, presentationElapsed_ = 0;
    float lastGameplayDt_ = 0, lastPresentationDt_ = 0, effectiveTimeScale_ = 1;
    bool recordingStarted_ = false;
    std::string outputDirectory_;
    unsigned frame_ = 0, sceneEpoch_ = 0;
    bool enabled_ = false, finished_ = false;
};
#endif
