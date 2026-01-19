#pragma once
#include <format>
#include <dinput.h>
#include <span>
#include <cassert>
#include <Xinput.h>
#include "Struct.h"

class Input {

public:

	// シングルトン
	static Input* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(const WNDCLASS &wc, const HWND &hwnd);

	/// <summary>
	/// 前のデータの保存
	/// </summary>
	void BeforeFrameData();

	// キーが押されている状態か
	bool IsPress(const uint8_t key);
	// キーが離されている状態か
	bool IsRelease(const uint8_t key);

	// キーが押された瞬間か
	bool IsTrigger(const uint8_t key, const uint8_t preKey);

	// キーが離された瞬間か
	bool IsMomentRelease(const uint8_t key, const uint8_t prekey);
	
	bool IsGamepadButtonPress(WORD button);
	bool IsGamepadButtonTrigger(WORD button);
	bool IsGamepadButtonRelease(WORD button);

	std::span<const BYTE> GetKey() const { return key_; }
	std::span<const BYTE> GetPreKey() const { return preKey_; }
	DIMOUSESTATE GetMouseState() { return mouseState_; }
	DIMOUSESTATE GetPreMouseState() { return preMouseState_; }
	XINPUT_STATE GetCurrentGamepadState() const { return currentGamepadState_; }
	Vector2 GetLeftStick() const;
	Vector2 GetRightStick() const;

	Vector2 GetMousePosition() const;
private:
	HWND hwnd_;
	
	// キーの配列
	BYTE key_[256] = {};
	BYTE preKey_[256] = {};

	// マウスの状態を格納する構造体
	DIMOUSESTATE mouseState_;
	DIMOUSESTATE preMouseState_;
	XINPUT_STATE currentGamepadState_{};
	XINPUT_STATE previousGamepadState_{};

	IDirectInputDevice8* mouse_ = nullptr;
	IDirectInputDevice8* keyboard_ = nullptr;
};
