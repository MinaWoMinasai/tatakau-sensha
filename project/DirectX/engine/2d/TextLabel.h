#pragma once
#include <memory>
#include <string>
#include "Sprite.h"
#include "TextRenderer.h"

class TextLabel {
public:
	void Initialize(SpriteCommon* spriteCommon, const std::string& text, const TextStyle& style = {});
	bool InitializeFromJson(SpriteCommon* spriteCommon, const std::string& configPath, const std::string& labelId);
	void SetText(const std::string& text);
	void SetStyle(const TextStyle& style);
	void SetPosition(const Vector2& position);
	void SetAnchorPoint(const Vector2& anchorPoint);
	void SetAlpha(float alpha);
	void Draw();

	const std::string& GetText() const { return text_; }
	const TextStyle& GetStyle() const { return style_; }
	Sprite* GetSprite() const { return sprite_.get(); }

private:
	void RebuildTexture();
	bool IsSameStyle(const TextStyle& style) const;

	SpriteCommon* spriteCommon_ = nullptr;
	std::unique_ptr<Sprite> sprite_;
	std::string text_;
	TextStyle style_{};
	Vector2 position_ = { 0.0f, 0.0f };
	Vector2 anchorPoint_ = { 0.0f, 0.0f };
	float alpha_ = 1.0f;
};
