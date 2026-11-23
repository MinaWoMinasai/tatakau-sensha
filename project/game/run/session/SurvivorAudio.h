#pragma once
#include "Audio.h"
#include "game/run/SurvivorAudioConfig.h"
#include <map>
#include <string>

namespace gameplay {
/// @brief 育つ戦車の試作の BGM と効果音を鳴らす（音の中身は survivor::AudioConfig）。
/// @note ファイルは resources/audio/ から読み、同じファイルは1回だけ読む。無いファイルの音は鳴らさない（遊びは止めない）。
/// 曲は場面ごとに1本を流し、替えるときは前の曲を下げて次の曲を上げる。エンジンの全体音量やほかの場面の音には触れない。
class SurvivorAudio {
public:
    SurvivorAudio() = default;
    ~SurvivorAudio()
    {
        Shutdown();
    }
    SurvivorAudio(const SurvivorAudio&) = delete;
    SurvivorAudio& operator=(const SurvivorAudio&) = delete;

    /// @brief 音源を読み込む。muted なら音量0で鳴らす（自動確認で、鳴らした回数だけを数える）。
    void Initialize(const survivor::AudioConfig& config, bool muted);
    void Shutdown();
    /// @brief 効果音を鳴らす。無い名前や、cooldown の間は鳴らさない。鳴らしたら真。
    bool Play(const std::string& cue);
    /// @brief 流す曲を替える（空なら止める）。
    void SetMusic(const std::string& track);
    /// @brief 曲の音量を合わせる。ducked なら下げる。dt は実時間。
    void Update(float dt, bool ducked);

    /// @brief 読み込めたファイルの数と、読み込めなかったファイルの数。
    int GetLoadedFiles() const
    {
        return loadedFiles_;
    }
    int GetMissingFiles() const
    {
        return missingFiles_;
    }
    /// @brief 効果音ごとに鳴らした回数（自動確認と記録）。
    const std::map<std::string, int>& GetPlayCounts() const
    {
        return playCounts_;
    }
    const std::string& GetMusic() const
    {
        return currentTrack_;
    }

private:
    /// @brief ファイルを読み込み、エンジンの音の名前を返す（読めなければ空）。
    std::wstring LoadFile(const std::string& file, int voices);
    struct Track {
        std::string name;
        myEngine::Audio::VoiceHandle voice;
        float gain = 0.0f;
        float target = 0.0f;
    };
    void StartTrack(Track& track, const std::string& name);

    survivor::AudioConfig config_;
    bool initialized_ = false;
    bool muted_ = false;
    std::map<std::string, std::wstring> fileKeys_;
    std::map<std::string, float> cooldowns_;
    std::map<std::string, int> playCounts_;
    int loadedFiles_ = 0;
    int missingFiles_ = 0;
    unsigned variant_ = 0;
    std::string currentTrack_;
    // 流している曲と、下げている途中の前の曲。
    Track current_;
    Track previous_;
};
} // namespace gameplay
