#include "TitleScene.h"
#include "GameScene.h"
#include "SceneManager.h"

#include <numbers>

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

	std::vector<std::string> files = {
		"ta.png","ta.png","ka.png","u.png",
		"se.png","nn.png","si.png","ya.png"
	};

	for (int i = 0; i < charCount; ++i) {

		LogoChar c{};
		c.sprite = std::make_unique<Sprite>();
		c.sprite->Initialize(SpriteCommon::GetInstance(), "resources/" + files[i]);
		c.sprite->SetAnchorPoint({ 0.5f, 0.5f });

		// サイズ設定
		c.sprite->SetSize(charSize);
		c.baseSize = charSize;

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

		c.sprite->SetPosition(c.startPos);

		// 落下スピードに個性を出す
		c.fallSpeed = Rand(550.0f, 750.0f);

		// タイミングずらし（少しランダム）
		c.delay = i * 0.12f + Rand(0.0f, 0.05f);

		c.timer = 0.0f;
		c.landed = false;

		logoChars.push_back(std::move(c));
	}

	// ロゴの少し下
	startY = screenH * 0.55f + 120.0f;

	startLogo.sprite = std::make_unique<Sprite>();
	startLogo.sprite->Initialize(
		SpriteCommon::GetInstance(),
		"resources/start.png"
	);
	startLogo.sprite->SetAnchorPoint({ 0.5f, 0.5f });

	// サイズ
	startLogo.sprite->SetSize({ 320.0f, 64.0f });
	startLogo.baseSize = startLogo.sprite->GetSize();

	// 位置
	startLogo.startPos = { screenW * 0.5f, -100.0f };
	startLogo.targetPos = { screenW * 0.5f, startY };
	startLogo.sprite->SetPosition(startLogo.startPos);

	// ロゴより少し遅れて落ちる
	startLogo.delay = 8 * 0.12f + 0.2f;
	startLogo.fallSpeed = 700.0f;

	startLogo.timer = 0.0f;
	startLogo.landed = false;

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

	TextStyle modeStyle{};
	modeStyle.fontFamily = "Meiryo";
	modeStyle.fontSize = 25.0f;
	modeStyle.color = { 0.58f, 1.0f, 0.72f, 1.0f };
	modeStyle.outlineColor = { 0.0f, 0.02f, 0.06f, 0.96f };
	modeStyle.outlineThickness = 3.0f;
	modeStyle.padding = 8.0f;
	modeMenuText_ = std::make_unique<TextLabel>();
	modeMenuText_->Initialize(SpriteCommon::GetInstance(), "", modeStyle);
	modeMenuText_->SetAnchorPoint({ 0.5f, 0.5f });
	modeMenuText_->SetPosition({ screenW * 0.5f, screenH * 0.80f });

	TextStyle hintStyle = modeStyle;
	hintStyle.fontSize = 16.0f;
	hintStyle.color = { 0.82f, 0.90f, 1.0f, 0.90f };
	hintStyle.outlineThickness = 2.0f;
	startHintText_ = std::make_unique<TextLabel>();
	startHintText_->Initialize(
		SpriteCommon::GetInstance(),
		"W/S or Arrow Keys: Select   Left Click / Enter: Start",
		hintStyle);
	startHintText_->SetAnchorPoint({ 0.5f, 0.5f });
	startHintText_->SetPosition({ screenW * 0.5f, screenH * 0.91f });
}

void TitleScene::Update() {

	for (auto& c : logoChars) {
		UpdateLogoChar(c, deltaTime);
	}

	UpdateLogoChar(startLogo, deltaTime);
	UpdateLogoChar(ruleLogo, deltaTime);
	rule->Update();

	switch (phase_) {
	case Phase::kFadeIn:
		fade_->Update();

		if (fade_->IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;
	case Phase::kMain: {
#ifdef USE_IMGUI
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
#endif // USE_IMGUI

		const bool up =
			input_->IsTrigger(input_->GetKey()[DIK_W], input_->GetPreKey()[DIK_W]) ||
			input_->IsTrigger(input_->GetKey()[DIK_UP], input_->GetPreKey()[DIK_UP]);
		const bool down =
			input_->IsTrigger(input_->GetKey()[DIK_S], input_->GetPreKey()[DIK_S]) ||
			input_->IsTrigger(input_->GetKey()[DIK_DOWN], input_->GetPreKey()[DIK_DOWN]);
		if (up || down) {
			menuSelection_ = 1 - menuSelection_;
		}

		if (modeMenuText_) {
			modeMenuText_->SetText(
				std::string(menuSelection_ == 0 ? "> " : "  ") + "NORMAL MODE\n" +
				(menuSelection_ == 1 ? "> " : "  ") + "SHOWCASE MODE");
		}

		const bool confirm =
			input_->IsTrigger(input_->GetKey()[DIK_RETURN], input_->GetPreKey()[DIK_RETURN]) ||
			input_->IsTrigger(input_->GetKey()[DIK_SPACE], input_->GetPreKey()[DIK_SPACE]) ||
			input_->IsTrigger(
				input_->GetMouseState().rgbButtons[0],
				input_->GetPreMouseState().rgbButtons[0]);
		if (IsSceneAvailable("GAME") && confirm) {
			GameScene::SetNextShowcaseMode(menuSelection_ == 1);
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

void TitleScene::DrawSprite() {
	//sprite_->Draw();
	for (auto& c : logoChars) {
		c.sprite->Draw();
	}
	if (IsSceneAvailable("GAME")) {
		startLogo.sprite->Draw();
	}
	if (modeMenuText_) {
		modeMenuText_->Draw();
	}
	if (startHintText_) {
		startHintText_->Draw();
	}
	fade_->Draw();
}

void TitleScene::UpdateLogoChar(LogoChar& c, float deltaTime)
{
	c.timer += deltaTime;
	if (c.timer < c.delay) return;

	Vector2 pos = c.sprite->GetPosition();

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

		c.sprite->SetSize({
			c.baseSize.x * scale,
			c.baseSize.y * scale
			});
	}

	c.sprite->SetPosition(pos);
	c.sprite->Update();
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
