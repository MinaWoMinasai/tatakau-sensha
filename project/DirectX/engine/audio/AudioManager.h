#pragma once
#include "Audio.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include "DirectXCommon.h"

#include <filesystem>
namespace cg2 {

namespace fs = std::filesystem;

/// @brief 音源ファイル・音量など、音声管理画面で保存する設定を表す。
struct AudioConfig {
    std::string name;           // 識別名 (JSONのキー)
    std::string filePath;       // ファイルパス
    float defaultVolume = 1.0f; // デフォルト音量
    bool loop = false;          // BGM用などのループ設定
    int maxConcurrency = 1;     // 同時発音数

    // JSON変換
    nlohmann::json ToJson() const
    {
        return nlohmann::json{{"filePath", filePath}, {"volume", defaultVolume}, {"loop", loop}, {"maxConcurrency", maxConcurrency}};
    }

    /// @brief JSONの値を対応する設定項目へ読み取る。省略時の扱いは各項目の既定値に従う。
    void FromJson(const std::string& key, const nlohmann::json& j)
    {
        name = key;
        filePath = j.at("filePath").get<std::string>();
        defaultVolume = j.value("volume", 1.0f);
        loop = j.value("loop", false);
        maxConcurrency = j.value("maxConcurrency", 1);
    }
};

/// @brief BGMとSEの登録・設定保存・開発画面からの再生をまとめる。
class AudioManager {
public:
    /// @brief 共有インスタンスを返す。呼び出し側は取得したポインターをdeleteしない。
    static AudioManager* GetInstance();

    // 全ての音設定をロードしてAudioクラスに登録する
    void LoadAllConfigs(const std::string& path);

    /// @brief ImGUIを更新する。
    void UpdateImGui();
    /// @brief SEを再生する。
    void PlaySE(const std::string& name);
    /// @brief BGMを再生する。
    void PlayBGM(const std::string& name);
    /// @brief 今流れているBGMを止める
    void StopBGM();

    /// @brief 音声ファイル一覧を最新の内容へ更新する。
    void RefreshAudioFileList();

private:
    std::map<std::string, AudioConfig> configs_;

    // 文字列変換用ヘルパー (std::string <-> std::wstring)
    std::wstring ConvertString(const std::string& str);

    /// @brief 全体Configsを保存する。
    void SaveAllConfigs(const std::string& path);

    std::string currentBGMName_ = "";

    std::vector<std::string> audioFileList_;
    const std::string kAudioDirPath = "resources/audio/";
};

} // namespace cg2
