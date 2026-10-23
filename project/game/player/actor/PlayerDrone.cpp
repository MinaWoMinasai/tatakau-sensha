#include "game/weapon/CombatTypes.h"
#include "PlayerDrone.h"
#include "game/enemy/visual/NeonDepthPlacement.h"
#include "Stage.h"
#include "game/exp/ExpEnemy.h"

PlayerDrone::~PlayerDrone() {}

void PlayerDrone::ConfigureRunAttack(const AttackParam& param, float reloadSeconds)
{
    runAttackParam_ = param;
    runAttackParam_.bulletCount = 1;
    runReloadSeconds_ = (std::max)(0.05f, reloadSeconds);
}

void PlayerDrone::Attack(float deltaTime)
{
    if (isDead_ || hp_ <= 0) {
        return;
    }
    runShotCooldown_ = (std::max)(0.0f, runShotCooldown_ - deltaTime);
    // 待ち時間は任務中も減らすが、通常射撃は護衛中だけ受け付ける。
    if (mission_.GetPhase() != tankspecial::DronePhase::Escort) {
        return;
    }
    const bool wantsAttack =
        runInputOverride_ ? runWantsAttack_ : (runRallyShotPending_ || input_->IsPress(input_->GetMouseState().rgbButtons[0]));
    if (runShotCooldown_ <= 0.0f && wantsAttack && runBulletManager_ &&
        runBulletManager_->GetBulletCounts().player + static_cast<size_t>(runAttackParam_.bulletCount) <= 240) {
        auto shot = runAttackParam_;
        // 整数化で失う小数を次の射撃へ繰り越し、継続射撃で設定した威力へ近づける。
        if (runExactDamage_ >= 1) {
            const float accrued = runExactDamage_ + runDamageRemainder_;
            shot.damage = static_cast<uint32_t>(std::floor(accrued));
            runDamageRemainder_ = accrued - static_cast<float>(shot.damage);
        }
        attackController_.Fire(GetWorldPosition(), dir, shot, BulletOwner::kPlayer);
        runShotCooldown_ = runReloadSeconds_;
        runRallyShotPending_ = false;
    }
}

void PlayerDrone::RotateToMouse(myEngine::Camera* viewProjection)
{
    // 自機が渡した照準位置を使う場合はマウス座標の逆変換を省く。ほぼ同位置なら前の向きを保つ。
    if (runInputOverride_) {
        const myEngine::Vector3 aim = runAimTarget_ - worldTransform_.translate;
        if (myEngine::Length(aim) > 0.001f) {
            dir = myEngine::Normalize(aim);
        }
        angle_ = std::atan2(dir.y, dir.x);
        worldTransform_.rotate.z = angle_;
        object_->SetRotate(worldTransform_.rotate);
        return;
    }
    if (input_ && input_->IsGamepadActive()) {
        myEngine::Vector2 aim{};
        if (input_->TryGetGamepadAim(aim)) {
            dir = myEngine::Normalize(myEngine::Vector3{aim.x, aim.y, 0.0f});
        }
        angle_ = std::atan2(dir.y, dir.x);
        worldTransform_.rotate.z = angle_;
        object_->SetRotate(worldTransform_.rotate);
        return;
    }
    if (neonDepthAimEnabled_) {
        if (!viewProjection) {
            return;
        }
        auto* window = myEngine::WinApp::GetInstance();
        POINT mousePosition{};
        if (!window || !GetCursorPos(&mousePosition) || !ScreenToClient(window->GetHwnd(), &mousePosition)) {
            return;
        }
        constexpr myEngine::Vector2 viewport{1280.0f, 720.0f};
        myEngine::Vector2 logicalMouse{static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)};
        window->ClientToLogical(logicalMouse.x, logicalMouse.y);
        myEngine::Vector2 pixel{};
        myEngine::Vector3 target{};
        if (!neondepth::TryClientToViewport(logicalMouse, viewport, viewport, pixel) ||
            !neondepth::TryScreenToFloorFromViewProjection(pixel, viewport, viewProjection->GetViewProjectionMatrix(), 0.0f, target)) {
            return;
        }
        const myEngine::Vector3 offset = target - worldTransform_.translate;
        const float length = myEngine::Length(offset);
        if (!neondepth::Finite(offset) || !std::isfinite(length) || length <= 0.001f) {
            return;
        }
        dir = myEngine::Normalize(offset);
        angle_ = std::atan2(dir.y, dir.x);
        worldTransform_.rotate.z = angle_;
        object_->SetRotate(worldTransform_.rotate);
        return;
    }
    // --- 1. マウス座標取得 ---
    POINT mousePosition;
    GetCursorPos(&mousePosition);
    HWND hwnd = myEngine::WinApp::GetInstance()->GetHwnd();
    ScreenToClient(hwnd, &mousePosition);
    float logicalMouseX = static_cast<float>(mousePosition.x);
    float logicalMouseY = static_cast<float>(mousePosition.y);
    myEngine::WinApp::GetInstance()->ClientToLogical(logicalMouseX, logicalMouseY);

    // --- 2. 逆変換用の行列を準備 ---
    myEngine::Matrix4x4 matViewport =
        myEngine::MakeViewportMatrix(0, 0, myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight, 0, 1);
    myEngine::Matrix4x4 matVPV = viewProjection->GetViewMatrix() * viewProjection->GetProjectionMatrix() * matViewport;
    myEngine::Matrix4x4 matInverseVPV = myEngine::Inverse(matVPV);

    // --- 3. マウス座標をワールドに変換 ---
    myEngine::Vector3 posNear = myEngine::Vector3(logicalMouseX, logicalMouseY, 0);
    myEngine::Vector3 posFar = myEngine::Vector3(logicalMouseX, logicalMouseY, 1);

    posNear = myEngine::TransformMatrix(posNear, matInverseVPV);
    posFar = myEngine::TransformMatrix(posFar, matInverseVPV);

    // 画面の近点・遠点からレイを作り、ゲームのXY平面（Z=0）で照準位置を求める。
    myEngine::Vector3 mouseDirection = posFar - posNear;
    myEngine::Vector3 rayDir = myEngine::Normalize(mouseDirection);
    float t = -posNear.z / rayDir.z;
    myEngine::Vector3 target = posNear + rayDir * t;

    // --- 5. プレイヤーの位置と方向ベクトル ---
    myEngine::Vector3 playerPos = worldTransform_.translate;
    myEngine::Vector3 targetPos = target - playerPos;
    dir = myEngine::Normalize(targetPos);

    // --- 6. 回転角度を算出 ---
    angle_ = atan2(dir.y, dir.x);
    worldTransform_.rotate.z = angle_;
    object_->SetRotate(worldTransform_.rotate);
}

void PlayerDrone::Initialize(const myEngine::Vector3& position, const myEngine::Vector3& velocity)
{
    neonVisual_ = false;
    object_ = std::make_unique<myEngine::Object3d>();
    object_->Initialize();

    object_->SetModel("enemy.obj");

    worldTransform_ = myEngine::InitWorldTransform();
    worldTransform_.translate = position;
    object_->SetTransform(worldTransform_);
    object_->Update();

    velocity_ = velocity;

    // シングルトンインスタンス
    input_ = myEngine::Input::GetInstance();

    SetDamage(1);

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributePlayerDrone);
    // 通知を受ける相手の属性を登録する。相手側のマスクでも接触が許可される点はCollisionManagerに従う。
    SetCollisionMask(kCollisionAttributePlayerDrone | kCollisionAttributeEnemyBullet | kCollisionAttributeEnemy |
                     kCollisionAttributeExpEnemy);
}

void PlayerDrone::Update(myEngine::Camera* viewProjection, Stage& stage, const myEngine::Vector3& playerPosition, float deltaTime)
{
    // ダメージを受けると死亡は即座に確定する。終了したアクターは移動も射撃もさせない。
    if (isDead_ || hp_ <= 0) {
        Die();
        return;
    }
    // 渡された秒数で進める。
    const float dt = (std::max)(0.0f, deltaTime);
    invincibleTimer_ -= dt;
    // Stepは時間による遷移と再構築完了を扱う。到着時のダメージはここではなく自機の追加能力更新で行う。
    const auto previousPhase = mission_.GetPhase();
    const bool home = myEngine::Length(playerPosition + runFollowOffset_ - GetWorldPosition()) < 1.2f;
    if (mission_.Step(dt, home)) {
        rebuilt_ = true;
        hp_ = kMaxHp;
        SetWorldPosition(playerPosition);
    }
    // 更新開始時に再構築中なら、完了した回も配置だけにする。移動・射撃の再開は次の更新から。
    if (previousPhase == tankspecial::DronePhase::Rebuilding) {
        SetWorldPosition(playerPosition + runFollowOffset_ * .45f);
        velocity_ = {};
        return;
    }

    RotateToMouse(viewProjection);

    myEngine::Vector3 toPlayer = playerPosition + runFollowOffset_ - worldTransform_.translate;
    // 予告中は停止し、突撃中は登録した位置へ向かう。通常の帰還・護衛は自機の追従位置を使う。
    if (mission_.GetPhase() == tankspecial::DronePhase::Warning) {
        toPlayer = {};
    }
    if (mission_.GetPhase() == tankspecial::DronePhase::Charging) {
        toPlayer = missionTarget_ - GetWorldPosition();
        if (myEngine::Length(toPlayer) < 1.1f) {
            mission_.Arrive();
            toPlayer = {};
        }
    }

    float distance = myEngine::Length(toPlayer);

    // 自機付近で静止していても、後段の任務到着・射撃処理まで進める。
    myEngine::Vector3 followDirection = distance < 0.01f ? myEngine::Vector3{} : myEngine::Normalize(toPlayer);

    // 追従速度は距離に応じて増やし、追い付き速度の設定値を上限にする。
    const float followSpeed =
        mission_.GetPhase() == tankspecial::DronePhase::Charging ? .90f : (std::min)(runCatchupSpeed_, runFollowSpeed_ + distance * 0.035f);
    myEngine::Vector3 targetVelocity = followDirection * (followSpeed * (std::min)(1.0f, distance / 1.2f));

    // 追従は秒数を使った指数補間。突撃は目標速度へ直接切り替え、予告では速度を0にする。
    float accel = runFollowResponse_;

    velocity_ += (targetVelocity - velocity_) * (1.0f - std::exp(-accel * dt));
    if (mission_.GetPhase() == tankspecial::DronePhase::Charging) {
        velocity_ = targetVelocity;
    }
    if (mission_.GetPhase() == tankspecial::DronePhase::Warning) {
        velocity_ = {};
    }

    // 速度を基準フレームから今回の移動量へ換算し、細分化してX・Yの順に地形へ通知する。
    myEngine::Vector3 frameMove = GetMove() * (dt * 60.0f);
    const float maxStep = 0.30f;
    const int subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(frameMove.x), std::abs(frameMove.y)) / maxStep) + 1);
    myEngine::Vector3 stepMove = frameMove / static_cast<float>(subStepCount);
    for (int i = 0; i < subStepCount; ++i) {
        const myEngine::Vector3 previous = GetWorldPosition();
        myEngine::Vector3 playerPos = GetWorldPosition();
        playerPos.x += stepMove.x;
        SetWorldPosition(playerPos);
        stage.ResolvePlayerDroneCollision(*this, myEngine::X);

        playerPos = GetWorldPosition();
        playerPos.y += stepMove.y;
        SetWorldPosition(playerPos);
        stage.ResolvePlayerDroneCollision(*this, myEngine::Y);
        // 小刻みな移動の間に目標を通り過ぎた場合も、線分の接触でArriveを通知する。
        if (mission_.GetPhase() == tankspecial::DronePhase::Charging) {
            const auto p = GetWorldPosition();
            if (tankspecial::SegmentTouches(previous.x, previous.y, p.x, p.y, missionTarget_.x, missionTarget_.y, 1.1f)) {
                mission_.Arrive();
            }
        }
    }

    myEngine::Vector3 pos = GetWorldPosition();

    // ワールド座標からマップインデックスに変換
    int xIndex = static_cast<int>(pos.x / MapChip::kBlockWidth);
    int yIndex = static_cast<int>(MapChip::kNumBlockVirtical - 1 - (pos.y / MapChip::kBlockHeight));

    // ここではマップ配列の範囲だけを調べる。範囲内のブロックとの衝突は上のStageの処理で解決する。
    if (xIndex < 0 || xIndex >= static_cast<int>(MapChip::kNumBlockHorizontal) || yIndex < 0 ||
        yIndex >= static_cast<int>(MapChip::kNumBlockVirtical)) {
        Die(); // そもそもマップ配列の範囲外なら死亡
    } else {
        // 範囲内なら追加処理なし。
    }

    // object_ の更新だけ（移動はしない）
    object_->SetTransform(worldTransform_);
    object_->Update();

    if (!isDead_) {
        // 攻撃処理
        Attack(dt);
    }

    if (hp_ <= 0) {
        Die();
    }
}

bool PlayerDrone::IsVisualVisible() const
{
    return !isDead_ && mission_.Available() && !(invincibleTimer_ > 0.0f && static_cast<int>(invincibleTimer_ * 10) % 2 == 0);
}

void PlayerDrone::Draw()
{
    if (!neonVisual_ && IsVisualVisible()) {
        object_->Draw();
    }
}

void PlayerDrone::DrawSprite() {}

myEngine::Vector3 PlayerDrone::GetWorldPosition() const
{
    myEngine::Vector3 worldPos;
    worldPos.x = worldTransform_.translate.x;
    worldPos.y = worldTransform_.translate.y;
    worldPos.z = worldTransform_.translate.z;

    return worldPos;
}

void PlayerDrone::OnCollision(Collider* other)
{
    if (isDead_ || hp_ <= 0) {
        return;
    }
    // 再構築中と資源への接触は対象外。予告・突撃・帰還はAvailableに含まれる。
    if (!mission_.Available()) {
        return;
    }
    if (const auto* resource = dynamic_cast<const ExpEnemy*>(other); resource && resource->IsRunResource()) {
        return;
    }

    // 接触によるHP減少はDamage()とは別経路で、ここでは無敵タイマーを検査しない。
    if (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet || other->GetCollisionAttribute() == kCollisionAttributeEnemy ||
        other->GetCollisionAttribute() == kCollisionAttributeExpEnemy) {
        hp_--;
        if (hp_ <= 0) {
            Die();
            return;
        }
    }

    myEngine::Vector3 hitDir = worldTransform_.translate - other->GetWorldPosition();

    if (myEngine::Length(hitDir) < 0.0001f) {
        return;
    }

    hitDir = myEngine::Normalize(hitDir);

    // 接触方向へ押し出し、保持する速度の大きさを上限へ制限する。
    const float kKnockBackPower = 0.1f;

    velocity_ += hitDir * kKnockBackPower * other->GetHitPower();
    const float maxKnockSpeed = 0.22f;
    if (myEngine::Length(velocity_) > maxKnockSpeed) {
        velocity_ = myEngine::Normalize(velocity_) * maxKnockSpeed;
    }
}

myEngine::AABB PlayerDrone::GetAABB()
{
    myEngine::Vector3 worldPos = GetWorldPosition();

    myEngine::AABB aabb;

    aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
    aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

    return aabb;
}

void PlayerDrone::Damage()
{
    if (isDead_ || hp_ <= 0) {
        return;
    }
    if (invincibleTimer_ <= 0.0f) {
        hp_--;
        invincibleTimer_ = 2.0f;
        if (hp_ <= 0) {
            Die();
        }
    }
}

void PlayerDrone::Die()
{
    if (isDead_) {
        return;
    }

    hp_ = 0;
    velocity_ = {};
    isDead_ = true;
}
