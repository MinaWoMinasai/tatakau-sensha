#pragma once
#include "DeveloperTools.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "GameplayScenario.h"
#include "externals/nlohmann/json.hpp"
#include <string>
#include <vector>

namespace gameplaytest {
nlohmann::json ToJson(const Snapshot& snapshot);
}

// Process-owned Developer validation session. A scene can be destroyed/recreated
// without losing its script frame or evidence. No gameplay/renderer pointers.
class GameplayScenarioSession {
public:
    static GameplayScenarioSession& Get();
    bool IsActive() const { return enabled_; }
    bool IsFinished() const { return finished_; }
    const gameplaytest::Settings& GetSettings() const { return settings_; }
    unsigned GetFrame() const { return frame_; }
    unsigned GetSceneEpoch() const { return sceneEpoch_; }
    const std::string& GetOutputDirectory() const { return outputDirectory_; }
    const std::vector<std::string>& GetErrors() const { return errors_; }
    void BeginScene();
    // Called once AFTER each simulated update. Input uses GetFrame() before
    // the update; recorded/capture frames count completed updates starting at 1.
    void Record(gameplaytest::Snapshot snapshot, float gameplayDt = 0.0f,
        float presentationDt = 0.0f, float effectiveTimeScale = 1.0f);
    bool BeginRecording(unsigned width, unsigned height);
    bool ShouldRecordCompletedFrame() const;
    unsigned GetRecordingSequenceFrame() const;
    nlohmann::json MakeRecordingMetadata() const;
    void ReportRecordedFrame(unsigned sequenceFrame, const std::string& name,
        const gameplaytest::Snapshot& snapshot, uint64_t pngBytes, uint64_t metadataBytes);
    void ReportDepthFrame(const nlohmann::json& row);
    void ReportDepthDraw(const nlohmann::json& proof);
    bool NotifyDepthSceneEntered(const std::string& name);
    void Fail(const std::string& error);
    bool ShouldFinish() const;
    // The adapter resolves the last GPU capture before Finish and owns quit.
    bool Finish();
    nlohmann::json MakeMetadata() const;
    void ReportCapture(const std::string& name, const gameplaytest::Snapshot& snapshot);
    void ReportDetail(const std::string& name, const nlohmann::json& detail);

private:
    GameplayScenarioSession();
    gameplaytest::Settings settings_;
    gameplaytest::InvariantValidator validator_;
    std::vector<gameplaytest::Snapshot> snapshots_;
    std::vector<std::string> errors_;
    nlohmann::json captures_ = nlohmann::json::array();
    nlohmann::json details_ = nlohmann::json::object();
    nlohmann::json depthRows_=nlohmann::json::array(),depthScenes_=nlohmann::json::array();
    uint64_t depthBytes_=0;
    bool depthTitleEntered_=false;
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
