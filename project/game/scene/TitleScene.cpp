#include "TitleScene.h"
#include "GameStartMode.h"
#include "SceneManager.h"

#include <algorithm>
#include <numbers>

namespace {

Sprite* GetLogoSprite(TitleScene::LogoChar& logo)
{
	if (logo.label) {
		logo.label->PrepareForDraw();
		return logo.label->GetSprite();
	}
	return logo.sprite.get();
}

Vector2 FitTextLabel(TextLabel& label, const Vector2& bounds)
{
	label.PrepareForDraw();
	Sprite* sprite = label.GetSprite();
	if (!sprite) {
		return bounds;
	}
	const Vector2 naturalSize = sprite->GetSize();
	const float scale = (std::min)(
		bounds.x / (std::max)(1.0f, naturalSize.x),
		bounds.y / (std::max)(1.0f, naturalSize.y));
	const Vector2 fittedSize = {
		naturalSize.x * scale,
		naturalSize.y * scale
	};
	sprite->SetSize(fittedSize);
	return fittedSize;
}

} // namespace

void TitleScene::Initialize() {

	input_ = Input::GetInstance();

	camera = std::make_unique<Camera>();
	camera->SetTranslate(Vector3(17.0f, 21.0f, -80.0f));

	fade_ = std::make_unique<Fade>();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, 1.0f);

	const float screenW = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
	const float screenH = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

	const int charCount = 8;

	// 文字サイズ（仮に統一）
	const Vector2 charSize = { 96.0f, 96.0f };

	// 横幅計算 → 中央寄せ
	const float spacing = charSize.x * 0.9f;
	const float totalWidth = spacing * (charCount - 1);
	const float baseX = screenW * 0.5f - totalWidth * 0.5f;

	// Y位置
	float startY = -charSize.y - 50.0f;          // 画面外
	const float targetY = screenH * 0.35f;              // 画面中央より少し上

	std::vector<std::string> characters = {
		"た", "た", "か", "う", "せ", "ん", "し", "ゃ"
	};
	TextStyle titleStyle{};
	titleStyle.fontFamily = "Zen Maru Gothic";
	titleStyle.fontPath = "resources/fonts/ZenMaruGothic-Bold.ttf";
	titleStyle.fontWeight = 700;
	titleStyle.fontSize = 88.0f;
	titleStyle.color = { 0.94f, 1.0f, 0.97f, 1.0f };
	titleStyle.outlineColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	titleStyle.outlineThickness = 0.0f;
	titleStyle.padding = 14.0f;

	for (int i = 0; i < charCount; ++i) {

		LogoChar c{};
		c.label = std::make_unique<TextLabel>();
		c.label->Initialize(SpriteCommon::GetInstance(), characters[i], titleStyle);
		c.label->SetAnchorPoint({ 0.5f, 0.5f });

		// 元の1文字96px枠に収め、既存の落下・拡縮演出をそのまま適用する。
		c.baseSize = FitTextLabel(*c.label, charSize);

		// Xは等間隔 + 微ランダム
		float x = baseX + spacing * i + Rand(-6.0f, 6.0f);

		// 開始位置は少し高さをバラす
		c.startPos = {
			x,
			startY + Rand(-50.0f, 50.0f)
		};

		c.targetPos = {
			x,
			targetY + Rand(-8.0f, 8.0f)
		};

		c.label->SetPosition(c.startPos);

		// 落下スピードに個性を出す
		c.fallSpeed = Rand(550.0f, 750.0f);

		// タイミングずらし（少しランダム）
		c.delay = i * 0.12f + Rand(0.0f, 0.05f);

		c.timer = 0.0f;
		c.landed = false;

		logoChars.push_back(std::move(c));
	}

	// ロゴの下へ、通常プレイとチュートリアルの2項目を並べる。
	startY = screenH * 0.67f;

	TextStyle startStyle = titleStyle;
	startStyle.fontSize = 40.0f;
	startStyle.color = { 0.90f, 1.0f, 0.96f, 1.0f };
	startStyle.padding = 12.0f;
	startLogo.label = std::make_unique<TextLabel>();
	startLogo.label->Initialize(
		SpriteCommon::GetInstance(),
		"PLAY",
		startStyle);
	startLogo.label->SetAnchorPoint({ 0.5f, 0.5f });

	// 従来のstart.pngと同じ表示枠へ収める。
	startLogo.baseSize = FitTextLabel(*startLogo.label, { 280.0f, 58.0f });

	// 位置
	startLogo.startPos = { screenW * 0.5f, -100.0f };
	startLogo.targetPos = { screenW * 0.5f, startY };
	startLogo.label->SetPosition(startLogo.startPos);

	// ロゴより少し遅れて落ちる
	startLogo.delay = 8 * 0.12f + 0.2f;
	startLogo.fallSpeed = 700.0f;

	startLogo.timer = 0.0f;
	startLogo.landed = false;

	tutorialLogo.label = std::make_unique<TextLabel>();
	tutorialLogo.label->Initialize(
		SpriteCommon::GetInstance(),
		"TUTORIAL",
		startStyle);
	tutorialLogo.label->SetAnchorPoint({ 0.5f, 0.5f });
	tutorialLogo.baseSize = FitTextLabel(*tutorialLogo.label, { 280.0f, 58.0f });
	tutorialLogo.startPos = { screenW * 0.5f, -100.0f };
	tutorialLogo.targetPos = { screenW * 0.5f, startY + 68.0f };
	tutorialLogo.label->SetPosition(tutorialLogo.startPos);
	tutorialLogo.delay = 9 * 0.12f + 0.2f;
	tutorialLogo.fallSpeed = 700.0f;
	tutorialLogo.timer = 0.0f;
	tutorialLogo.landed = false;
	menuSelection_ = 0;
	UpdateMenuVisuals();

	// ロゴの少し下
	startY = screenH * 0.45f + 120.0f;

	ruleLogo.sprite = std::make_unique<Sprite>();
	ruleLogo.sprite->Initialize(
		SpriteCommon::GetInstance(),
		"resources/toRule.png"
	);
	ruleLogo.sprite->SetAnchorPoint({ 0.5f, 0.5f });

	// サイズ
	ruleLogo.sprite->SetSize({ 320.0f, 64.0f });
	ruleLogo.baseSize = ruleLogo.sprite->GetSize();

	// 位置
	ruleLogo.startPos = { screenW * 0.5f, -100.0f };
	ruleLogo.targetPos = { screenW * 0.5f, startY };
	ruleLogo.sprite->SetPosition(ruleLogo.startPos);

	// ロゴより少し遅れて落ちる
	ruleLogo.delay = 9 * 0.12f + 0.2f;
	ruleLogo.fallSpeed = 700.0f;

	ruleLogo.timer = 0.0f;
	ruleLogo.landed = false;

	rule = std::make_unique<Sprite>();
	rule->Initialize(SpriteCommon::GetInstance(), "resources/rule.png");
	rule->SetPosition({ 640.0f, 360.0f });
	rule->SetAnchorPoint({ 0.5f,0.5f });
	rule->SetAlpha(0.50f);

	titleTextNeonStyle_.enabled = true;
	titleTextNeonStyle_.glowColor = { 0.18f, 1.0f, 0.48f, 1.0f };
	titleTextNeonStyle_.sourceBrightness = 2.2f;
	titleTextNeonStyle_.threshold = 0.0f;
	titleTextNeonStyle_.innerIntensity = 0.82f;
	titleTextNeonStyle_.outerIntensity = 0.48f;
	titleTextNeonEffect_ = std::make_unique<NeonTextEffect>();
	titleTextNeonEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager());
	titleTextNeonEffect_->SetStyle(titleTextNeonStyle_);
}

void TitleScene::Update() {

	for (auto& c : logoChars) {
		UpdateLogoChar(c, deltaTime);
	}

	UpdateLogoChar(startLogo, deltaTime);
	UpdateLogoChar(tutorialLogo, deltaTime);
	UpdateLogoChar(ruleLogo, deltaTime);
	rule->Update();
	blinkTimer_ += deltaTime;
	const float selectedAlpha = 0.90f + std::sin(blinkTimer_ * 3.5f) * 0.10f;
	if (startLogo.label) {
		startLogo.label->SetAlpha(menuSelection_ == 0 ? selectedAlpha : 0.66f);
	}
	if (tutorialLogo.label) {
		tutorialLogo.label->SetAlpha(menuSelection_ == 1 ? selectedAlpha : 0.66f);
	}

	switch (phase_) {
	case Phase::kFadeIn:
		fade_->Update();

		if (fade_->IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;
	case Phase::kMain: {
#if defined(USE_IMGUI) && !defined(NDEBUG)
		if (input_->IsTrigger(input_->GetKey()[DIK_F3], input_->GetPreKey()[DIK_F3])) {
			if (StartTransitionIfAvailable("TEST", 0.35f)) {
				break;
			}
		}
		if (input_->IsTrigger(input_->GetKey()[DIK_F2], input_->GetPreKey()[DIK_F2])) {
			if (StartTransitionIfAvailable("PLAYER_LAB", 0.35f)) {
				break;
			}
		}
		if (input_->IsTrigger(input_->GetKey()[DIK_F4], input_->GetPreKey()[DIK_F4])) {
			if (StartTransitionIfAvailable("NAVAL_BATTLE", 0.35f)) {
				break;
			}
		}
		if (input_->IsTrigger(input_->GetKey()[DIK_F5], input_->GetPreKey()[DIK_F5])) {
			if (StartTransitionIfAvailable("GRAPHICS_LAB", 0.35f)) {
				break;
			}
		}
		if (input_->IsTrigger(input_->GetKey()[DIK_F6], input_->GetPreKey()[DIK_F6])) {
			if (StartTransitionIfAvailable("VFX_LAB", 0.35f)) {
				break;
			}
		}
#endif // defined(USE_IMGUI) && !defined(NDEBUG)
		const bool selectPrevious =
			input_->IsTrigger(input_->GetKey()[DIK_UP], input_->GetPreKey()[DIK_UP]) ||
			input_->IsTrigger(input_->GetKey()[DIK_W], input_->GetPreKey()[DIK_W]);
		const bool selectNext =
			input_->IsTrigger(input_->GetKey()[DIK_DOWN], input_->GetPreKey()[DIK_DOWN]) ||
			input_->IsTrigger(input_->GetKey()[DIK_S], input_->GetPreKey()[DIK_S]);
		if (selectPrevious || selectNext) {
			menuSelection_ = menuSelection_ == 0 ? 1 : 0;
			UpdateMenuVisuals();
		}

		const bool confirm =
			input_->IsTrigger(input_->GetKey()[DIK_RETURN], input_->GetPreKey()[DIK_RETURN]) ||
			input_->IsTrigger(input_->GetKey()[DIK_SPACE], input_->GetPreKey()[DIK_SPACE]) ||
			input_->IsTrigger(
				input_->GetMouseState().rgbButtons[0],
				input_->GetPreMouseState().rgbButtons[0]);
		if (IsSceneAvailable("GAME") && confirm) {
			GameStartSession::SetMode(
				menuSelection_ == 0 ? GameStartMode::Normal : GameStartMode::Tutorial);
			StartTransitionIfAvailable("GAME", 0.75f);
		}
		break;
	}
	case Phase::kFadeOut:
		fade_->Update();
		if (fade_->IsFinished()) {
			finished_ = true;
		}
		break;
	}
};

void TitleScene::Draw() {};

void TitleScene::DrawAfterPostEffect3D()
{
	if (!titleTextNeonEffect_) {
		return;
	}

	std::vector<TextLabel*> labels;
	labels.reserve(logoChars.size() + 1);
	for (auto& logo : logoChars) {
		if (logo.label) {
			labels.push_back(logo.label.get());
		}
	}
	if (IsSceneAvailable("GAME")) {
		TextLabel* selectedLabel = menuSelection_ == 0
			? startLogo.label.get()
			: tutorialLogo.label.get();
		if (selectedLabel) {
			labels.push_back(selectedLabel);
		}
	}
	titleTextNeonEffect_->DrawBloom(labels);
}

void TitleScene::DrawSprite() {
	//sprite_->Draw();
	for (auto& c : logoChars) {
		if (c.label) {
			c.label->Draw();
		} else if (c.sprite) {
			c.sprite->Draw();
		}
	}
	if (IsSceneAvailable("GAME")) {
		if (startLogo.label) {
			startLogo.label->Draw();
		} else if (startLogo.sprite) {
			startLogo.sprite->Draw();
		}
		if (tutorialLogo.label) {
			tutorialLogo.label->Draw();
		} else if (tutorialLogo.sprite) {
			tutorialLogo.sprite->Draw();
		}
	}
	fade_->Draw();
}

void TitleScene::UpdateLogoChar(LogoChar& c, float deltaTime)
{
	Sprite* sprite = GetLogoSprite(c);
	if (!sprite) {
		return;
	}

	c.timer += deltaTime;
	if (c.timer < c.delay) return;

	Vector2 pos = sprite->GetPosition();

	if (!c.landed) {
		pos.y += c.fallSpeed * deltaTime;
		if (pos.y >= c.targetPos.y) {
			pos.y = c.targetPos.y;
			c.landed = true;
			c.timer = 0.0f;
		}
	} else {
		float t = c.timer * 4.0f;
		float scale = 1.0f + std::sin(t) * 0.12f;

		sprite->SetSize({
			c.baseSize.x * scale,
			c.baseSize.y * scale
			});
	}

	if (c.label) {
		c.label->SetPosition(pos);
	} else {
		sprite->SetPosition(pos);
	}
	sprite->Update();
}

void TitleScene::UpdateMenuVisuals()
{
	const auto applyStyle = [](TextLabel* label, bool selected) {
		if (!label) {
			return;
		}
		TextStyle style = label->GetStyle();
		style.color = selected
			? Vector4{ 0.48f, 1.0f, 0.72f, 1.0f }
			: Vector4{ 0.74f, 0.88f, 0.92f, 0.82f };
		label->SetStyle(style);
		label->PrepareForDraw();
	};
	applyStyle(startLogo.label.get(), menuSelection_ == 0);
	applyStyle(tutorialLogo.label.get(), menuSelection_ == 1);
}

std::string TitleScene::GetNextSceneName() const
{
	return nextSceneName_;
}

bool TitleScene::IsSceneAvailable(std::string_view sceneName) const
{
	return SceneManager::GetInstance()->ContainsScene(sceneName);
}

bool TitleScene::StartTransitionIfAvailable(
	std::string_view sceneName,
	float fadeDuration)
{
	if (!IsSceneAvailable(sceneName)) {
		return false;
	}

	nextSceneName_ = sceneName;
	fade_->Start(Fade::Status::FadeOut, fadeDuration);
	phase_ = Phase::kFadeOut;
	return true;
}
