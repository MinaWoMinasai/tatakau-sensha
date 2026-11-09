#pragma once
#include <map>
#include <string>

namespace survivor {
/// @brief 音1つの設定。file は resources/audio/ からの場所（wav か mp3）。
struct AudioCue {
    std::string file;
    float volume = 0.5f;
    /// @brief 音の高さ（再生の速さ）の倍率と、鳴らすたびに揺らす幅。
    float pitch = 1.0f;
    float pitchJitter = 0.03f;
    /// @brief 同じ音を次に鳴らせるまでの秒（連射で鳴りすぎないように）。
    float cooldown = 0.04f;
    /// @brief 同時に重ねて鳴らせる数。
    int voices = 3;
};

/// @brief 育つ戦車の試作の BGM と効果音（survivor_proto.json の audio）。
/// @note music は場面ごとの曲（base 拠点・stage1・stage2・boss・rest 休憩部屋）。cues は場面ごとの効果音。
/// 音源を差し替えるときは、resources/audio/ の下にファイルを置き、file を書き換える。
struct AudioConfig {
    float musicVolume = 0.5f;
    float effectsVolume = 0.8f;
    /// @brief 曲を切り替えるときに、前の曲を下げて次の曲を上げる秒。
    float fadeSeconds = 1.0f;
    /// @brief 持ち物の画面を開いている間などに、曲を下げる割合。
    float duckRatio = 0.45f;
    std::map<std::string, AudioCue> music;
    std::map<std::string, AudioCue> cues;
};

/// @brief 仮の音（遠征版のために合成した音）を当てた既定値。好きな音源が見つかったら、設定の file を差し替える。
inline AudioConfig DefaultAudioConfig()
{
    AudioConfig config;
    const std::string base = "tank_expedition/";
    auto music = [&](const char* key, const char* file, float volume) {
        config.music[key] = {base + file, volume, 1.0f, 0.0f, 0.0f, 1};
    };
    music("base", "music_base.wav", 0.35f);
    music("stage1", "music_base.wav", 0.5f);
    music("stage2", "music_base.wav", 0.55f);
    music("boss", "music_intensity.wav", 0.6f);
    music("rest", "music_base.wav", 0.3f);
    auto cue = [&](const char* key, const char* file, float volume, float pitch, float cooldown, int voices = 3) {
        config.cues[key] = {base + file, volume, pitch, 0.03f, cooldown, voices};
    };
    // 戦闘：武器ごとの発射（無い武器は fire を使う）、命中、撃破、爆発、被弾。
    cue("fire", "shot.wav", 0.35f, 1.0f, 0.05f);
    cue("fire_cannon", "shot.wav", 0.35f, 1.0f, 0.05f);
    cue("fire_machineGun", "shot.wav", 0.25f, 1.2f, 0.06f);
    cue("fire_grenade", "shot.wav", 0.45f, 0.65f, 0.1f);
    cue("fire_beam", "shot.wav", 0.4f, 0.8f, 0.06f);
    cue("fire_shotgun", "shot.wav", 0.45f, 0.85f, 0.08f);
    cue("fire_sniper", "shot.wav", 0.55f, 0.6f, 0.1f);
    cue("fire_homing", "shot.wav", 0.3f, 1.1f, 0.06f);
    cue("fire_ricochet", "shot.wav", 0.35f, 1.05f, 0.06f);
    cue("hit", "hit.wav", 0.25f, 1.0f, 0.04f);
    cue("kill", "kill.wav", 0.35f, 1.0f, 0.05f);
    cue("explosion", "armor_break.wav", 0.4f, 0.8f, 0.08f);
    cue("player_hurt", "armor_break.wav", 0.55f, 1.0f, 0.2f, 2);
    // 武器の操作。
    cue("reload", "dash.wav", 0.3f, 1.3f, 0.1f, 2);
    cue("swap", "dash.wav", 0.35f, 0.9f, 0.1f, 2);
    // バリア。
    cue("barrier_on", "upgrade.wav", 0.25f, 0.8f, 0.2f, 2);
    cue("barrier_block", "hit.wav", 0.35f, 0.7f, 0.05f);
    cue("stamina_out", "warning.wav", 0.45f, 1.0f, 0.5f, 1);
    // 拾う。
    cue("pickup", "hit.wav", 0.2f, 1.5f, 0.05f);
    cue("pickup_rare", "upgrade.wav", 0.45f, 1.2f, 0.2f, 2);
    // ステージ2からの敵。
    cue("mine_explode", "armor_break.wav", 0.45f, 0.9f, 0.1f);
    cue("warp", "dash.wav", 0.3f, 0.6f, 0.1f, 2);
    cue("heal", "upgrade.wav", 0.25f, 1.3f, 0.3f, 2);
    // 画面。
    cue("ui_click", "hit.wav", 0.3f, 1.8f, 0.03f, 2);
    cue("ui_drop", "dash.wav", 0.35f, 1.1f, 0.05f, 2);
    cue("sell", "upgrade.wav", 0.35f, 1.4f, 0.05f, 2);
    cue("craft", "upgrade.wav", 0.45f, 1.0f, 0.1f, 2);
    cue("evolve", "upgrade.wav", 0.5f, 0.85f, 0.1f, 2);
    cue("sortie", "upgrade.wav", 0.45f, 0.9f, 0.3f, 1);
    // 流れ。
    cue("stage_clear", "upgrade.wav", 0.5f, 1.0f, 0.5f, 1);
    cue("boss_appear", "warning.wav", 0.55f, 0.7f, 0.5f, 1);
    cue("boss_defeat", "kill.wav", 0.6f, 0.7f, 0.5f, 1);
    return config;
}
} // namespace survivor
