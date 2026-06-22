#include "TestScene.h"

TestScene::TestScene() {}

TestScene::~TestScene() {}

void TestScene::Initialize() {

	worldTransform_ = InitWorldTransform();

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();

	camera = std::make_unique<Camera>();

	camera->SetTranslate(Vector3(17.0f, 61.0f, -500.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());
	
	trailManager_ = std::make_unique<TrailManager>();
	trailManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), Object3dCommon::GetInstance(), "resources/gradation.png");

	ringManager_ = std::make_unique<RingManager>();
	ringManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/gradationLine.png");

	cylinderManager_ = std::make_unique<CylinderManager>();
	cylinderManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), "resources/gradationLine.png");

	effectSequencer_ = std::make_unique<EffectSequencer>();
	effectSequencer_->Initialize(
		Object3dCommon::GetInstance(),
		Object3dCommon::GetInstance()->GetDxCommon(),
		camera.get(),
		ParticleManager::GetInstance(),
		trailManager_.get()
	);

	objectPostEffect_ = std::make_unique<ObjectPostEffect>();
	objectPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr
	);
	{
		BloomParam& objectPost = objectPostEffect_->GetParam();
		objectPost.intensity = 0.0f;
		objectPost.outlineWidth = 2.0f;
		objectPost.outlineThreshold = 0.05f;
		objectPost.outlineColor = { 1.0f, 0.9f, 0.2f };
		objectPost.outlineBloomIntensity = 0.6f;
		objectPost.outlineBloomWidth = 6.0f;
	}

	groundObj_ = std::make_unique<Object3d>();
	groundObj_->Initialize();
	groundObj_->SetModel("ground.obj");
	groundObj_->SetTranslate(Vector3(0.0f, -30.0f, 0.0f));
	groundObj_->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
	groundObj_->Update();
	groundObj_->SetLighting(true);

	blockObj_ = std::make_unique<Object3d>();
	blockObj_->Initialize();
	blockObj_->SetTranslate(Vector3(-10.0f, 0.0f, 0.0f));
	blockObj_->SetScale(Vector3(1.0f, 1.0f, 1.0f));
	blockObj_->Update();
	blockObj_->SetModel("bloomBall.obj");
	blockObj_->SetColor(Vector4(0.06f, 0.45f, 0.08f, 1.0f));
	//blockObj_->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	blockObj_->SetLighting(false);

	// Animation単元の確認用。外部アセットに依存せず、
	// Vector3線形補間とQuaternion球面線形補間をTestSceneだけで確認する。
	blockObj2_ = std::make_unique<Object3d>();
	blockObj2_->Initialize();
	blockObj2_->SetModel("cube.obj");
	blockObj2_->SetColor({ 0.2f, 0.75f, 1.0f, 1.0f });
	blockObj2_->SetLighting(false);
	keyframeTestAnimation_.name = "TestScene_Keyframe";
	keyframeTestAnimation_.duration = 2.0f;
	NodeAnimation& testNode = keyframeTestAnimation_.nodeAnimations["Root"];
	testNode.translate.keyframes = {
		{ 0.0f, { -30.0f, 0.0f, 0.0f } },
		{ 1.0f, { -30.0f, 15.0f, 0.0f } },
		{ 2.0f, { -30.0f, 0.0f, 0.0f } },
	};
	testNode.rotate.keyframes = {
		{ 0.0f, { 0.0f, 0.0f, 0.0f, 1.0f } },
		{ 1.0f, MakeRotateAxisAngleQuaternion({ 0.0f, 0.0f, 1.0f }, pi) },
		{ 2.0f, MakeRotateAxisAngleQuaternion({ 0.0f, 0.0f, 1.0f }, pi * 2.0f) },
	};
	testNode.scale.keyframes = {
		{ 0.0f, { 3.0f, 3.0f, 3.0f } },
		{ 1.0f, { 5.0f, 2.0f, 3.0f } },
		{ 2.0f, { 3.0f, 3.0f, 3.0f } },
	};
	// Assimp連携確認用の最小glTF。失敗時は上の手作りアニメーションを維持する。
	try {
		keyframeTestAnimation_ = AnimationLoader::LoadFromFile("resources/animation/assimp_test.gltf");
		assimpAnimationLoaded_ = true;
		assimpAnimationStatus_ = "Assimp glTF animation: loaded";
	} catch (const std::exception& error) {
		assimpAnimationStatus_ = std::string("Assimp glTF animation: fallback / ") + error.what();
	}
	keyframeTestPlayer_.SetAnimation(&keyframeTestAnimation_);
	SkeletonNode testSkeletonRoot;
	testSkeletonRoot.name = "Root";
	SkeletonNode testSkeletonChild;
	testSkeletonChild.name = "Child";
	testSkeletonChild.transform.translate = { 0.0f, 5.0f, 0.0f };
	testSkeletonRoot.children.push_back(testSkeletonChild);
	keyframeTestSkeleton_ = SkeletonSystem::Create(testSkeletonRoot);

	// Skinning単元: 配布simpleSkinからMesh / Skeleton / Weightを抽出し、
	// SkinClusterのInfluenceとPaletteを生成する。GPU描画は次の単元で接続する。
	try {
		const std::string skinPath = "resources/models/simpleSkin/simpleSkin.gltf";
		skinningTestAsset_ = SkinningModelLoader::LoadFromFile(skinPath);
		skinningTestSkeleton_ = SkeletonSystem::Create(skinningTestAsset_.rootNode);
		skinningTestCluster_ = SkinCluster::Create(skinningTestSkeleton_, skinningTestAsset_);
		skinningTestAnimation_ = AnimationLoader::LoadFromFile(skinPath);
		skinningTestPlayer_.SetAnimation(&skinningTestAnimation_);
		skinnedTestModel_ = std::make_unique<SkinnedModel>();
		skinnedTestModel_->Initialize(
			Object3dCommon::GetInstance()->GetDxCommon(),
			Object3dCommon::GetInstance()->GetSrvManager(), skinPath);
		skinnedTestObject_ = std::make_unique<Object3d>();
		skinnedTestObject_->Initialize();
		skinnedTestObject_->SetTranslate({ 30.0f, -5.0f, 0.0f });
		skinnedTestObject_->SetScale({ 10.0f, 10.0f, 10.0f });
		skinnedTestObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		skinnedTestObject_->SetLighting(false);
		skinnedTestObject_->Update();
		skinClusterLoaded_ = true;
		skinClusterStatus_ = "simpleSkin GPU Skinning: loaded";
	} catch (const std::exception& error) {
		skinClusterStatus_ = std::string("simpleSkin SkinCluster: failed / ") + error.what();
	}
	try {
		const std::string humanPath = "resources/models/human/walk.gltf";
		humanTestModel_ = std::make_unique<SkinnedModel>();
		humanTestModel_->Initialize(
			Object3dCommon::GetInstance()->GetDxCommon(),
			Object3dCommon::GetInstance()->GetSrvManager(), humanPath);
		humanTestObject_ = std::make_unique<Object3d>();
		humanTestObject_->Initialize();
		humanTestObject_->SetTranslate(humanActionPosition_);
		humanTestObject_->SetScale({ 8.0f, 8.0f, 8.0f });
		humanTestObject_->SetColor({ 0.55f, 0.9f, 1.0f, 1.0f });
		humanTestObject_->SetLighting(false);
		humanTestObject_->Update();
		humanSkinningLoaded_ = true;
		humanSkinningStatus_ = "human/walk GPU Skinning: loaded";
	} catch (const std::exception& error) {
		humanSkinningStatus_ = std::string("human/walk GPU Skinning: failed / ") + error.what();
	}

	// 2. 剣に見立てた細長いブロックを作る
	swordObj_ = std::make_unique<Object3d>();
	swordObj_->Initialize();
	swordObj_->SetModel("weapon.obj"); // 既存のモデル
	swordObj_->SetScale({ 2.0f, 2.0f, 2.0f }); // 剣っぽく細長く

	TextureManager::GetInstance()->LoadTexture("resources/skybox.dds");

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize("resources/skybox.dds"); // ファイル名を指定するだけ
	
	uint32_t skyboxTextureIndex = TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds");
	blockObj_->SetEnvironmentMap(skyboxTextureIndex);
	blockObj_->SetEnvironmentCoefficient(0.5f); // 50%反射

	effectStartMarker_ = std::make_unique<Object3d>();
	effectStartMarker_->Initialize();
	effectStartMarker_->SetModel("ball.obj");
	effectStartMarker_->SetScale({ 2.0f, 2.0f, 2.0f });
	effectStartMarker_->SetColor({ 0.1f, 0.8f, 1.0f, 1.0f });
	effectStartMarker_->SetLighting(false);

	effectTargetMarker_ = std::make_unique<Object3d>();
	effectTargetMarker_->Initialize();
	effectTargetMarker_->SetModel("ball.obj");
	effectTargetMarker_->SetScale({ 2.0f, 2.0f, 2.0f });
	effectTargetMarker_->SetColor({ 1.0f, 0.25f, 0.1f, 1.0f });
	effectTargetMarker_->SetLighting(false);

	transplantTestProfile_.projectile.modelPath = "ball.obj";
	transplantTestProfile_.projectile.scale = { 1.4f, 1.4f, 1.4f };
	transplantTestProfile_.projectile.rotationSpeed = { 2.0f, 6.0f, 1.0f };
	transplantTestProfile_.flyParticle = "HitSpark";
	transplantTestProfile_.hitParticle = "HitSpark";
	transplantTestProfile_.flyParticleCount = 0;
	transplantTestProfile_.hitParticleCount = 2;
	transplantTestProfile_.duration = 1.2f;
	transplantTestProfile_.hitDuration = 0.45f;
	transplantTestProfile_.enableTrail = true;
	transplantTestProfile_.trail.startColor = { 0.2f, 0.8f, 1.0f, 0.85f };
	transplantTestProfile_.trail.endColor = { 1.0f, 0.2f, 0.1f, 0.0f };
	transplantTestProfile_.trail.tipOffset = { 0.0f, 1.2f, 0.0f };
	transplantTestProfile_.trail.baseOffset = { 0.0f, -1.2f, 0.0f };
	transplantTestProfile_.trail.maxPoints = 80;
	transplantTestProfile_.trail.interpolationSteps = 6;
	transplantTestProfile_.trail.lifetime = 0.55f;

	ringConfig_.startRadius = 1.0f;
	ringConfig_.endRadius = 18.0f;
	ringConfig_.startWidth = 0.6f;
	ringConfig_.endWidth = 2.0f;
	ringConfig_.lifeTime = 0.75f;
	ringConfig_.startColor = { 0.1f, 0.85f, 1.0f, 1.0f };
	ringConfig_.endColor = { 0.3f, 0.15f, 1.0f, 0.0f };

	cylinderConfig_.startRadius = 2.0f;
	cylinderConfig_.endRadius = 11.0f;
	cylinderConfig_.startHeight = 2.0f;
	cylinderConfig_.endHeight = 32.0f;
	cylinderConfig_.lifeTime = 0.85f;
	cylinderConfig_.startColor = { 0.1f, 0.85f, 1.0f, 0.95f };
	cylinderConfig_.endColor = { 0.3f, 0.15f, 1.0f, 0.0f };
}

void TestScene::Update() {

#ifdef USE_IMGUI

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
	ImGui::Text("deltaTime: %.8f", finalDeltaTime * 60.0f);
	ImGui::End();

	ImGui::Begin("Keyframe Animation Test");
	bool keyframePlaying = keyframeTestPlayer_.IsPlaying();
	if (ImGui::Checkbox("Play", &keyframePlaying)) {
		keyframeTestPlayer_.SetPlaying(keyframePlaying);
	}
	ImGui::DragFloat("Playback Speed", &keyframeTestPlaybackSpeed_, 0.05f, -3.0f, 3.0f);
	float keyframeTime = keyframeTestPlayer_.GetTime();
	if (ImGui::SliderFloat("Time", &keyframeTime, 0.0f, keyframeTestAnimation_.duration)) {
		keyframeTestPlayer_.Seek(keyframeTime);
	}
	ImGui::Text("Cyan cube: Vector3 Lerp + Quaternion Slerp");
	ImGui::Text("Skeleton joints: %zu", keyframeTestSkeleton_.joints.size());
	ImGui::TextColored(
		assimpAnimationLoaded_ ? ImVec4{ 0.4f, 1.0f, 0.6f, 1.0f } : ImVec4{ 1.0f, 0.45f, 0.3f, 1.0f },
		"%s", assimpAnimationStatus_.c_str());
	ImGui::Separator();
	ImGui::TextColored(
		skinClusterLoaded_ ? ImVec4{ 0.4f, 1.0f, 0.6f, 1.0f } : ImVec4{ 1.0f, 0.45f, 0.3f, 1.0f },
		"%s", skinClusterStatus_.c_str());
	if (skinClusterLoaded_) {
		ImGui::Text("Skin vertices: %zu", skinningTestAsset_.modelData.vertices.size());
		ImGui::Text("Skin indices: %zu", skinningTestAsset_.modelData.indices.size());
		ImGui::Text("Skin joints: %zu", skinningTestSkeleton_.joints.size());
		ImGui::Text("Assigned influences: %u", skinningTestCluster_.GetAssignedInfluenceCount());
		ImGui::Text("Palette entries: %zu", skinningTestCluster_.GetPalette().size());
	}
	ImGui::TextColored(
		humanSkinningLoaded_ ? ImVec4{ 0.4f, 1.0f, 0.6f, 1.0f } : ImVec4{ 1.0f, 0.45f, 0.3f, 1.0f },
		"%s", humanSkinningStatus_.c_str());
	if (humanSkinningLoaded_) {
		ImGui::Text("Human vertices: %zu", humanTestModel_->GetAsset().modelData.vertices.size());
		ImGui::Text("Human joints: %zu", humanTestModel_->GetSkeleton().joints.size());
		ImGui::Text("Human influences: %u", humanTestModel_->GetSkinCluster().GetAssignedInfluenceCount());
	}
	ImGui::SeparatorText("3D Action Skinning");
	ImGui::Checkbox("Humanを表示", &showHumanSkinning_);
	ImGui::Checkbox("simpleSkinを表示", &showSimpleSkin_);
	ImGui::Checkbox("旧テストオブジェクトを表示", &showLegacyTestObjects_);
	ImGui::Checkbox("WASDでHumanを操作", &enableHumanActionControl_);
	ImGui::DragFloat("移動速度", &humanActionMoveSpeed_, 0.5f, 1.0f, 100.0f);
	ImGui::End();

	ImGui::Begin("Block");
	ImGui::DragFloat3("position", &blockObj_->GetTranslate().x);
	ImGui::DragFloat3("scale", &blockObj_->GetScale().x);
	ImGui::End();

	ImGui::Begin("Ground");
	ImGui::DragFloat3("position", &groundObj_->GetTranslate().x);
	ImGui::DragFloat3("rotate", &groundObj_->GetRotate().x, 0.01f);
	ImGui::End();

#endif // USE_IMGUI

	// 1. 剣をぶん回すアニメーション（テスト用）
	static float timer = 0.0f;
	timer += 0.02f;
	swordObj_->SetRotate({ 0.0f, 0.0f, timer * 2.0f }); // Z軸でぐるぐる
	//swordObj_->SetTranslate({ std::sin(timer) * 20.0f, 0.0f, 0.0f }); // 左右に揺らす
	swordObj_->Update();

	// 2. ワールド行列から先端と根元の座標を計算
	// ※Object3dに GetWorldMatrix() がある前提。なければ計算してください
	Matrix4x4 worldMat = MakeAffineMatrix(swordObj_->GetScale(), swordObj_->GetRotate(), swordObj_->GetTranslate());

	Vector3 localBase = { 0.0f, 0.0f, 0.0f };   // 剣の根元
	Vector3 localTip = { 0.0f, 6.0f, 0.0f };  // 剣の先端（Scale.yが10ならこのあたり）

	// ローカル座標をワールド座標へ変換
	Vector3 worldBase = TransformNormal(localBase, worldMat);
	Vector3 worldTip = TransformNormal(localTip, worldMat);

	// 3. 軌跡を更新！
	//trailManager_->Update(worldTip, worldBase);

	camera->Update();
	debugCamera->Update(input_->GetMouseState(), input_->GetKey(), input_->GetLeftStick());
	groundObj_->Update();
	blockObj_->Update();
	keyframeTestPlayer_.SetPlaybackSpeed(keyframeTestPlaybackSpeed_);
	keyframeTestPlayer_.Update(finalDeltaTime);
	SkeletonSystem::ApplyAnimation(keyframeTestSkeleton_, keyframeTestPlayer_);
	const QuaternionTransform& keyframeTransform = keyframeTestSkeleton_.joints[keyframeTestSkeleton_.root].transform;
	blockObj2_->SetScale(keyframeTransform.scale);
	blockObj2_->SetTranslate(keyframeTransform.translate);
	blockObj2_->SetQuaternionRotate(keyframeTransform.rotate);
	blockObj2_->Update();
	if (skinClusterLoaded_) {
		skinningTestPlayer_.Update(finalDeltaTime);
		SkeletonSystem::ApplyAnimation(skinningTestSkeleton_, skinningTestPlayer_);
		skinningTestCluster_.Update(skinningTestSkeleton_);
		skinnedTestModel_->Update(finalDeltaTime);
		skinnedTestObject_->Update();
	}
	if (humanSkinningLoaded_) {
		Vector3 move{};
		if (enableHumanActionControl_) {
			if (input_->IsPress(input_->GetKey()[DIK_A])) move.x -= 1.0f;
			if (input_->IsPress(input_->GetKey()[DIK_D])) move.x += 1.0f;
			if (input_->IsPress(input_->GetKey()[DIK_W])) move.z += 1.0f;
			if (input_->IsPress(input_->GetKey()[DIK_S])) move.z -= 1.0f;
		}
		const float moveLength = std::sqrt(move.x * move.x + move.z * move.z);
		const bool isMoving = moveLength > 0.0001f;
		if (isMoving) {
			move.x /= moveLength;
			move.z /= moveLength;
			humanActionPosition_.x += move.x * humanActionMoveSpeed_ * finalDeltaTime;
			humanActionPosition_.z += move.z * humanActionMoveSpeed_ * finalDeltaTime;
			humanTestObject_->SetRotate({ 0.0f, std::atan2(move.x, move.z), 0.0f });
		}
		humanTestObject_->SetTranslate(humanActionPosition_);
		humanTestModel_->GetAnimationPlayer().SetPlaying(isMoving || !enableHumanActionControl_);
		humanTestModel_->Update(finalDeltaTime);
		humanTestObject_->Update();
	}
	effectStartMarker_->SetTranslate(effectStartPos_);
	effectStartMarker_->Update();
	effectTargetMarker_->SetTranslate(effectTargetPos_);
	effectTargetMarker_->Update();
	//swordObj_->Update();
	skybox_->Update(camera.get(), debugCamera.get()); // カメラ追従もクラス内で完結

#ifdef USE_IMGUI

	ParticleManager::GetInstance()->DrawImGuiEditor();
	effectSequencer_->DrawImGuiEditor({ -20.0f, 10.0f, 0.0f }, { 20.0f, 10.0f, 0.0f });

	ImGui::Begin("Transplant Feature Test");
	ImGui::Text("EffectSequencer + TrailManager + ParticleManager");
	ImGui::DragFloat3("Start Pos", &effectStartPos_.x, 0.2f);
	ImGui::DragFloat3("Target Pos", &effectTargetPos_.x, 0.2f);
	ImGui::DragFloat("Duration", &transplantTestProfile_.duration, 0.05f, 0.1f, 5.0f);
	ImGui::Checkbox("Trail", &transplantTestProfile_.enableTrail);
	bool useGpuParticle = ParticleManager::GetInstance()->IsUseGpuUpdate();
	if (ImGui::Checkbox("GPU Particle Update", &useGpuParticle)) {
		ParticleManager::GetInstance()->SetUseGpuUpdate(useGpuParticle);
	}
	int hitCount = static_cast<int>(transplantTestProfile_.hitParticleCount);
	if (ImGui::SliderInt("Hit Burst Count", &hitCount, 1, 8)) {
		transplantTestProfile_.hitParticleCount = static_cast<uint32_t>(hitCount);
	}
	if (ImGui::Button("Fire Migrated Sample")) {
		effectSequencer_->Fire(transplantTestProfile_, effectStartPos_, effectTargetPos_);
		ringManager_->Emit(effectStartPos_, ringConfig_);
		cylinderManager_->Emit(effectStartPos_, cylinderConfig_);
	}
	ImGui::SameLine();
	if (ImGui::Button("Hit At Target")) {
		ParticleManager::GetInstance()->EmitHitEffect(effectTargetPos_);
		ringManager_->Emit(effectTargetPos_, ringConfig_);
		cylinderManager_->Emit(effectTargetPos_, cylinderConfig_);
	}
	ImGui::SameLine();
	if (ImGui::Button("Ring")) {
		ringManager_->Emit(effectTargetPos_, ringConfig_);
	}
	ImGui::SameLine();
	if (ImGui::Button("Cylinder")) {
		cylinderManager_->Emit(effectTargetPos_, cylinderConfig_);
	}
	ImGui::DragFloat("Ring Life", &ringConfig_.lifeTime, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("Ring Start Radius", &ringConfig_.startRadius, 0.1f, 0.0f, 50.0f);
	ImGui::DragFloat("Ring End Radius", &ringConfig_.endRadius, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Ring Start Width", &ringConfig_.startWidth, 0.05f, 0.0f, 20.0f);
	ImGui::DragFloat("Ring End Width", &ringConfig_.endWidth, 0.05f, 0.0f, 20.0f);
	ImGui::ColorEdit4("Ring Start Color", &ringConfig_.startColor.x);
	ImGui::ColorEdit4("Ring End Color", &ringConfig_.endColor.x);
	ImGui::DragFloat("Cylinder Life", &cylinderConfig_.lifeTime, 0.01f, 0.05f, 5.0f);
	ImGui::DragFloat("Cylinder Start Radius", &cylinderConfig_.startRadius, 0.1f, 0.0f, 50.0f);
	ImGui::DragFloat("Cylinder End Radius", &cylinderConfig_.endRadius, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Cylinder Start Height", &cylinderConfig_.startHeight, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Cylinder End Height", &cylinderConfig_.endHeight, 0.1f, 0.0f, 200.0f);
	ImGui::ColorEdit4("Cylinder Start Color", &cylinderConfig_.startColor.x);
	ImGui::ColorEdit4("Cylinder End Color", &cylinderConfig_.endColor.x);
	if (ImGui::Button("GPU Burst Test")) {
		ParticleManager::GetInstance()->Emit("HitSpark", effectTargetPos_, 2000);
	}
	ImGui::Checkbox("Auto Fire", &autoFireEffect_);
	ImGui::Text("State: %d", static_cast<int>(effectSequencer_->GetState()));
	ImGui::Text("Start marker: cyan / Target marker: red");
	ImGui::End();

	ImGui::Begin("Object Post Effect Test");
	ImGui::Checkbox("Enable Object Post", &enableObjectPostEffect_);
	BloomParam& objectPost = objectPostEffect_->GetParam();
	ImGui::DragFloat("Intensity", &objectPost.intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Distortion", &objectPost.distortionAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("ChromAb", &objectPost.chromAbAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Glitch", &objectPost.glitchAmount, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Dissolve", &objectPost.dissolveThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Outline Width", &objectPost.outlineWidth, 0.1f, 0.0f, 10.0f);
	ImGui::DragFloat("Outline Threshold", &objectPost.outlineThreshold, 0.01f, 0.0f, 1.0f);
	ImGui::ColorEdit3("Outline Color", &objectPost.outlineColor.x);
	ImGui::DragFloat("Outline Bloom Intensity", &objectPost.outlineBloomIntensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Outline Bloom Width", &objectPost.outlineBloomWidth, 0.1f, 0.0f, 30.0f);
	ImGui::Text("Target object: green block only");
	ImGui::End();

	ImGuiIO& io = ImGui::GetIO();
	if (!io.WantCaptureMouse && input_->IsTrigger(input_->GetMouseState().rgbButtons[0], input_->GetPreMouseState().rgbButtons[0])) {
		Matrix4x4 viewMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera() ? debugCamera->GetViewMatrix() : camera->GetViewMatrix();
		Matrix4x4 projectionMatrix = Object3dCommon::GetInstance()->GetIsDebugCamera() ? debugCamera->GetProjectionMatrix() : camera->GetProjectionMatrix();
		Vector3 worldPos = ScreenToWorldOnZ0(input_->GetMousePosition(), viewMatrix, projectionMatrix, WinApp::kClientWidth, WinApp::kClientHeight);
		ParticleManager::GetInstance()->EmitHitEffect(worldPos);
	}

#endif // USE_IMGUI

	objectPostEffect_->Update(finalDeltaTime);
	if (autoFireEffect_) {
		autoFireTimer_ += finalDeltaTime;
		if (autoFireTimer_ >= 1.6f && effectSequencer_->IsFinished()) {
			effectSequencer_->Fire(transplantTestProfile_, effectStartPos_, effectTargetPos_);
			ringManager_->Emit(effectStartPos_, ringConfig_);
			cylinderManager_->Emit(effectStartPos_, cylinderConfig_);
			autoFireTimer_ = 0.0f;
		}
	} else {
		autoFireTimer_ = 0.0f;
	}

	effectSequencer_->Update(finalDeltaTime);
	trailManager_->Update(finalDeltaTime);
	ringManager_->Update(finalDeltaTime);
	cylinderManager_->Update(finalDeltaTime);
	ParticleManager::GetInstance()->Update(finalDeltaTime, camera.get(), debugCamera.get());

}

void TestScene::Draw() {

}

void TestScene::DrawPostEffect3D() {

	if (showLegacyTestObjects_) {
		skybox_->Draw();
	}

	Object3dCommon::GetInstance()->PreDraw(kNone);

	if (showLegacyTestObjects_) {
		if (!enableObjectPostEffect_) {
			blockObj_->Draw();
		}
		effectStartMarker_->Draw();
		effectTargetMarker_->Draw();
		blockObj2_->Draw();
		swordObj_->Draw();
	}

	groundObj_->Draw();
	if (showSimpleSkin_ && skinClusterLoaded_) {
		skinnedTestObject_->DrawSkinned(*skinnedTestModel_);
	}
	if (showHumanSkinning_ && humanSkinningLoaded_) {
		humanTestObject_->DrawSkinned(*humanTestModel_);
	}

	if (showLegacyTestObjects_) {
		effectSequencer_->Draw();
	}

	if (showLegacyTestObjects_ && enableObjectPostEffect_) {
		objectPostEffect_->BeginCapture();
		Object3dCommon::GetInstance()->PreDraw(kNone);
		blockObj_->Draw();
		objectPostEffect_->EndCapture();
		Object3dCommon::GetInstance()->PreDraw(kNone);
	}

	Matrix4x4 vp = Object3dCommon::GetInstance()->GetIsDebugCamera()
		? debugCamera->GetViewProjectionMatrix()
		: camera->GetViewProjectionMatrix();
	if (showLegacyTestObjects_) {
		trailManager_->DrawAll(vp);
		ringManager_->DrawAll(vp);
		cylinderManager_->DrawAll(vp);
		ParticleManager::GetInstance()->Draw();
	}
}

void TestScene::DrawShadow() {

	Object3dCommon::GetInstance()->PreDraw(kShadow);

	if (showLegacyTestObjects_) {
		blockObj_->DrawShadow();
		blockObj2_->DrawShadow();
		swordObj_->DrawShadow();
	}
	if (showSimpleSkin_ && skinClusterLoaded_) {
		skinnedTestObject_->DrawSkinnedShadow(*skinnedTestModel_);
	}
	if (showHumanSkinning_ && humanSkinningLoaded_) {
		humanTestObject_->DrawSkinnedShadow(*humanTestModel_);
	}
}

void TestScene::DrawSprite() {

}
