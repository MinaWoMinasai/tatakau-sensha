#pragma once
#include <format>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <span>
#include <cassert>
#include <Xinput.h>
#include "Struct.h"
#include "DirectX/engine/commom/DeveloperTools.h"

namespace cg2 {

/// @brief キーボード・マウス・ゲームパッドの前回と今回の状態を保持する。
class Input {

public:
    // シングルトン
    static Input* GetInstance();

    /// @brief 初期化
    void Initialize(const WNDCLASS& wc, const HWND& hwnd);

    /// @brief 前のデータの保存
    void BeforeFrameData();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    // A validation driver may replace hardware state for this frame only.
    // The next BeforeFrameData restores ordinary polling.
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

private:
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
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

    IDirectInputDevice8* mouse_ = nullptr;
    IDirectInputDevice8* keyboard_ = nullptr;
};

} // namespace cg2
