#include "Input.h"
#include "FixedTimestep.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

namespace myEngine {

Input* Input::GetInstance()
{
    static Input instance;
    return &instance;
}

void Input::Initialize(const WNDCLASS& wc, const HWND& hwnd)
{
    IDirectInput8* directInput = nullptr;
    HRESULT hr = DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
    assert(SUCCEEDED(hr));

    hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
    assert(SUCCEEDED(hr));

    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard); // 標準形式
    assert(SUCCEEDED(hr));

    // 排他制御レベルのセット
    hr = keyboard_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));

    hr = directInput->CreateDevice(GUID_SysMouse, &mouse_, NULL);
    assert(SUCCEEDED(hr));

    hr = mouse_->SetDataFormat(&c_dfDIMouse); // 標準形式
    assert(SUCCEEDED(hr));

    // 排他制御レベルのセット
    hr = mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
    ZeroMemory(&currentGamepadState_, sizeof(XINPUT_STATE));
    ZeroMemory(&previousGamepadState_, sizeof(XINPUT_STATE));

    hwnd_ = hwnd;
}

void Input::BeforeFrameData()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    validationFrame_ = false;
#endif
    memcpy(frameKeyPress_, pendingKeyPress_, sizeof(frameKeyPress_));
    ZeroMemory(pendingKeyPress_, sizeof(pendingKeyPress_));
    // 前のフレームのキー状態を保存
    memcpy(preKey_, key_, sizeof(key_));
    if (keyboard_) {
        HRESULT hr = keyboard_->Acquire();
        hr = keyboard_->GetDeviceState(sizeof(key_), key_);
        if (FAILED(hr)) {
            ZeroMemory(key_, sizeof(key_));
        }
    }

    // マウス情報の取得
    memcpy(&preMouseState_, &mouseState_, sizeof(DIMOUSESTATE));
    if (mouse_) {
        HRESULT hr = mouse_->Acquire();
        hr = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);
        if (FAILED(hr)) {
            ZeroMemory(&mouseState_, sizeof(mouseState_));
        }
    }

    // キーボード・マウスを触ったかは、ゲームパッドの合成より前の実機の状態で判定する。
    bool keyboardMouseActivity = std::abs(mouseState_.lX) + std::abs(mouseState_.lY) > 4;
    for (int i = 0; i < 4 && !keyboardMouseActivity; ++i) {
        keyboardMouseActivity = (mouseState_.rgbButtons[i] & 0x80) != 0;
    }
    for (int i = 0; i < 256 && !keyboardMouseActivity; ++i) {
        keyboardMouseActivity = (key_[i] & 0x80) != 0 || frameKeyPress_[i];
    }

    // ゲームパッドの更新。未接続のポートへの問い合わせは重いので、未接続の間は約1秒ごとに確認する。
    previousGamepadState_ = currentGamepadState_;
    ZeroMemory(&currentGamepadState_, sizeof(XINPUT_STATE));
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (PollFakeGamepad()) {
    } else
#endif
        if (gamepadConnected_ || gamepadPollCountdown_ == 0) {
        gamepadConnected_ = XInputGetState(0, &currentGamepadState_) == ERROR_SUCCESS;
        if (!gamepadConnected_) {
            ZeroMemory(&currentGamepadState_, sizeof(XINPUT_STATE));
            gamepadPollCountdown_ = 60;
        }
    } else {
        --gamepadPollCountdown_;
    }

    const XINPUT_GAMEPAD& pad = currentGamepadState_.Gamepad;
    const XINPUT_GAMEPAD& previousPad = previousGamepadState_.Gamepad;
    const auto stickActive = [](SHORT x, SHORT y, SHORT deadzone) {
        return static_cast<int>(x) * x + static_cast<int>(y) * y > static_cast<int>(deadzone) * deadzone;
    };
    const bool gamepadActivity =
        gamepadConnected_ && ((pad.wButtons & ~previousPad.wButtons) != 0 || pad.bLeftTrigger > 60 || pad.bRightTrigger > 60 ||
                              stickActive(pad.sThumbLX, pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ||
                              stickActive(pad.sThumbRX, pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE));

    InputDevice device = activeDevice_;
    if (!gamepadConnected_) {
        device = InputDevice::KeyboardMouse;
    } else if (gamepadActivity && !keyboardMouseActivity) {
        device = InputDevice::Gamepad;
    } else if (keyboardMouseActivity && !gamepadActivity) {
        device = InputDevice::KeyboardMouse;
    }
    if (device != activeDevice_) {
        activeDevice_ = device;
        ++promptRevision_;
    }

    gameplayContext_ = gameplayContextRequested_;
    gameplayContextRequested_ = false;
    if (gamepadConnected_) {
        TranslateGamepad();
    }
    UpdateVibration();
}

void Input::RepeatFrameData()
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    validationFrame_ = false;
#endif
    ZeroMemory(frameKeyPress_, sizeof(frameKeyPress_));
    memcpy(preKey_, key_, sizeof(key_));
    memcpy(&preMouseState_, &mouseState_, sizeof(DIMOUSESTATE));
    // 相対移動とホイールは1回目のステップで使い切っている。
    mouseState_.lX = 0;
    mouseState_.lY = 0;
    mouseState_.lZ = 0;
    previousGamepadState_ = currentGamepadState_;

    // ゲームパッドの割り当ては、ステップごとの文脈（戦闘中かメニューか）とオートリピートの間隔に合わせて進める。
    gameplayContext_ = gameplayContextRequested_;
    gameplayContextRequested_ = false;
    if (gamepadConnected_) {
        TranslateGamepad();
    }
    UpdateVibration();
}

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
bool Input::PollFakeGamepad()
{
    if (!fakeGamepadLoaded_) {
        fakeGamepadLoaded_ = true;
        char path[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableA("MYENGINE_FAKE_GAMEPAD", path, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            if (FILE* file = nullptr; fopen_s(&file, path, "r") == 0 && file) {
                FakeGamepadSegment segment;
                unsigned buttons = 0;
                float lx, ly, rx, ry, lt, rt;
                while (fscanf_s(file, "%u %u %x %f %f %f %f %f %f", &segment.first, &segment.end, &buttons, &lx, &ly, &rx, &ry, &lt, &rt) ==
                       9) {
                    const auto axis = [](float v) {
                        return static_cast<SHORT>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
                    };
                    segment.pad.wButtons = static_cast<WORD>(buttons);
                    segment.pad.sThumbLX = axis(lx);
                    segment.pad.sThumbLY = axis(ly);
                    segment.pad.sThumbRX = axis(rx);
                    segment.pad.sThumbRY = axis(ry);
                    segment.pad.bLeftTrigger = static_cast<BYTE>(std::clamp(lt, 0.0f, 1.0f) * 255.0f);
                    segment.pad.bRightTrigger = static_cast<BYTE>(std::clamp(rt, 0.0f, 1.0f) * 255.0f);
                    fakeGamepad_.push_back(segment);
                }
                fclose(file);
            }
        }
    }
    if (fakeGamepad_.empty()) {
        return false;
    }
    gamepadConnected_ = true;
    const uint32_t frame = fakeGamepadFrame_++;
    for (const auto& segment : fakeGamepad_) {
        if (frame >= segment.first && frame < segment.end) {
            currentGamepadState_.Gamepad = segment.pad;
        }
    }
    return true;
}
#endif

void Input::TranslateGamepad()
{
    const XINPUT_GAMEPAD& pad = currentGamepadState_.Gamepad;
    const XINPUT_GAMEPAD& previousPad = previousGamepadState_.Gamepad;
    const auto down = [&](WORD button) {
        return (pad.wButtons & button) != 0;
    };
    const auto pressed = [&](WORD button) {
        return (pad.wButtons & button) != 0 && (previousPad.wButtons & button) == 0;
    };
    const auto hold = [&](uint8_t dik) {
        key_[dik] = 0x80;
    };
    const auto tap = [&](uint8_t dik) {
        key_[dik] = 0x80;
        frameKeyPress_[dik] = true;
    };
    const bool rightTrigger = pad.bRightTrigger > 60, leftTrigger = pad.bLeftTrigger > 60;

    // 共通: STARTでポーズ、VIEWで詳細、Yでマップ、Aで決定/スキップ。
    if (pressed(XINPUT_GAMEPAD_START)) {
        tap(DIK_ESCAPE);
    }
    if (pressed(XINPUT_GAMEPAD_BACK)) {
        tap(DIK_TAB);
    }
    if (pressed(XINPUT_GAMEPAD_A)) {
        tap(DIK_RETURN);
    }

    if (gameplayContext_) {
        // ツインスティック戦闘: 左スティックで移動（キーを読むコード向けにWASDとしても扱う）、右スティックで照準、
        // RT / RBで射撃（左クリック）、LT / LBでダッシュ（右クリック）。
        const Vector2 move = GetGamepadMove();
        if (move.x < -0.35f) {
            hold(DIK_A);
        }
        if (move.x > 0.35f) {
            hold(DIK_D);
        }
        if (move.y > 0.35f) {
            hold(DIK_W);
        }
        if (move.y < -0.35f) {
            hold(DIK_S);
        }
        if (rightTrigger || down(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
            mouseState_.rgbButtons[0] = 0x80;
        }
        if (leftTrigger || down(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
            mouseState_.rgbButtons[1] = 0x80;
        }
        if (pressed(XINPUT_GAMEPAD_Y)) {
            tap(DIK_G);
        }
        heldDirections_ = 0;
        return;
    }

    // メニュー: 十字キー/左スティックで選択（オートリピート付きの矢印キー）、Aで決定、Bで戻る、
    // LB / RBでページ切り替え（Q / E）、Xで整備（E）、Yでスキップ（Xキー）、LB長押しでShift（払い戻し）。
    if (pressed(XINPUT_GAMEPAD_B)) {
        tap(DIK_ESCAPE);
    }
    if (pressed(XINPUT_GAMEPAD_X) || pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
        tap(DIK_E);
    }
    if (pressed(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
        tap(DIK_Q);
    }
    if (down(XINPUT_GAMEPAD_LEFT_SHOULDER)) {
        hold(DIK_LSHIFT);
    }
    if (pressed(XINPUT_GAMEPAD_Y)) {
        tap(DIK_X);
    }

    const Vector2 stick = GetLeftStick();
    uint8_t directions = 0;
    if (down(XINPUT_GAMEPAD_DPAD_UP) || stick.y > 0.6f) {
        directions |= 1;
    }
    if (down(XINPUT_GAMEPAD_DPAD_DOWN) || stick.y < -0.6f) {
        directions |= 2;
    }
    if (down(XINPUT_GAMEPAD_DPAD_LEFT) || stick.x < -0.6f) {
        directions |= 4;
    }
    if (down(XINPUT_GAMEPAD_DPAD_RIGHT) || stick.x > 0.6f) {
        directions |= 8;
    }
    const uint8_t newlyPressed = directions & ~heldDirections_;
    bool repeat = false;
    if (directions != 0 && directions == heldDirections_) {
        // 長押しでスクロール: 約0.4秒後に最初のリピート、以降は約0.13秒ごと。
        if (++directionRepeatFrames_ >= 24) {
            repeat = true;
            directionRepeatFrames_ = 16;
        }
    } else {
        directionRepeatFrames_ = 0;
    }
    heldDirections_ = directions;
    const uint8_t fire = repeat ? directions : newlyPressed;
    const uint8_t keys[4] = {DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT};
    for (int i = 0; i < 4; ++i) {
        if (directions & (1 << i)) {
            hold(keys[i]);
        }
        if (fire & (1 << i)) {
            frameKeyPress_[keys[i]] = true;
        }
    }

    // ポインターで操作する画面向けに、右スティックでポインターを動かしRTでクリックする。
    if (rightTrigger) {
        mouseState_.rgbButtons[0] = 0x80;
    }
    const Vector2 pointer = GetRightStick();
    if ((pointer.x != 0.0f || pointer.y != 0.0f) && hwnd_ && GetForegroundWindow() == hwnd_) {
        POINT cursor{};
        RECT client{};
        if (GetCursorPos(&cursor) && GetClientRect(hwnd_, &client)) {
            POINT topLeft{client.left, client.top}, bottomRight{client.right - 1, client.bottom - 1};
            ClientToScreen(hwnd_, &topLeft);
            ClientToScreen(hwnd_, &bottomRight);
            const float speed = 18.0f * static_cast<float>(client.bottom - client.top) / 720.0f;
            const float magnitude = std::sqrt(pointer.x * pointer.x + pointer.y * pointer.y);
            const float curve = magnitude * magnitude; // 中心付近は細かく操作できるようにする
            cursor.x = std::clamp(cursor.x + static_cast<LONG>(pointer.x / magnitude * curve * speed), topLeft.x, bottomRight.x);
            cursor.y = std::clamp(cursor.y - static_cast<LONG>(pointer.y / magnitude * curve * speed), topLeft.y, bottomRight.y);
            SetCursorPos(cursor.x, cursor.y);
        }
    }
}

Vector2 Input::GetGamepadMove() const
{
    if (!gamepadConnected_) {
        return {};
    }
    const XINPUT_GAMEPAD& pad = currentGamepadState_.Gamepad;
    Vector2 move{static_cast<float>(pad.sThumbLX) / 32767.0f, static_cast<float>(pad.sThumbLY) / 32767.0f};
    const float length = std::sqrt(move.x * move.x + move.y * move.y);
    constexpr float kDeadzone = 0.24f;
    if (length <= kDeadzone) {
        move = {};
        if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
            move.x -= 1.0f;
        }
        if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
            move.x += 1.0f;
        }
        if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
            move.y += 1.0f;
        }
        if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
            move.y -= 1.0f;
        }
        const float dpad = std::sqrt(move.x * move.x + move.y * move.y);
        return dpad > 0.0f ? Vector2{move.x / dpad, move.y / dpad} : Vector2{};
    }
    // デッドゾーンを越えた分で正規化し直し、少し傾けたときに飛ばずにゆっくり動き出すようにする。
    const float scaled = (std::min)(1.0f, (length - kDeadzone) / (1.0f - kDeadzone));
    return {move.x / length * scaled, move.y / length * scaled};
}

bool Input::TryGetGamepadAim(Vector2& direction) const
{
    if (!gamepadConnected_) {
        return false;
    }
    const XINPUT_GAMEPAD& pad = currentGamepadState_.Gamepad;
    const float x = static_cast<float>(pad.sThumbRX) / 32767.0f, y = static_cast<float>(pad.sThumbRY) / 32767.0f;
    const float length = std::sqrt(x * x + y * y);
    if (length < 0.3f) {
        return false;
    }
    direction = {x / length, y / length};
    return true;
}

void Input::Vibrate(float lowFrequency, float highFrequency, float seconds)
{
    if (!gamepadConnected_ || activeDevice_ != InputDevice::Gamepad) {
        return;
    }
    vibrationLow_ = (std::max)(vibrationLow_, std::clamp(lowFrequency, 0.0f, 1.0f));
    vibrationHigh_ = (std::max)(vibrationHigh_, std::clamp(highFrequency, 0.0f, 1.0f));
    vibrationSeconds_ = (std::max)(vibrationSeconds_, (std::max)(0.0f, seconds));
}

void Input::UpdateVibration()
{
    if (vibrationSeconds_ > 0.0f && gamepadConnected_) {
        vibrationSeconds_ -= kFixedStepSeconds; // 入力の更新は固定ステップ1回ごとに呼ばれる
        XINPUT_VIBRATION vibration{};
        vibration.wLeftMotorSpeed = static_cast<WORD>(vibrationLow_ * 65535.0f);
        vibration.wRightMotorSpeed = static_cast<WORD>(vibrationHigh_ * 65535.0f);
        XInputSetState(0, &vibration);
        vibrationApplied_ = true;
        return;
    }
    vibrationLow_ = vibrationHigh_ = vibrationSeconds_ = 0.0f;
    if (vibrationApplied_) {
        XINPUT_VIBRATION stop{};
        if (gamepadConnected_) {
            XInputSetState(0, &stop);
        }
        vibrationApplied_ = false;
    }
}

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
void Input::OverrideValidationFrame(const Vector2& mousePosition)
{
    ZeroMemory(key_, sizeof(key_));
    ZeroMemory(preKey_, sizeof(preKey_));
    ZeroMemory(pendingKeyPress_, sizeof(pendingKeyPress_));
    ZeroMemory(frameKeyPress_, sizeof(frameKeyPress_));
    ZeroMemory(&mouseState_, sizeof(mouseState_));
    ZeroMemory(&preMouseState_, sizeof(preMouseState_));
    currentGamepadState_ = {};
    previousGamepadState_ = {};
    validationMousePosition_ = mousePosition;
    validationFrame_ = true;
}
#endif

void Input::OnFocusChanged(bool active)
{
    if (active) {
        if (keyboard_) {
            keyboard_->Acquire();
        }
        if (mouse_) {
            mouse_->Acquire();
        }
    } else {
        if (keyboard_) {
            keyboard_->Unacquire();
        }
        if (mouse_) {
            mouse_->Unacquire();
        }
    }

    ZeroMemory(key_, sizeof(key_));
    ZeroMemory(preKey_, sizeof(preKey_));
    ZeroMemory(pendingKeyPress_, sizeof(pendingKeyPress_));
    ZeroMemory(frameKeyPress_, sizeof(frameKeyPress_));
    ZeroMemory(&mouseState_, sizeof(mouseState_));
    ZeroMemory(&preMouseState_, sizeof(preMouseState_));
    ZeroMemory(&currentGamepadState_, sizeof(currentGamepadState_));
    ZeroMemory(&previousGamepadState_, sizeof(previousGamepadState_));
}

void Input::RecordKeyDown(unsigned int scanCode, bool repeated)
{
    if (!repeated && scanCode < 256) {
        pendingKeyPress_[scanCode] = true;
    }
}

bool Input::IsKeyTriggered(uint8_t scanCode) const
{
    // UIの入力エッジは1つの入力元から取る。ポーリングしたデバイスのエッジと次フレームに届く
    // メッセージを混ぜると、同じメニューが2回切り替わることがある。
    return frameKeyPress_[scanCode];
}

bool Input::IsPress(const uint8_t key)
{
    if (key) {
        return true;
    }
    return false;
}

bool Input::IsRelease(const uint8_t key)
{
    if (!key) {
        return true;
    }
    return false;
}

bool Input::IsTrigger(const uint8_t key, const uint8_t preKey)
{
    if (!preKey && key) {
        return true;
    }
    return false;
}

bool Input::IsMomentRelease(const uint8_t key, const uint8_t prekey)
{
    if (prekey && !key) {
        return true;
    }
    return false;
}

bool Input::IsGamepadButtonPress(WORD button)
{
    return (currentGamepadState_.Gamepad.wButtons & button) != 0;
}

bool Input::IsGamepadButtonTrigger(WORD button)
{
    return !(previousGamepadState_.Gamepad.wButtons & button) && (currentGamepadState_.Gamepad.wButtons & button);
}

bool Input::IsGamepadButtonRelease(WORD button)
{
    return (previousGamepadState_.Gamepad.wButtons & button) && !(currentGamepadState_.Gamepad.wButtons & button);
}

Vector2 Input::GetLeftStick() const
{
    const SHORT deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    SHORT rawX = currentGamepadState_.Gamepad.sThumbLX;
    SHORT rawY = currentGamepadState_.Gamepad.sThumbLY;

    Vector2 result = {0.0f, 0.0f};
    if (abs(rawX) > deadzone) {
        result.x = rawX / 32767.0f;
    }
    if (abs(rawY) > deadzone) {
        result.y = rawY / 32767.0f;
    }
    return result;
}

Vector2 Input::GetRightStick() const
{
    const SHORT deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
    SHORT rawX = currentGamepadState_.Gamepad.sThumbRX;
    SHORT rawY = currentGamepadState_.Gamepad.sThumbRY;

    Vector2 result = {0.0f, 0.0f};
    if (abs(rawX) > deadzone) {
        result.x = rawX / 32767.0f;
    }
    if (abs(rawY) > deadzone) {
        result.y = rawY / 32767.0f;
    }
    return result;
}

Vector2 Input::GetMousePosition() const
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (validationFrame_) {
        return validationMousePosition_;
    }
#endif
    POINT point;
    GetCursorPos(&point);          // デスクトップ上のマウス座標を取得
    ScreenToClient(hwnd_, &point); // ウィンドウ座標に変換
    // UIの当たり判定は論理座標で書かれているため、実ウィンドウサイズから換算する。
    Vector2 position{static_cast<float>(point.x), static_cast<float>(point.y)};
    WinApp::GetInstance()->ClientToLogical(position.x, position.y);
    return position;
}

} // namespace myEngine
