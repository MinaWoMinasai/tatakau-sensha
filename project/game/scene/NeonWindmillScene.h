#pragma once
#include "IScene.h"
#include "Camera.h"
#include "NeonWindmillMotion.h"
#include "DeveloperTools.h"
#include "game/render/WindmillEmojiRenderer.h"
#include <chrono>
#include <memory>
#include <map>
#include <vector>

namespace cg2 {
class NeonGridRenderer;
class Input;
class TextLabel;
} // namespace cg2
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
class NeonShowcaseCapture;
#endif

/// @brief iPhoneの顔・四方向の指を固定した板ポリゴンに貼り、3D空間でネオン風車を描画する。
class NeonWindmillScene final : public IScene {
public:
    /// @brief 描画資源の未初期化状態を作る。
    NeonWindmillScene();
    /// @brief シーンが借りた既定カメラを復元し、所有する描画資源を解放する。
    ~NeonWindmillScene() override;
    /// @brief カメラ・生成メッシュ・表示ラベルを初期化する。
    void Initialize() override;
    /// @brief 実時間の秒で演出を進め、入力とカメラを反映する。
    void Update() override;
    /// @brief 通常描画口は空にし、HDR描画口にまとめる。
    void Draw() override {}
    /// @brief 不透明面を先に描画し、ネオン線を深度判定付きで重ねる。
    void DrawPostEffect3D() override;
    /// @brief 操作説明を描画し、Developmentでは要求したGPU画像を記録する。
    void DrawSprite() override;
    /// @brief デモの局所的なHDRブルーム・露出・カメラ設定を返す。
    DeveloperShowcaseState GetDeveloperShowcaseState() override;
    /// @brief 生成輪郭の保持と履歴残像の抑制を指示する。
    ScreenEffectState GetScreenEffectState() const override;
    /// @brief 描画に実際に使われたブルーム設定を検証用に保存する。
    void RecordDeveloperPostParameters(const cg2::BloomParam& param) override
    {
        renderedPost_ = param;
    }
    /// @brief 終了はウィンドウまたはEscで行い、他シーンへ自動遷移しない。
    bool IsFinished() const override
    {
        return false;
    }

private:
    /// @brief 注視点を向くカメラを更新する。yawはラジアン、distanceはワールド単位。
    void UpdateCamera();
    /// @brief 深度面と発光線を同一バッファの二つの範囲に生成する。
    void BuildGeometry();
    /// @brief 暗い床・柱・遠方の扉と点光源による床の照り返しを生成する。
    void BuildRoom(bool solid);
    /// @brief ワールドXY面に固定した顔・手の板と、Alpha輪郭に沿ったネオンを生成する。
    void BuildCharacter(bool solid);
    /// @brief 線分をカメラへ向けたHDRネオンとして追加する。
    void Line(cg2::Vector3 a, cg2::Vector3 b, float width, cg2::Vector4 color);
    /// @brief 平面の凸多角形を不透明な色で追加する。
    void Fill(const cg2::Vector3* points, uint32_t count, cg2::Vector4 color);
    /// @brief 時刻とブルーム状態を示すラベルを必要時だけ更新する。
    void UpdateLabels();
    /// @brief 比較モードのみを切り替え、再生時刻・カメラ・姿勢は保持する。
    void SelectMode(WindmillRenderMode mode);
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 同一時刻のブルーム比較・側面・認識姿勢のGPUキャプチャを順番に検証する。
    void UpdateVerification();
    /// @brief 風車だけの発光設定と時刻シークを開発UIへ表示する。
    void DrawSettingsUi();
    /// @brief 同条件モード・寄与画像、連番、GPU計測を既存キャプチャ経路で保存する。
    void UpdateQualityVerification();
#endif
    cg2::Camera camera_;
    cg2::Camera* previousCamera_ = nullptr;
    cg2::Input* input_ = nullptr;
    std::unique_ptr<cg2::NeonGridRenderer> renderer_;
    std::unique_ptr<WindmillEmojiRenderer> emojiRenderer_;
    /// @brief 一文字分のアトラス範囲と、Alphaから抽出した局所輪郭を保持する。
    struct Glyph {
        std::array<float, 4> uv{};
        std::vector<cg2::Vector2> contour;
    };
    std::map<std::string, Glyph> glyphs_;
    std::vector<std::unique_ptr<cg2::TextLabel>> labels_;
    cg2::BloomParam renderedPost_{};
    neonwindmill::Pose pose_{};
    cg2::Vector3 cameraForward_{};
    std::chrono::steady_clock::time_point previousTime_;
    double seconds_ = 0;
    float speed_ = 1, yaw_ = 0.12f, distance_ = 11.5f, bloomGain_ = 0.65f;
    bool paused_ = false, bloomEnabled_ = true, showUi_ = true;
    bool showBackground_ = true;
    WindmillRenderMode renderMode_ = WindmillRenderMode::Legacy;
    std::array<WindmillNeonSettings, 3> appearances_{};
    uint32_t solidEnd_ = 0;
    int lastStatus_ = -1;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    std::unique_ptr<NeonShowcaseCapture> capture_;
    bool verification_ = false, verificationPending_ = false;
    unsigned verificationIndex_ = 0, verificationFrames_ = 0, manualCaptureIndex_ = 0;
    std::vector<std::string> capturedNames_;
    std::vector<std::string> verificationErrors_;
    std::string qualityRun_, qualityOutput_, qualityCaptureName_;
    bool settingsUi_ = false, qualityMeasureStarted_ = false;
#endif
};
