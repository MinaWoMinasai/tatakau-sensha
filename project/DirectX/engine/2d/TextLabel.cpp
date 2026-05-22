#include "TextLabel.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace {

Vector4 ReadVector4(const nlohmann::json& json, const Vector4& fallback)
{
	if (!json.is_array() || json.size() < 4) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>(),
		json[3].get<float>()
	};
}

Vector2 ReadVector2(const nlohmann::json& json, const Vector2& fallback)
{
	if (!json.is_array() || json.size() < 2) {
		return fallback;
	}
	return {
		json[0].get<float>(),
		json[1].get<float>()
	};
}

} // namespace

void TextLabel::Initialize(SpriteCommon* spriteCommon, const std::string& text, const TextStyle& style)
{
	spriteCommon_ = spriteCommon;
	text_ = text;
	style_ = style;
	RebuildTexture();
}

bool TextLabel::InitializeFromJson(SpriteCommon* spriteCommon, const std::string& configPath, const std::string& labelId)
{
	std::ifstream file(configPath);
	if (!file.is_open()) {
		return false;
	}

	nlohmann::json root{};
	file >> root;
	if (!root.contains("labels") || !root["labels"].contains(labelId)) {
		return false;
	}

	const nlohmann::json& label = root["labels"][labelId];
	TextStyle style{};
	style.fontFamily = label.value("fontFamily", style.fontFamily);
	style.fontSize = label.value("fontSize", style.fontSize);
	style.color = ReadVector4(label.value("color", nlohmann::json::array()), style.color);
	style.outlineColor = ReadVector4(label.value("outlineColor", nlohmann::json::array()), style.outlineColor);
	style.outlineThickness = label.value("outlineThickness", style.outlineThickness);
	style.padding = label.value("padding", style.padding);

	Initialize(spriteCommon, label.value("text", std::string{}), style);
	SetPosition(ReadVector2(label.value("position", nlohmann::json::array()), position_));
	SetAnchorPoint(ReadVector2(label.value("anchorPoint", nlohmann::json::array()), anchorPoint_));
	SetAlpha(label.value("alpha", alpha_));
	return true;
}

void TextLabel::SetText(const std::string& text)
{
	if (text_ == text) {
		return;
	}
	text_ = text;
	RebuildTexture();
}

void TextLabel::SetStyle(const TextStyle& style)
{
	style_ = style;
	RebuildTexture();
}

void TextLabel::SetPosition(const Vector2& position)
{
	position_ = position;
	if (sprite_) {
		sprite_->SetPosition(position_);
	}
}

void TextLabel::SetAnchorPoint(const Vector2& anchorPoint)
{
	anchorPoint_ = anchorPoint;
	if (sprite_) {
		sprite_->SetAnchorPoint(anchorPoint_);
	}
}

void TextLabel::SetAlpha(float alpha)
{
	alpha_ = alpha;
	if (sprite_) {
		sprite_->SetAlpha(alpha_);
	}
}

void TextLabel::Draw()
{
	if (!sprite_) {
		return;
	}
	sprite_->Update();
	sprite_->Draw();
}

void TextLabel::RebuildTexture()
{
	if (!spriteCommon_) {
		return;
	}

	const std::string texturePath = TextRenderer::GetInstance()->GetOrCreateTexture(text_, style_);
	if (!sprite_) {
		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize(spriteCommon_, texturePath);
		sprite_->SetPosition(position_);
		sprite_->SetAnchorPoint(anchorPoint_);
		sprite_->SetAlpha(alpha_);
		return;
	}

	sprite_->SetTexture(texturePath);
	sprite_->SetPosition(position_);
	sprite_->SetAnchorPoint(anchorPoint_);
	sprite_->SetAlpha(alpha_);
}
