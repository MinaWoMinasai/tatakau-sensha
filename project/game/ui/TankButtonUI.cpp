#include "TankButtonUI.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>

#include "Object3dCommon.h"
#include "ObjectPostEffect.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {

constexpr float kPi = 3.1415926535f;
constexpr float kTwoPi = kPi * 2.0f;
constexpr const char* kWhiteTexture = "resources/white512x512.png";

Vector4 ReadVector4(const nlohmann::json& value, const Vector4& fallback)
{
	if (!value.is_array() || value.size() < 4) {
		return fallback;
	}
	return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>() };
}

Vector2 ReadVector2(const nlohmann::json& value, const Vector2& fallback)
{
	if (!value.is_array() || value.size() < 2) {
		return fallback;
	}
	return { value[0].get<float>(), value[1].get<float>() };
}

nlohmann::json WriteVector4(const Vector4& value)
{
	return nlohmann::json::array({ value.x, value.y, value.z, value.w });
}

nlohmann::json WriteVector2(const Vector2& value)
{
	return nlohmann::json::array({ value.x, value.y });
}

void ClampStyle(TankButtonUiStyle& style)
{
	style.buttonWidth = (std::clamp)(style.buttonWidth, 80.0f, 320.0f);
	// 全Rankを並べる進化回路のコンパクトカードも、同じネオン戦車描画を再利用する。
	style.buttonHeight = (std::clamp)(style.buttonHeight, 48.0f, 320.0f);
	style.cornerRadius = (std::clamp)(style.cornerRadius, 0.0f, (std::min)(style.buttonWidth, style.buttonHeight) * 0.48f);
	style.borderWidth = (std::clamp)(style.borderWidth, 0.5f, 12.0f);
	style.glowWidth = (std::clamp)(style.glowWidth, style.borderWidth, 40.0f);
	style.glowIntensity = (std::clamp)(style.glowIntensity, 0.0f, 2.0f);
	style.bloomBoost = (std::clamp)(style.bloomBoost, 0.0f, 2.0f);
	style.normalGlowMultiplier = (std::clamp)(style.normalGlowMultiplier, 0.0f, 2.0f);
	style.hoverGlowMultiplier = (std::clamp)(style.hoverGlowMultiplier, 0.0f, 2.0f);
	style.selectedGlowMultiplier = (std::clamp)(style.selectedGlowMultiplier, 0.0f, 2.5f);
	style.lockedGlowMultiplier = (std::clamp)(style.lockedGlowMultiplier, 0.0f, 1.0f);
	style.fillOpacity = (std::clamp)(style.fillOpacity, 0.0f, 1.0f);
	style.iconScale = (std::clamp)(style.iconScale, 0.35f, 2.5f);
	style.labelFontSize = (std::clamp)(style.labelFontSize, 10.0f, 42.0f);
	style.labelOutlineWidth = (std::clamp)(style.labelOutlineWidth, 0.0f, 3.0f);
	style.buttonSpacingX = (std::clamp)(style.buttonSpacingX, 0.0f, 160.0f);
	style.buttonSpacingY = (std::clamp)(style.buttonSpacingY, 0.0f, 160.0f);
	style.previewAreaColumns = (std::clamp)(style.previewAreaColumns, 1, 4);
}

Vector4 MultiplyColor(const Vector4& color, const Vector4& tint)
{
	return { color.x * tint.x, color.y * tint.y, color.z * tint.z, color.w * tint.w };
}

void SetLine(Sprite* sprite, const Vector2& a, const Vector2& b, float width, const Vector4& color)
{
	const float dx = b.x - a.x;
	const float dy = b.y - a.y;
	sprite->SetPosition(a);
	sprite->SetRotation(std::atan2(dy, dx));
	sprite->SetSize({ std::sqrt(dx * dx + dy * dy), (std::max)(0.35f, width) });
	sprite->SetColor(color);
	sprite->Update();
}

float StateGlowMultiplier(TankButtonState state, const TankButtonUiStyle& style)
{
	switch (state) {
	case TankButtonState::Hover:
		return style.hoverGlowMultiplier;
	case TankButtonState::Selected:
		return style.selectedGlowMultiplier;
	case TankButtonState::Locked:
		return style.lockedGlowMultiplier;
	case TankButtonState::Normal:
	default:
		return style.normalGlowMultiplier;
	}
}

} // namespace

bool LoadTankButtonUiStyle(TankButtonUiStyle& style, const std::string& path, std::string* status)
{
	try {
		std::ifstream file(path);
		if (!file.is_open()) {
			if (status) {
				*status = "設定ファイルが見つからないため初期値を使用します。";
			}
			return false;
		}
		nlohmann::json root;
		file >> root;

		const auto& shape = root.value("shape", nlohmann::json::object());
		style.buttonWidth = shape.value("buttonWidth", style.buttonWidth);
		style.buttonHeight = shape.value("buttonHeight", style.buttonHeight);
		style.cornerRadius = shape.value("cornerRadius", style.cornerRadius);
		style.borderWidth = shape.value("borderWidth", style.borderWidth);

		const auto& glow = root.value("glow", nlohmann::json::object());
		style.glowWidth = glow.value("glowWidth", style.glowWidth);
		style.glowIntensity = glow.value("glowIntensity", style.glowIntensity);
		style.bloomBoost = glow.value("bloomBoost", style.bloomBoost);
		style.normalGlowMultiplier = glow.value("normalGlowMultiplier", style.normalGlowMultiplier);
		style.hoverGlowMultiplier = glow.value("hoverGlowMultiplier", style.hoverGlowMultiplier);
		style.selectedGlowMultiplier = glow.value("selectedGlowMultiplier", style.selectedGlowMultiplier);
		style.lockedGlowMultiplier = glow.value("lockedGlowMultiplier", style.lockedGlowMultiplier);

		const auto& colors = root.value("colors", nlohmann::json::object());
		style.fillColor = ReadVector4(colors.value("fillColor", nlohmann::json::array()), style.fillColor);
		style.fillOpacity = colors.value("fillOpacity", style.fillOpacity);
		style.lockedTint = ReadVector4(colors.value("lockedTint", nlohmann::json::array()), style.lockedTint);
		const auto& borderColors = colors.value("borderColorPerRank", nlohmann::json::array());
		const auto& glowColors = colors.value("glowColorPerRank", nlohmann::json::array());
		for (size_t i = 0; i < style.borderColors.size(); ++i) {
			if (i < borderColors.size()) {
				style.borderColors[i] = ReadVector4(borderColors[i], style.borderColors[i]);
			}
			if (i < glowColors.size()) {
				style.glowColors[i] = ReadVector4(glowColors[i], style.glowColors[i]);
			}
		}

		const auto& icon = root.value("tankIcon", nlohmann::json::object());
		style.iconScale = icon.value("iconScale", style.iconScale);
		style.iconOffsetY = icon.value("iconOffsetY", style.iconOffsetY);

		const auto& label = root.value("label", nlohmann::json::object());
		style.labelOffsetY = label.value("labelOffsetY", style.labelOffsetY);
		style.labelFontSize = label.value("labelFontSize", style.labelFontSize);
		style.labelFontFamily = label.value("fontFamily", style.labelFontFamily);
		style.labelFontPath = label.value("fontPath", style.labelFontPath);
		style.labelFontWeight = label.value("fontWeight", style.labelFontWeight);
		style.labelColor = ReadVector4(label.value("labelColor", nlohmann::json::array()), style.labelColor);
		style.labelOutlineColor = ReadVector4(label.value("labelOutlineColor", nlohmann::json::array()), style.labelOutlineColor);
		style.labelOutlineWidth = label.value("labelOutlineWidth", style.labelOutlineWidth);

		const auto& preview = root.value("previewArea", nlohmann::json::object());
		style.buttonSpacingX = preview.value("buttonSpacingX", style.buttonSpacingX);
		style.buttonSpacingY = preview.value("buttonSpacingY", style.buttonSpacingY);
		style.previewAreaOffset = ReadVector2(preview.value("previewAreaOffset", nlohmann::json::array()), style.previewAreaOffset);
		style.previewAreaColumns = preview.value("previewAreaColumns", style.previewAreaColumns);
		ClampStyle(style);
		if (status) {
			*status = "tankButtonUiStyle.json を再読込しました。";
		}
		return true;
	} catch (const std::exception& error) {
		if (status) {
			*status = std::string("読込エラー: ") + error.what();
		}
		return false;
	}
}

bool SaveTankButtonUiStyle(const TankButtonUiStyle& sourceStyle, const std::string& path, std::string* status)
{
	try {
		TankButtonUiStyle style = sourceStyle;
		ClampStyle(style);
		nlohmann::json borderColors = nlohmann::json::array();
		nlohmann::json glowColors = nlohmann::json::array();
		for (size_t i = 0; i < style.borderColors.size(); ++i) {
			borderColors.push_back(WriteVector4(style.borderColors[i]));
			glowColors.push_back(WriteVector4(style.glowColors[i]));
		}
		nlohmann::json root = {
			{ "version", 1 },
			{ "shape", {
				{ "buttonWidth", style.buttonWidth },
				{ "buttonHeight", style.buttonHeight },
				{ "cornerRadius", style.cornerRadius },
				{ "borderWidth", style.borderWidth },
			} },
			{ "glow", {
				{ "glowWidth", style.glowWidth },
				{ "glowIntensity", style.glowIntensity },
				{ "bloomBoost", style.bloomBoost },
				{ "normalGlowMultiplier", style.normalGlowMultiplier },
				{ "hoverGlowMultiplier", style.hoverGlowMultiplier },
				{ "selectedGlowMultiplier", style.selectedGlowMultiplier },
				{ "lockedGlowMultiplier", style.lockedGlowMultiplier },
			} },
			{ "colors", {
				{ "fillColor", WriteVector4(style.fillColor) },
				{ "fillOpacity", style.fillOpacity },
				{ "borderColorPerRank", borderColors },
				{ "glowColorPerRank", glowColors },
				{ "lockedTint", WriteVector4(style.lockedTint) },
			} },
			{ "tankIcon", {
				{ "iconScale", style.iconScale },
				{ "iconOffsetY", style.iconOffsetY },
			} },
			{ "label", {
				{ "labelOffsetY", style.labelOffsetY },
				{ "labelFontSize", style.labelFontSize },
				{ "fontFamily", style.labelFontFamily },
				{ "fontPath", style.labelFontPath },
				{ "fontWeight", style.labelFontWeight },
				{ "labelColor", WriteVector4(style.labelColor) },
				{ "labelOutlineColor", WriteVector4(style.labelOutlineColor) },
				{ "labelOutlineWidth", style.labelOutlineWidth },
			} },
			{ "previewArea", {
				{ "buttonSpacingX", style.buttonSpacingX },
				{ "buttonSpacingY", style.buttonSpacingY },
				{ "previewAreaOffset", WriteVector2(style.previewAreaOffset) },
				{ "previewAreaColumns", style.previewAreaColumns },
			} },
		};

		std::ofstream file(path);
		if (!file.is_open()) {
			if (status) {
				*status = "設定ファイルを開けませんでした。";
			}
			return false;
		}
		file << std::setw(2) << root << '\n';
		if (status) {
			*status = "tankButtonUiStyle.json へ保存しました。";
		}
		return true;
	} catch (const std::exception& error) {
		if (status) {
			*status = std::string("保存エラー: ") + error.what();
		}
		return false;
	}
}

void TankButtonUI::Initialize(SpriteCommon* spriteCommon)
{
	spriteCommon_ = spriteCommon;
	for (auto& sprite : fillSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, kWhiteTexture);
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
	}
	for (auto& layer : frameSprites_) {
		for (auto& sprite : layer) {
			sprite = std::make_unique<Sprite>();
			sprite->Initialize(spriteCommon_, kWhiteTexture);
			sprite->SetAnchorPoint({ 0.0f, 0.5f });
		}
	}
	for (auto& sprite : iconSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, kWhiteTexture);
		sprite->SetAnchorPoint({ 0.0f, 0.5f });
	}
	for (auto& sprite : iconBloomSprites_) {
		sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_, kWhiteTexture);
		sprite->SetAnchorPoint({ 0.0f, 0.5f });
	}
	TextStyle textStyle{};
	textStyle.fontFamily = "Meiryo";
	textStyle.fontSize = 17.0f;
	label_ = std::make_unique<TextLabel>();
	label_->Initialize(spriteCommon_, visualData_.hiraganaName, textStyle);
	label_->SetAnchorPoint({ 0.5f, 0.5f });
}

void TankButtonUI::SetVisualData(const TankButtonVisualData& visualData)
{
	visualData_ = visualData;
	rank_ = visualData.rank;
	if (label_) {
		label_->SetText(visualData_.hiraganaName);
	}
}

void TankButtonUI::SetRank(int rank)
{
	rank_ = (std::clamp)(rank, 1, 4);
}

void TankButtonUI::SetState(TankButtonState state)
{
	state_ = state;
}

void TankButtonUI::Update(const Vector2& center, const TankButtonUiStyle& sourceStyle)
{
	TankButtonUiStyle style = sourceStyle;
	ClampStyle(style);
	UpdateFrame(center, style);
	UpdateIcon(center, style);
	UpdateLabel(center, style);
}

void TankButtonUI::UpdateFrame(const Vector2& center, const TankButtonUiStyle& style)
{
	const float halfW = style.buttonWidth * 0.5f;
	const float halfH = style.buttonHeight * 0.5f;
	const float radius = (std::clamp)(style.cornerRadius, 0.0f, (std::min)(halfW, halfH));
	Vector4 fill = style.fillColor;
	fill.w = style.fillOpacity;
	if (state_ == TankButtonState::Locked) {
		fill = MultiplyColor(fill, style.lockedTint);
		fill.w = (std::min)(1.0f, style.fillOpacity * 1.05f);
	}
	fillSprites_[0]->SetPosition(center);
	fillSprites_[0]->SetSize({ (std::max)(1.0f, style.buttonWidth - radius * 2.0f), style.buttonHeight });
	fillSprites_[0]->SetColor(fill);
	fillSprites_[0]->Update();
	fillSprites_[1]->SetPosition(center);
	fillSprites_[1]->SetSize({ style.buttonWidth, (std::max)(1.0f, style.buttonHeight - radius * 2.0f) });
	fillSprites_[1]->SetColor(fill);
	fillSprites_[1]->Update();

	constexpr int kArcSegments = 16;
	std::array<Vector2, kFrameSegmentCount> points{};
	size_t pointCount = 0;
	const std::array<Vector2, 4> cornerCenters = {{
		{ center.x + halfW - radius, center.y - halfH + radius },
		{ center.x + halfW - radius, center.y + halfH - radius },
		{ center.x - halfW + radius, center.y + halfH - radius },
		{ center.x - halfW + radius, center.y - halfH + radius },
	}};
	const std::array<float, 4> startAngles = {{ -kPi * 0.5f, 0.0f, kPi * 0.5f, kPi }};
	for (size_t cornerIndex = 0; cornerIndex < cornerCenters.size(); ++cornerIndex) {
		for (int segmentIndex = 0; segmentIndex <= kArcSegments; ++segmentIndex) {
			if (pointCount >= points.size()) {
				break;
			}
			const float t = static_cast<float>(segmentIndex) / static_cast<float>(kArcSegments);
			const float angle = startAngles[cornerIndex] + t * kPi * 0.5f;
			points[pointCount++] = {
				cornerCenters[cornerIndex].x + std::cos(angle) * radius,
				cornerCenters[cornerIndex].y + std::sin(angle) * radius,
			};
		}
	}

	const int colorIndex = (std::clamp)(rank_, 1, 4) - 1;
	Vector4 borderColor = style.borderColors[colorIndex];
	Vector4 glowColor = style.glowColors[colorIndex];
	if (state_ == TankButtonState::Locked) {
		borderColor = MultiplyColor(borderColor, style.lockedTint);
		glowColor = MultiplyColor(glowColor, style.lockedTint);
	}
	const float stateGlow = StateGlowMultiplier(state_, style);
	const float glowStrength = style.glowIntensity * stateGlow;
	const float coreStrength = state_ == TankButtonState::Locked
		? 0.34f
		: state_ == TankButtonState::Normal ? 0.74f : state_ == TankButtonState::Hover ? 0.94f : 1.0f;
	const float widthMultiplier = state_ == TankButtonState::Selected ? 1.18f : state_ == TankButtonState::Hover ? 1.08f : 1.0f;
	const std::array<float, kFrameLayerCount> widths = {{
		(std::max)(style.borderWidth, style.glowWidth * 0.18f) * widthMultiplier,
		style.borderWidth,
		style.borderWidth * widthMultiplier,
	}};
	std::array<Vector4, kFrameLayerCount> colors = {{ glowColor, glowColor, borderColor }};
	const float hdrBoost = 1.0f + style.bloomBoost * 2.2f;
	colors[0].x *= hdrBoost;
	colors[0].y *= hdrBoost;
	colors[0].z *= hdrBoost;
	colors[0].w = (std::min)(1.0f, 0.82f * glowStrength);
	colors[1].w = 0.0f;
	colors[2].w = (std::min)(1.0f, borderColor.w * coreStrength);

	for (size_t layerIndex = 0; layerIndex < frameSprites_.size(); ++layerIndex) {
		for (size_t segmentIndex = 0; segmentIndex < frameSprites_[layerIndex].size(); ++segmentIndex) {
			Sprite* sprite = frameSprites_[layerIndex][segmentIndex].get();
			if (segmentIndex < pointCount) {
				SetLine(sprite, points[segmentIndex], points[(segmentIndex + 1) % pointCount], widths[layerIndex], colors[layerIndex]);
			} else {
				sprite->SetSize({ 0.0f, 0.0f });
				sprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
				sprite->Update();
			}
		}
	}
}

void TankButtonUI::UpdateIcon(const Vector2& buttonCenter, const TankButtonUiStyle& style)
{
	struct LineCommand {
		Vector2 a{};
		Vector2 b{};
		float width = 1.0f;
		Vector4 color{};
	};
	std::array<LineCommand, kIconSpriteCount> commands{};
	size_t commandCount = 0;
	auto addLine = [&](const Vector2& a, const Vector2& b, float width, const Vector4& color) {
		if (commandCount < commands.size()) {
			commands[commandCount++] = { a, b, width, color };
		}
	};

	const Vector2 center{ buttonCenter.x, buttonCenter.y + style.iconOffsetY };
	const float iconScale = style.iconScale;
	const float radius = 25.0f * iconScale;
	const float stateAlpha = state_ == TankButtonState::Locked ? 0.34f : state_ == TankButtonState::Normal ? 0.78f : 1.0f;
	Vector4 bodyOutline = visualData_.bodyOutlineColor;
	bodyOutline.w = stateAlpha;
	if (state_ == TankButtonState::Locked) {
		bodyOutline = MultiplyColor(bodyOutline, style.lockedTint);
	}

	// GameScene::queueTankBillboard と同じ、砲身の外周四辺を描く方式。
	const std::array<Vector4, 6> fireGroupColors = {{
		{ 0.55f, 1.00f, 0.35f, 1.0f },
		{ 1.00f, 0.86f, 0.25f, 1.0f },
		{ 0.30f, 0.92f, 1.00f, 1.0f },
		{ 1.00f, 0.36f, 0.72f, 1.0f },
		{ 0.76f, 0.54f, 1.00f, 1.0f },
		{ 1.00f, 0.58f, 0.32f, 1.0f },
	}};
	const float lineWidth = 1.8f * iconScale;
	for (const WeaponMountConfig& mount : visualData_.weaponMounts) {
		const float angle = mount.angleDeg * kPi / 180.0f;
		const Vector2 forward{ std::cos(angle), std::sin(angle) };
		const Vector2 right{ -forward.y, forward.x };
		const Vector2 barrelCenter{
			center.x + mount.offset.x * radius,
			center.y + mount.offset.y * radius,
		};
		float length = (std::max)(radius * 0.28f, mount.scale.x * radius * 0.72f);
		float halfWidth = (std::max)(lineWidth * 1.5f, mount.scale.y * radius * 0.75f);
		if (mount.barrelShape == BarrelShape::Heavy) {
			length *= 1.12f;
			halfWidth *= 1.45f;
		} else if (mount.barrelShape == BarrelShape::Short) {
			length *= 0.58f;
			halfWidth *= 1.08f;
		} else if (mount.barrelShape == BarrelShape::Wide) {
			length *= 0.86f;
			halfWidth *= 1.80f;
		}
		const Vector2 base{ barrelCenter.x - forward.x * length * 0.20f, barrelCenter.y - forward.y * length * 0.20f };
		const Vector2 tip{ barrelCenter.x + forward.x * length * 0.80f, barrelCenter.y + forward.y * length * 0.80f };
		Vector4 barrelColor = mount.fireGroup >= 0
			? fireGroupColors[static_cast<size_t>(mount.fireGroup) % fireGroupColors.size()]
			: mount.outlineColor;
		barrelColor.w = stateAlpha;
		if (state_ == TankButtonState::Locked) {
			barrelColor = MultiplyColor(barrelColor, style.lockedTint);
		}
		const float baseHalf = mount.barrelShape == BarrelShape::Trapezoid ? halfWidth * 1.28f : halfWidth;
		const float tipHalf = mount.barrelShape == BarrelShape::Trapezoid ? halfWidth * 0.72f : halfWidth;
		const Vector2 baseLeft{ base.x - right.x * baseHalf, base.y - right.y * baseHalf };
		const Vector2 tipLeft{ tip.x - right.x * tipHalf, tip.y - right.y * tipHalf };
		const Vector2 tipRight{ tip.x + right.x * tipHalf, tip.y + right.y * tipHalf };
		const Vector2 baseRight{ base.x + right.x * baseHalf, base.y + right.y * baseHalf };
		addLine(baseLeft, tipLeft, lineWidth, barrelColor);
		addLine(tipLeft, tipRight, lineWidth, barrelColor);
		addLine(tipRight, baseRight, lineWidth, barrelColor);
		addLine(baseRight, baseLeft, lineWidth, barrelColor);
	}

	int polygonCount = 28;
	float rotation = 0.0f;
	switch (visualData_.bodyShape) {
	case TankButtonBodyShape::Box:
		polygonCount = 4;
		rotation = kPi * 0.25f;
		break;
	case TankButtonBodyShape::Triangle:
		polygonCount = 3;
		rotation = -kPi * 0.5f;
		break;
	case TankButtonBodyShape::Pentagon:
		polygonCount = 5;
		rotation = -kPi * 0.5f;
		break;
	case TankButtonBodyShape::Circle:
	default:
		break;
	}
	std::array<Vector2, 28> polygon{};
	const float radiusX = radius * (std::clamp)(visualData_.bodyScale.x, 0.45f, 1.8f);
	const float radiusY = radius * (std::clamp)(visualData_.bodyScale.y, 0.45f, 1.8f);
	for (int i = 0; i < polygonCount; ++i) {
		const float angle = rotation + static_cast<float>(i) * kTwoPi / static_cast<float>(polygonCount);
		polygon[i] = { center.x + std::cos(angle) * radiusX, center.y + std::sin(angle) * radiusY };
	}

	for (int i = 0; i < polygonCount; ++i) {
		addLine(polygon[i], polygon[(i + 1) % polygonCount], lineWidth, bodyOutline);
	}

	if (visualData_.usesDrone) {
		for (float side : { -1.0f, 1.0f }) {
			const Vector2 droneCenter{ center.x - radiusX - 11.0f * iconScale, center.y + side * 15.0f * iconScale };
			const float droneRadius = 5.5f * iconScale;
			const std::array<Vector2, 4> dronePoints = {{
				{ droneCenter.x, droneCenter.y - droneRadius },
				{ droneCenter.x + droneRadius, droneCenter.y },
				{ droneCenter.x, droneCenter.y + droneRadius },
				{ droneCenter.x - droneRadius, droneCenter.y },
			}};
			for (int i = 0; i < 4; ++i) {
				addLine(dronePoints[i], dronePoints[(i + 1) % 4], 1.4f * iconScale, bodyOutline);
			}
		}
	}

	for (size_t i = 0; i < iconSprites_.size(); ++i) {
		Sprite* sprite = iconSprites_[i].get();
		Sprite* bloomSprite = iconBloomSprites_[i].get();
		if (i < commandCount) {
			SetLine(sprite, commands[i].a, commands[i].b, commands[i].width, commands[i].color);
			Vector4 bloomColor = commands[i].color;
			const float stateGlow = StateGlowMultiplier(state_, style);
			const float hdrBoost = 1.20f + style.bloomBoost * 2.4f + stateGlow * 0.75f;
			bloomColor.x *= hdrBoost;
			bloomColor.y *= hdrBoost;
			bloomColor.z *= hdrBoost;
			bloomColor.w = (std::clamp)(bloomColor.w * (0.58f + stateGlow), 0.0f, 1.0f);
			SetLine(
				bloomSprite,
				commands[i].a,
				commands[i].b,
				commands[i].width * 1.30f,
				bloomColor);
		} else {
			sprite->SetSize({ 0.0f, 0.0f });
			sprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
			sprite->Update();
			bloomSprite->SetSize({ 0.0f, 0.0f });
			bloomSprite->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
			bloomSprite->Update();
		}
	}
}

void TankButtonUI::UpdateLabel(const Vector2& center, const TankButtonUiStyle& style)
{
	TextStyle textStyle{};
	textStyle.fontFamily = style.labelFontFamily;
	textStyle.fontPath = style.labelFontPath;
	textStyle.fontWeight = style.labelFontWeight;
	textStyle.fontSize = style.labelFontSize;
	textStyle.color = state_ == TankButtonState::Locked
		? MultiplyColor(style.labelColor, style.lockedTint)
		: style.labelColor;
	textStyle.outlineColor = style.labelOutlineColor;
	textStyle.outlineThickness = style.labelOutlineWidth;
	textStyle.padding = 4.0f;
	label_->SetStyle(textStyle);
	label_->SetPosition({ center.x, center.y + style.labelOffsetY });
	label_->SetAnchorPoint({ 0.5f, 0.5f });
}

void TankButtonUI::Draw()
{
	for (const auto& sprite : fillSprites_) {
		if (sprite && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f) {
			sprite->Draw();
		}
	}
	for (const auto& sprite : frameSprites_[2]) {
		if (sprite && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f && sprite->GetColor().w > 0.001f) {
			sprite->Draw();
		}
	}
	for (const auto& sprite : iconSprites_) {
		if (sprite && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f && sprite->GetColor().w > 0.001f) {
			sprite->Draw();
		}
	}
	if (label_) {
		label_->Draw();
	}
}

void TankButtonUI::DrawBloomSource()
{
	for (const auto& sprite : frameSprites_[0]) {
		if (sprite && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f && sprite->GetColor().w > 0.001f) {
			sprite->Draw();
		}
	}
	for (const auto& sprite : iconBloomSprites_) {
		if (sprite && sprite->GetSize().x > 0.0f && sprite->GetSize().y > 0.0f && sprite->GetColor().w > 0.001f) {
			sprite->Draw();
		}
	}
}

void TankButtonGallery::Initialize(
	SpriteCommon* spriteCommon,
	const std::array<TankButtonVisualData, 4>& classVisuals)
{
	spriteCommon_ = spriteCommon;
	LoadTankButtonUiStyle(style_, "resources/configs/tankButtonUiStyle.json", &styleStatus_);
	bloomEffect_ = std::make_unique<ObjectPostEffect>();
	bloomEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		1.0f);
	backdrop_ = std::make_unique<Sprite>();
	backdrop_->Initialize(spriteCommon_, kWhiteTexture);
	backdrop_->SetAnchorPoint({ 0.0f, 0.0f });
	for (auto& button : buttons_) {
		button = std::make_unique<TankButtonUI>();
		button->Initialize(spriteCommon_);
	}
	BuildSamples(classVisuals);

	TextStyle titleStyle{};
	titleStyle.fontFamily = "Meiryo";
	titleStyle.fontSize = 24.0f;
	titleStyle.color = { 0.76f, 1.0f, 0.94f, 1.0f };
	titleStyle.outlineColor = { 0.0f, 0.02f, 0.05f, 0.86f };
	titleStyle.outlineThickness = 0.8f;
	titleStyle.padding = 5.0f;
	titleLabel_ = std::make_unique<TextLabel>();
	titleLabel_->Initialize(spriteCommon_, "TANK BUTTON UI // VISUAL SAMPLES", titleStyle);
	titleLabel_->SetPosition({ 38.0f, 18.0f });

	TextStyle rowStyle = titleStyle;
	rowStyle.fontSize = 16.0f;
	rowStyle.color = { 0.58f, 0.76f, 0.84f, 1.0f };
	rowStyle.outlineThickness = 0.35f;
	const std::array<const char*, 3> rowTexts = {{
		"RANK COLOR\n1 / 2 / 3 / 4",
		"STATE\nN / H / S / L",
		"CLASS SHAPE\n4 TYPES",
	}};
	for (size_t i = 0; i < rowLabels_.size(); ++i) {
		rowLabels_[i] = std::make_unique<TextLabel>();
		rowLabels_[i]->Initialize(spriteCommon_, rowTexts[i], rowStyle);
		rowLabels_[i]->SetAnchorPoint({ 0.0f, 0.5f });
	}
	noteLabel_ = std::make_unique<TextLabel>();
	noteLabel_->Initialize(spriteCommon_, "VIEW ONLY // NO GAMEPLAY INPUT", rowStyle);
	noteLabel_->SetPosition({ 812.0f, 26.0f });
	noteLabel_->SetAnchorPoint({ 1.0f, 0.0f });
	Update();
}

void TankButtonGallery::BuildSamples(const std::array<TankButtonVisualData, 4>& classVisuals)
{
	for (int i = 0; i < 4; ++i) {
		sampleVisuals_[i] = classVisuals[0];
		sampleRanks_[i] = i + 1;
		sampleStates_[i] = TankButtonState::Normal;
	}
	for (int i = 0; i < 4; ++i) {
		sampleVisuals_[i + 4] = classVisuals[1];
		sampleRanks_[i + 4] = 2;
		sampleStates_[i + 4] = static_cast<TankButtonState>(i);
	}
	for (int i = 0; i < 4; ++i) {
		sampleVisuals_[i + 8] = classVisuals[i];
		sampleRanks_[i + 8] = classVisuals[i].rank;
		sampleStates_[i + 8] = TankButtonState::Normal;
	}
	for (size_t i = 0; i < buttons_.size(); ++i) {
		buttons_[i]->SetVisualData(sampleVisuals_[i]);
		buttons_[i]->SetRank(sampleRanks_[i]);
		buttons_[i]->SetState(sampleStates_[i]);
	}
}

void TankButtonGallery::Update()
{
	if (!visible_ || !backdrop_) {
		return;
	}
	ClampStyle(style_);
	if (bloomEffect_) {
		BloomParam bloomParam = bloomEffect_->GetParam();
		bloomParam.threshold = 0.0f;
		bloomParam.intensity = 1.10f + style_.bloomBoost * 2.5f;
		bloomParam.outlineWidth = 0.0f;
		bloomEffect_->SetParam(bloomParam);
		bloomEffect_->Update(0.0f);
	}
	backdrop_->SetPosition({ 0.0f, 0.0f });
	backdrop_->SetSize({
		static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
		static_cast<float>(WinApp::GetInstance()->GetClientHeight()),
	});
	backdrop_->SetColor({ 0.006f, 0.012f, 0.024f, 0.975f });
	backdrop_->Update();

	const int columns = style_.previewAreaColumns;
	const int groupRows = (4 + columns - 1) / columns;
	const float groupHeight = static_cast<float>(groupRows) * style_.buttonHeight +
		static_cast<float>((std::max)(0, groupRows - 1)) * style_.buttonSpacingY;
	for (int group = 0; group < 3; ++group) {
		const float groupTopCenter = style_.previewAreaOffset.y + static_cast<float>(group) * (groupHeight + style_.buttonSpacingY);
		for (int localIndex = 0; localIndex < 4; ++localIndex) {
			const int row = localIndex / columns;
			const int column = localIndex % columns;
			const Vector2 center{
				style_.previewAreaOffset.x + static_cast<float>(column) * (style_.buttonWidth + style_.buttonSpacingX),
				groupTopCenter + static_cast<float>(row) * (style_.buttonHeight + style_.buttonSpacingY),
			};
			buttons_[static_cast<size_t>(group * 4 + localIndex)]->Update(center, style_);
		}
	}
	UpdateLabels();
}

void TankButtonGallery::DrawAfterPostEffects()
{
	if (!visible_) {
		return;
	}
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	if (backdrop_) {
		backdrop_->Draw();
	}
	if (bloomEffect_) {
		bloomEffect_->BeginCapture();
		SpriteCommon::GetInstance()->PreDrawForScene(kNormal);
		for (const auto& button : buttons_) {
			button->DrawBloomSource();
		}
		bloomEffect_->EndCaptureBloomOnlyToBackBuffer();
	}
}

void TankButtonGallery::UpdateLabels()
{
	const int columns = style_.previewAreaColumns;
	const int groupRows = (4 + columns - 1) / columns;
	const float groupHeight = static_cast<float>(groupRows) * style_.buttonHeight +
		static_cast<float>((std::max)(0, groupRows - 1)) * style_.buttonSpacingY;
	for (int group = 0; group < 3; ++group) {
		const float y = style_.previewAreaOffset.y + static_cast<float>(group) * (groupHeight + style_.buttonSpacingY);
		rowLabels_[group]->SetPosition({ 42.0f, y });
	}
}

void TankButtonGallery::Draw()
{
	if (!visible_) {
		return;
	}
	SpriteCommon::GetInstance()->PreDraw(kNormal);
	for (const auto& button : buttons_) {
		button->Draw();
	}
	if (titleLabel_) {
		titleLabel_->Draw();
	}
	for (const auto& label : rowLabels_) {
		if (label) {
			label->Draw();
		}
	}
	if (noteLabel_) {
		noteLabel_->Draw();
	}
}

void TankButtonGallery::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SetNextWindowSize(ImVec2(430.0f, 710.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(840.0f, 8.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Tank Button UI / Visual Samples")) {
		ImGui::End();
		return;
	}
	ImGui::Checkbox("見本画面を表示", &visible_);
	if (ImGui::Button("JSONへ保存")) {
		SaveTankButtonUiStyle(style_, "resources/configs/tankButtonUiStyle.json", &styleStatus_);
	}
	ImGui::SameLine();
	if (ImGui::Button("JSONを再読込")) {
		LoadTankButtonUiStyle(style_, "resources/configs/tankButtonUiStyle.json", &styleStatus_);
	}
	if (!styleStatus_.empty()) {
		ImGui::TextWrapped("%s", styleStatus_.c_str());
	}

	if (ImGui::CollapsingHeader("形状", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("buttonWidth", &style_.buttonWidth, 1.0f, 80.0f, 320.0f);
		ImGui::DragFloat("buttonHeight", &style_.buttonHeight, 1.0f, 80.0f, 320.0f);
		ImGui::DragFloat("cornerRadius", &style_.cornerRadius, 0.5f, 0.0f, 80.0f);
		ImGui::DragFloat("borderWidth", &style_.borderWidth, 0.1f, 0.5f, 12.0f);
	}
	if (ImGui::CollapsingHeader("発光", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("glowWidth", &style_.glowWidth, 0.25f, 0.5f, 40.0f);
		ImGui::DragFloat("glowIntensity", &style_.glowIntensity, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("bloomBoost", &style_.bloomBoost, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("normalGlowMultiplier", &style_.normalGlowMultiplier, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("hoverGlowMultiplier", &style_.hoverGlowMultiplier, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("selectedGlowMultiplier", &style_.selectedGlowMultiplier, 0.01f, 0.0f, 2.5f);
		ImGui::DragFloat("lockedGlowMultiplier", &style_.lockedGlowMultiplier, 0.01f, 0.0f, 1.0f);
	}
	if (ImGui::CollapsingHeader("色")) {
		ImGui::ColorEdit3("fillColor", &style_.fillColor.x);
		ImGui::DragFloat("fillOpacity", &style_.fillOpacity, 0.01f, 0.0f, 1.0f);
		ImGui::ColorEdit4("lockedTint", &style_.lockedTint.x);
		for (int rank = 0; rank < 4; ++rank) {
			ImGui::PushID(rank);
			char borderLabel[32]{};
			char glowLabel[32]{};
			std::snprintf(borderLabel, sizeof(borderLabel), "Rank %d Border", rank + 1);
			std::snprintf(glowLabel, sizeof(glowLabel), "Rank %d Glow", rank + 1);
			ImGui::ColorEdit4(borderLabel, &style_.borderColors[rank].x);
			ImGui::ColorEdit4(glowLabel, &style_.glowColors[rank].x);
			ImGui::PopID();
		}
	}
	if (ImGui::CollapsingHeader("戦車表示")) {
		ImGui::DragFloat("iconScale", &style_.iconScale, 0.01f, 0.35f, 2.5f);
		ImGui::DragFloat("iconOffsetY", &style_.iconOffsetY, 0.5f, -120.0f, 120.0f);
	}
	if (ImGui::CollapsingHeader("文字")) {
		ImGui::DragFloat("labelOffsetY", &style_.labelOffsetY, 0.5f, -120.0f, 140.0f);
		ImGui::DragFloat("labelFontSize", &style_.labelFontSize, 0.5f, 10.0f, 42.0f);
		ImGui::ColorEdit4("labelColor", &style_.labelColor.x);
		ImGui::ColorEdit4("labelOutlineColor", &style_.labelOutlineColor.x);
		ImGui::DragFloat("labelOutlineWidth", &style_.labelOutlineWidth, 0.05f, 0.0f, 3.0f);
	}
	if (ImGui::CollapsingHeader("見本一覧")) {
		ImGui::DragFloat("buttonSpacingX", &style_.buttonSpacingX, 0.5f, 0.0f, 160.0f);
		ImGui::DragFloat("buttonSpacingY", &style_.buttonSpacingY, 0.5f, 0.0f, 160.0f);
		ImGui::DragFloat2("previewAreaOffset", &style_.previewAreaOffset.x, 1.0f, 0.0f, 1600.0f);
		ImGui::DragInt("previewAreaColumns", &style_.previewAreaColumns, 1.0f, 1, 4);
	}
	ClampStyle(style_);
	ImGui::End();
#endif
}
