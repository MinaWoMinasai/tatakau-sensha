#include "Input.h"
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

void Input::Initialize(const WNDCLASS& wc, const HWND& hwnd)
{
	IDirectInput8* directInput = nullptr;
	HRESULT hr = DirectInput8Create(
		wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		(void**)&directInput, nullptr
	);
	assert(SUCCEEDED(hr));
	
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
	assert(SUCCEEDED(hr));

	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = keyboard_->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));
	
	hr = directInput->CreateDevice(GUID_SysMouse, &mouse_, NULL);
	assert(SUCCEEDED(hr));

	hr = mouse_->SetDataFormat(&c_dfDIMouse); // 標準形式
	assert(SUCCEEDED(hr));

	// 排他制御レベルのセット
	hr = mouse_->SetCooperativeLevel(
		hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));
	ZeroMemory(&currentGamepadState_, sizeof(XINPUT_STATE));
	ZeroMemory(&previousGamepadState_, sizeof(XINPUT_STATE));
}

void Input::BeforeFrameData()
{
	// 前のフレームのキー状態を保存
	memcpy(preKey_, key_, sizeof(key_));
	keyboard_->Acquire();
	keyboard_->GetDeviceState(sizeof(key_), key_);

	// マウス情報の取得
	memcpy(&preMouseState_, &mouseState_, sizeof(DIMOUSESTATE));
	mouse_->Acquire();
	mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);

	// ゲームパッドの更新
	previousGamepadState_ = currentGamepadState_;
	ZeroMemory(&currentGamepadState_, sizeof(XINPUT_STATE));
	XInputGetState(0, &currentGamepadState_);
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
bool Input::IsGamepadButtonPress(WORD button) {
	return (currentGamepadState_.Gamepad.wButtons & button) != 0;
}

bool Input::IsGamepadButtonTrigger(WORD button) {
	return !(previousGamepadState_.Gamepad.wButtons & button) &&
		(currentGamepadState_.Gamepad.wButtons & button);
}

bool Input::IsGamepadButtonRelease(WORD button) {
	return (previousGamepadState_.Gamepad.wButtons & button) &&
		!(currentGamepadState_.Gamepad.wButtons & button);
}

Vector2 Input::GetLeftStick() const {
	const SHORT deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
	SHORT rawX = currentGamepadState_.Gamepad.sThumbLX;
	SHORT rawY = currentGamepadState_.Gamepad.sThumbLY;

	Vector2 result = { 0.0f, 0.0f };
	if (abs(rawX) > deadzone) result.x = rawX / 32767.0f;
	if (abs(rawY) > deadzone) result.y = rawY / 32767.0f;
	return result;
}

Vector2 Input::GetRightStick() const {
	const SHORT deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
	SHORT rawX = currentGamepadState_.Gamepad.sThumbRX;
	SHORT rawY = currentGamepadState_.Gamepad.sThumbRY;

	Vector2 result = { 0.0f, 0.0f };
	if (abs(rawX) > deadzone) result.x = rawX / 32767.0f;
	if (abs(rawY) > deadzone) result.y = rawY / 32767.0f;
	return result;
}