#include "GameplayScenarioSession.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
using nlohmann::json;
namespace fs = std::filesystem;

void Require(bool condition, const char* error) {
    if (!condition) throw std::runtime_error(error);
}
fs::path WorkspaceDirectory() {
    fs::path directory = fs::current_path();
    for (unsigned level = 0; level < 10; ++level) {
        if (fs::is_regular_file(directory / "project/CG2.sln")) return directory;
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    throw std::runtime_error("Gameplay scenarios require a repository working directory.");
}
std::string Utf8(const fs::path& path) {
    const auto utf8 = path.generic_u8string();
    return std::string(utf8.begin(), utf8.end());
}
fs::path Utf8Path(const std::string& value) {
    // C++20 equivalent of u8path without its MSVC deprecation warning.
    return fs::path(std::u8string(value.begin(), value.end()));
}
fs::path OutputDirectory(const json& manifest, const fs::path& workspace, const gameplaytest::Settings& settings) {
    const std::string requested = manifest.value("outputDirectory", std::string("generated/gameplay_scenarios/") + gameplaytest::Name(settings.scenario));
    Require(!requested.empty() && requested.size() <= 2048, "Invalid scenario output directory.");
    fs::path directory = Utf8Path(requested);
    if (!directory.is_absolute()) directory = workspace / directory;
    // The output may not exist yet. Normalize and verify the absolute workspace
    // prefix before creation, without probing missing OneDrive/sandbox paths.
    directory = directory.lexically_normal();
    const auto relative = directory.lexically_relative(workspace.lexically_normal());
    Require(!relative.empty() && !relative.is_absolute(), "Scenario output must be inside workspace generated directories.");
    auto part = relative.begin();
    if (*part == "project") ++part;
    Require(part != relative.end() && *part == "generated", "Scenario output must be inside workspace generated directories.");
    for (const auto& component : relative) Require(component != "..", "Scenario output cannot escape the workspace.");
    return directory;
}

template<class Integer>
Integer IntegerField(const json& data, const char* key, Integer fallback) {
    if (!data.contains(key)) return fallback;
    const auto& value = data.at(key);
    Require(value.is_number_integer(), "Scenario integer field has the wrong type.");
    if (value.is_number_unsigned()) {
        const auto number = value.get<uint64_t>();
        Require(number <= static_cast<uint64_t>((std::numeric_limits<Integer>::max)()), "Scenario integer is out of range.");
        return static_cast<Integer>(number);
    }
    const auto number = value.get<int64_t>();
    if constexpr (std::is_unsigned_v<Integer>) {
        Require(number >= 0 && static_cast<uint64_t>(number) <= (std::numeric_limits<Integer>::max)(),
            "Scenario integer is out of range.");
    } else {
        Require(number >= static_cast<int64_t>((std::numeric_limits<Integer>::min)()) &&
            number <= static_cast<int64_t>((std::numeric_limits<Integer>::max)()), "Scenario integer is out of range.");
    }
    return static_cast<Integer>(number);
}
template<size_t Size>
std::array<float, Size> FloatArray(const json& data, const char* key, std::array<float, Size> fallback) {
    if (!data.contains(key)) return fallback;
    const auto& values = data.at(key);
    Require(values.is_array() && values.size() == Size, "Scenario vector must have the expected dimensions.");
    for (size_t i = 0; i < Size; ++i) {
        Require(values.at(i).is_number(), "Scenario vector contains a non-number.");
        fallback[i] = values.at(i).get<float>();
    }
    return fallback;
}
void CheckKeys(const json& data, std::initializer_list<const char*> allowed) {
    Require(data.is_object(), "Scenario instructions must be JSON objects.");
    for (auto item = data.begin(); item != data.end(); ++item) {
        Require(std::any_of(allowed.begin(), allowed.end(), [&](const char* key) { return item.key() == key; }),
                "Unknown scenario manifest field.");
    }
}
gameplaytest::Settings ReadSettings(const json& manifest) {
    CheckKeys(manifest, {"schemaVersion", "scenario", "seed", "fixedDeltaTime", "frames", "playerStyle", "playerHp", "bossHp",
        "enemyCount", "initialProjectiles", "upgrades", "room", "enemyWave", "input", "captureFrames", "outputDirectory", "recording", "depthFixture"});
    Require(IntegerField(manifest, "schemaVersion", 1) == 1, "Unknown scenario schema version.");
    gameplaytest::Settings settings;
    Require(manifest.contains("scenario") && manifest.at("scenario").is_string() &&
        gameplaytest::ParseScenario(manifest.at("scenario").get<std::string>(), settings.scenario), "Unknown or missing scenario name.");
    settings.seed = IntegerField(manifest, "seed", settings.seed);
    settings.frames = IntegerField(manifest, "frames", settings.frames);
    settings.playerStyle = IntegerField(manifest, "playerStyle", settings.playerStyle);
    settings.playerHp = IntegerField(manifest, "playerHp", settings.playerHp);
    settings.bossHp = IntegerField(manifest, "bossHp", settings.bossHp);
    settings.enemyCount = IntegerField(manifest, "enemyCount", settings.enemyCount);
    settings.initialProjectiles = IntegerField(manifest, "initialProjectiles", settings.initialProjectiles);
    if (manifest.contains("fixedDeltaTime")) {
        Require(manifest.at("fixedDeltaTime").is_number(), "fixedDeltaTime must be numeric.");
        settings.fixedDeltaTime = manifest.at("fixedDeltaTime").get<float>();
    }
    settings.room = manifest.value("room", settings.room);
    if (manifest.contains("upgrades")) {
        Require(manifest.at("upgrades").is_array() && manifest.at("upgrades").size() <= 42, "Too many upgrade instructions.");
        settings.upgrades = manifest.at("upgrades").get<std::vector<std::string>>();
    }
    if (manifest.contains("enemyWave")) {
        Require(manifest.at("enemyWave").is_array() && manifest.at("enemyWave").size() <= 128, "Too many enemy-wave instructions.");
        for (const auto& entry : manifest.at("enemyWave")) {
            CheckKeys(entry, {"type", "position", "hp"});
            gameplaytest::EnemyWaveEntry enemy;
            enemy.type = entry.at("type").get<std::string>();
            enemy.position = FloatArray<3>(entry, "position", enemy.position);
            enemy.hp = IntegerField(entry, "hp", enemy.hp);
            settings.enemyWave.push_back(std::move(enemy));
        }
    }
    if (manifest.contains("input")) {
        Require(manifest.at("input").is_array() && manifest.at("input").size() <= 128, "Too many scripted input segments.");
        for (const auto& entry : manifest.at("input")) {
            CheckKeys(entry, {"firstFrame", "endFrame", "movement", "aim", "shoot", "dash"});
            gameplaytest::InputSegment segment;
            segment.firstFrame = IntegerField(entry, "firstFrame", segment.firstFrame);
            segment.endFrame = IntegerField(entry, "endFrame", segment.endFrame);
            segment.input.movement = FloatArray<2>(entry, "movement", segment.input.movement);
            segment.input.aim = FloatArray<3>(entry, "aim", segment.input.aim);
            segment.input.shoot = entry.value("shoot", segment.input.shoot);
            segment.input.dash = entry.value("dash", segment.input.dash);
            settings.input.push_back(segment);
        }
    }
    if (manifest.contains("captureFrames")) {
        Require(manifest.at("captureFrames").is_array() && manifest.at("captureFrames").size() <= 32, "Too many capture instructions.");
        settings.captureFrames.clear();
        for (const auto& frame : manifest.at("captureFrames")) {
            settings.captureFrames.push_back(IntegerField(json{{"frame", frame}}, "frame", 0u));
        }
    } else {
        settings.captureFrames.erase(std::remove_if(settings.captureFrames.begin(), settings.captureFrames.end(),
            [&](unsigned frame) { return frame > settings.frames; }), settings.captureFrames.end());
    }
    if (manifest.contains("recording")) {
        const auto& object = manifest.at("recording");
        CheckKeys(object, {"enabled", "firstFrame", "frameCount", "encodedFps", "maxOutputBytes"});
        if (object.contains("enabled")) Require(object.at("enabled").is_boolean(), "Recording enabled must be boolean.");
        settings.recording.enabled = object.value("enabled", false);
        settings.recording.firstFrame = IntegerField(object, "firstFrame", settings.recording.firstFrame);
        settings.recording.frameCount = IntegerField(object, "frameCount", settings.recording.frameCount);
        settings.recording.encodedFps = IntegerField(object, "encodedFps", settings.recording.encodedFps);
        settings.recording.maxOutputBytes = IntegerField(object, "maxOutputBytes", settings.recording.maxOutputBytes);
    }
    if (manifest.contains("depthFixture")) {
        const auto& object=manifest.at("depthFixture");
        CheckKeys(object,{"probe","attack","phase","minimumProgress","visualEnabled","reducedMotion","injectPhaseTwo"});
        for (const char* key:{"probe","attack","phase"}) if(object.contains(key))
            Require(object.at(key).is_string(),"Depth names must be strings.");
        for (const char* key:{"visualEnabled","reducedMotion","injectPhaseTwo"}) if(object.contains(key))
            Require(object.at(key).is_boolean(),"Depth switches must be booleans.");
        auto& depth=settings.depthFixture;
        depth.probe=object.value("probe",depth.probe); depth.attack=object.value("attack",depth.attack);
        depth.phase=object.value("phase",depth.phase);
        if(object.contains("minimumProgress")) {
            Require(object.at("minimumProgress").is_number(),"Depth trigger progress must be numeric.");
            depth.minimumProgress=object.at("minimumProgress").get<float>();
        }
        depth.visualEnabled=object.value("visualEnabled",depth.visualEnabled);
        depth.reducedMotion=object.value("reducedMotion",depth.reducedMotion);
        depth.injectPhaseTwo=object.value("injectPhaseTwo",depth.injectPhaseTwo);
        Require(gameplaytest::IsDepthScenario(settings.scenario),"Depth object is not allowed in a legacy fixture.");
    }
    std::string error;
    // Validate before taking c_str(): argument evaluation order must not leave
    // Require with a pointer into the string that validation just replaced.
    const bool valid = gameplaytest::ValidateSettings(settings, error);
    Require(valid, error.c_str());
    return settings;
}
json SettingsJson(const gameplaytest::Settings& settings) {
    json result = {{"scenario", gameplaytest::Name(settings.scenario)}, {"seed", settings.seed}, {"fixedDeltaTime", settings.fixedDeltaTime},
        {"frames", settings.frames}, {"playerStyle", settings.playerStyle}, {"playerHp", settings.playerHp}, {"bossHp", settings.bossHp},
        {"enemyCount", settings.enemyCount}, {"initialProjectiles", settings.initialProjectiles}, {"upgrades", settings.upgrades},
        {"room", settings.room}, {"captureFrames", settings.captureFrames}, {"enemyWave", json::array()}, {"input", json::array()}};
    for (const auto& enemy : settings.enemyWave)
        result["enemyWave"].push_back({{"type", enemy.type}, {"position", enemy.position}, {"hp", enemy.hp}});
    for (const auto& segment : settings.input)
        result["input"].push_back({{"firstFrame", segment.firstFrame}, {"endFrame", segment.endFrame}, {"movement", segment.input.movement},
            {"aim", segment.input.aim}, {"shoot", segment.input.shoot}, {"dash", segment.input.dash}});
    if (settings.recording.enabled) result["recording"] = {{"enabled",true},
        {"firstFrame",settings.recording.firstFrame},{"frameCount",settings.recording.frameCount},
        {"encodedFps",settings.recording.encodedFps},{"maxOutputBytes",settings.recording.maxOutputBytes}};
    if(gameplaytest::IsDepthScenario(settings.scenario)) {
        const auto& depth=settings.depthFixture;
        result["depthFixture"]={{"probe",depth.probe},{"attack",depth.attack},{"phase",depth.phase},
            {"minimumProgress",depth.minimumProgress},{"visualEnabled",depth.visualEnabled},
            {"reducedMotion",depth.reducedMotion},{"injectPhaseTwo",depth.injectPhaseTwo}};
    }
    return result;
}
bool FiniteJson(const json& value, unsigned depth = 0) {
    if (depth > 16) return false;
    if (value.is_number_float() && !std::isfinite(value.get<double>())) return false;
    if (value.is_structured()) for (const auto& child : value) if (!FiniteJson(child, depth + 1)) return false;
    return true;
}
void WriteJson(const fs::path& file, const json& value) {
    std::ofstream output(file, std::ios::binary);
    // Some Windows filesystem exceptions contain ACP text in what(). Preserve
    // a valid failure report even when that diagnostic is not UTF-8.
    if (!output || !(output << value.dump(2, ' ', false, json::error_handler_t::replace) << '\n'))
        throw std::runtime_error("Could not write scenario evidence.");
}
}

namespace gameplaytest {
nlohmann::json ToJson(const Snapshot& s) {
    return {{"frame", s.frame}, {"sceneEpoch", s.sceneEpoch}, {"bossEncounterGeneration", s.bossEncounterGeneration}, {"simulationTime", s.simulationTime},
        {"playerPosition", s.playerPosition}, {"bossPosition", s.bossPosition}, {"playerHp", s.playerHp}, {"playerMaxHp", s.playerMaxHp},
        {"playerStyle", s.playerStyle}, {"bossHp", s.bossHp}, {"playerDead", s.playerDead}, {"bossActive", s.bossActive}, {"bossDead", s.bossDead},
        {"primaryAttacks", s.primaryAttacks}, {"projectiles", s.projectiles}, {"enemies", s.enemies}, {"activeAttacks", s.activeAttacks},
        {"playerProjectiles", s.playerProjectiles}, {"enemyProjectiles", s.enemyProjectiles}, {"activeDrones", s.activeDrones}, {"minimumEnemyHp", s.minimumEnemyHp},
        {"bossShots", s.bossShots}, {"bossDashes", s.bossDashes}, {"bossPhase", s.bossPhase}, {"descriptors", s.descriptors},
        {"visualCreates", s.visualCreates}, {"visualReleases", s.visualReleases}, {"visualConstantBuffers", s.visualConstantBuffers},
        {"dissolveProgress", s.dissolveProgress}, {"visualHasResources", s.visualHasResources}, {"flow", s.flow}, {"expeditionPhase", s.expeditionPhase},
        {"visitedNodes", s.visitedNodes}, {"classId", s.classId}, {"roomId", s.roomId}, {"nodeId", s.nodeId}};
}
}

GameplayScenarioSession& GameplayScenarioSession::Get() {
    static GameplayScenarioSession session;
    return session;
}
GameplayScenarioSession::GameplayScenarioSession() {
    const DWORD required = GetEnvironmentVariableW(L"CG2_GAMEPLAY_SCENARIO", nullptr, 0);
    if (required <= 1) return; // An unset or empty environment value is inactive.
    try {
        const auto workspace = WorkspaceDirectory();
        outputDirectory_ = Utf8(workspace / "generated/scenario_error");
        Require(required <= 32767, "Scenario manifest path is too long.");
        std::wstring manifestPath(required, L'\0');
        const DWORD written = GetEnvironmentVariableW(L"CG2_GAMEPLAY_SCENARIO", manifestPath.data(), required);
        Require(written != 0 && written < required, "Could not read scenario manifest environment setting.");
        manifestPath.resize(written);
        const fs::path file(manifestPath);
        Require(fs::is_regular_file(file) && fs::file_size(file) <= 256 * 1024, "Scenario manifest is missing or larger than 256 KiB.");
        std::ifstream input(file, std::ios::binary);
        Require(static_cast<bool>(input), "Could not open scenario manifest.");
        const auto manifest = json::parse(input);
        const auto candidate = ReadSettings(manifest);
        const auto directory = OutputDirectory(manifest, workspace, candidate);
        settings_ = candidate;
        outputDirectory_ = Utf8(directory);
        snapshots_.reserve(settings_.frames);
        enabled_ = true;
    } catch (const std::exception& error) {
        Fail(std::string("Invalid scenario manifest: ") + error.what());
        Finish();
        PostQuitMessage(9);
    }
}
void GameplayScenarioSession::BeginScene() {
    if (enabled_ && !finished_) ++sceneEpoch_;
}
void GameplayScenarioSession::Record(gameplaytest::Snapshot snapshot, float gameplayDt,
    float presentationDt, float effectiveTimeScale) {
    if (!enabled_ || finished_) return;
    if (frame_ >= settings_.frames) { Fail("Scenario recorded beyond its requested duration."); return; }
    if (settings_.recording.enabled) {
        if (!std::isfinite(gameplayDt) || gameplayDt < 0 || gameplayDt > settings_.fixedDeltaTime + 1.0e-6f ||
            !std::isfinite(presentationDt) || presentationDt < 0 || presentationDt > settings_.fixedDeltaTime + 1.0e-6f ||
            !std::isfinite(effectiveTimeScale) || effectiveTimeScale < 0 || effectiveTimeScale > 1.0001f) {
            Fail("Invalid actual recording clock delta."); return;
        }
        gameplayElapsed_ += gameplayDt; presentationElapsed_ += presentationDt;
        lastGameplayDt_ = gameplayDt; lastPresentationDt_ = presentationDt; effectiveTimeScale_ = effectiveTimeScale;
    }
    snapshot.frame = ++frame_;
    snapshot.sceneEpoch = sceneEpoch_;
    snapshot.simulationTime = static_cast<float>(frame_) * settings_.fixedDeltaTime;
    validator_.Observe(snapshot);
    for (const auto& error : validator_.GetErrors()) Fail(error);
    snapshots_.push_back(std::move(snapshot));
}
bool GameplayScenarioSession::BeginRecording(unsigned width, unsigned height) {
    if (!settings_.recording.enabled) return true;
    try {
        Require(width >= 16 && width <= 8192 && height >= 16 && height <= 8192 && width % 2 == 0 && height % 2 == 0,
            "Recording requires bounded even native backbuffer dimensions.");
        if (recordingStarted_) {
            Require(width == recordingWidth_ && height == recordingHeight_, "Native recording resolution changed during the sequence.");
            return true;
        }
        const auto directory = Utf8Path(outputDirectory_);
        const auto recording = directory / "recording";
        Require(!fs::exists(recording), "Recording requires a fresh directory; existing evidence is never overwritten.");
        fs::create_directories(directory);
        // RGBA8 worst-case PNG plus bounded JSON/PNG framing allowance per frame.
        const uint64_t frameBytes = uint64_t(width) * height * 4 + 256 * 1024;
        recordingPreflightBytes_ = frameBytes * settings_.recording.frameCount;
        Require(recordingPreflightBytes_ <= settings_.recording.maxOutputBytes, "Native recording exceeds its source-byte budget.");
        recordingAvailableBytes_ = fs::space(directory).available;
        Require(recordingAvailableBytes_ >= recordingPreflightBytes_ + 64 * 1024 * 1024,
            "Not enough available disk space for bounded native recording.");
        fs::create_directories(recording / "frames");
        recordingWidth_ = width; recordingHeight_ = height; recordingStarted_ = true;
        return true;
    } catch (const std::exception& error) {
        Fail(std::string("Recording preflight: ") + error.what()); return false;
    }
}
bool GameplayScenarioSession::ShouldRecordCompletedFrame() const {
    return gameplaytest::ShouldRecordCompletedFrame(settings_, frame_);
}
unsigned GameplayScenarioSession::GetRecordingSequenceFrame() const {
    return ShouldRecordCompletedFrame() ? frame_ - settings_.recording.firstFrame : 0;
}
nlohmann::json GameplayScenarioSession::MakeRecordingMetadata() const {
    return {{"sequenceFrame",GetRecordingSequenceFrame()},{"sequenceRate",settings_.recording.encodedFps},
        {"firstSimulationFrame",settings_.recording.firstFrame},{"plannedFrameCount",settings_.recording.frameCount},
        {"comparisonFreeze",false},{"heldDrawCount",0},{"recordingClocks",{
            {"baseElapsed",double(frame_) * settings_.fixedDeltaTime},{"gameplayElapsed",gameplayElapsed_},
            {"presentationElapsed",presentationElapsed_},{"gameplayDelta",lastGameplayDt_},
            {"presentationDelta",lastPresentationDt_},{"timeScale",effectiveTimeScale_}}}};
}
void GameplayScenarioSession::ReportRecordedFrame(unsigned sequenceFrame, const std::string& name,
    const gameplaytest::Snapshot& snapshot, uint64_t pngBytes, uint64_t metadataBytes) {
    if (!enabled_ || finished_) return;
    char expected[32]{};
    std::snprintf(expected, sizeof(expected), "frame_%05u", sequenceFrame);
    if (!recordingStarted_ || sequenceFrame != recordingFrames_.size() ||
        sequenceFrame >= settings_.recording.frameCount || name != expected ||
        snapshot.frame != settings_.recording.firstFrame + sequenceFrame || snapshot.frame != frame_ ||
        !pngBytes || !metadataBytes || metadataBytes > 256 * 1024 ||
        pngBytes > settings_.recording.maxOutputBytes || metadataBytes > settings_.recording.maxOutputBytes - pngBytes ||
        recordingBytes_ > settings_.recording.maxOutputBytes - pngBytes - metadataBytes) {
        Fail("Invalid, skipped, duplicate or excessive recorded frame evidence."); return;
    }
    recordingBytes_ += pngBytes + metadataBytes;
    auto entry = MakeRecordingMetadata();
    entry["simulationFrame"] = snapshot.frame; entry["sceneEpoch"] = snapshot.sceneEpoch;
    entry["png"] = name + ".png"; entry["metadata"] = name + ".json";
    entry["pngBytes"] = pngBytes; entry["metadataBytes"] = metadataBytes;
    recordingFrames_.push_back(std::move(entry));
}

void GameplayScenarioSession::ReportDepthFrame(const nlohmann::json& row) {
    if(!enabled_ || finished_ || !gameplaytest::IsDepthScenario(settings_.scenario)) return;
    const auto bytes=row.dump().size();
    if(!row.is_object() || !FiniteJson(row) || !row.contains("frame") || row.at("frame")!=frame_ ||
        frame_==0 || depthRows_.size()+1!=frame_ || depthRows_.size()>=3600 ||
        bytes>64*1024 || bytes>128ull*1024*1024-depthBytes_) {
        Fail("Invalid, nonfinite, skipped or excessive actual Depth frame evidence."); return;
    }
    depthBytes_+=bytes; depthRows_.push_back(row);
}
void GameplayScenarioSession::ReportDepthDraw(const nlohmann::json& proof) {
    if(!enabled_ || finished_ || !gameplaytest::IsDepthScenario(settings_.scenario)) return;
    if(depthRows_.empty() || depthRows_.back().at("frame")!=frame_ || !proof.is_object() ||
        !FiniteJson(proof) || proof.dump().size()>2048 || depthRows_.back().contains("drawProof")) {
        Fail("Invalid or duplicate actual Depth Draw proof."); return;
    }
    auto changed=depthRows_.back(); changed["drawProof"]=proof;
    const auto oldBytes=depthRows_.back().dump().size(),newBytes=changed.dump().size();
    if(newBytes>64*1024 || newBytes-oldBytes>128ull*1024*1024-depthBytes_) {
        Fail("Excessive actual Depth Draw proof bytes."); return;
    }
    depthBytes_+=newBytes-oldBytes; depthRows_.back()=std::move(changed);
}
bool GameplayScenarioSession::NotifyDepthSceneEntered(const std::string& name) {
    if(!enabled_ || finished_ || !gameplaytest::IsDepthScenario(settings_.scenario)) return false;
    if(!gameplaytest::IsSafeIdentifier(name) || depthScenes_.size()>=16) {
        Fail("Invalid or excessive actual Depth scene notification."); return false;
    }
    depthScenes_.push_back({{"scene",name},{"frame",frame_},{"sceneEpoch",sceneEpoch_},
        {"actualInitializedScene",true}});
    if(settings_.scenario==gameplaytest::Scenario::NeonDepthLifecycle && settings_.depthFixture.probe=="title_return" && name=="TITLE") {
        if(frame_==0 || frame_>settings_.frames || depthRows_.size()!=frame_ || settings_.recording.enabled) {
            Fail("Title completion has no bounded actual gameplay rows.");
        } else depthTitleEntered_=true;
        Finish(); // Reports must be saved before the Game observer posts quit.
        return true;
    }
    return false;
}

void GameplayScenarioSession::Fail(const std::string& error) {
    const auto bounded = error.substr(0, 512);
    if (errors_.size() < 32 && std::find(errors_.begin(), errors_.end(), bounded) == errors_.end()) errors_.push_back(bounded);
}
bool GameplayScenarioSession::ShouldFinish() const {
    return enabled_ && !finished_ && (frame_ >= settings_.frames || !errors_.empty());
}
nlohmann::json GameplayScenarioSession::MakeMetadata() const {
    json result = {{"schemaVersion", 1}, {"scenario", gameplaytest::Name(settings_.scenario)}, {"seed", settings_.seed},
        {"fixedDeltaTime", settings_.fixedDeltaTime}, {"frame", frame_}, {"sceneEpoch", sceneEpoch_}};
    if (!snapshots_.empty()) result["snapshot"] = gameplaytest::ToJson(snapshots_.back());
    return result;
}
void GameplayScenarioSession::ReportCapture(const std::string& name, const gameplaytest::Snapshot& snapshot) {
    if (!enabled_ || finished_) return;
    if (!gameplaytest::IsSafeIdentifier(name) || captures_.size() >= 64) { Fail("Invalid or excessive scenario capture evidence."); return; }
    captures_.push_back({{"name", name}, {"snapshot", gameplaytest::ToJson(snapshot)}});
}
void GameplayScenarioSession::ReportDetail(const std::string& name, const nlohmann::json& detail) {
    if (!enabled_ || finished_) return;
    if (!gameplaytest::IsSafeIdentifier(name) || (!details_.contains(name) && details_.size() >= 32) ||
        !FiniteJson(detail) || detail.dump().size() > 16384) { Fail("Invalid or excessive scenario detail evidence."); return; }
    details_[name] = detail;
}
bool GameplayScenarioSession::Finish() {
    if (finished_) return errors_.empty();
    const bool actualTitle=depthTitleEntered_ && settings_.scenario==gameplaytest::Scenario::NeonDepthLifecycle &&
        settings_.depthFixture.probe=="title_return" && !settings_.recording.enabled;
    if (enabled_ && frame_ < settings_.frames && !actualTitle) Fail("Scenario finished before its requested duration.");
    if(enabled_ && settings_.scenario==gameplaytest::Scenario::NeonDepthLifecycle &&
        settings_.depthFixture.probe=="title_return" && !actualTitle)
        Fail("Title-return never initialized the actual TITLE scene.");
    if(enabled_ && gameplaytest::IsDepthScenario(settings_.scenario) && depthRows_.size()!=frame_)
        Fail("Every actual Depth update requires exactly one evidence row.");
    if (settings_.recording.enabled && (!recordingStarted_ || recordingFrames_.size() != settings_.recording.frameCount))
        Fail("Continuous recording finished without every requested saved frame.");
    finished_ = true;
    try {
        Require(!outputDirectory_.empty(), "Scenario evidence has no workspace output directory.");
        const auto directory = Utf8Path(outputDirectory_);
        fs::create_directories(directory);
        json snapshots = json::array();
        for (const auto& snapshot : snapshots_) snapshots.push_back(gameplaytest::ToJson(snapshot));
        WriteJson(directory / "snapshots.json", snapshots);
        json report = {{"schemaVersion", 1}, {"completed", errors_.empty() && enabled_ && (frame_ == settings_.frames || actualTitle)},
            {"scenario", gameplaytest::Name(settings_.scenario)}, {"settings", SettingsJson(settings_)}, {"frameCount", frame_},
            {"sceneEpochs", sceneEpoch_}, {"errors", errors_}, {"captures", captures_}, {"details", details_},
            {"maximumProjectiles", validator_.GetMaximumProjectiles()}, {"maximumEnemies", validator_.GetMaximumEnemies()},
            {"maximumPrimaryAttacks", validator_.GetMaximumPrimaryAttacks()}, {"maximumActiveAttacks", validator_.GetMaximumActiveAttacks()},
            {"sawPlayerDeath", validator_.SawPlayerDeath()}, {"firstSnapshot", snapshots_.empty() ? json(nullptr) : gameplaytest::ToJson(snapshots_.front())},
            {"lastSnapshot", snapshots_.empty() ? json(nullptr) : gameplaytest::ToJson(snapshots_.back())}};
        if (settings_.recording.enabled) {
            json recording = {{"schemaVersion",1},{"completed",errors_.empty() && enabled_ && frame_ == settings_.frames &&
                    recordingStarted_ && recordingFrames_.size() == settings_.recording.frameCount},
                {"firstSimulationFrame",settings_.recording.firstFrame},{"frameCount",recordingFrames_.size()},
                {"plannedFrameCount",settings_.recording.frameCount},{"encodedFps",settings_.recording.encodedFps},
                {"fixedDeltaTime",settings_.fixedDeltaTime},{"nativeResolution",{recordingWidth_,recordingHeight_}},
                {"maximumSourceBytes",settings_.recording.maxOutputBytes},{"sourceBytes",recordingBytes_},
                {"preflightBytes",recordingPreflightBytes_},{"availableBytesAtPreflight",recordingAvailableBytes_},
                {"heldDrawCount",0},{"comparisonFreeze",false},{"framesDirectory","frames"},
                {"sequencePattern","frame_%05d.png"},{"metadataPattern","frame_%05d.json"},{"errors",errors_},
                {"clockDescription","One actual completed scene update per source PNG; actual gameplay and boss-presentation clock deltas recorded; PNG-save wall time omitted."},
                {"frames",recordingFrames_}};
            recording["status"] = recording.at("completed").get<bool>() ? "COMPLETE" : "INCOMPLETE";
            report["recording"] = recording;
            fs::create_directories(directory / "recording");
            WriteJson(directory / "recording/recording-report.json", recording);
        }
        if(gameplaytest::IsDepthScenario(settings_.scenario)) {
            WriteJson(directory/"depth-frames.json",depthRows_);
            report["depth"]={{"frameCount",depthRows_.size()},{"expectedMaxFrames",settings_.frames},
                {"earlyTitleCompletion",actualTitle},{"actualSceneNotifications",depthScenes_},
                {"rowsFile","depth-frames.json"},{"source","actual Enemy/Player/scene observables"}};
        }
        WriteJson(directory / "report.json", report);
    } catch (const std::exception& error) {
        Fail(std::string("Scenario report failure: ") + error.what());
    }
    return errors_.empty() && enabled_ && (frame_ == settings_.frames || actualTitle);
}
#endif
