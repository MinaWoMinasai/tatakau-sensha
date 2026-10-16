#pragma once
#include <format>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <span>
#include <vector>
#include <cassert>
#include <Xinput.h>
#include "Struct.h"
#include "DirectX/engine/common/DeveloperTools.h"

namespace myEngine {

/// @brief 最後に操作された入力機器。操作説明の表記と照準の取り方を切り替える。
enum class InputDevice {
    KeyboardMouse,
    Gamepad
};

/// @brief キーボード・マウス・ゲームパッドの前回と今回の状態を保持する。
/// ゲームパッドの操作は、既存のキー・マウスの状態へ変換して合成する（メニュー等の既存処理をそのまま使う）。
class Input {
public:
    // シングルトン
    static Input* GetInstance();

    /// @brief 初期化
    void Initialize(const WNDCLASS& wc, const HWND& hwnd);

    /// @brief 前のデータの保存
    void BeforeFrameData();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    // 検証用ドライバは、このフレームに限りハードウェアの状態を置き換えられる。
    // 次のBeforeFrameDataで通常のポーリングに戻る。
    void OverrideValidationFrame(const Vector2& mousePosition);
#endif
    /// @brief 注目点Changedの通知を受けて、このオブジェクトの状態を反映する。
    void OnFocusChanged(bool active);
    // Retain short key presses that begin and end between two game frames.
    /// @brief キーDownを記録する。
    void RecordKeyDown(unsigned int scanCode, bool repeated);
    /// @brief キーTriggeredであるか判定する。
    bool IsKeyTriggered(uint8_t scanCode) const;

    // キーが押されている状態か
    bool IsPress(const uint8_t key);
    // キーが離されている状態か
    bool IsRelease(const uint8_t key);

    // キーが押された瞬間か
    bool IsTrigger(const uint8_t key, const uint8_t preKey);

    // キーが離された瞬間か
    bool IsMomentRelease(const uint8_t key, const uint8_t prekey);

    /// @brief ゲームパッドボタン押下であるか判定する。
    bool IsGamepadButtonPress(WORD button);
    /// @brief ゲームパッドボタン押下開始であるか判定する。
    bool IsGamepadButtonTrigger(WORD button);
    /// @brief ゲームパッドボタンReleaseであるか判定する。
    bool IsGamepadButtonRelease(WORD button);

    /// @brief キーを返す。
    std::span<const BYTE> GetKey() const
    {
        return key_;
    }

    /// @brief 前処理キーを返す。
    std::span<const BYTE> GetPreKey() const
    {
        return preKey_;
    }

    /// @brief マウス状態を返す。
    DIMOUSESTATE GetMouseState()
    {
        return mouseState_;
    }

    /// @brief 前処理マウス状態を返す。
    DIMOUSESTATE GetPreMouseState()
    {
        return preMouseState_;
    }

    /// @brief 現在ゲームパッド状態を返す。
    XINPUT_STATE GetCurrentGamepadState() const
    {
        return currentGamepadState_;
    }

    /// @brief 左Stickを返す。
    Vector2 GetLeftStick() const;
    /// @brief 右Stickを返す。
    Vector2 GetRightStick() const;

    /// @brief ウィンドウ内のマウス位置を返す。
    Vector2 GetMousePosition() const;

    /// @brief 最後に操作された入力機器を返す。
    InputDevice GetActiveDevice() const
    {
        return activeDevice_;
    }

    /// @brief 最後の操作がゲームパッドであるか。
    bool IsGamepadActive() const
    {
        return activeDevice_ == InputDevice::Gamepad;
    }

    /// @brief ゲームパッドが接続されているか。
    bool IsGamepadConnected() const
    {
        return gamepadConnected_;
    }

    /// @brief 入力機器が切り替わるたびに増える番号。操作説明の文字を作り直す判定に使う。
    uint32_t GetPromptRevision() const
    {
        return promptRevision_;
    }

    /// @brief 戦闘を操作中のフレームで毎回呼ぶ。次のフレームのボタン割り当てを戦闘用にする。
    void RequestGamepadGameplayContext()
    {
        gameplayContextRequested_ = true;
    }

    /// @brief 移動入力（左スティック、戦闘中は十字キーも）。半径方向のデッドゾーン処理済み、長さ0〜1。
    Vector2 GetGamepadMove() const;
    /// @brief 右スティックの照準方向。倒していなければfalse。
    bool TryGetGamepadAim(Vector2& direction) const;
    /// @brief ゲームパッドを振動させる（0〜1、秒）。強い方・長い方を残す。
    void Vibrate(float lowFrequency, float highFrequency, float seconds);

private:
    /// @brief ゲームパッドの操作を、キー・マウスの状態へ変換して合成する。
    void TranslateGamepad();
    /// @brief 振動の残り時間を進め、モーターへ反映する。
    void UpdateVibration();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool validationFrame_ = false;
    Vector2 validationMousePosition_{};
#endif
    HWND hwnd_;

    // キーの配列
    BYTE key_[256] = {};
    BYTE preKey_[256] = {};
    bool pendingKeyPress_[256] = {};
    bool frameKeyPress_[256] = {};

    // マウスの状態を格納する構造体
    DIMOUSESTATE mouseState_;
    DIMOUSESTATE preMouseState_;
    XINPUT_STATE currentGamepadState_{};
    XINPUT_STATE previousGamepadState_{};
    bool gamepadConnected_ = false;
    uint32_t gamepadPollCountdown_ = 0;
    InputDevice activeDevice_ = InputDevice::KeyboardMouse;
    uint32_t promptRevision_ = 0;
    bool gameplayContextRequested_ = false;
    bool gameplayContext_ = false;
    // メニュー操作: 方向の押下状態とオートリピートのタイマー（フレーム単位）。
    uint8_t heldDirections_ = 0;
    uint32_t directionRepeatFrames_ = 0;
    float vibrationLow_ = 0.0f, vibrationHigh_ = 0.0f, vibrationSeconds_ = 0.0f;
    bool vibrationApplied_ = false;
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    // 実機なしでの開発確認用: MYENGINE_FAKE_GAMEPADに、パッド状態を記述したテキストファイルを指定する。
    // 1行1区間で firstFrame endFrame buttons(16進) lx ly rx ry lt rt（スティックは-1..1、トリガーは0..1）。
    struct FakeGamepadSegment {
        uint32_t first = 0, end = 0;
        XINPUT_GAMEPAD pad{};
    };

    std::vector<FakeGamepadSegment> fakeGamepad_;
    bool fakeGamepadLoaded_ = false;
    uint32_t fakeGamepadFrame_ = 0;
    /// @brief MYENGINE_FAKE_GAMEPADで指定したファイルからパッドの状態を読み込む。有効なら true を返す。
    bool PollFakeGamepad();
#endif

    IDirectInputDevice8* mouse_ = nullptr;
    IDirectInputDevice8* keyboard_ = nullptr;
};

} // namespace myEngine
