#pragma once
#include "Struct.h"

namespace myEngine {

class Camera;

/// @brief 開発者用ショーケース表示の状態。
struct DeveloperShowcaseState {
    bool active = false;
    Camera* camera = nullptr;
    int diagnostic = 0;
    float threshold = 0.65f, intensity = 0.75f, exposure = 0.75f;
    // -1ならエンジンの設定を保つ。0/1/2/3でOff/Legacy/Quality/Lightを選ぶ。
    int bloomComparisonMode = -1;
    float bloomSoftKnee = 0.5f, bloomScatter = 0.3f, bloomRadius = 0.75f, bloomGain = 0.15f;
    int toneMappingMode = -1;
    bool comparisonFreeze = false;
    // 実際のゲームプレイ向けの範囲を限ったDeveloper用診断（Showcaseは有効にしない）。
    int renderDebugOverride = -1;
    int ssaoOverride = -1, ssrOverride = -1;
};

/// @brief 一時的な画面効果の強度と継続時間を表す。
struct PostEffectPulse {
    float bloomBoost = 0.0f;
    float chromAbAmount = 0.0f;
    Vector2 center = {0.5f, 0.5f};
    float radius = 0.0f;
    float width = 0.05f;
    float strength = 0.0f;
};

/// @brief 現在の画面効果の合成結果を描画側へ渡す。
struct ScreenEffectState {
    bool active = false;
    float bloomScale = 1.0f;
    bool suppressPostEffectDebugUi = false;
    // 生成したスクリーン空間のエッジを抑え、作成したネオンの形状は残す。
    bool suppressOutlines = false;
    // 既存のテンポラル経路では、動くスキン形状にオブジェクトごとの速度が無い。
    // シーンは範囲を限って、安定したジッターなしの描画設定を使える。
    bool suppressTemporal = false;
    BloomParam param{};
};

/// @brief 水面に関連する画面効果の設定を表す。
struct WaterPostProcessSettings {
    bool diagnosticsEnabled = false;
    bool taaEnabled = true;
    bool bloomEnabled = true;
    float historyWeight = 0.08f;
    int debugMode = 0;
};

/// @brief ベクター管ディスプレイ風のアリーナ表示（ブラウン管の歪み・残光・星雲）を表す。
/// enabled=falseのシーンでは従来の後処理値をそのまま使う。
struct ArenaLookSettings {
    bool enabled = false;
    float curvature = 0.0f;   // 樽型歪み
    float bezelMargin = 0.0f; // 角丸画面の外側に残す黒枠（UV比）
    float bezelRadius = 0.0f; // 角丸半径（UV比）
    float bezelSoftness = 0.0f;
    float vignetteIntensity = 0.0f;
    float vignetteScale = 0.0f;
    float chromAbAmount = 0.0f;
    float scanlineIntensity = 0.0f;
    float scanlinePeriodPixels = 3.0f; // 内部描画解像度での走査線の周期
    float noiseIntensity = 0.0f;
    float grainIntensity = 0.0f;
    float phosphorDecay = 0.0f;
    float nebulaIntensity = 0.0f;
    float nebulaScale = 0.035f;
    float nebulaDrift = 0.004f;
    Vector3 nebulaColorA{0.30f, 0.05f, 0.42f};
    Vector3 nebulaColorB{0.06f, 0.02f, 0.30f};
    // 残光を出さない円（画面のUV、半径は画面の高さに対する比）。半径0で無効。
    // 自機に付いて動くゲージのように、カメラと一緒に動くものの残光だけを消したいときに使う。
    Vector2 persistenceMaskCenter{0.5f, 0.5f};
    float persistenceMaskRadius = 0.0f;
};

/// @brief ブルームの説明用の表示切り替え。view: 0=通常, 1=OFF, 2=ピラミッドの1段だけ, 3=光の成分だけ。
struct BloomViewSettings {
    int view = 0;
    int level = 0; // view==2 のときの段（0=1/2解像度 … 4=1/32解像度）
};

/// @brief Bloom::Updateへ毎フレーム渡す、現在のシーンが求める描画設定。
struct BloomFrameSettings {
    WaterPostProcessSettings water{};
    ArenaLookSettings arenaLook{};
    BloomViewSettings bloomView{};
    // 画面効果の時間を進める量（秒）。
    float deltaTime = 1.0f / 60.0f;
};

} // namespace myEngine
