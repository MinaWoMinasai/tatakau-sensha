#include "TextLabel.h"
#include <fstream>
#include <nlohmann/json.hpp>
#if defined(USE_IMGUI) && !defined(NDEBUG)
#include <chrono>
#endif
namespace cg2 {


#if defined(USE_IMGUI) && !defined(NDEBUG)
TextLabel::ProfileStats TextLabel::profileStats_{};

void TextLabel::ResetProfileStats()
{
	profileStats_ = {};
}

const TextLabel::ProfileStats& TextLabel::GetProfileStats()
{
	return profileStats_;
}
#endif

namespace {

/// @brief JSON配列から4成分を読む。要素数が不足する場合はfallbackを返す。
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

/// @brief ベクトル2を読み取る。
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
	style.fontPath = label.value("fontPath", style.fontPath);
	style.fontWeight = label.value("fontWeight", style.fontWeight);
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
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto setTextStart = std::chrono::steady_clock::now();
#endif
	if (text_ == text) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
		profileStats_.setTextCpuMs += std::chrono::duration<float, std::milli>(
			std::chrono::steady_clock::now() - setTextStart).count();
#endif
		return;
	}
	text_ = text;
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto rebuildStart = std::chrono::steady_clock::now();
#endif
	RebuildTexture();
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto setTextEnd = std::chrono::steady_clock::now();
	profileStats_.setTextRebuildCpuMs += std::chrono::duration<float, std::milli>(
		setTextEnd - rebuildStart).count();
	profileStats_.setTextCpuMs += std::chrono::duration<float, std::milli>(
		setTextEnd - setTextStart).count();
#endif
}

void TextLabel::SetStyle(const TextStyle& style)
{
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto setStyleStart = std::chrono::steady_clock::now();
#endif
	if (IsSameStyle(style)) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
		profileStats_.setStyleCpuMs += std::chrono::duration<float, std::milli>(
			std::chrono::steady_clock::now() - setStyleStart).count();
#endif
		return;
	}
	style_ = style;
	RebuildTexture();
#if defined(USE_IMGUI) && !defined(NDEBUG)
	profileStats_.setStyleCpuMs += std::chrono::duration<float, std::milli>(
		std::chrono::steady_clock::now() - setStyleStart).count();
#endif
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
	PrepareForDraw();
	if (!sprite_) {
		return;
	}
	sprite_->Update();
	sprite_->Draw();
}

void TextLabel::PrepareForDraw()
{
	if (builtFontRevision_ != TextRenderer::GetInstance()->GetFontRevision()) {
		RebuildTexture();
	}
}

void TextLabel::RebuildTexture()
{
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto rebuildTextureStart = std::chrono::steady_clock::now();
#endif
	if (!spriteCommon_) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
		profileStats_.rebuildTextureCpuMs += std::chrono::duration<float, std::milli>(
			std::chrono::steady_clock::now() - rebuildTextureStart).count();
#endif
		return;
	}

	TextRenderer* textRenderer = TextRenderer::GetInstance();
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto getOrCreateTextureStart = std::chrono::steady_clock::now();
#endif
	texturePath_ = textRenderer->GetOrCreateTexture(text_, style_);
#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto getOrCreateTextureEnd = std::chrono::steady_clock::now();
	profileStats_.getOrCreateTextureCpuMs += std::chrono::duration<float, std::milli>(
		getOrCreateTextureEnd - getOrCreateTextureStart).count();
	const TextRenderer::GetOrCreateTextureProfile& textureProfile =
		textRenderer->GetLastGetOrCreateTextureProfile();
	profileStats_.cacheFileExistedCount += textureProfile.cacheFileExisted ? 1u : 0u;
	profileStats_.generatedPngCount += textureProfile.generatedPng ? 1u : 0u;
#endif
	builtFontRevision_ = textRenderer->GetFontRevision();
	if (!sprite_) {
		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize(spriteCommon_, texturePath_);
		sprite_->SetPosition(position_);
		sprite_->SetAnchorPoint(anchorPoint_);
		sprite_->SetAlpha(alpha_);
#if defined(USE_IMGUI) && !defined(NDEBUG)
		profileStats_.rebuildTextureCpuMs += std::chrono::duration<float, std::milli>(
			std::chrono::steady_clock::now() - rebuildTextureStart).count();
#endif
		return;
	}

#if defined(USE_IMGUI) && !defined(NDEBUG)
	const auto spriteSetTextureStart = std::chrono::steady_clock::now();
#endif
	sprite_->SetTexture(texturePath_);
#if defined(USE_IMGUI) && !defined(NDEBUG)
	profileStats_.spriteSetTextureCpuMs += std::chrono::duration<float, std::milli>(
		std::chrono::steady_clock::now() - spriteSetTextureStart).count();
#endif
	sprite_->SetPosition(position_);
	sprite_->SetAnchorPoint(anchorPoint_);
	sprite_->SetAlpha(alpha_);
#if defined(USE_IMGUI) && !defined(NDEBUG)
	profileStats_.rebuildTextureCpuMs += std::chrono::duration<float, std::milli>(
		std::chrono::steady_clock::now() - rebuildTextureStart).count();
#endif
}

bool TextLabel::IsSameStyle(const TextStyle& style) const
{
	return style_.fontFamily == style.fontFamily &&
		style_.fontPath == style.fontPath &&
		style_.fontWeight == style.fontWeight &&
		style_.fontSize == style.fontSize &&
		style_.color.x == style.color.x &&
		style_.color.y == style.color.y &&
		style_.color.z == style.color.z &&
		style_.color.w == style.color.w &&
		style_.outlineColor.x == style.outlineColor.x &&
		style_.outlineColor.y == style.outlineColor.y &&
		style_.outlineColor.z == style.outlineColor.z &&
		style_.outlineColor.w == style.outlineColor.w &&
		style_.outlineThickness == style.outlineThickness &&
		style_.padding == style.padding &&
		style_.preserveOutline == style.preserveOutline;
}

} // namespace cg2
