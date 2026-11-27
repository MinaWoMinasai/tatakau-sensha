#include "game/settings/UserSettings.h"
#include "WinApp.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace usersettings {
namespace {
/// @brief 保存する場所。空なら、読みも書きもしない。
const std::string& Path()
{
    static const std::string path = [] {
        char value[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableA("MYENGINE_SETTINGS_FILE", value, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            return std::string(value);
        }
        char hidden[8]{};
        if (GetEnvironmentVariableA("MYENGINE_HIDDEN_WINDOW", hidden, sizeof(hidden)) == 1 && hidden[0] == '1') {
            return std::string();
        }
        return std::string("save/settings.json");
    }();
    return path;
}

UserSettings Load()
{
    UserSettings settings;
    if (Path().empty()) {
        return settings;
    }
    std::ifstream file(Path());
    const nlohmann::json root = file ? nlohmann::json::parse(file, nullptr, false) : nlohmann::json();
    if (!root.is_object()) {
        return settings;
    }
    auto level = [&](const char* key, int& value) {
        if (root.contains(key) && root.at(key).is_number_integer()) {
            value = (std::clamp)(root.at(key).get<int>(), 0, 10);
        }
    };
    auto flag = [&](const char* key, bool& value) {
        if (root.contains(key) && root.at(key).is_boolean()) {
            value = root.at(key).get<bool>();
        }
    };
    level("music", settings.music);
    level("effects", settings.effects);
    flag("fullscreen", settings.fullscreen);
    level("shake", settings.shake);
    flag("calm", settings.calm);
    level("curve", settings.curve);
    flag("autoFire", settings.autoFire);
    flag("tutorialDone", settings.tutorialDone);
    if (root.contains("bestSeconds") && root.at("bestSeconds").is_number()) {
        settings.bestSeconds = (std::clamp)(root.at("bestSeconds").get<float>(), 0.0f, 36000.0f);
    }
    return settings;
}
} // namespace

UserSettings& Get()
{
    static UserSettings settings = Load();
    return settings;
}

void Save()
{
    if (Path().empty()) {
        return;
    }
    const UserSettings& s = Get();
    const nlohmann::json root{{"music", s.music}, {"effects", s.effects}, {"fullscreen", s.fullscreen}, {"shake", s.shake},
                              {"calm", s.calm},   {"curve", s.curve},     {"autoFire", s.autoFire},     {"tutorialDone", s.tutorialDone},
                              {"bestSeconds", s.bestSeconds}};
    std::error_code error;
    const std::filesystem::path path(Path());
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    // 書いている途中で落ちても前の設定が残るように、別の名前で書いてから替える。
    const std::filesystem::path temporary(Path() + ".tmp");
    {
        std::ofstream file(temporary);
        if (!file) {
            return;
        }
        file << root.dump(2) << "\n";
    }
    std::filesystem::rename(temporary, path, error);
}

void ApplyWindow()
{
    myEngine::WinApp::GetInstance()->SetFullscreen(Get().fullscreen);
}

float MusicScale()
{
    return static_cast<float>(Get().music) / 7.0f;
}

float EffectsScale()
{
    return static_cast<float>(Get().effects) / 8.0f;
}

float ShakeScale()
{
    return static_cast<float>(Get().shake) / 6.0f;
}

void ApplyLook(IScene::ArenaLookSettings& look)
{
    look.curvature *= static_cast<float>(Get().curve) / 4.0f;
}
} // namespace usersettings
