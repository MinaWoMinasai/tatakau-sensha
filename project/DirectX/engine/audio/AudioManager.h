#pragma once
#include "Audio.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include "DirectXCommon.h"

#include <filesystem>
namespace fs = std::filesystem;

struct AudioConfig {
    std::string name;           // 識別名 (JSONのキー)
    std::string filePath;       // ファイルパス
    float defaultVolume = 1.0f; // デフォルト音量
    bool loop = false;          // BGM用などのループ設定
    int maxConcurrency = 1;     // 同時発音数

    // JSON変換
    nlohmann::json ToJson() const {
        return nlohmann::json{
            {"filePath", filePath},
            {"volume", defaultVolume},
            {"loop", loop},
            {"maxConcurrency", maxConcurrency}
        };
    }

    void FromJson(const std::string& key, const nlohmann::json& j) {
        name = key;
        filePath = j.at("filePath").get<std::string>();
        defaultVolume = j.value("volume", 1.0f);
        loop = j.value("loop", false);
        maxConcurrency = j.value("maxConcurrency", 1);
    }
};


class AudioManager {
public:
    static AudioManager* GetInstance();

    // 全ての音設定をロードしてAudioクラスに登録する
    void LoadAllConfigs(const std::string& path);

    void UpdateImGui();
    void PlaySE(const std::string& name);
    void PlayBGM(const std::string& name);
    /// <summary>
    /// 今流れているBGMを止める
    /// </summary>
    void StopBGM();

    void RefreshAudioFileList();

private:
    std::map<std::string, AudioConfig> configs_;

    // 文字列変換用ヘルパー (std::string <-> std::wstring)
    std::wstring ConvertString(const std::string& str);

    void SaveAllConfigs(const std::string& path);

    std::string currentBGMName_ = "";

    std::vector<std::string> audioFileList_;
    const std::string kAudioDirPath = "resources/audio/";
};