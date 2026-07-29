#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "Sprite.h"
#include "TextLabel.h"
#include "game/weapon/WeaponMount.h"

class ObjectPostEffect;

enum class TankButtonBodyShape {
	Circle = 0,
	Box,
	Triangle,
	Pentagon,
};

enum class TankButtonState {
	Normal = 0,
	Hover,
	Selected,
	Locked,
};

struct TankButtonVisualData {
	std::string classId = "Basic";
	std::string hiraganaName = "べーしっく";
	int rank = 1;
	TankButtonBodyShape bodyShape = TankButtonBodyShape::Circle;
	Vector2 bodyScale{ 1.0f, 1.0f };
	Vector4 bodyFillColor{ 0.10f, 0.22f, 0.30f, 0.95f };
	Vector4 bodyOutlineColor{ 0.58f, 1.0f, 0.72f, 1.0f };
	std::vector<WeaponMountConfig> weaponMounts;
	bool usesDrone = false;
};

struct TankButtonUiStyle {
	float buttonWidth = 150.0f;
	float buttonHeight = 150.0f;
	float cornerRadius = 16.0f;
	float borderWidth = 1.6f;

	float glowWidth = 10.0f;
	float glowIntensity = 0.78f;
	float bloomBoost = 0.22f;
	float normalGlowMultiplier = 0.42f;
	float hoverGlowMultiplier = 0.78f;
	float selectedGlowMultiplier = 1.15f;
	float lockedGlowMultiplier = 0.10f;

	Vector4 fillColor{ 0.012f, 0.026f, 0.052f, 1.0f };
	float fillOpacity = 0.94f;
	std::array<Vector4, 4> borderColors{
		Vector4{ 0.58f, 1.0f, 0.28f, 1.0f },
		Vector4{ 0.22f, 0.76f, 1.0f, 1.0f },
		Vector4{ 0.76f, 0.36f, 1.0f, 1.0f },
		Vector4{ 1.0f, 0.78f, 0.24f, 1.0f },
	};
	std::array<Vector4, 4> glowColors{
		Vector4{ 0.48f, 1.0f, 0.24f, 1.0f },
		Vector4{ 0.14f, 0.70f, 1.0f, 1.0f },
		Vector4{ 0.70f, 0.24f, 1.0f, 1.0f },
		Vector4{ 1.0f, 0.66f, 0.12f, 1.0f },
	};
	Vector4 lockedTint{ 0.38f, 0.44f, 0.50f, 0.62f };

	float iconScale = 1.0f;
	float iconOffsetY = -22.0f;
	float labelOffsetY = 49.0f;
	float labelFontSize = 17.0f;
	std::string labelFontFamily = "Meiryo";
	std::string labelFontPath;
	int labelFontWeight = 400;
	Vector4 labelColor{ 0.94f, 0.98f, 1.0f, 1.0f };
	Vector4 labelOutlineColor{ 0.0f, 0.02f, 0.05f, 0.82f };
	float labelOutlineWidth = 0.45f;

	float buttonSpacingX = 22.0f;
	float buttonSpacingY = 28.0f;
	Vector2 previewAreaOffset{ 240.0f, 132.0f };
	int previewAreaColumns = 4;
};

bool LoadTankButtonUiStyle(
	TankButtonUiStyle& style,
	const std::string& path = "resources/configs/tankButtonUiStyle.json",
	std::string* status = nullptr);
bool SaveTankButtonUiStyle(
	const TankButtonUiStyle& style,
	const std::string& path = "resources/configs/tankButtonUiStyle.json",
	std::string* status = nullptr);

class TankButtonUI {
public:
	void Initialize(SpriteCommon* spriteCommon);
	void SetVisualData(const TankButtonVisualData& visualData);
	void SetRank(int rank);
	void SetState(TankButtonState state);
	void Update(const Vector2& center, const TankButtonUiStyle& style);
	void Draw();
	void DrawBloomSource();
	TextLabel* GetLabel() const { return label_.get(); }

private:
	static constexpr size_t kFrameSegmentCount = 68;
	static constexpr size_t kFrameLayerCount = 3;
	static constexpr size_t kIconSpriteCount = 96;
	static constexpr size_t kFillSpriteCount = 2;

	void UpdateFrame(const Vector2& center, const TankButtonUiStyle& style);
	void UpdateIcon(const Vector2& center, const TankButtonUiStyle& style);
	void UpdateLabel(const Vector2& center, const TankButtonUiStyle& style);

	SpriteCommon* spriteCommon_ = nullptr;
	TankButtonVisualData visualData_{};
	int rank_ = 1;
	TankButtonState state_ = TankButtonState::Normal;
	std::array<std::unique_ptr<Sprite>, kFillSpriteCount> fillSprites_;
	std::array<std::array<std::unique_ptr<Sprite>, kFrameSegmentCount>, kFrameLayerCount> frameSprites_;
	std::array<std::unique_ptr<Sprite>, kIconSpriteCount> iconSprites_;
	std::array<std::unique_ptr<Sprite>, kIconSpriteCount> iconBloomSprites_;
	std::unique_ptr<TextLabel> label_;
};

class TankButtonGallery {
public:
	void Initialize(
		SpriteCommon* spriteCommon,
		const std::array<TankButtonVisualData, 4>& classVisuals);
	void Update();
	void DrawAfterPostEffects();
	void Draw();
	void DrawImGui();
	void SetVisible(bool visible) { visible_ = visible; }
	bool IsVisible() const { return visible_; }

private:
	static constexpr size_t kSampleCount = 12;
	void BuildSamples(const std::array<TankButtonVisualData, 4>& classVisuals);
	void UpdateLabels();

	SpriteCommon* spriteCommon_ = nullptr;
	bool visible_ = true;
	TankButtonUiStyle style_{};
	std::string styleStatus_;
	std::unique_ptr<Sprite> backdrop_;
	std::unique_ptr<ObjectPostEffect> bloomEffect_;
	std::array<std::unique_ptr<TankButtonUI>, kSampleCount> buttons_;
	std::array<TankButtonVisualData, kSampleCount> sampleVisuals_{};
	std::array<int, kSampleCount> sampleRanks_{};
	std::array<TankButtonState, kSampleCount> sampleStates_{};
	std::unique_ptr<TextLabel> titleLabel_;
	std::array<std::unique_ptr<TextLabel>, 3> rowLabels_;
	std::unique_ptr<TextLabel> noteLabel_;
};
