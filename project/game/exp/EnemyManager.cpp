#include "EnemyManager.h"
#include "Stage.h"
#include "Player.h"
#include "Calculation.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace {
static_assert(static_cast<int>(ExpEnemyType::ReflectArmor) == static_cast<int>(tankcontent::EnemyBehavior::ReflectArmor),
              "Authored behavior IDs must keep the same enum order as runtime enemies");

/// @brief 互換prefab名を実行時の種類へ変換する。未対応ならtypeを変更せずfalseを返す。
bool TryGetExpEnemyType(const std::string& prefab, ExpEnemyType& type)
{
    if (prefab == "Default" || prefab == "Basic" || prefab == "Square") {
        type = ExpEnemyType::Square;
        return true;
    }
    if (prefab == "Triangle") {
        type = ExpEnemyType::Triangle;
        return true;
    }
    if (prefab == "Pentagon") {
        type = ExpEnemyType::Pentagon;
        return true;
    }
    if (prefab == "Shooter") {
        type = ExpEnemyType::Shooter;
        return true;
    }
    if (prefab == "Charger") {
        type = ExpEnemyType::Charger;
        return true;
    }
    if (prefab == "Sniper") {
        type = ExpEnemyType::Sniper;
        return true;
    }
    if (prefab == "Skirmisher") {
        type = ExpEnemyType::Skirmisher;
        return true;
    }
    if (prefab == "Flanker") {
        type = ExpEnemyType::Flanker;
        return true;
    }
    if (prefab == "Suppressor") {
        type = ExpEnemyType::Suppressor;
        return true;
    }
    if (prefab == "ShieldGuard") {
        type = ExpEnemyType::ShieldGuard;
        return true;
    }
    if (prefab == "BladeGuard") {
        type = ExpEnemyType::BladeGuard;
        return true;
    }
    if (prefab == "SummonerCommander") {
        type = ExpEnemyType::SummonerCommander;
        return true;
    }
    if (prefab == "EMPJammer") {
        type = ExpEnemyType::EMPJammer;
        return true;
    }
    if (prefab == "ReflectArmor") {
        type = ExpEnemyType::ReflectArmor;
        return true;
    }
    return false;
}

} // namespace

void EnemyManager::Initialize(Player* player, BulletManager* bulletManager, Enemy* boss)
{
    player_ = player;
    bulletManager_ = bulletManager;
    boss_ = boss;
}

void EnemyManager::Update(Stage& stage, float deltaTime)
{
    // ランダム生成と領域生成は、死亡実体を取り除く前の管理数を上限に使う。
    if (defaultRandomSpawnEnabled_) {
        spawnTimer_ -= deltaTime;
    }
    if (defaultRandomSpawnEnabled_ && spawnTimer_ <= 0.0f && enemies_.size() < kMaxEnemies) {
        Spawn(stage);
        spawnTimer_ = kSpawnInterval;
    }
    UpdateLevelSpawnAreas(stage, deltaTime);
    // 召喚元が既に死亡していれば、この更新でその召喚個体も削除対象にする。
    DismissOrphanedSummons();

    // 生存個体だけを更新し、既に死亡していた個体と更新中に期限切れとなった個体を破棄する。
    // 後続の衝突で死亡した個体は、次のUpdateまで借用先として残る。
    for (auto it = enemies_.begin(); it != enemies_.end();) {
        if (!(*it)->IsDead())
            (*it)->Update(stage, deltaTime);

        if ((*it)->IsDead()) {
            it = enemies_.erase(it); // 削除
        } else {
            ++it;
        }
    }
    // 全個体の走査が終わってから要求を回収し、生成した召喚個体は次回から更新する。
    UpdateSummonedUnits(stage);
}

void EnemyManager::DismissOrphanedSummons()
{
    std::unordered_set<uint64_t> commanders;
    for (const auto& actor : enemies_)
        if (actor && !actor->IsDead() && actor->GetType() == ExpEnemyType::SummonerCommander)
            commanders.insert(actor->GetCollisionId());
    for (auto& actor : enemies_)
        if (actor && actor->IsSummonedUnit() && !commanders.contains(actor->GetSummonerId()))
            actor->DismissSummonedUnit();
}

void EnemyManager::UpdateSummonedUnits(Stage& stage)
{
    // 要求を先に消費して指揮官を集め、enemies_を走査している間は追加しない。
    // vectorの再確保は所有ポインターの配列を移すが、指揮官の実体アドレスは保たれる。
    // 召喚は期限付きの無報酬個体として生成し、出現領域には登録しない。
    std::vector<ExpEnemy*> commanders;
    size_t liveSummons = 0;
    for (const auto& actor : enemies_)
        if (actor && !actor->IsDead()) {
            if (actor->IsSummonedUnit())
                ++liveSummons;
            if (actor->GetType() == ExpEnemyType::SummonerCommander && actor->ConsumeSummonRequest())
                commanders.push_back(actor.get());
        }
    for (auto* commander : commanders) {
        int owned = 0;
        for (const auto& actor : enemies_)
            if (actor && !actor->IsDead() && actor->GetSummonerId() == commander->GetCollisionId())
                ++owned;
        // 所有者の同時生存数と生成累計を分け、全体の生存召喚数・管理個体数も制限する。
        const int slots = expguard::SummonSlots(owned, commander->GetSummonTotal());
        for (int slot = 0; slot < slots && liveSummons < 24 && enemies_.size() < 128; ++slot) {
            bool spawned = false;
            for (int candidate = 0; candidate < 12; ++candidate) {
                const float angle = (static_cast<float>(candidate) + slot * 4.0f) * 0.52359878f;
                const cg2::Vector3 position =
                    commander->GetWorldPosition() + cg2::Vector3{std::cos(angle) * 2.8f, std::sin(angle) * 2.8f, 0};
                if (stage.IsCollisionWithAnyBlock(position, 0.85f) ||
                    (player_ && cg2::Length(position - player_->GetWorldPosition()) < 3.0f))
                    continue;
                bool occupied = false;
                for (const auto& actor : enemies_)
                    if (actor && !actor->IsDead() && cg2::Length(position - actor->GetWorldPosition()) < 1.8f) {
                        occupied = true;
                        break;
                    }
                if (occupied)
                    continue;
                auto unit = std::make_unique<ExpEnemy>();
                unit->Initialize(position, player_, ExpEnemyType::Charger);
                unit->SetBossTarget(boss_);
                unit->SetAttackControllerBulletManager(bulletManager_);
                unit->ConfigureSummonedUnit(commander->GetCollisionId());
                enemies_.push_back(std::move(unit));
                commander->RecordSummonedUnit();
                ++liveSummons;
                spawned = true;
                break;
            }
            if (!spawned)
                break; // 空き位置がなければ消費済み要求を戻さず、次のパルスまで待つ。
        }
    }
}

void EnemyManager::Spawn(Stage& stage)
{
    const float kMapMin = MapChip::kBlockWidth * 2.0f;
    const float kMapMaxX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 3.0f);
    const float kMapMaxY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 3.0f);

    for (int i = 0; i < 10; ++i) { // 最大10回リトライ
        cg2::Vector3 spawnPos = {cg2::Rand(kMapMin, kMapMaxX), cg2::Rand(kMapMin, kMapMaxY), 0.0f};

        // 壁と重なっていないか確認
        if (!stage.IsCollisionWithAnyBlock(spawnPos, 1.0f)) {
            // プレイヤーのすぐ近くには出さない（安全のため）
            float dist = cg2::Length(spawnPos - player_->GetWorldPosition());
            if (dist < 10.0f)
                continue;

            ExpEnemyType type = ExpEnemyType::Square;
            const int roll = static_cast<int>(cg2::Rand(0.0f, 100.0f));
            if (roll >= 88) {
                type = ExpEnemyType::Pentagon;
            } else if (roll >= 68) {
                type = ExpEnemyType::Shooter;
            } else if (roll >= 38) {
                type = ExpEnemyType::Triangle;
            }

            // 生成
            auto newEnemy = std::make_unique<ExpEnemy>();
            newEnemy->Initialize(spawnPos, player_, type);
            newEnemy->SetBossTarget(boss_);
            newEnemy->SetAttackControllerBulletManager(bulletManager_);
            enemies_.push_back(std::move(newEnemy));
            break;
        }
    }
}

void EnemyManager::Draw(bool drawBody)
{
    if (!drawBody) {
        return;
    }
    DrawBodyOnly();
}

void EnemyManager::DrawBodyOnly()
{
    for (auto& enemy : enemies_) {
        enemy->DrawBodyOnly();
    }
}

bool EnemyManager::SpawnLevelEnemy(const cg2::Vector3& position, const std::string& prefab, int hp)
{
    ExpEnemyType type = ExpEnemyType::Square;
    // 制作カタログを優先し、見つからない識別子だけ互換prefab名として解決する。
    const auto* authored = useExpeditionContent_ ? tankcontent::FindEnemy(expeditionContent_, prefab) : nullptr;
    if (authored)
        type = static_cast<ExpEnemyType>(authored->behavior);
    else if (!TryGetExpEnemyType(prefab, type)) {
        std::cerr << "[LevelLoader] Unsupported Enemy prefab: " << prefab << std::endl;
        return false;
    }

    auto newEnemy = std::make_unique<ExpEnemy>();
    newEnemy->Initialize(position, player_, type);
    if (authored)
        newEnemy->ApplyAuthoredDefinition(*authored);
    newEnemy->SetBossTarget(boss_);
    newEnemy->SetAttackControllerBulletManager(bulletManager_);
    if (hp > 0) {
        newEnemy->SetHp(hp);
    }
    enemies_.push_back(std::move(newEnemy));
    return true;
}

void EnemyManager::AddLevelSpawnArea(const SpawnArea& spawnArea)
{
    if (!spawnArea.enabled) {
        return;
    }
    SpawnArea area = spawnArea;
    area.spawnInterval = (std::max)(0.1f, area.spawnInterval);
    area.maxAlive = (std::max)(0, area.maxAlive);
    area.size.x = (std::max)(0.0f, area.size.x);
    area.size.y = (std::max)(0.0f, area.size.y);
    spawnAreas_.push_back(area);
}

ExpEnemy* EnemyManager::SpawnRunResource(const cg2::Vector3& position, int hp, std::function<void(bool playerOwned)> onClaim)
{
    auto resource = std::make_unique<ExpEnemy>();
    resource->Initialize(position, player_, ExpEnemyType::Pentagon);
    resource->SetBossTarget(boss_);
    resource->SetAttackControllerBulletManager(bulletManager_);
    resource->SetHp((std::max)(1, hp));
    resource->SetRunResource(std::move(onClaim));
    ExpEnemy* result = resource.get();
    enemies_.push_back(std::move(resource));
    return result;
}

void EnemyManager::ClearLevelData()
{
    spawnAreas_.clear();
    enemies_.clear();
    spawnTimer_ = 0.0f;
}

void EnemyManager::ClearRunActors()
{
    ClearLevelData();
    defaultRandomSpawnEnabled_ = false;
    // 撃破の共有コールバックはシーンが設定しており、部屋移動時の消去でも保持する。
}

void EnemyManager::SetExpEnemyHostileToBoss(bool hostile)
{
    ExpEnemy::EnemyInteractionConfig config{};
    config.hostileToBoss = hostile;
    ExpEnemy::SetEnemyInteractionConfig(config);
    for (auto& enemy : enemies_) {
        if (enemy) {
            enemy->RefreshCollisionMask();
        }
    }
}

ExpEnemy* EnemyManager::FindNearestEnemy(const cg2::Vector3& position, float maxDistance, bool includeShooters) const
{
    ExpEnemy* nearest = nullptr;
    float bestDistance = maxDistance;
    for (const auto& enemy : enemies_) {
        // includeShooters=falseの除外条件は、現在はすべての移動戦闘役にも及ぶ。
        if (!enemy || enemy->IsDead() || (!includeShooters && enemy->IsCombatThreat())) {
            continue;
        }
        const float distance = cg2::Length(enemy->GetWorldPosition() - position);
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = enemy.get();
        }
    }
    return nearest;
}

ExpEnemy* EnemyManager::FindNearestRunResource(const cg2::Vector3& position, float maxDistance) const
{
    ExpEnemy* nearest = nullptr;
    float bestDistance = maxDistance;
    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead() || !enemy->IsRunResource())
            continue;
        const float distance = cg2::Length(enemy->GetWorldPosition() - position);
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = enemy.get();
        }
    }
    return nearest;
}

void EnemyManager::UpdateLevelSpawnAreas(Stage& stage, float deltaTime)
{
    for (SpawnArea& area : spawnAreas_) {
        if (!area.enabled || area.maxAlive <= 0 || enemies_.size() >= kMaxEnemies) {
            continue;
        }

        area.timer -= deltaTime;
        if (area.timer > 0.0f || CountEnemiesInArea(area) >= area.maxAlive) {
            continue;
        }

        for (int i = 0; i < 10; ++i) {
            cg2::Vector3 spawnPos = {cg2::Rand(area.center.x - area.size.x * 0.5f, area.center.x + area.size.x * 0.5f),
                                     cg2::Rand(area.center.y - area.size.y * 0.5f, area.center.y + area.size.y * 0.5f), area.center.z};
            if (stage.IsCollisionWithAnyBlock(spawnPos, 1.0f)) {
                continue;
            }
            if (player_ && cg2::Length(spawnPos - player_->GetWorldPosition()) < 6.0f) {
                continue;
            }
            SpawnLevelEnemy(spawnPos, area.prefab, area.hp);
            break;
        }
        // 壁や自機付近で全候補が失敗した場合やprefabが未対応の場合も、次の試行まで間隔を空ける。
        area.timer = area.spawnInterval;
    }
}

int EnemyManager::CountEnemiesInArea(const SpawnArea& spawnArea) const
{
    int count = 0;
    const float halfX = spawnArea.size.x * 0.5f;
    const float halfY = spawnArea.size.y * 0.5f;
    for (const auto& enemy : enemies_) {
        if (!enemy || enemy->IsDead()) {
            continue;
        }
        const cg2::Vector3 pos = enemy->GetWorldPosition();
        if (pos.x >= spawnArea.center.x - halfX && pos.x <= spawnArea.center.x + halfX && pos.y >= spawnArea.center.y - halfY &&
            pos.y <= spawnArea.center.y + halfY) {
            ++count;
        }
    }
    return count;
}

void EnemyManager::DrawBodyOnlyVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight)
{
    for (auto& enemy : enemies_) {
        const cg2::Vector3 pos = enemy->GetWorldPosition();
        const float radius = enemy->GetRadius();
        if (pos.x + radius < cameraPos.x - halfWidth || pos.x - radius > cameraPos.x + halfWidth ||
            pos.y + radius < cameraPos.y - halfHeight || pos.y - radius > cameraPos.y + halfHeight) {
            continue;
        }
        enemy->DrawBodyOnly();
    }
}

std::vector<ExpEnemy*> EnemyManager::GetEnemyPtrs() const
{
    std::vector<ExpEnemy*> result;
    result.reserve(enemies_.size());
    for (const auto& e : enemies_) {
        result.push_back(e.get());
    }
    return result;
}
