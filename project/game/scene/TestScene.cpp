#include "TestScene.h"

TestScene::TestScene() {}

TestScene::~TestScene() {}

float TestScene::GetVrmGroundHeight(const Vector3& position) {
	const float probeDown = vrmActionState_ == VrmActionState::Jump
		? 200.0f
		: vrmGroundProbeDown_;
	const Vector3 rayOrigin = {
		position.x,
		position.y + vrmGroundProbeUp_,
		position.z,
	};
	GroundRayHit hit{};
	GroundRayHit candidate{};
	bool found = false;
	const float minimumNormalY = std::cos(vrmMaximumSlopeDegrees_ * pi / 180.0f);
	if (flatGroundMesh_.RaycastDown(
		rayOrigin, vrmGroundProbeUp_ + probeDown, minimumNormalY, candidate)) {
		hit = candidate;
		found = true;
	}
	if (enableSlopeGround_ && slopeGroundMesh_.RaycastDown(
		rayOrigin, vrmGroundProbeUp_ + probeDown, minimumNormalY, candidate) &&
		(!found || candidate.distance < hit.distance)) {
		hit = candidate;
		found = true;
	}
	if (found) {
		vrmGroundNormal_ = hit.normal;
		return hit.position.y;
	}
	vrmGroundNormal_ = { 0.0f, 1.0f, 0.0f };
	return vrmGroundY_;
}

void TestScene::Initialize() {

	worldTransform_ = InitWorldTransform();

	input_ = Input::GetInstance();

	debugCamera = std::make_unique<DebugCamera>();
	// The VRoid clothes contain several nearly overlapping surfaces.  The old
	// 0.1-5000 depth range loses too much D24 precision around this scene's
	// roughly 500-unit camera distance and makes the skirt flicker.
	debugCamera->SetNearClip(0.5f);
	debugCamera->SetFarClip(1500.0f);

	camera = std::make_unique<Camera>();
	camera->SetNearClip(2.0f);
	camera->SetFarClip(1500.0f);

	camera->SetTranslate(Vector3(17.0f, 61.0f, -500.0f));

	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	Object3dCommon::GetInstance()->SetDebugDefaultCamera(debugCamera.get());
	
	trailManager_ = std::make_unique<TrailManager>();
	trailManager_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(), Object3dCommon::GetInstance(), "resources/gradation.png");
	swordTrail_ = trailManager_->CreateInstance();
	swordTrail_->SetIsPermanent(true);
	swordTrail_->SetActive(false);
	swordTrailConfig_.startColor = { 0.55f, 1.35f, 2.4f, 0.95f };
	swordTrailConfig_.endColor = { 0.05f, 0.35f, 1.2f, 0.0f };
	swordTrailConfig_.interpolationSteps = 5;
	swordTrailConfig_.maxPoints = 28;
	swordTrailConfig_.lifetime = 0.24f;
	swordTrailConfig_.startWidthScale = 1.0f;
	swordTrailConfig_.endWidthScale = 0.15f;
	swordTrailConfig_.widthCurvePower = 1.25f;
	swordTrailConfig_.colorCurvePower = 1.15f;

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
	swordPostEffect_ = std::make_unique<ObjectPostEffect>();
	swordPostEffect_->Initialize(
		Object3dCommon::GetInstance()->GetDxCommon(),
		Object3dCommon::GetInstance()->GetSrvManager(),
		nullptr,
		1.0f);
	{
		BloomParam& swordBloom = swordPostEffect_->GetParam();
		swordBloom.threshold = 0.05f;
		swordBloom.intensity = 1.35f;
		swordBloom.outlineBloomIntensity = 0.0f;
	}

	groundObj_ = std::make_unique<Object3d>();
	groundObj_->Initialize();
	groundObj_->SetModel("ground.obj");
	groundObj_->SetTranslate(Vector3(0.0f, -30.0f, 0.0f));
	groundObj_->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
	groundObj_->Update();
	groundObj_->SetLighting(true);

	// TestScene用の傾斜床。接地計算と同じ変換を使うため、見た目と
	// キャラクターの足元がずれない。
	const float slopeCenterY = vrmGroundY_ +
		slopeGroundScale_.x * std::sin(slopeGroundAngle_) -
		slopeGroundScale_.y * std::cos(slopeGroundAngle_);
	slopeGroundCenter_.y = slopeCenterY;
	slopeGroundObj_ = std::make_unique<Object3d>();
	slopeGroundObj_->Initialize();
	slopeGroundObj_->SetModel("cube.obj");
	slopeGroundObj_->SetTranslate(slopeGroundCenter_);
	slopeGroundObj_->SetScale(slopeGroundScale_);
	slopeGroundObj_->SetRotate({ 0.0f, 0.0f, slopeGroundAngle_ });
	slopeGroundObj_->SetColor({ 0.16f, 0.19f, 0.24f, 1.0f });
	slopeGroundObj_->SetLighting(true);
	slopeGroundObj_->Update();

	flatGroundMesh_.Clear();
	slopeGroundMesh_.Clear();
	if (Model* groundModel = ModelManager::GetInstance()->FindModel("ground.obj")) {
		flatGroundMesh_.AddMesh(
			groundModel->GetModelData(),
			MakeAffineMatrix(
				Vector3{ 1.0f, 1.0f, 1.0f },
				Vector3{ 0.0f, 0.0f, 0.0f },
				Vector3{ 0.0f, vrmGroundY_, 0.0f }));
	}
	if (Model* slopeModel = ModelManager::GetInstance()->FindModel("cube.obj")) {
		slopeGroundMesh_.AddMesh(
			slopeModel->GetModelData(),
			MakeAffineMatrix(
				slopeGroundScale_, Vector3{ 0.0f, 0.0f, slopeGroundAngle_ }, slopeGroundCenter_));
	}

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
	try {
		const std::string vrmGlbPath = "resources/models/player/testModel_animated.glb";
		vrmTestModel_ = std::make_unique<SkinnedModel>();
		vrmTestModel_->Initialize(
			Object3dCommon::GetInstance()->GetDxCommon(),
			Object3dCommon::GetInstance()->GetSrvManager(), vrmGlbPath);
		vrmTestObject_ = std::make_unique<Object3d>();
		vrmTestObject_->Initialize();
		vrmTestObject_->SetTranslate(vrmActionPosition_);
		vrmTestObject_->SetScale({ 25.0f, 25.0f, 25.0f });
		vrmTestObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		// VRoidのMToonは現行Object3dライティングと特性が異なるため、
		// まずはベーステクスチャ色を正確に確認できるUnlit表示にする。
		vrmTestObject_->SetLighting(false);
		vrmTestObject_->Update();
		vrmTestModel_->SetAnimation("Idle");
		vrmTestLoaded_ = true;
		vrmTestStatus_ = "VRoid + Mixamo: Idle / Walk / Run loaded";
		showHumanSkinning_ = false;
	} catch (const std::exception& error) {
		vrmTestStatus_ = std::string("VRoid testModel.glb: failed / ") + error.what();
	}

	// 2. 剣に見立てた細長いブロックを作る
	swordObj_ = std::make_unique<Object3d>();
	swordObj_->Initialize();
	ModelManager::GetInstance()->LoadModel("light.obj");
	swordObj_->SetModel("light.obj");
	swordObj_->SetColor({ 0.25f, 0.85f, 1.0f, 1.0f });
	swordObj_->SetLighting(false);

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
	ImGui::SeparatorText("VRoid GLB Test");
	ImGui::TextColored(
		vrmTestLoaded_ ? ImVec4{ 0.4f, 1.0f, 0.6f, 1.0f } : ImVec4{ 1.0f, 0.45f, 0.3f, 1.0f },
		"%s", vrmTestStatus_.c_str());
	ImGui::Checkbox("VRoid testModelを表示", &showVrmTestModel_);
	if (vrmTestLoaded_) {
		ImGui::Text("Vertices: %zu", vrmTestModel_->GetAsset().modelData.vertices.size());
		ImGui::Text("Indices: %zu", vrmTestModel_->GetAsset().modelData.indices.size());
		ImGui::Text("Submeshes: %zu", vrmTestModel_->GetAsset().submeshes.size());
		ImGui::Text("Embedded textures: %zu", vrmTestModel_->GetAsset().embeddedTextures.size());
		ImGui::Text("Skeleton joints: %zu", vrmTestModel_->GetSkeleton().joints.size());
		ImGui::Text("Animations: %zu", vrmTestModel_->GetAnimations().size());
		ImGui::Text("Current animation: %s", vrmCurrentAnimation_.c_str());
		ImGui::Text(
			"Animation time: %.3f / %.3f  playing=%s loop=%s",
			vrmTestModel_->GetCurrentAnimationTime(),
			vrmTestModel_->GetCurrentAnimationDuration(),
			vrmTestModel_->IsCurrentAnimationPlaying() ? "true" : "false",
			vrmTestModel_->IsCurrentAnimationLooping() ? "true" : "false");
		ImGui::Checkbox("WASDでVRoidを操作", &enableVrmActionControl_);
		ImGui::DragFloat("VRoid歩行速度", &vrmActionMoveSpeed_, 0.25f, 1.0f, 50.0f);
		ImGui::DragFloat("Shift走行倍率", &vrmActionRunMultiplier_, 0.05f, 1.0f, 4.0f);
		ImGui::DragFloat("アニメーション補間秒", &vrmAnimationBlendDuration_, 0.01f, 0.0f, 0.6f);
		ImGui::DragFloat("VRoid旋回速度", &vrmFacingTurnSpeed_, 0.25f, 1.0f, 30.0f);
		ImGui::Checkbox("戦闘向きを固定（4方向回避確認）", &vrmLockFacing_);
		ImGui::Text("Space: Jump / Left Ctrl: Dodge / F: Sword attack");
		ImGui::DragFloat("回避距離", &vrmDodgeDistance_, 0.25f, 1.0f, 60.0f);
		ImGui::DragFloat("ジャンプ初速", &vrmJumpSpeed_, 0.25f, 1.0f, 80.0f);
		ImGui::DragFloat("重力", &vrmGravity_, 0.25f, 1.0f, 100.0f);
		ImGui::DragFloat("踏み切り再生速度", &vrmTakeoffPlaybackSpeed_, 0.05f, 0.25f, 3.0f);
		ImGui::SliderFloat("踏み切りで上昇を始める時点", &vrmTakeoffLaunchPhase_, 0.0f, 0.9f);
		ImGui::SliderFloat("着地クリップ開始位置", &vrmLandingStartPhase_, 0.0f, 0.8f);
		ImGui::DragFloat("着地再生速度", &vrmLandingPlaybackSpeed_, 0.05f, 0.25f, 3.0f);
		ImGui::SeparatorText("3-hit sword combo");
		ImGui::Text(
			"Combo step: %d  queued=%s  hit=%s",
			vrmComboStep_, vrmComboQueued_ ? "true" : "false", vrmAttackHitActive_ ? "active" : "off");
		ImGui::DragFloat("攻撃再生速度", &vrmAttackPlaybackSpeed_, 0.05f, 0.25f, 3.0f);
		ImGui::SliderFloat("先行入力受付開始", &vrmComboBufferStart_, 0.0f, 0.8f);
		ImGui::SliderFloat("次段へつなぐ時点", &vrmComboChainPoint_, 0.2f, 0.95f);
		ImGui::SeparatorText("Right-hand light sword");
		ImGui::DragFloat3("Sword local offset", &vrmSwordLocalOffset_.x, 0.005f, -2.0f, 2.0f);
		ImGui::DragFloat3("Sword local rotation", &vrmSwordLocalRotate_.x, 0.01f, -6.3f, 6.3f);
		ImGui::DragFloat3(
			"Attack2 sword correction", &vrmSwordAttack2RotateCorrection_.x, 0.01f, -3.2f, 3.2f);
		ImGui::DragFloat3("Sword local scale", &vrmSwordLocalScale_.x, 0.005f, 0.001f, 2.0f);
		ImGui::Checkbox("Sword bloom", &enableSwordBloom_);
		BloomParam& swordBloom = swordPostEffect_->GetParam();
		ImGui::DragFloat("Sword bloom intensity", &swordBloom.intensity, 0.05f, 0.0f, 5.0f);
		ImGui::DragFloat("Sword bloom threshold", &swordBloom.threshold, 0.01f, 0.0f, 2.0f);
		ImGui::Checkbox("Sword attack ribbon", &enableSwordTrail_);
		ImGui::ColorEdit4("Ribbon start color", &swordTrailConfig_.startColor.x);
		ImGui::ColorEdit4("Ribbon end color", &swordTrailConfig_.endColor.x);
		ImGui::DragFloat("Ribbon lifetime", &swordTrailConfig_.lifetime, 0.01f, 0.03f, 1.0f);
		ImGui::DragFloat("Ribbon root Y", &vrmSwordTrailBaseY_, 0.02f, 0.0f, 6.0f);
		ImGui::DragFloat("Ribbon tip Y", &vrmSwordTrailTipY_, 0.02f, 0.1f, 10.0f);
		if (ImGui::Button("Idle")) vrmCurrentAnimation_ = "Idle";
		ImGui::SameLine();
		if (ImGui::Button("Walk")) vrmCurrentAnimation_ = "Walk";
		ImGui::SameLine();
		if (ImGui::Button("Run")) vrmCurrentAnimation_ = "Run";
		ImGui::DragFloat3("VRoid Position", &vrmTestObject_->GetTranslate().x, 0.1f);
		ImGui::DragFloat3("VRoid Rotation", &vrmTestObject_->GetRotate().x, 0.01f);
		ImGui::DragFloat3("VRoid Scale", &vrmTestObject_->GetScale().x, 0.1f, 0.01f, 100.0f);
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
	ImGui::Checkbox("Enable slope ground", &enableSlopeGround_);
	ImGui::Text("Slope test: walk to the right side of the character (world +X)");
	ImGui::SliderFloat("Maximum walkable slope", &vrmMaximumSlopeDegrees_, 1.0f, 75.0f, "%.1f deg");
	ImGui::DragFloat("Ground probe up", &vrmGroundProbeUp_, 0.1f, 0.1f, 20.0f);
	ImGui::DragFloat("Ground probe down", &vrmGroundProbeDown_, 0.1f, 0.5f, 50.0f);
	ImGui::Text("Ground normal: %.2f, %.2f, %.2f", vrmGroundNormal_.x, vrmGroundNormal_.y, vrmGroundNormal_.z);
	ImGui::Text("Collision triangles: flat=%zu slope=%zu",
		flatGroundMesh_.GetTriangleCount(), slopeGroundMesh_.GetTriangleCount());
	ImGui::DragFloat3("position", &groundObj_->GetTranslate().x);
	ImGui::DragFloat3("rotate", &groundObj_->GetRotate().x, 0.01f);
	ImGui::End();

#endif // USE_IMGUI

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
	if (vrmTestLoaded_) {
		Vector3 move{};
		Vector3 localMove{};
		bool isMoving = false;
		bool isRunning = false;
		if (enableVrmActionControl_) {
			if (input_->IsPress(input_->GetKey()[DIK_A])) move.x -= 1.0f;
			if (input_->IsPress(input_->GetKey()[DIK_D])) move.x += 1.0f;
			if (input_->IsPress(input_->GetKey()[DIK_W])) move.z += 1.0f;
			if (input_->IsPress(input_->GetKey()[DIK_S])) move.z -= 1.0f;
			localMove = move;
			const float moveLength = std::sqrt(move.x * move.x + move.z * move.z);
			isMoving = moveLength > 0.0001f;
			isRunning = isMoving && input_->IsPress(input_->GetKey()[DIK_LSHIFT]);
			if (isMoving) {
				move.x /= moveLength;
				move.z /= moveLength;
				localMove.x /= moveLength;
				localMove.z /= moveLength;
				if (vrmLockFacing_) {
					const Vector3 forward = { std::sin(vrmFacingYaw_), 0.0f, std::cos(vrmFacingYaw_) };
					const Vector3 right = { std::cos(vrmFacingYaw_), 0.0f, -std::sin(vrmFacingYaw_) };
					move = {
						right.x * localMove.x + forward.x * localMove.z,
						0.0f,
						right.z * localMove.x + forward.z * localMove.z,
					};
				}
			}

				auto returnToLocomotion = [&]() {
				vrmActionState_ = VrmActionState::Locomotion;
				vrmComboStep_ = 0;
				vrmComboQueued_ = false;
				vrmAttackHitActive_ = false;
				vrmTestModel_->SetAnimationLoop(true);
				vrmTestModel_->SetAnimationPlaybackSpeed(1.0f);
				const std::string desired = isMoving ? (isRunning ? "Run" : "Walk") : "Idle";
				vrmTestModel_->TransitionToAnimation(desired, vrmAnimationBlendDuration_, false);
				vrmCurrentAnimation_ = desired;
			};
			const auto key = input_->GetKey();
			const auto preKey = input_->GetPreKey();
			const bool attackTriggered = input_->IsTrigger(key[DIK_F], preKey[DIK_F]);

			if (vrmActionState_ == VrmActionState::Locomotion) {
				if (input_->IsTrigger(key[DIK_SPACE], preKey[DIK_SPACE])) {
					vrmActionState_ = VrmActionState::Jump;
					vrmJumpPhase_ = VrmJumpPhase::Takeoff;
					vrmVerticalVelocity_ = 0.0f;
					vrmTestModel_->TransitionToAnimation("JumpingUp", 0.08f, false);
					vrmTestModel_->SetAnimationLoop(false);
					vrmTestModel_->SetAnimationPlaybackSpeed(vrmTakeoffPlaybackSpeed_);
					vrmCurrentAnimation_ = "JumpingUp";
				} else if (input_->IsTrigger(key[DIK_LCONTROL], preKey[DIK_LCONTROL])) {
					vrmActionState_ = VrmActionState::Dodge;
					vrmDodgeElapsed_ = 0.0f;
					vrmDodgePreviousProgress_ = 0.0f;
					const Vector3 forward = { std::sin(vrmFacingYaw_), 0.0f, std::cos(vrmFacingYaw_) };
					const Vector3 right = { std::cos(vrmFacingYaw_), 0.0f, -std::sin(vrmFacingYaw_) };
					vrmDodgeDirection_ = isMoving ? move : forward;
					const float localForward = vrmLockFacing_ ? localMove.z
						: vrmDodgeDirection_.x * forward.x + vrmDodgeDirection_.z * forward.z;
					const float localRight = vrmLockFacing_ ? localMove.x
						: vrmDodgeDirection_.x * right.x + vrmDodgeDirection_.z * right.z;
					std::string dodgeAnimation;
					if (std::abs(localForward) >= std::abs(localRight)) {
						dodgeAnimation = localForward >= 0.0f ? "DodgeForward" : "DodgeBackward";
					} else {
						dodgeAnimation = localRight >= 0.0f ? "DodgeRight" : "DodgeLeft";
					}
					vrmTestModel_->TransitionToAnimation(dodgeAnimation, 0.06f, false);
					vrmTestModel_->SetAnimationLoop(false);
					vrmTestModel_->SetAnimationPlaybackSpeed(1.0f);
					vrmCurrentAnimation_ = dodgeAnimation;
				} else if (attackTriggered) {
					vrmActionState_ = VrmActionState::Attack;
					if (swordTrail_) {
						swordTrail_->Clear();
					}
					vrmAttackElapsed_ = 0.0f;
					vrmComboStep_ = 1;
					vrmComboQueued_ = false;
					vrmAttackHitActive_ = false;
					vrmTestModel_->TransitionToAnimation("Attack1", 0.07f, false);
					vrmTestModel_->SetAnimationLoop(false);
					vrmTestModel_->SetAnimationPlaybackSpeed(vrmAttackPlaybackSpeed_);
					vrmCurrentAnimation_ = "Attack1";
				}
			}

			if (vrmActionState_ == VrmActionState::Locomotion) {
				if (isMoving) {
					const float speed = vrmActionMoveSpeed_ * (isRunning ? vrmActionRunMultiplier_ : 1.0f);
					vrmActionPosition_.x += move.x * speed * finalDeltaTime;
					vrmActionPosition_.z += move.z * speed * finalDeltaTime;
					if (!vrmLockFacing_) {
						const float targetYaw = std::atan2(move.x, move.z);
						const float yawDifference = std::atan2(
							std::sin(targetYaw - vrmFacingYaw_), std::cos(targetYaw - vrmFacingYaw_));
						vrmFacingYaw_ += yawDifference * (std::min)(1.0f, vrmFacingTurnSpeed_ * finalDeltaTime);
					}
				}
				vrmActionPosition_.y = GetVrmGroundHeight(vrmActionPosition_);
				const std::string desiredAnimation = isMoving ? (isRunning ? "Run" : "Walk") : "Idle";
				if (desiredAnimation != vrmCurrentAnimation_) {
					const bool previousIsLocomotion = vrmCurrentAnimation_ == "Walk" || vrmCurrentAnimation_ == "Run";
					const bool nextIsLocomotion = desiredAnimation == "Walk" || desiredAnimation == "Run";
					vrmTestModel_->SetAnimationLoop(true);
					vrmTestModel_->TransitionToAnimation(
						desiredAnimation, vrmAnimationBlendDuration_, previousIsLocomotion && nextIsLocomotion);
					vrmCurrentAnimation_ = desiredAnimation;
				}
			} else if (vrmActionState_ == VrmActionState::Dodge) {
				vrmDodgeElapsed_ += finalDeltaTime;
				const float duration = (std::max)(0.1f, vrmTestModel_->GetCurrentAnimationDuration());
				const float t = (std::clamp)(vrmDodgeElapsed_ / duration, 0.0f, 1.0f);
				const float progress = t * t * (3.0f - 2.0f * t);
				const float deltaProgress = progress - vrmDodgePreviousProgress_;
				vrmDodgePreviousProgress_ = progress;
				vrmActionPosition_.x += vrmDodgeDirection_.x * vrmDodgeDistance_ * deltaProgress;
				vrmActionPosition_.z += vrmDodgeDirection_.z * vrmDodgeDistance_ * deltaProgress;
				vrmActionPosition_.y = GetVrmGroundHeight(vrmActionPosition_);
				if (t >= 1.0f) returnToLocomotion();
			} else if (vrmActionState_ == VrmActionState::Jump) {
				if (vrmJumpPhase_ == VrmJumpPhase::Takeoff) {
					const float duration = (std::max)(
						vrmTestModel_->GetCurrentAnimationDuration(), 0.001f);
					const float takeoffPhase = vrmTestModel_->GetCurrentAnimationTime() / duration;
					if (takeoffPhase >= vrmTakeoffLaunchPhase_) {
						vrmJumpPhase_ = VrmJumpPhase::Rising;
						vrmVerticalVelocity_ = vrmJumpSpeed_;
					}
				}

				if (vrmJumpPhase_ == VrmJumpPhase::Rising || vrmJumpPhase_ == VrmJumpPhase::Falling) {
					if (isMoving) {
						vrmActionPosition_.x += move.x * vrmActionMoveSpeed_ * 0.55f * finalDeltaTime;
						vrmActionPosition_.z += move.z * vrmActionMoveSpeed_ * 0.55f * finalDeltaTime;
					}
					vrmVerticalVelocity_ -= vrmGravity_ * finalDeltaTime;
					vrmActionPosition_.y += vrmVerticalVelocity_ * finalDeltaTime;
				}

				if (vrmJumpPhase_ == VrmJumpPhase::Rising && vrmVerticalVelocity_ <= 0.0f) {
					vrmJumpPhase_ = VrmJumpPhase::Falling;
					vrmTestModel_->TransitionToAnimation("FallingIdle", 0.08f, false);
					vrmTestModel_->SetAnimationLoop(true);
					vrmTestModel_->SetAnimationPlaybackSpeed(1.0f);
					vrmCurrentAnimation_ = "FallingIdle";
				}

				const float currentGroundHeight = GetVrmGroundHeight(vrmActionPosition_);
				if (vrmJumpPhase_ != VrmJumpPhase::Landing &&
					vrmActionPosition_.y <= currentGroundHeight && vrmVerticalVelocity_ < 0.0f) {
					vrmActionPosition_.y = currentGroundHeight;
					vrmVerticalVelocity_ = 0.0f;
					vrmJumpPhase_ = VrmJumpPhase::Landing;
					vrmTestModel_->TransitionToAnimation("FallingToLanding", 0.05f, false);
					vrmTestModel_->SetAnimationLoop(false);
					vrmTestModel_->SeekCurrentAnimation(
						vrmLandingStartPhase_ * vrmTestModel_->GetCurrentAnimationDuration());
					vrmTestModel_->SetAnimationPlaybackSpeed(vrmLandingPlaybackSpeed_);
					vrmCurrentAnimation_ = "FallingToLanding";
				} else if (vrmJumpPhase_ == VrmJumpPhase::Landing &&
					!vrmTestModel_->IsCurrentAnimationPlaying()) {
					returnToLocomotion();
				}
			} else if (vrmActionState_ == VrmActionState::Attack) {
				vrmAttackElapsed_ += finalDeltaTime;
				const float duration = (std::max)(vrmTestModel_->GetCurrentAnimationDuration(), 0.001f);
				const float normalizedTime = (std::clamp)(
					vrmTestModel_->GetCurrentAnimationTime() / duration, 0.0f, 1.0f);
				vrmAttackHitActive_ = normalizedTime >= 0.28f && normalizedTime <= 0.62f;
				if (attackTriggered && vrmComboStep_ < 3 && normalizedTime >= vrmComboBufferStart_) {
					vrmComboQueued_ = true;
				}
				if (vrmComboQueued_ && vrmComboStep_ < 3 && normalizedTime >= vrmComboChainPoint_) {
					++vrmComboStep_;
					vrmComboQueued_ = false;
					vrmAttackHitActive_ = false;
					const std::string nextAttack = "Attack" + std::to_string(vrmComboStep_);
					vrmTestModel_->TransitionToAnimation(nextAttack, 0.055f, false);
					vrmTestModel_->SetAnimationLoop(false);
					vrmTestModel_->SetAnimationPlaybackSpeed(vrmAttackPlaybackSpeed_);
					vrmCurrentAnimation_ = nextAttack;
				} else if (!vrmTestModel_->IsCurrentAnimationPlaying()) {
					returnToLocomotion();
				}
			}
			vrmTestObject_->SetRotate({ 0.0f, vrmFacingYaw_, 0.0f });
		}
		vrmTestObject_->SetTranslate(vrmActionPosition_);
		vrmTestModel_->Update(finalDeltaTime);
		vrmTestObject_->Update();

		vrmSwordAttached_ = false;
		const Skeleton& skeleton = vrmTestModel_->GetSkeleton();
		const auto hand = skeleton.jointMap.find("J_Bip_R_Hand");
		if (hand != skeleton.jointMap.end()) {
			Vector3 swordRotation = vrmSwordLocalRotate_;
			if (vrmActionState_ == VrmActionState::Attack && vrmComboStep_ == 2) {
				swordRotation.x += vrmSwordAttack2RotateCorrection_.x;
				swordRotation.y += vrmSwordAttack2RotateCorrection_.y;
				swordRotation.z += vrmSwordAttack2RotateCorrection_.z;
			}
			const Matrix4x4 swordLocal = MakeAffineMatrix(
				vrmSwordLocalScale_, swordRotation, vrmSwordLocalOffset_);
			const Matrix4x4 characterWorld = MakeAffineMatrix(
				vrmTestObject_->GetScale(), vrmTestObject_->GetRotate(), vrmTestObject_->GetTranslate());
			const Matrix4x4 swordWorld = Multiply(
				Multiply(swordLocal, skeleton.joints[hand->second].skeletonSpaceMatrix), characterWorld);
			swordObj_->UpdateWithWorldMatrix(swordWorld);
			vrmSwordAttached_ = true;

			const bool recordSwordTrail = enableSwordTrail_ &&
				vrmActionState_ == VrmActionState::Attack && vrmAttackHitActive_;
			swordTrail_->SetActive(recordSwordTrail);
			if (recordSwordTrail) {
				auto transformSwordPoint = [&](float localY) {
					Vector3 result = TransformNormal({ 0.0f, localY, 0.0f }, swordWorld);
					result.x += swordWorld.m[3][0];
					result.y += swordWorld.m[3][1];
					result.z += swordWorld.m[3][2];
					return result;
				};
				swordTrail_->Update(
					finalDeltaTime,
					transformSwordPoint(vrmSwordTrailTipY_),
					transformSwordPoint(vrmSwordTrailBaseY_),
					swordTrailConfig_);
			}
		}
		if (!vrmSwordAttached_ && swordTrail_) {
			swordTrail_->SetActive(false);
		}
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
	swordPostEffect_->Update(finalDeltaTime);
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
	}

	groundObj_->Draw();
	if (enableSlopeGround_) {
		slopeGroundObj_->Draw();
	}
	if (showSimpleSkin_ && skinClusterLoaded_) {
		skinnedTestObject_->DrawSkinned(*skinnedTestModel_);
	}
	if (showHumanSkinning_ && humanSkinningLoaded_) {
		humanTestObject_->DrawSkinned(*humanTestModel_);
	}
	if (showVrmTestModel_ && vrmTestLoaded_) {
		vrmTestObject_->DrawSkinned(*vrmTestModel_);
		if (vrmSwordAttached_) {
			swordObj_->Draw();
		}
	}
	if (showVrmTestModel_ && vrmSwordAttached_ && enableSwordBloom_) {
		swordPostEffect_->BeginCaptureWithCurrentDepth();
		Object3dCommon::GetInstance()->PreDraw(kNone);
		swordObj_->Draw();
		swordPostEffect_->EndCaptureBloomOnly();
		Object3dCommon::GetInstance()->PreDraw(kNone);
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
	trailManager_->DrawAll(vp);
	if (enableSwordTrail_ && enableSwordBloom_ && swordTrail_ && !swordTrail_->GetPoints().empty()) {
		swordPostEffect_->BeginCaptureWithCurrentDepth();
		trailManager_->DrawAll(vp);
		swordPostEffect_->EndCaptureBloomOnly();
		Object3dCommon::GetInstance()->PreDraw(kNone);
	}
	if (showLegacyTestObjects_) {
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
	}
	if (enableSlopeGround_) {
		slopeGroundObj_->DrawShadow();
	}
	if (showSimpleSkin_ && skinClusterLoaded_) {
		skinnedTestObject_->DrawSkinnedShadow(*skinnedTestModel_);
	}
	if (showHumanSkinning_ && humanSkinningLoaded_) {
		humanTestObject_->DrawSkinnedShadow(*humanTestModel_);
	}
	if (showVrmTestModel_ && vrmTestLoaded_) {
		vrmTestObject_->DrawSkinnedShadow(*vrmTestModel_);
		if (vrmSwordAttached_) {
			swordObj_->DrawShadow();
		}
	}
}

void TestScene::DrawSprite() {

}
