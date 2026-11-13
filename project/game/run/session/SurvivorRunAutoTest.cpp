#include "game/run/session/SurvivorRunController.h"
#include "game/run/session/SurvivorRunShared.h"
#include "game/session/GameplaySystems.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>

// 育つ戦車の試作の自動確認（区画・四隅・200体の fps）。MYENGINE_SURVIVOR_AUTOTEST で動き、結果を書いて終了する。

namespace gameplay {

using namespace survivorrun;

namespace {
struct AutoTestWaypoint {
    const char* name;
    myEngine::Vector3 target;
};

// 中央から四隅を回って中央へ戻る。柱の並ぶ線（x=16〜18・30・58・70〜72、y=8・28〜30・50）を避けた経路にする。
const std::array<AutoTestWaypoint, 5> kTourWaypoints{
    {{"northeast", {82, 54, 0}}, {"southeast", {82, 4, 0}}, {"southwest", {6, 4, 0}}, {"northwest", {6, 54, 0}}, {"center", {44, 28, 0}}}};
// 入力を止めても少し滑るので、着いたかどうかはこの距離で見る。
constexpr float kAutoTestReachDistance = 4.0f;
// 区画の自動確認で画面を保存する時刻（秒）。
constexpr std::array<float, 3> kSectorCaptureSeconds{30.0f, 125.0f, 183.0f};
// 区画の自動確認が出口へ向かい始める時刻（秒）。
constexpr float kSectorAutoTestExitSeconds = 240.0f;
} // namespace

float SurvivorRunController::MeasureAimRoundTripError() const
{
    const myEngine::Vector3 target = world_.resources.player_->GetWorldPosition() + myEngine::Vector3{5.0f, 3.0f, 0.0f};
    const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(target);
    const myEngine::Vector3 back =
        myEngine::ScreenToWorldOnZ0(screen, world_.resources.camera->GetViewMatrix(), world_.resources.camera->GetProjectionMatrix(),
                                    myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight);
    return myEngine::Length(myEngine::Vector3{back.x - target.x, back.y - target.y, 0.0f});
}

void SurvivorRunController::RequestCapture(const std::string& name)
{
    if (world_.run.tankRunCapturePath_.empty()) {
        world_.run.tankRunCapturePath_ = std::string(kAutoTestDirectory) + "/" + name + ".png";
    }
}

void SurvivorRunController::UpdateSectorAutoTest(float dt)
{
    // 部屋の中央の周りを回りながら近い敵を撃ち続け、近くの欠片は拾いに行く。4:00 を過ぎたら出口へ向かう
    // （レベルの曲線を4分で確かめるため、出口が開いてもすぐには入らない）。自機は無敵。
    auto& player = *world_.resources.player_;
    // 拠点の画面：画面を保存し、4つ目の武器を3つ目の枠へ入れてから元に戻し（入れ替えの確認）、作る・進化を1回ずつ試して
    // 派生の木の画面を保存し、出撃する。区画の時間は進めない。
    if (baseScreenOpen_) {
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        // 機体の遊び方の拠点（K6）：画面を保存し、解放ポイントを足して最初の候補の機体を解放してみてから、
        // 決めた機体（設定の start か MYENGINE_SURVIVOR_HERO）を選び直して画面を保存し、出撃する。
        if (HeroMode()) {
            auto click = [this](const SurvivorItemScreen::Hit& hit) {
                const myEngine::Vector2 at = itemScreen_.CenterOf(hit);
                DriveScreenPointer(at, false, false, 0);
                DriveScreenPointer(at, true, false, 0);
                DriveScreenPointer(at, false, false, 0);
            };
            const std::string start = config_.heroes.start;
            if (autoHeroBaseState_ == 0) {
                RequestCapture("hero_select");
                autoHeroBaseState_ = 1;
            } else if (autoHeroBaseState_ == 1) {
                const std::vector<int> candidates = survivor::HeroUnlockCandidates(heroSave_, config_.heroes.heroes);
                if (!candidates.empty()) {
                    const survivor::HeroDefinition& target = config_.heroes.heroes[static_cast<size_t>(candidates.front())];
                    heroSave_.points = (std::max)(heroSave_.points, target.unlockCost);
                    const std::vector<int> order = HeroCardOrder();
                    const auto card = std::find(order.begin(), order.end(), candidates.front());
                    if (card != order.end()) {
                        click({SurvivorItemScreen::Zone::Card, static_cast<int>(card - order.begin())});
                        click({SurvivorItemScreen::Zone::Button, 1});
                    }
                    autoUnlockOk_ = survivor::IsHeroUnlocked(heroSave_, target) && heroSave_.points == 0;
                }
                // 決めた機体を選び直す（解放で選び替わったので）。
                const std::vector<int> order = HeroCardOrder();
                const int wanted = survivor::FindHero(config_.heroes.heroes, start);
                const auto card = std::find(order.begin(), order.end(), wanted);
                if (card != order.end()) {
                    click({SurvivorItemScreen::Zone::Card, static_cast<int>(card - order.begin())});
                }
                RequestCapture("hero_select_after");
                autoHeroBaseState_ = 2;
            } else {
                click({SurvivorItemScreen::Zone::Button, 0});
            }
            return;
        }
        if (autoBaseState_ == 0) {
            RequestCapture("base");
            autoBaseState_ = 1;
        } else if (autoBaseState_ == 1) {
            // マウスの操作を実際の道筋で試す。drag は押して動かして放す、click はその場で押して放す。
            auto drag = [this](const SurvivorItemScreen::Hit& from, const SurvivorItemScreen::Hit& to) {
                const myEngine::Vector2 a = itemScreen_.CenterOf(from), b = itemScreen_.CenterOf(to);
                DriveScreenPointer(a, false, false, 0);
                DriveScreenPointer(a, true, false, 0);
                DriveScreenPointer(b, true, false, 0);
                DriveScreenPointer(b, false, false, 0);
            };
            auto click = [this](const SurvivorItemScreen::Hit& hit) {
                const myEngine::Vector2 a = itemScreen_.CenterOf(hit);
                DriveScreenPointer(a, false, false, 0);
                DriveScreenPointer(a, true, false, 0);
                DriveScreenPointer(a, false, false, 0);
            };
            using Zone = SurvivorItemScreen::Zone;
            const std::array<int, survivor::kWeaponSlots> before = base_.equipped;
            // 4つ目の武器を3つ目の枠へドラッグし、元の武器を戻す（入れ替えの確認）。
            drag({Zone::Grid, 3}, {Zone::Card, 2});
            const bool swapped = base_.equipped[2] == 3;
            drag({Zone::Grid, before[2]}, {Zone::Card, 2});
            autoBaseEquipOk_ = swapped && base_.equipped == before;
            // 作る・進化：試しに素材とお金を足し（自動確認だけ。出撃の前に0へ戻す）、作る欄で榴弾砲をクリックし、武器の欄で作った物をクリックして
            // 派生の木を開き、次の段をクリックする。払った分だけ減り、レア度が上がり、威力が上がるかを見る。
            base_.materials = {100, 100, 100, 20};
            base_.money = 5000;
            click({Zone::Tab, 1});
            size_t recipe = 0;
            for (size_t i = 0; i < config_.crafting.recipes.size(); ++i) {
                if (config_.crafting.recipes[i].key == "grenade") {
                    recipe = i;
                }
            }
            const size_t owned = base_.weapons.size();
            const survivor::CraftCost craftCost = config_.crafting.recipes[recipe].cost;
            click({Zone::Grid, static_cast<int>(recipe)});
            const bool crafted = base_.weapons.size() == owned + 1 && base_.materials[0] == 100 - craftCost.materials[0] &&
                                 base_.materials[2] == 100 - craftCost.materials[2];
            const std::array<int, survivor::kMaterialCount> materials = base_.materials;
            const int money = base_.money;
            const int crafted_index = static_cast<int>(base_.weapons.size()) - 1;
            const survivor::CraftCost evolveCost = survivor::EvolveCost(config_.crafting, "grenade", survivor::Rarity::Uncommon);
            click({Zone::Tab, 0});
            click({Zone::Grid, crafted_index});
            click({Zone::Grid, 1});
            bool evolved = base_.weapons.back().rarity == survivor::Rarity::Uncommon && base_.money == money - evolveCost.money;
            for (size_t i = 0; i < survivor::kMaterialCount; ++i) {
                evolved = evolved && base_.materials[i] == materials[i] - evolveCost.materials[i];
            }
            const int grenade = survivor::FindWeapon(config_.weapons, "grenade");
            const bool stronger = grenade >= 0 && survivor::ApplyRarity(config_.weapons[static_cast<size_t>(grenade)], survivor::Rarity::Uncommon,
                                                                         config_.crafting)
                                                          .damageScale > config_.weapons[static_cast<size_t>(grenade)].damageScale;
            autoCraftOk_ = crafted && evolved && stronger;
            // 枝：機関砲を作ってアンコモンへ進化させ、散弾砲へ分かれる。枝の絵のマスは2段目の先頭（10番）。
            click({Zone::Tab, 1});
            size_t gunRecipe = 0;
            for (size_t i = 0; i < config_.crafting.recipes.size(); ++i) {
                if (config_.crafting.recipes[i].key == "machineGun") {
                    gunRecipe = i;
                }
            }
            click({Zone::Grid, static_cast<int>(gunRecipe)});
            const int gunIndex = static_cast<int>(base_.weapons.size()) - 1;
            click({Zone::Tab, 0});
            click({Zone::Grid, gunIndex});
            click({Zone::Grid, 1});
            click({Zone::Grid, static_cast<int>(SurvivorItemScreen::kColumns)});
            autoBranchOk_ = base_.weapons[static_cast<size_t>(gunIndex)].key == "shotgun" && base_.weapons[static_cast<size_t>(gunIndex)].rarity == survivor::Rarity::Uncommon;
            // 枝のある武器（最初の単発の砲か機関砲）の木を開き直して保存する。説明の札は枝のマスに合わせる。
            click({Zone::Tab, 0});
            for (int i = 0; i < static_cast<int>(base_.weapons.size()); ++i) {
                if (base_.weapons[static_cast<size_t>(i)].key == "cannon" || base_.weapons[static_cast<size_t>(i)].key == "machineGun") {
                    click({Zone::Grid, i});
                    break;
                }
            }
            // 枝のマスに合わせて説明の札を出した画面を保存する。
            DriveScreenPointer(itemScreen_.CenterOf({Zone::Grid, static_cast<int>(SurvivorItemScreen::kColumns)}), false, false, 0);
            RequestCapture("base_tree");
            autoBaseState_ = 2;
        } else if (autoBaseState_ == 2) {
            base_.materials = {};
            base_.money = 0;
            basePage_ = 0;
            RefreshBaseScreen();
            // 3ステージを通す確認は、強い持ち物（装備の3つをレジェンド）で始める。
            if (autoFullRun_) {
                for (int index : base_.equipped) {
                    if (index >= 0 && static_cast<size_t>(index) < base_.weapons.size()) {
                        base_.weapons[static_cast<size_t>(index)].rarity = survivor::Rarity::Legendary;
                    }
                }
            }
            {
                const myEngine::Vector2 button = itemScreen_.CenterOf({SurvivorItemScreen::Zone::Button, 0});
                DriveScreenPointer(button, false, false, 0);
                DriveScreenPointer(button, true, false, 0);
                DriveScreenPointer(button, false, false, 0);
            }
            autoBaseState_ = 3;
        }
        return;
    }
    // 休憩部屋：画面を保存し、袋のチップ枠のある武器を3つ目の枠へ付け替え、チップを付けて外して付け直し、
    // チップを1つ売り、四角の素材を全部売り、袋の武器を1つ捨ててから、もう一度画面を保存して帰る。
    if (restRoomOpen_) {
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        if (autoRestState_ == 0) {
            RequestCapture(HeroMode() ? "wave_break" : "rest_room");
            autoRestState_ = 1;
        } else if ((autoRestState_ == 1 || autoRestState_ == 4) && HeroMode()) {
            // 機体の遊び方の波の間（ショップ）：修理を押せれば押し（体力の決まりの確認で傷が付いている）、
            // 買える物を安い順に買い、残りが多ければ3回まで引き直してまた買う。画面を保存する（波の間ごとに毎回）。
            auto click = [this](const SurvivorItemScreen::Hit& hit) {
                const myEngine::Vector2 at = itemScreen_.CenterOf(hit);
                DriveScreenPointer(at, false, false, 0);
                DriveScreenPointer(at, true, false, 0);
                DriveScreenPointer(at, false, false, 0);
            };
            click({SurvivorItemScreen::Zone::Button, 2});
            auto buyAll = [&] {
                for (int guard = 0; guard < 12; ++guard) {
                    int cheapest = -1;
                    for (size_t i = 0; i < shopOffers_.size(); ++i) {
                        const int price = OfferPrice(shopOffers_[i]);
                        if (price <= bag_.money && (cheapest < 0 || price < OfferPrice(shopOffers_[static_cast<size_t>(cheapest)]))) {
                            cheapest = static_cast<int>(i);
                        }
                    }
                    if (cheapest < 0) {
                        return;
                    }
                    click({SurvivorItemScreen::Zone::Grid, cheapest});
                }
            };
            buyAll();
            for (int reroll = 0; reroll < 3 && bag_.money >= RerollPrice() + 150; ++reroll) {
                click({SurvivorItemScreen::Zone::Button, 3});
                buyAll();
            }
            RequestCapture("wave_break_after");
            autoRestState_ = 2;
        } else if (autoRestState_ == 1) {
            using Zone = SurvivorItemScreen::Zone;
            auto drag = [this](const SurvivorItemScreen::Hit& from, const SurvivorItemScreen::Hit& to) {
                const myEngine::Vector2 a = itemScreen_.CenterOf(from), b = itemScreen_.CenterOf(to);
                DriveScreenPointer(a, false, false, 0);
                DriveScreenPointer(a, true, false, 0);
                DriveScreenPointer(b, true, false, 0);
                DriveScreenPointer(b, false, false, 0);
            };
            auto rightClick = [this](const SurvivorItemScreen::Hit& hit) {
                const myEngine::Vector2 a = itemScreen_.CenterOf(hit);
                DriveScreenPointer(a, false, false, 0);
                DriveScreenPointer(a, false, true, 0);
            };
            // 右の欄から、種類の合う最初のマスを探す（条件は Item を見る）。
            auto find = [this](ScreenItem kind, auto condition) {
                for (int i = 0;; ++i) {
                    const SurvivorItemScreen::Item* item = itemScreen_.ItemAt({SurvivorItemScreen::Zone::Grid, i});
                    if (!item) {
                        return -1;
                    }
                    if (item->kind == static_cast<int>(kind) && condition(*item)) {
                        return i;
                    }
                }
            };
            auto any = [](const SurvivorItemScreen::Item&) {
                return true;
            };
            // チップ枠のある武器を3つ目の枠へドラッグする（レア度の高い順に並んでいるので最初の物。コモンならチップは付かない）。
            const int weaponCell = find(ScreenItem::BagWeapon, any);
            if (weaponCell >= 0) {
                drag({Zone::Grid, weaponCell}, {Zone::Card, 2});
            }
            // チップを付ける → 右クリックで外す → もう一度付ける。
            int chipCell = find(ScreenItem::BagChip, any);
            if (chipCell >= 0) {
                const int attaches = restAttaches_;
                drag({Zone::Grid, chipCell}, {Zone::Card, 2});
                if (restAttaches_ > attaches) {
                    rightClick({Zone::Socket, 2, static_cast<int>(runSlots_[2].weapon.chips.size()) - 1});
                    chipCell = find(ScreenItem::BagChip, any);
                    drag({Zone::Grid, chipCell}, {Zone::Card, 2});
                    // 3つ目の枠を構えて、チップが部品の段として効くかを見る。
                    EquipWeapon(2);
                    for (int level : partLevels_) {
                        autoRestChipLevels_ += level;
                    }
                    RefreshRestRoom();
                }
            }
            // チップを1つ売り、素材を1種類売り、袋の武器を1つ Z で捨てる。
            chipCell = find(ScreenItem::BagChip, any);
            if (chipCell >= 0) {
                rightClick({Zone::Grid, chipCell});
            }
            const int materialCell = find(ScreenItem::Material, any);
            if (materialCell >= 0) {
                rightClick({Zone::Grid, materialCell});
            }
            const int discardCell = find(ScreenItem::BagWeapon, any);
            if (discardCell >= 0) {
                DriveScreenPointer(itemScreen_.CenterOf({Zone::Grid, discardCell}), false, false, 0);
                DiscardHoveredItem();
            }
            // 最初のマスに合わせて、説明の札を出した画面を保存する。
            DriveScreenPointer(itemScreen_.CenterOf({Zone::Grid, 0}), false, false, 0);
            RequestCapture("rest_room_after");
            autoRestState_ = 2;
        } else if (autoRestState_ == 2 || autoRestState_ == 4) {
            // 3ステージを通す確認では、ボスステージまで「次へ進む」、ボスを倒した後の休憩部屋で帰る。ボタンを押して進む。
            const bool next = autoFullRun_ && !IsBossStage();
            const myEngine::Vector2 button = itemScreen_.CenterOf({SurvivorItemScreen::Zone::Button, next ? 1 : 0});
            autoRestState_ = next ? 4 : 3;
            DriveScreenPointer(button, false, false, 0);
            DriveScreenPointer(button, true, false, 0);
            DriveScreenPointer(button, false, false, 0);
        }
        return;
    }
    autoTestTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (const ExpEnemy* nearest = world_.resources.enemyManager_->FindNearestEnemy(position, 30.0f)) {
        aim = nearest->GetWorldPosition();
    }
    // 宿敵がいて近ければ、宿敵を撃つ。
    if (IsRivalActive()) {
        const myEngine::Vector3 rival = world_.resources.enemy_->GetWorldPosition();
        if (myEngine::Length(myEngine::Vector3{rival.x - position.x, rival.y - position.y, 0.0f}) < 30.0f) {
            aim = rival;
        }
    }
    myEngine::Vector3 goal{};
    myEngine::Vector3 shard{};
    if (exitOpen_ && (autoFullRun_ || elapsed_ >= kSectorAutoTestExitSeconds)) {
        goal = exitPosition_;
    } else if (IsRivalActive()) {
        // 宿敵がいる間は、宿敵から8ほど離れた位置へ寄る。
        const myEngine::Vector3 rival = world_.resources.enemy_->GetWorldPosition();
        myEngine::Vector3 away{position.x - rival.x, position.y - rival.y, 0.0f};
        const float distance = myEngine::Length(away);
        goal = distance > 0.1f ? rival + away * (8.0f / distance) : rival;
    } else if (WeaponMode() && lootField_.FindNearest(position, 14.0f, shard)) {
        // 落ちた物（弾・素材・チップ・武器）を、欠片より先に拾いに行く。
        goal = shard;
    } else if (pickups_.FindNearest(position, 12.0f, shard)) {
        goal = shard;
    } else {
        const float angle = autoTestTime_ * 0.35f;
        goal = {44.0f + std::cos(angle) * 18.0f, 28.0f + std::sin(angle) * 11.0f, 0.0f};
    }
    myEngine::Vector3 move{goal.x - position.x, goal.y - position.y, 0.0f};
    const float length = myEngine::Length(move);
    move = length > 0.5f ? move / length : myEngine::Vector3{};
    // トゲ：5秒で一度、自機のすぐ横に四角を置き、すぐにトゲの判定をして壊れるかを見る
    // （同じフレームの体当たりや弾で壊れる前に確かめる）。
    if (player.HasSpikes() && autoSpikeProbe_ == 0 && autoTestTime_ >= 5.0f) {
        const myEngine::Vector3 beside = position + myEngine::Vector3{0.0f, -(player.GetRadius() + 0.5f), 0.0f};
        const int before = spikeKills_;
        if (SpawnShape("Square", beside, false)) {
            UpdateSpikes();
            autoSpikeProbe_ = spikeKills_ > before ? 2 : 3;
        } else {
            autoSpikeProbe_ = -1;
        }
    }
    // 3秒ごとに1回ダッシュする（衝撃放電・推進器の確認）。
    const bool dash = !WeaponMode() && static_cast<int>(autoTestTime_ / 3.0f) != static_cast<int>((autoTestTime_ - dt) / 3.0f);
    // 誘導：誘導の武器を構えている間、自機の弾の向きが飛んでいる途中で変わった回数を数える。
    if (const survivor::WeaponDefinition* weapon = WeaponMode() ? ActiveWeapon() : nullptr; weapon && weapon->homingPower > 0.0f) {
        for (const Bullet* bullet : world_.resources.bulletManager_->GetBulletPtrs()) {
            if (!bullet || bullet->IsDead() || bullet->GetOwner() != kPlayer || myEngine::Length(bullet->GetMove()) < 0.001f) {
                continue;
            }
            const myEngine::Vector3 direction = myEngine::Normalize(bullet->GetMove());
            const auto [entry, added] = autoHomingDirections_.try_emplace(bullet->GetCollisionId(), direction);
            if (!added && myEngine::Length(entry->second - direction) > 0.002f) {
                ++autoHomingTurns_;
            }
            entry->second = direction;
        }
    } else {
        autoHomingDirections_.clear();
    }
    // 調整パネル：ステージ1の8秒から1秒だけ開いて、画面を保存する（止めない）。
    if (WeaponMode() && stage_ == 0) {
        const bool show = elapsed_ >= 8.0f && elapsed_ < 9.0f;
        tuningPause_ = false;
        tuningOpen_ = show;
        if (show && !autoTuningCaptured_ && elapsed_ >= 8.5f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("tuning");
            autoTuningCaptured_ = true;
        }
    }
    // バリア：20秒のうち10秒押し続ける（途中でスタミナが尽きるかも見る）。弾を止めた瞬間の画面を1回保存する。
    autoBarrierWanted_ = WeaponMode() && std::fmod(autoTestTime_, 20.0f) < 10.0f;
    // 機体（計画書 p4 の K0）：Shift と E は使えるたびに、Q はゲージが満タンになったら押す（次の更新で使う）。
    // 走っている間とそうでない間の、実際の移動の速さの一番大きい値を控え、走っている間の画面を1回保存する。
    if (HeroMode()) {
        using survivor::AbilitySlot;
        autoHeroPress_[static_cast<size_t>(AbilitySlot::Mobility)] = abilityCooldown_[static_cast<size_t>(AbilitySlot::Mobility)] <= 0.0f;
        autoHeroPress_[static_cast<size_t>(AbilitySlot::Skill)] = abilityCooldown_[static_cast<size_t>(AbilitySlot::Skill)] <= 0.0f;
        autoHeroPress_[static_cast<size_t>(AbilitySlot::Ultimate)] = ultimateCharge_ >= Hero().ultimateCharge;
        // 右クリック（押した瞬間の技）：使えて、近くに敵がいれば撃つ。
        autoHeroPress_[static_cast<size_t>(AbilitySlot::Secondary)] = abilityCooldown_[static_cast<size_t>(AbilitySlot::Secondary)] <= 0.0f &&
                                                                       world_.resources.enemyManager_->FindNearestEnemy(position, 15.0f) != nullptr;
        // 回復の場の確認：初めて置く前に HP を半分にする（自動確認は無敵なので、そのままでは回復する分が無い）。
        if (FindAbility(survivor::AbilityKind::HealZone) && abilityUses_[static_cast<size_t>(AbilitySlot::Skill)] == 0 &&
            autoHeroPress_[static_cast<size_t>(AbilitySlot::Skill)] && player.GetHp() == player.GetMaxHp() && player.IsDebugNoDamage()) {
            player.SpendRunHealth(player.GetMaxHp() / 2);
        }
        // 体力の決まり（H6・H7）：5秒で一度だけ無敵を外し、同じ相手から2回・別の相手から2回当てて、1回の上限・同じ相手の間隔・
        // 1秒の上限で削られるか、傷の分だけ HP が戻らないかを見る（消える移動の間は待つ）。
        if (!autoHealthChecked_ && autoTestTime_ >= 5.0f && !player.IsSurvivorPhased()) {
            autoHealthChecked_ = true;
            ApplyHeroHealthRules();
            const int maxHp = player.GetMaxHp();
            player.HealRunPlayer(maxHp);
            player.SetDebugNoDamage(false);
            std::array<int, 4> taken{};
            const std::array<std::pair<uint64_t, uint32_t>, 4> hits{{{0xA1, 1000}, {0xA1, 5}, {0xA2, 1000}, {0xA3, 5}}};
            for (size_t i = 0; i < hits.size(); ++i) {
                const int before = player.GetHp();
                player.TakeSurvivorHit(hits[i].first, hits[i].second);
                taken[i] = before - player.GetHp();
            }
            player.SetDebugNoDamage(true);
            const int wound = player.GetSurvivorWound();
            player.HealRunPlayer(maxHp);
            autoHealthCheck_ << "{\"maxHp\":" << maxHp << ",\"taken\":[" << taken[0] << "," << taken[1] << "," << taken[2] << ","
                             << taken[3] << "],\"wound\":" << wound << ",\"hpAfterHeal\":" << player.GetHp() << "}";
        }
        // 自分を回復（モイラ型）：吸収ゲージがほぼ満タンで HP も満タンなら HP を半分にし（自動確認は無敵なので）、
        // HP が減っていてゲージがあれば押し続ける。
        if (const survivor::AbilityDefinition* heal = FindAbility(survivor::AbilityKind::SelfHeal)) {
            if (absorb_ >= heal->duration * 0.95f && player.GetHp() == player.GetMaxHp() && player.IsDebugNoDamage()) {
                player.SpendRunHealth(player.GetMaxHp() / 2);
            }
            autoSecondaryHold_ = absorb_ > 0.0f && player.GetHp() < player.GetMaxHp();
        }
        // 技の画面（タンク型）：突進・衝撃波・大きな盾を、それぞれ1回ずつ保存する。
        if (world_.run.tankRunCapturePath_.empty() && FindAbility(survivor::AbilityKind::Fortress)) {
            if (!autoChargeCaptured_ && chargeTimer_ > 0.1f && elapsed_ >= 10.0f) {
                RequestCapture("hero_charge");
                autoChargeCaptured_ = true;
            } else if (!autoShockCaptured_ && shockAge_ > 0.05f && shockAge_ < 0.25f && elapsed_ >= 10.0f) {
                RequestCapture("hero_shockwave");
                autoShockCaptured_ = true;
            } else if (!autoFortressCaptured_ && fortressTimer_ > 1.0f && fortressTimer_ < 4.0f) {
                RequestCapture("hero_fortress");
                autoFortressCaptured_ = true;
            }
        }
        // 技の画面（モイラ型）：吸い付いた光線・跳ねる球・太い光線・消える移動を、それぞれ1回ずつ保存する。
        if (world_.run.tankRunCapturePath_.empty() && HeroBeamPrimary()) {
            if (!autoBeamCaptured_ && beamTarget_ != 0 && elapsed_ >= 6.0f) {
                RequestCapture("hero_beam");
                autoBeamCaptured_ = true;
            } else if (!autoOrbCaptured_ && orbTimer_ > 0.0f && orbTimer_ < 2.5f) {
                RequestCapture("hero_orb");
                autoOrbCaptured_ = true;
            } else if (!autoLanceCaptured_ && lanceTimer_ > 0.0f && lanceTimer_ < 2.5f) {
                RequestCapture("hero_lance");
                autoLanceCaptured_ = true;
            } else if (!autoPhaseCaptured_ && phaseTimer_ > 0.3f && elapsed_ >= 10.0f) {
                RequestCapture("hero_phase");
                autoPhaseCaptured_ = true;
            }
        }
        // 技の画面：回復の場と弾幕を、それぞれ効いている途中で1回ずつ保存する。
        if (world_.run.tankRunCapturePath_.empty()) {
            if (!autoHealCaptured_ && healZoneTimer_ > 0.0f && healZoneTimer_ < 3.5f) {
                RequestCapture("hero_heal");
                autoHealCaptured_ = true;
            } else if (!autoBarrageCaptured_ && barrageTimer_ > 0.0f && barrageTimer_ < 3.0f) {
                RequestCapture("hero_barrage");
                autoBarrageCaptured_ = true;
            }
        }
        // 動こうとしている間（目標が遠い）だけ、動いた距離を走っている間とそうでない間に分けて足す。
        if (dt > 0.0f && autoTestTime_ > 1.0f && length > 2.0f) {
            const float distance = myEngine::Length(myEngine::Vector3{position.x - autoLastPosition_.x, position.y - autoLastPosition_.y, 0.0f});
            // 部屋を移ったときの飛びは数えない。
            if (distance < dt * 100.0f) {
                const size_t index = sprintTimer_ > 0.0f ? 1 : 0;
                autoMoveDistance_[index] += distance;
                autoMoveSeconds_[index] += dt;
            }
        }
        autoLastPosition_ = position;
        if (!autoHeroCaptured_ && sprintTimer_ > 1.0f && elapsed_ >= 10.0f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("hero_sprint");
            autoHeroCaptured_ = true;
        }
    }
    // 新しい敵：それぞれが自機の近くにいる画面を1回ずつ保存する（ワープする敵は消えている途中、ばらばらな三角は役割なしで見分ける）。
    if (ElitesActive() && world_.run.tankRunCapturePath_.empty()) {
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || myEngine::Length(myEngine::Vector3{enemy->GetWorldPosition().x - position.x, enemy->GetWorldPosition().y - position.y, 0.0f}) > 13.0f) {
                continue;
            }
            const size_t role = static_cast<size_t>(enemy->GetSurvivorRole());
            const bool ready = role != static_cast<size_t>(ExpEnemy::SurvivorRole::Warper) || (enemy->GetWarpDissolve() > 0.3f && enemy->GetWarpDissolve() < 0.9f);
            if (role > 0 && !autoEliteCaptured_[role] && ready) {
                static constexpr std::array<const char*, 5> kNames{"", "elite_healer", "elite_warper", "elite_layer", "elite_mine"};
                RequestCapture(kNames[role]);
                autoEliteCaptured_[role] = true;
                if (role == static_cast<size_t>(ExpEnemy::SurvivorRole::Warper)) {
                    autoWarperOffset_ = {enemy->GetWorldPosition().x - position.x, enemy->GetWorldPosition().y - position.y, enemy->GetWarpDissolve()};
                }
                break;
            }
        }
    }
    if (WeaponMode()) {
        autoStaminaMin_ = (std::min)(autoStaminaMin_, barrierStamina_);
        if (!autoBarrierCaptured_ && barrierOn_ && barrierFlash_ > 0.5f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("barrier");
            autoBarrierCaptured_ = true;
        }
    }
    // 不動砲台を持っていれば、6秒のうち2秒は止まって撃つ（止まっている間の威力の確認）。
    // 機体の回復の場を置いている間も、場の中で止まって撃つ（回復の確認）。
    const bool standStill = (relics_[static_cast<size_t>(survivor::RelicId::FixedTurret)] && std::fmod(autoTestTime_, 6.0f) < 2.0f) ||
                            (HeroMode() && healZoneTimer_ > 0.0f && !exitOpen_);
    player.SetDemoInput(true, standStill ? myEngine::Vector2{} : myEngine::Vector2{move.x, move.y}, aim, true, dash);
    autoMaxHeat_ = (std::max)(autoMaxHeat_, player.GetHeat().GetHeat());

    // 武器の枠：15秒ごとに次の枠へ切り替え、枠ごとに使った秒・撃った回数を足す。構えて0.5秒後に砲門の数・威力と発射間隔の倍率を控え、
    // 1.5秒後にその武器の画面を保存する（枠ごとに最初の1回）。
    // 持ち物の画面：2:30 に1回開き、画面を保存して1秒後に閉じる。開いている間に区画の時計が進まないかを見る。
    if (WeaponMode() && autoInventoryState_ == 0 && elapsed_ >= 150.0f && swapTarget_ < 0) {
        OpenInventory();
        autoInventoryElapsed_ = elapsed_;
        autoInventoryTimer_ = 0.0f;
        autoInventoryState_ = 1;
    }
    if (inventoryOpen_) {
        autoInventoryTimer_ += dt;
        if (autoInventoryState_ == 1 && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("inventory");
            autoInventoryState_ = 2;
        }
        if (autoInventoryState_ == 2 && autoInventoryTimer_ >= 1.0f) {
            autoInventoryFrozen_ = elapsed_ == autoInventoryElapsed_;
            CloseInventory();
            autoInventoryState_ = 3;
        }
    }
    if (WeaponMode() && !inventoryOpen_) {
        // 15秒ごとに、ホイールを1刻み回したのと同じ切り替えで次の枠へ。切り替えの途中の画面を1回保存する。
        // このフレームの射撃は、切り替えや（自動の）リロードが始まる前に済んでいるので、撃てない間かは前のフレームの終わりの状態で見る。
        const bool lockedBefore = autoWasLocked_;
        const size_t slot = static_cast<size_t>(autoTestTime_ / 15.0f) % weaponSlots_.size();
        if (slot != activeSlot_ && swapTarget_ < 0 && weaponSlots_[slot] >= 0) {
            HandleWeaponWheel(1);
            autoSwapStartedAt_ = autoTestTime_;
        }
        if (swapTarget_ >= 0 && !autoSwapCaptured_ && swapTimer_ >= swapDuration_ * 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("weapon_swap");
            autoSwapCaptured_ = true;
        }
        // 切り替えが終わって撃てるようになった時刻から、実際にかかった秒を測る。
        if (autoSwapStartedAt_ >= 0.0f && swapTarget_ < 0) {
            autoSwapMeasured_[activeSlot_] = autoTestTime_ - autoSwapStartedAt_;
            autoSwapStartedAt_ = -1.0f;
        }
        AutoWeaponStat& stat = autoWeaponStats_[activeSlot_];
        if (stat.seconds < 0.5f && stat.seconds + dt >= 0.5f) {
            const PlayerClassConfig* active = player.GetActiveClassConfig();
            stat.barrels = active ? active->barrels.size() : 0;
            stat.damageScale = active ? active->bulletDamageScale : 0.0f;
            stat.shotDamage = static_cast<uint32_t>((std::max)(1.0f, std::round(player.GetStats().bulletDamage * stat.damageScale)));
            stat.reloadScale = active ? active->reloadScale : 0.0f;
            stat.captureAt = autoTestTime_ + 1.0f;
        }
        stat.seconds += dt;
        const uint32_t attacks = player.GetPrimaryAttackCount();
        const uint32_t rails = static_cast<uint32_t>(player.GetSpecialCombatStats().railShots);
        const uint32_t shot = (attacks >= autoWeaponAttacks_ ? attacks - autoWeaponAttacks_ : 0) +
                              (rails >= autoWeaponRailShots_ ? rails - autoWeaponRailShots_ : 0);
        stat.attacks += attacks >= autoWeaponAttacks_ ? attacks - autoWeaponAttacks_ : 0;
        stat.railShots += rails >= autoWeaponRailShots_ ? rails - autoWeaponRailShots_ : 0;
        autoWeaponAttacks_ = attacks;
        autoWeaponRailShots_ = rails;
        // 武器ごとのオーバーヒートと、榴弾砲の爆発の回数（区画を出る前の値から差を取る）。
        const int overheats = player.GetHeat().GetOverheatCount();
        const uint64_t blasts = world_.resources.bulletManager_->GetShooterStats().impactBlasts;
        stat.overheats += overheats >= autoWeaponOverheats_ ? overheats - autoWeaponOverheats_ : 0;
        stat.blasts += blasts >= autoWeaponBlasts_ ? blasts - autoWeaponBlasts_ : 0;
        autoWeaponOverheats_ = overheats;
        autoWeaponBlasts_ = blasts;
        // 撃てないはずの間（切り替え・リロード）に撃っていないか。
        if (lockedBefore) {
            autoLockedAttacks_ += shot;
            autoLockedSeconds_ += dt;
        }
        // 弾倉が空のまま（リロードも切り替えもしていない）の間を数える。予備がある限り自動でリロードが始まるので、ほぼ0になるはず。
        const survivor::WeaponDefinition* weapon = ActiveWeapon();
        if (weapon && weapon->magazine > 0 && swapTarget_ < 0 && reloadTimer_ < 0.0f && player.GetSurvivorRounds() == 0 &&
            reserves_[static_cast<size_t>(weapon->ammo)] > 0) {
            // 空になった最初のフレームの1発は、空にした1発なので数えない。
            if (autoEmptyTimer_ > 0.0f) {
                autoEmptyAttacks_ += shot;
            }
            autoEmptyTimer_ += dt;
            autoEmptySeconds_ += dt;
        } else {
            autoEmptyTimer_ = 0.0f;
        }
        autoWasLocked_ = swapTarget_ >= 0 || reloadTimer_ >= 0.0f;
        if (reloadTimer_ >= 0.0f && !autoReloadCaptured_ && reloadTimer_ <= reloadDuration_ * 0.5f &&
            world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("weapon_reload");
            autoReloadCaptured_ = true;
        }
        if (!stat.captured && stat.captureAt > 0.0f && autoTestTime_ >= stat.captureAt && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("weapon_" + formKey_);
            stat.captured = true;
        }
    }
    // 特性：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で選ぶ（既定は冷却炉）。
    if (traitChoiceOpen_) {
        autoTraitTimer_ += dt;
        if (!autoTraitCaptured_ && autoTraitTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("trait_open");
            autoTraitCaptured_ = true;
        }
        if (autoTraitCaptured_ && autoTraitTimer_ > 0.6f) {
            ChooseTrait(autoTraitIndex_);
        }
        return;
    }
    // レリック：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で1枚目を選ぶ。選ぶ前後の主砲の威力・連射・ダッシュを記録する。
    if (relicChoiceOpen_) {
        autoRelicTimer_ += dt;
        if (!autoRelicCaptured_ && autoRelicTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture("relic_open");
            autoRelicCaptured_ = true;
        }
        if (autoRelicTimer_ > 0.6f) {
            const auto before = player.GetStats();
            const survivor::RelicId id = relicOffers_.front();
            ChooseRelic(0);
            autoRelicTimer_ = 0.0f;
            autoRelicLog_ << (autoRelicLog_.tellp() > 0 ? "," : "") << "{\"t\":" << std::lround(elapsed_) << ",\"relic\":\""
                          << (id == survivor::RelicId::Count ? "premium" : survivor::RelicKey(id)) << "\",\"bulletDamage\":["
                          << before.bulletDamage << "," << player.GetStats().bulletDamage << "],\"reloadSpeed\":[" << before.reloadSpeed
                          << "," << player.GetStats().reloadSpeed << "]}";
        }
        return;
    }
    // 形態：2択が開いたら0.3秒（実時間）で画面を保存し、0.6秒で選ぶ。選んで2秒後に、変わった自機の画面を保存する。
    if (formChoiceOpen_) {
        autoFormTimer_ += dt;
        if (!autoFormCaptured_ && autoFormTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture(formStage_ == 0 ? "form_open" : "evolve_open");
            autoFormCaptured_ = true;
        }
        if (autoFormCaptured_ && autoFormTimer_ > 0.6f) {
            // 第1段は連装か長砲身（MYENGINE_SURVIVOR_FORM）、第2段は MYENGINE_SURVIVOR_FORM2 の形態（無ければ1枚目）。
            size_t index = formStage_ == 0 ? autoFormIndex_ : 0;
            for (size_t i = 0; formStage_ >= 1 && i < formOffers_.size(); ++i) {
                if (autoForm2Key_ == config_.forms[static_cast<size_t>(formOffers_[i])].key) {
                    index = i;
                }
            }
            if (formStage_ >= 1) {
                autoEvolveOffers_ = static_cast<int>(formOffers_.size());
            }
            ChooseForm(index);
            if (formStage_ >= 2) {
                autoEvolvedAt_ = elapsed_;
            } else {
                autoFormChosenAt_ = elapsed_;
            }
            autoFormCaptureAt_ = autoTestTime_ + 2.0f;
            autoFormCaptured_ = false;
            autoFormTimer_ = 0.0f;
        }
        return;
    }
    // 進化の演出の途中（光が広がった頃）の画面を保存する（段ごとに1回）。
    if (cineMode_ == CineMode::Evolve && cineTime_ >= 0.8f && autoEvolveCineStage_ < formStage_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture(formStage_ >= 2 ? "evolve_cine" : "form_cine");
        autoEvolveCineStage_ = formStage_;
    }
    if (autoFormCaptureAt_ > 0.0f && autoTestTime_ >= autoFormCaptureAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("form_" + formKey_);
        autoFormCaptureAt_ = -1.0f;
    }
    // Q の入れ替え（MYENGINE_SURVIVOR_Q_SWAP=1）：10秒で一度、排熱砲を積んでいれば、緊急放出の札だけの3択を作って選び、
    // 排熱砲が外れて緊急放出に替わるかを見る。
    if (autoQSwapProbe_ == 0 && autoTestTime_ >= 10.0f && !choiceOpen_ &&
        survivor::OwnedQSkill(config_.parts, partLevels_) == survivor::PartId::HeatVent) {
        wchar_t swap[4]{};
        if (GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_Q_SWAP", swap, 4) > 0 && swap[0] == L'1') {
            ++pendingChoices_;
            pendingSince_.push_back(elapsed_);
            choiceOpen_ = true;
            offers_ = {{survivor::PartId::EmergencyPurge, 1}};
            ChooseOffer(0);
            autoQSwapProbe_ = survivor::OwnedQSkill(config_.parts, partLevels_) == survivor::PartId::EmergencyPurge &&
                                      survivor::LevelOf(partLevels_, survivor::PartId::HeatVent) == 0
                                  ? 2
                                  : 3;
        } else {
            autoQSwapProbe_ = -1;
        }
    }
    // Q：排熱砲は、熱が上限の2割（使える下限より低ければ下限）を超え、待ち時間が無ければ使う。
    // 緊急放出は、0:20 を過ぎて部品を3つ以上積んでいれば使う。使った0.15秒後の画面を保存する（それぞれ最初の1回）。
    const survivor::PartId qSkill = survivor::OwnedQSkill(config_.parts, partLevels_);
    bool qReady = false;
    if (qSkill == survivor::PartId::HeatVent) {
        qReady = player.GetHeat().GetHeat() >= (std::max)(config_.heatVent.minHeat, player.GetHeat().GetMaxHeat() * 0.2f);
    } else if (qSkill == survivor::PartId::EmergencyPurge) {
        int owned = 0;
        for (int level : partLevels_) {
            owned += level > 0 ? 1 : 0;
        }
        // 部品を指定して始めた確認（START_PARTS）では、確かめたい部品を壊さないよう使わない。
        qReady = elapsed_ >= 20.0f && owned >= 3 && !autoKeepParts_;
    }
    if (!choiceOpen_ && qCooldown_ <= 0.0f && qReady && UseQ()) {
        if (qSkill == survivor::PartId::HeatVent && autoQCaptureAt_ < 0.0f) {
            autoQCaptureAt_ = autoTestTime_ + 0.15f;
            autoQCaptureName_ = "q_vent";
        } else if (qSkill == survivor::PartId::EmergencyPurge && autoPurgeCaptured_ == false) {
            autoQCaptureAt_ = autoTestTime_ + 0.15f;
            autoQCaptureName_ = "q_purge";
            autoPurgeCaptured_ = true;
        }
    }
    if (autoQCaptureAt_ > 0.0f && autoTestTime_ >= autoQCaptureAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture(autoQCaptureName_);
        autoQCaptureAt_ = 0.0f;
    }
    // 長砲身：5秒ごとに0.7秒、右クリックを押してため撃ちする。
    player.SetDemoSpecialHold(player.HasRailChargeForm() && std::fmod(autoTestTime_, 5.0f) < 0.7f);

    // シールド：8秒で一度だけ無敵を外して2回当て、先にシールドが減ること、3秒は戻らず、その後に満タンへ戻ることを見る。
    // 無敵の間の被弾は TakeDamage の入口で弾かれるので、回復の待ち時間は動かない。
    constexpr std::array<float, 4> kShieldCheckSeconds{8.0f, 10.5f, 12.0f, 16.0f};
    if (autoShieldStep_ < static_cast<int>(kShieldCheckSeconds.size()) &&
        autoTestTime_ >= kShieldCheckSeconds[static_cast<size_t>(autoShieldStep_)]) {
        auto record = [&](const char* label) {
            autoShieldLog_ << (autoShieldLog_.tellp() > 0 ? "," : "") << "{\"at\":\"" << label << "\",\"t\":" << autoTestTime_
                           << ",\"shield\":" << player.GetShield() << ",\"shieldMax\":" << player.GetShieldMax()
                           << ",\"hp\":" << player.GetHp() << "}";
        };
        if (autoShieldStep_ == 0) {
            record("before");
            player.SetDebugNoDamage(false);
            player.TakeDamage(10, 0.0f);
            record("hit10");
            player.TakeDamage(40, 0.0f);
            record("hit40");
            player.SetDebugNoDamage(true);
        } else {
            record(autoShieldStep_ == 1 ? "wait2.5" : autoShieldStep_ == 2 ? "wait4" : "wait8");
        }
        ++autoShieldStep_;
    }

    // 3択：選択が2つ以上貯まるか、1つを3秒持ったら開き、0.6秒（実時間）見てから札を順番に選ぶ。
    if (choiceOpen_) {
        autoMinTimeScale_ = (std::min)(autoMinTimeScale_, world_.combat.finalDeltaTime / myEngine::kFixedStepSeconds);
        autoChoiceTimer_ += dt;
        if (!autoChoiceCaptured_ && autoChoiceTimer_ > 0.3f) {
            RequestCapture("choice_open");
            autoChoiceCaptured_ = true;
        }
        // 積載を超えて外す画面が開いたら、0.3秒で画面を保存し、0.6秒で一番 Lv の低い部品を外す。
        if (removalOpen_) {
            if (!autoRemovalCaptured_ && autoChoiceTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
                RequestCapture("removal_open");
                autoRemovalCaptured_ = true;
            }
            if (autoChoiceTimer_ > 0.6f && loadoutHud_.RowCount() > 0) {
                size_t lowest = 0;
                for (size_t row = 1; row < loadoutHud_.RowCount(); ++row) {
                    if (partLevels_[static_cast<size_t>(loadoutHud_.RowPart(row))] <
                        partLevels_[static_cast<size_t>(loadoutHud_.RowPart(lowest))]) {
                        lowest = row;
                    }
                }
                RemovePart(loadoutHud_.RowPart(lowest));
                autoChoiceTimer_ = 0.0f;
            }
            return;
        }
        // 合体の札が出たら、0.3秒で画面を保存してから、ほかの札より先に選ぶ。
        const auto fusion = std::find_if(offers_.begin(), offers_.end(), [&](const survivor::PartOffer& item) {
            return item.part != survivor::PartId::Count && survivor::IsFusion(config_.parts[static_cast<size_t>(item.part)]);
        });
        if (fusion != offers_.end() && !autoFusionCaptured_) {
            if (autoChoiceTimer_ > 0.3f && world_.run.tankRunCapturePath_.empty()) {
                RequestCapture("fusion_offer");
                autoFusionCaptured_ = true;
            }
            return;
        }
        if (autoChoiceTimer_ > 0.6f && !offers_.empty()) {
            const size_t index = fusion != offers_.end() ? static_cast<size_t>(fusion - offers_.begin()) : autoPick_++ % offers_.size();
            if (fusion != offers_.end()) {
                autoFusionLoad_[0] = survivor::LoadOf(config_.parts, partLevels_);
                autoFusionShotAt_ = autoTestTime_ + 4.0f;
            }
            const survivor::PartOffer offer = offers_[index];
            const int maxHpBefore = player.GetMaxHp();
            ChooseOffer(index);
            if (autoChoiceLog_.tellp() > 0) {
                autoChoiceLog_ << ",";
            }
            autoChoiceLog_ << "{\"t\":" << static_cast<int>(elapsed_) << ",\"part\":\""
                           << (offer.part == survivor::PartId::Count ? "repair" : config_.parts[static_cast<size_t>(offer.part)].key)
                           << "\",\"level\":" << offer.nextLevel << ",\"maxHp\":[" << maxHpBefore << "," << player.GetMaxHp() << "]}";
            autoChoiceTimer_ = 0.0f;
        }
    } else if (pendingChoices_ > 0) {
        autoChoiceTimer_ += dt;
        if (pendingChoices_ >= 2 || autoChoiceTimer_ > 3.0f) {
            OpenChoices();
            autoChoiceTimer_ = 0.0f;
        }
    }

    // 弾の集計は区画を出るときに消えるので、毎フレーム控える。再発動は、連鎖崩壊を積んでいる間の回数も分けて控える。
    {
        const auto& stats = world_.resources.bulletManager_->GetShooterStats();
        autoChainHits_ = (std::max)(autoChainHits_, stats.chainHits);
        autoDetonations_ = (std::max)(autoDetonations_, stats.detonations);
        autoWallBounces_ = (std::max)(autoWallBounces_, world_.resources.bulletManager_->GetGrowthStats().wallBounces);
        autoPierces_ = (std::max)(autoPierces_, world_.resources.bulletManager_->GetGrowthStats().actorPierces);
        autoBurstStats_ = {(std::max)(autoBurstStats_[0], stats.burstTriggers), (std::max)(autoBurstStats_[1], stats.burstRepeats),
                           (std::max)(autoBurstStats_[2], stats.burstChildren)};
        if (survivor::LevelOf(partLevels_, survivor::PartId::ChainCollapse) > 0) {
            autoFusedSeconds_ += dt;
            autoRepeatsWhileFused_ = (std::max)(autoRepeatsWhileFused_, stats.burstRepeats);
        }
    }
    // 合体の4秒後（再発動が起きている頃）の画面を保存する。
    if (autoFusionShotAt_ > 0.0f && autoTestTime_ >= autoFusionShotAt_ && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("fusion_after");
        autoFusionShotAt_ = -1.0f;
        autoFusionLoad_[1] = survivor::LoadOf(config_.parts, partLevels_);
    }

    // 案内：それぞれの案内が出て0.8秒の画面を保存する。
    if (hintShowing_ >= 0 && hintTimer_ >= 0.8f && !autoHintCaptured_[static_cast<size_t>(hintShowing_)] &&
        world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("hint_" + std::to_string(hintShowing_));
        autoHintCaptured_[static_cast<size_t>(hintShowing_)] = true;
    }
    // 演出：宿敵の登場（1.6秒）・撃破（0.9秒）・自機が倒れた演出（1.0秒）の画面を保存する。
    if (world_.run.tankRunCapturePath_.empty()) {
        if (cineMode_ == CineMode::RivalIntro && !autoIntroCaptured_ && cineTime_ >= 1.6f) {
            RequestCapture("rival_intro");
            autoIntroCaptured_ = true;
        } else if (cineMode_ == CineMode::RivalDefeat && !autoDefeatCaptured_ && cineTime_ >= 0.9f) {
            RequestCapture("rival_defeat");
            autoDefeatCaptured_ = true;
        } else if (cineMode_ == CineMode::PlayerDown && !autoDownCaptured_ && cineTime_ >= 1.0f) {
            RequestCapture("player_down");
            autoDownCaptured_ = true;
        }
    }
    // MYENGINE_SURVIVOR_DEATH=秒：その区画の時刻で無敵を外して倒れ、倒れた演出と結果画面を確かめる。
    if (autoDeathAt_ < 0.0f) {
        wchar_t death[16]{};
        autoDeathAt_ = GetEnvironmentVariableW(L"MYENGINE_SURVIVOR_DEATH", death, 16) > 0 ? static_cast<float>(_wtof(death)) : 1.0e9f;
    }
    if (!player.IsDead() && elapsed_ >= autoDeathAt_) {
        world_.presentation.debugPlayerNoDamage_ = false;
        player.SetDebugNoDamage(false);
        // 機体の遊び方では1回の被弾に上限があるので、無敵を外して敵に倒されるのを待つ（死因も本物になる）。
        if (!HeroMode()) {
            player.TakeDamage(1000000, 0.0f);
        }
    }
    if (player.IsDead() && IsResultVisible() && autoTestClearedTime_ < 0.0f) {
        autoTestClearedTime_ = autoTestTime_;
    }
    // 予告：群れと輪のそれぞれ最初の予告を、湧く0.5秒前に保存する。寄ってくる敵の数の最大も見る。
    for (const PendingWave& wave : pendingWaves_) {
        bool& captured = wave.ring ? autoRingWarningCaptured_ : autoSwarmWarningCaptured_;
        if (!captured && elapsed_ >= wave.at - 0.5f && world_.run.tankRunCapturePath_.empty()) {
            RequestCapture(wave.ring ? "ring_warning" : "swarm_warning");
            captured = true;
        }
    }
    autoMaxChasers_ = (std::max)(autoMaxChasers_, CountChasers());
    // 包囲の輪は最初の輪の1秒後、宿敵は現れて10秒後の画面を保存する。
    if (ringsSpawned_ == 1 && !autoRingCaptured_ && elapsed_ >= nextRing_ - config_.threat.ringInterval + 1.0f &&
        world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("ring");
        autoRingCaptured_ = true;
    }
    if (IsRivalActive() && !autoRivalCaptured_ && elapsed_ >= rivalSpawnedAt_ + 10.0f && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("rival");
        autoRivalCaptured_ = true;
    }
    if (autoTestNextCapture_ < static_cast<int>(kSectorCaptureSeconds.size()) &&
        elapsed_ >= kSectorCaptureSeconds[static_cast<size_t>(autoTestNextCapture_)]) {
        RequestCapture("sector_" + std::to_string(static_cast<int>(kSectorCaptureSeconds[static_cast<size_t>(autoTestNextCapture_)])));
        ++autoTestNextCapture_;
    }

    const AliveCounts alive = CountAlive();
    if (elapsed_ >= 120.0f) {
        maxAliveAfterTwoMinutes_ = (std::max)(maxAliveAfterTwoMinutes_, alive.total);
    }
    if (autoTestTime_ >= autoTestNextSample_) {
        autoTestNextSample_ += 10.0f;
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sampleStart_).count();
        const double fps = seconds > 0.0 ? static_cast<double>(drawnFrames_ - sampleFrames_) / seconds : 0.0;
        sampleStart_ = now;
        sampleFrames_ = drawnFrames_;
        if (autoTestLog_.tellp() > 0) {
            autoTestLog_ << ",";
        }
        autoTestLog_ << "{\"t\":" << static_cast<int>(elapsed_) << ",\"shapes\":" << alive.shapes << ",\"tanks\":" << alive.tanks
                     << ",\"big\":" << alive.bigPentagons << ",\"total\":" << alive.total << ",\"fps\":" << std::fixed
                     << std::setprecision(1) << fps << std::defaultfloat << ",\"hp\":" << player.GetHp() << ",\"level\":" << level_
                     << ",\"collected\":" << expCollected_ << ",\"dropped\":" << expDropped_ << ",\"shards\":" << pickups_.GetCount()
                     << ",\"shots\":" << player.GetPrimaryAttackCount() - autoSampleShots_ << ",\"heat\":" << player.GetHeat().GetHeat()
                     << ",\"burst\":" << autoBurstStats_[0] << ",\"repeat\":" << autoBurstStats_[1]
                     << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"threat\":" << threatStage_
                     << ",\"rivalHp\":" << (IsRivalActive() ? world_.resources.enemy_->GetHp() : -1) << "}";
        autoSampleShots_ = player.GetPrimaryAttackCount();
    }

    if (cleared_ && !restRoomOpen_ && autoTestClearedTime_ < 0.0f) {
        autoTestClearedTime_ = autoTestTime_;
    }
    // 機体の遊び方で波を全部通すときは、波の秒の合計とボスの分だけ長く待つ。
    const float fullRunSeconds = HeroMode() ? config_.waves.seconds * static_cast<float>(config_.waves.count) + 300.0f : 360.0f;
    const bool timedOut = autoTestTime_ > kSectorAutoTestExitSeconds + (autoFullRun_ ? fullRunSeconds : 90.0f);
    if ((autoTestClearedTime_ >= 0.0f && autoTestTime_ - autoTestClearedTime_ > 1.5f) || timedOut) {
        if (!autoTestCaptureQueued_) {
            RequestCapture(cleared_ ? "cleared" : "timeout");
            autoTestCaptureQueued_ = true;
            return;
        }
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        // 調整パネルの保存：今の設定を別のファイルへ書いて読み戻し、主な値が同じになるかを見る。
        bool roundTrip = false;
        {
            const std::string path = std::string(kAutoTestDirectory) + "/config_roundtrip.json";
            std::error_code ignored;
            std::filesystem::copy_file(kConfigPath, path, std::filesystem::copy_options::overwrite_existing, ignored);
            std::string error;
            survivor::SpawnConfig loaded;
            if (survivor::SaveSpawnConfig(path, ConfigForSaving(), error) && survivor::LoadSpawnConfig(path, loaded, error)) {
                roundTrip = loaded.ExpToNextLevel(10) == config_.ExpToNextLevel(10) && loaded.heat.maxHeat == config_.heat.maxHeat &&
                            loaded.heat.coolPerSecondWhileFiring == config_.heat.coolPerSecondWhileFiring &&
                            loaded.shapeTargetAlive == config_.shapeTargetAlive && loaded.choiceSlowTimeScale == config_.choiceSlowTimeScale &&
                            loaded.parts[0].perLevel == config_.parts[0].perLevel && loaded.forms[1].damageScale == config_.forms[1].damageScale &&
                            loaded.tankTypes == config_.tankTypes && loaded.weaponsEnabled == config_.weaponsEnabled &&
                            loaded.weapons.size() == config_.weapons.size() && loaded.weapons[1].damageScale == config_.weapons[1].damageScale &&
                            loaded.weapons[2].ammo == config_.weapons[2].ammo && loaded.startWeapons == config_.startWeapons &&
                            loaded.loot.weaponPrice == config_.loot.weaponPrice && loaded.stages.swarmScale == config_.stages.swarmScale &&
                            loaded.barrier.radius == config_.barrier.radius && loaded.elites.healRatio == config_.elites.healRatio &&
                            loaded.crafting.branches.size() == config_.crafting.branches.size() && loaded.exitOpenSeconds == fileExitOpenSeconds_ &&
                            loaded.rival.enabled == fileRivalEnabled_ && loaded.heroes.start == config_.heroes.start &&
                            loaded.heroes.heroes.size() == config_.heroes.heroes.size() && !config_.heroes.heroes.empty() &&
                            loaded.heroes.heroes[0].abilities[1].cooldown == config_.heroes.heroes[0].abilities[1].cooldown &&
                            loaded.heroes.heroes[0].abilities[1].kind == config_.heroes.heroes[0].abilities[1].kind &&
                            loaded.heroes.heroes[0].ultimateCharge == config_.heroes.heroes[0].ultimateCharge &&
                            loaded.heroes.heroes.back().abilities[3].rate == config_.heroes.heroes.back().abilities[3].rate &&
                            loaded.heroes.heroes.back().abilities[2].radius == config_.heroes.heroes.back().abilities[2].radius;
            }
        }
        // 拠点：結果画面で持ち帰ってセーブした物を読み直し、同じになるかを見る。
        CommitRunToBase();
        bool baseRoundTrip = false;
        {
            survivor::BaseInventory loaded;
            std::string error;
            baseRoundTrip = survivor::LoadBaseInventory(BasePath(), config_.parts, loaded, error) && loaded == base_;
        }
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"configRoundTrip\":" << (roundTrip ? "true" : "false") << ",\"baseRoundTrip\":" << (baseRoundTrip ? "true" : "false")
            << ",\"baseEquipOk\":" << (autoBaseEquipOk_ ? "true" : "false") << ",\"craftOk\":" << (autoCraftOk_ ? "true" : "false") << ",\"branchOk\":" << (autoBranchOk_ ? "true" : "false")
            << ",\"barrier\":{\"seconds\":" << barrierSeconds_ << ",\"blocked\":" << barrierBlocked_ << ",\"pushes\":" << barrierPushes_
            << ",\"breaks\":" << barrierBreaks_ << ",\"staminaMin\":" << autoStaminaMin_ << ",\"staminaEnd\":" << barrierStamina_
            << ",\"netSegments\":" << BarrierNetSegmentCount() << ",\"dashes\":" << player.GetDashStartedCount() << "}"
            << ",\"warperOffset\":[" << autoWarperOffset_.x << "," << autoWarperOffset_.y << "," << autoWarperOffset_.z << "]"
            << ",\"audio\":{\"loaded\":" << audio_.GetLoadedFiles() << ",\"missing\":" << audio_.GetMissingFiles() << ",\"plays\":{" << [&] {
                   std::ostringstream plays;
                   bool first = true;
                   for (const auto& [cue, count] : audio_.GetPlayCounts()) {
                       plays << (first ? "" : ",") << "\"" << cue << "\":" << count;
                       first = false;
                   }
                   return plays.str();
               }() << "}}"
            << ",\"homingTurns\":" << autoHomingTurns_ << ",\"elites\":{\"erratic\":" << eliteStats_.erratic << ",\"healers\":" << eliteStats_.spawned[0] << ",\"warpers\":" << eliteStats_.spawned[1]
            << ",\"layers\":" << eliteStats_.spawned[2] << ",\"heals\":" << eliteStats_.heals << ",\"healedHp\":" << eliteStats_.healedHp << ",\"warps\":" << eliteStats_.warps
            << ",\"minesLaid\":" << eliteStats_.minesLaid << ",\"mineExplosions\":" << eliteStats_.mineExplosions << ",\"mineHits\":" << eliteStats_.mineHits << "}"
            << ",\"defeated\":" << world_.combat.defeatedEnemies_ << ",\"swarms\":" << swarmsSpawned_ << ",\"crafted\":" << crafted_ << ",\"evolved\":" << evolved_ << ",\"fullRun\":" << (autoFullRun_ ? "true" : "false")
            << ",\"stage\":" << stage_ + 1 << ",\"bossDefeated\":" << (IsBossStage() && rivalDefeated_ ? "true" : "false")
            << ",\"bossDefeatedAt\":" << rivalDefeatedAt_ << ",\"baseBroughtHome\":" << (baseBroughtHome_ ? "true" : "false")
            << ",\"base\":{\"money\":" << base_.money << ",\"materials\":[" << base_.materials[0] << "," << base_.materials[1] << ","
            << base_.materials[2] << "," << base_.materials[3] << "],\"chips\":" << base_.chips.size() << ",\"weapons\":" << base_.weapons.size()
            << ",\"sorties\":" << base_.sorties << ",\"clears\":" << base_.clears << ",\"equipped3\":\""
            << (base_.equipped[2] >= 0 ? base_.weapons[static_cast<size_t>(base_.equipped[2])].key + ":" +
                                             survivor::RarityKey(base_.weapons[static_cast<size_t>(base_.equipped[2])].rarity) + " chips " +
                                             std::to_string(base_.weapons[static_cast<size_t>(base_.equipped[2])].chips.size())
                                       : std::string())
            << "\"}" << ",\"rest\":{\"state\":" << autoRestState_ << ",\"equips\":" << restEquips_ << ",\"attaches\":" << restAttaches_
            << ",\"detaches\":" << restDetaches_ << ",\"sales\":" << restSales_ << ",\"soldMoney\":" << restSoldMoney_
            << ",\"discards\":" << restDiscards_ << ",\"chipLevels\":" << autoRestChipLevels_ << "}" << ",\"completed\":" << (cleared_ ? "true" : "false") << ",\"cleared\":" << (cleared_ ? "true" : "false")
            << ",\"sectorSeconds\":" << elapsed_ << ",\"exitOpen\":" << (exitOpen_ ? "true" : "false") << ",\"exit\":[" << exitPosition_.x
            << "," << exitPosition_.y << "],\"maxAliveAfterTwoMinutes\":" << maxAliveAfterTwoMinutes_
            << ",\"resultState\":" << static_cast<int>(world_.combat.combatFlow_.GetState()) << ",\"level\":" << level_
            << ",\"pendingChoices\":" << pendingChoices_ << ",\"choicesTaken\":" << choicesTaken_
            << ",\"minTimeScaleWhileOpen\":" << autoMinTimeScale_ << ",\"partLevels\":{" <<
            [&] {
                std::ostringstream levels;
                for (size_t i = 0; i < survivor::kPartCount; ++i) {
                    levels << (i ? "," : "") << "\"" << config_.parts[i].key << "\":" << partLevels_[i];
                }
                return levels.str();
            }()
            << "},\"maxHp\":" << player.GetMaxHp() << ",\"choices\":[" << autoChoiceLog_.str() << "]"
            << ",\"expCollected\":" << expCollected_ << ",\"expDropped\":" << expDropped_ << ",\"maxHeat\":" << autoMaxHeat_
            << ",\"overheats\":" << player.GetHeat().GetOverheatCount() << ",\"form\":\"" << formKey_
            << "\",\"formChosenAt\":" << autoFormChosenAt_ << ",\"formPath\":\"" << formPath_ << "\",\"evolvedAt\":" << autoEvolvedAt_
            << ",\"evolveOffers\":" << autoEvolveOffers_ << ",\"formBarrels\":"
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->barrels.size() : 0) << ",\"formSpread\":"
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->spreadAngleDeg : 0.0f) << ",\"formFan\":"
            << (player.GetActiveClassConfig() && !player.GetActiveClassConfig()->barrels.empty()
                    ? player.GetActiveClassConfig()->barrels.front().angleDeg
                    : 0.0f)
            << ",\"loaderRamp\":" << (MakePartEffects().loaderRamp ? "true" : "false") << ",\"detonations\":" << autoDetonations_
            << ",\"wallBounces\":" << autoWallBounces_ << ",\"pierces\":" << autoPierces_ << ",\"formClass\":\""
            << (player.GetActiveClassConfig() ? player.GetActiveClassConfig()->id : std::string("none"))
            << "\",\"railShots\":" << player.GetSpecialCombatStats().railShots << ",\"critShots\":" << player.GetCritShots()
            << ",\"spikeKills\":" << spikeKills_ << ",\"spikeProbe\":" << autoSpikeProbe_ << ",\"premiumChests\":" << premiumTotal_
            << ",\"dashExplosions\":" << player.GetDashExplosionCount() << ",\"maxStamina\":" << player.GetStats().maxStamina
            << ",\"relics\":[" << autoRelicLog_.str() << "],\"dashBlocked\":" << player.GetDashBlocked()
            << ",\"stillShots\":" << player.GetStillShots() << ",\"trait\":\"" << survivor::TraitKey(trait_) << "\",\"coolPerSecond\":" << player.GetHeat().GetConfig().coolPerSecond
            << ",\"coolPerSecondWhileFiring\":" << player.GetHeat().GetConfig().coolPerSecondWhileFiring
            << ",\"heatMax\":" << player.GetHeat().GetMaxHeat() << ",\"shieldMax\":" << player.GetShieldMax() << ",\"shieldCheck\":[" << autoShieldLog_.str() << "]"
            << ",\"threatStage\":" << threatStage_ << ",\"rings\":" << ringsSpawned_ << ",\"rivalSpawnedAt\":" << rivalSpawnedAt_
            << ",\"rivalDefeatedAt\":" << rivalDefeatedAt_ << ",\"removals\":" << removals_ << ",\"maxLoad\":" << maxLoad_
            << ",\"loadAtEnd\":" << survivor::LoadOf(config_.parts, partLevels_) << ",\"capacityAtEnd\":" << Capacity()
            << ",\"removalLog\":[" << removalRecord_.str() << "]" << ",\"qUses\":" << qUses_ << ",\"qLog\":[" << qRecord_.str() << "]"
            << ",\"swarms\":" << swarmsSpawned_ << ",\"wavesSkipped\":" << wavesSkipped_ << ",\"heavySquares\":" << heavySpawned_
            << ",\"lungePentagons\":" << lungeSpawned_ << ",\"maxChasers\":" << autoMaxChasers_ << ",\"qSwapProbe\":" << autoQSwapProbe_ << ",\"purges\":" << purges_ << ",\"moraleKills\":" << moraleKills_
            << ",\"effects\":{\"shotDamageScale\":" << MakePartEffects().shotDamageScale << ",\"residualHeat\":" << MakePartEffects().residualHeat
            << ",\"ramShield\":" << MakePartEffects().ramShield << ",\"stillDamageBonus\":" << MakePartEffects().stillDamageBonus
            << ",\"bodyDamage\":" << player.GetStats().bodyDamage
            << ",\"colliderDamage\":" << player.GetDamage() << ",\"shield\":\"" << player.GetShield() << "/" << player.GetShieldMax() << "\"}"
            << ",\"fusions\":" << fusions_ << ",\"fusionAt\":" << fusionTime_ << ",\"fusionLoad\":[" << autoFusionLoad_[0] << ","
            << autoFusionLoad_[1] << "],\"burstTriggers\":" << autoBurstStats_[0] << ",\"burstRepeats\":" << autoBurstStats_[1]
            << ",\"burstChildren\":" << autoBurstStats_[2] << ",\"repeatsWhileFused\":" << autoRepeatsWhileFused_
            << ",\"damageNumbers\":" << damageNumbers_.GetSpawnedCount() << ",\"fusedSeconds\":" << autoFusedSeconds_ << ",\"chainHits\":" << autoChainHits_ << ",\"criticalBlasts\":" << criticalBlasts_
            << ",\"partsAtEnd\":\"" << DescribeParts(99) << "\""
            << ",\"hero\":{\"enabled\":" << (HeroMode() ? "true" : "false") << ",\"key\":\"" << (HeroMode() ? Hero().key : std::string())
            << "\",\"primary\":\"" << formKey_ << "\",\"uses\":[" << abilityUses_[0] << "," << abilityUses_[1] << "," << abilityUses_[2] << ","
            << abilityUses_[3] << "],\"cooldownSeen\":[" << abilityCooldownSeen_[0] << "," << abilityCooldownSeen_[1] << ","
            << abilityCooldownSeen_[2] << "," << abilityCooldownSeen_[3] << "],\"sprintSeconds\":" << sprintSeconds_
            << ",\"walkSpeed\":" << autoMoveDistance_[0] / (std::max)(0.001f, autoMoveSeconds_[0])
            << ",\"sprintSpeed\":" << autoMoveDistance_[1] / (std::max)(0.001f, autoMoveSeconds_[1]) << ",\"ultimateCharge\":" << ultimateCharge_
            << ",\"ultimateFullAt\":" << ultimateFullAt_ << ",\"ultimateFills\":" << ultimateFills_ << ",\"rocketBlasts\":" << rocketBlasts_
            << ",\"rocketKills\":" << rocketKills_ << ",\"healed\":" << healedByZone_ << ",\"healInsideSeconds\":" << healZoneInsideSeconds_
            << ",\"barrageShots\":" << barrageShots_ << ",\"barrageSeconds\":" << barrageSeconds_ << ",\"overheats\":" << player.GetHeat().GetOverheatCount()
            << ",\"roundsCounted\":" << (player.GetSurvivorRounds() >= 0 ? "true" : "false")
            << ",\"bulletDamage\":" << player.GetStats().bulletDamage << ",\"drainDamage\":" << drainDamage_
            << ",\"beamLockedSeconds\":" << beamLockedSeconds_ << ",\"absorbGained\":" << absorbGained_ << ",\"absorb\":" << absorb_
            << ",\"selfHealed\":" << selfHealed_ << ",\"selfHealSeconds\":" << selfHealSeconds_
            << ",\"phaseAvoided\":" << player.GetSurvivorPhaseAvoided() << ",\"orbDamage\":" << orbDamage_
            << ",\"orbBounces\":" << orbBounces_ << ",\"lanceDamage\":" << lanceDamage_ << ",\"lanceMaxHits\":" << lanceMaxHits_
            << ",\"primaryShots\":" << player.GetPrimaryAttackCount() << ",\"barrierBlocked\":" << barrierBlocked_
            << ",\"barrierPushes\":" << barrierPushes_ << ",\"chargePushes\":" << chargePushes_ << ",\"shockwavePushes\":" << shockwavePushes_
            << ",\"fortressBlocked\":" << fortressBlocked_ << ",\"fortressBlasts\":" << fortressBlasts_ << ",\"fortressDamage\":" << fortressDamage_
            << "}"
            << ",\"waves\":{\"count\":" << config_.waves.count << ",\"seconds\":" << config_.waves.seconds << ",\"maxHp\":" << player.GetMaxHp()
            << ",\"damageTaken\":" << player.GetSurvivorDamageTaken() << ",\"damageCapped\":" << player.GetSurvivorDamageCapped()
            << ",\"hits\":" << player.GetSurvivorHits() << ",\"sameSourceBlocked\":" << player.GetSurvivorSameSourceBlocked()
            << ",\"repairs\":" << repairs_ << ",\"repairSpent\":" << repairSpent_ << ",\"points\":" << bag_.money
            << ",\"healthCheck\":" << (autoHealthCheck_.tellp() > 0 ? autoHealthCheck_.str() : std::string("null")) << ",\"records\":[" << [&] {
                   std::ostringstream records;
                   for (size_t i = 0; i < waveRecords_.size(); ++i) {
                       const WaveRecord& r = waveRecords_[i];
                       records << (i ? "," : "") << "{\"wave\":" << r.wave << ",\"points\":" << r.points << ",\"damage\":" << r.damage
                               << ",\"capped\":" << r.capped << ",\"wound\":" << r.wound << ",\"hp\":" << r.hp << ",\"maxHp\":" << r.maxHp
                               << ",\"seconds\":" << r.seconds << "}";
                   }
                   return records.str();
               }() << "]}"
            << ",\"shop\":{\"spent\":" << shopSpent_ << ",\"rerolls\":" << shopRerollsTotal_ << ",\"tiers\":[" << upgradeTiers_[0] << ","
            << upgradeTiers_[1] << "," << upgradeTiers_[2] << "," << upgradeTiers_[3] << "," << upgradeTiers_[4] << "],\"perks\":\"" << [&] {
                   std::string names;
                   for (size_t i = 0; i < survivor::kPerkCount; ++i) {
                       if (perks_[i]) {
                           names += std::string(names.empty() ? "" : " ") + survivor::GetPerkInfo(static_cast<survivor::PerkId>(i)).key;
                       }
                   }
                   return names;
               }() << "\",\"bought\":[" << [&] {
                   std::ostringstream bought;
                   for (size_t i = 0; i < shopLog_.size(); ++i) {
                       bought << (i ? "," : "") << "\"" << shopLog_[i] << "\"";
                   }
                   return bought.str();
               }() << "],\"maxHp\":" << player.GetMaxHp() << "}"
            << ",\"result\":{\"lastHit\":\"" << player.GetSurvivorLastHit() << "\",\"dead\":" << (player.IsDead() ? "true" : "false")
            << ",\"pointsEarned\":" << pointsEarned_ << ",\"score\":" << (HeroMode() ? ComputeHeroScore().total : 0) << "}"
            // 機体のセーブ（K6）：書いて読み直し、同じになるか。解放の確認・結果画面で足した解放ポイント・次の機体まで。
            << ",\"heroSave\":{\"roundTrip\":" << [&] {
                   std::string error;
                   survivor::HeroSave loaded;
                   const bool same = survivor::SaveHeroSave(HeroSavePath(), heroSave_, error) &&
                                     survivor::LoadHeroSave(HeroSavePath(), loaded, error) && loaded == heroSave_;
                   return same ? "true" : "false";
               }() << ",\"unlockOk\":" << (autoUnlockOk_ ? "true" : "false") << ",\"points\":" << heroSave_.points
            << ",\"gained\":" << heroAwardGained_ << ",\"bonus\":" << heroAwardBonus_ << ",\"unlocked\":" << heroSave_.unlocked.size()
            << ",\"selected\":\"" << heroSave_.selected << "\",\"sorties\":" << heroSave_.sorties << ",\"next\":\"" << NextHeroText() << "\"}"
            << ",\"weaponMode\":" << (WeaponMode() ? "true" : "false") << ",\"weaponSwaps\":" << weaponSwaps_ << ",\"reloads\":" << reloads_ << ",\"lockedAttacks\":" << autoLockedAttacks_
            << ",\"lockedSeconds\":" << autoLockedSeconds_ << ",\"emptyAttacks\":" << autoEmptyAttacks_ << ",\"emptySeconds\":" << autoEmptySeconds_
            << ",\"reserves\":[" << reserves_[0] << "," << reserves_[1] << "," << reserves_[2] << "],\"swapMeasured\":[" << autoSwapMeasured_[0] << ","
            << autoSwapMeasured_[1] << "," << autoSwapMeasured_[2] << "]"
            << ",\"inventoryOpens\":" << inventoryOpens_ << ",\"inventoryFrozen\":" << (autoInventoryFrozen_ ? "true" : "false")
            << ",\"lootDropped\":[" << lootDropped_[0] << "," << lootDropped_[1] << "," << lootDropped_[2] << "," << lootDropped_[3]
            << "],\"lootPicked\":[" << lootPicked_[0] << "," << lootPicked_[1] << "," << lootPicked_[2] << "," << lootPicked_[3]
            << "],\"ammoPicked\":[" << ammoPicked_[0] << "," << ammoPicked_[1] << "," << ammoPicked_[2] << "],\"money\":" << bag_.money
            << ",\"materials\":[" << bag_.materials[0] << "," << bag_.materials[1] << "," << bag_.materials[2] << "," << bag_.materials[3]
            << "],\"bag\":\"" << [&] {
                   std::ostringstream names;
                   for (const auto& chip : bag_.chips) {
                       names << config_.parts[static_cast<size_t>(chip.part)].key << ":" << survivor::RarityKey(chip.rarity) << " ";
                   }
                   for (const auto& weapon : bag_.weapons) {
                       names << config_.weapons[static_cast<size_t>(weapon.weapon)].key << ":" << survivor::RarityKey(weapon.rarity) << " ";
                   }
                   return names.str();
               }() << "\"" << ",\"weapons\":[" <<
            [&] {
                std::ostringstream weapons;
                for (size_t i = 0; i < weaponSlots_.size(); ++i) {
                    const AutoWeaponStat& stat = autoWeaponStats_[i];
                    weapons << (i ? "," : "") << "{\"key\":\""
                            << (weaponSlots_[i] >= 0 ? config_.weapons[static_cast<size_t>(weaponSlots_[i])].key : std::string())
                            << "\",\"seconds\":" << stat.seconds << ",\"attacks\":" << stat.attacks << ",\"barrels\":" << stat.barrels
                            << ",\"damageScale\":" << stat.damageScale << ",\"shotDamage\":" << stat.shotDamage << ",\"reloadScale\":" << stat.reloadScale
                            << ",\"railShots\":" << stat.railShots << ",\"overheats\":" << stat.overheats << ",\"blasts\":" << stat.blasts << ",\"captured\":" << (stat.captured ? "true" : "false") << "}";
                }
                return weapons.str();
            }() << "]"
            << ",\"samples\":[" << autoTestLog_.str() << "]}\n";
        autoTest_ = AutoTest::None;
        PostQuitMessage(0);
    }
}

void SurvivorRunController::UpdateTourAutoTest(float dt)
{
    // 各地点へまっすぐ進み、着いたら（または10秒で）止まってカメラが落ち着くのを待ち、画面を保存する。
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    autoTestLegTime_ += dt;
    if (autoTourForm_ && formStage_ == 0) {
        EvolveTo(static_cast<survivor::FormId>(autoFormIndex_));
    }
    const myEngine::Vector3 position = player.GetWorldPosition();
    const myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};

    if (autoTestCaptureQueued_) {
        // 保存の要求は描画で写され、次の更新の初めに書き出されて空に戻る。
        if (!world_.run.tankRunCapturePath_.empty()) {
            return;
        }
        autoTestCaptureQueued_ = false;
        autoTestSettling_ = false;
        autoTestLegTime_ = 0.0f;
        ++autoTestWaypoint_;
    }

    if (autoTestWaypoint_ >= kTourWaypoints.size()) {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), minX, minY, maxX, maxY);
        std::ofstream(std::string(kAutoTestDirectory) + "/autotest.json")
            << "{\"completed\":true,\"arena\":[" << minX << "," << minY << "," << maxX << "," << maxY << "],\"halfView\":["
            << arenacamera::HalfViewHeight() * arenacamera::kAspect << "," << arenacamera::HalfViewHeight() << "],\"waypoints\":["
            << autoTestLog_.str() << "],\"enemies\":" << world_.resources.enemyManager_->GetEnemyCount()
            << ",\"rivalActive\":" << (world_.gameplayQueries->IsRunRivalActive() ? "true" : "false")
            << ",\"menuOpen\":" << (world_.arenaRunController->IsTankRunMenuOpen() ? "true" : "false")
            << ",\"playerDead\":" << (player.IsDead() ? "true" : "false") << ",\"seconds\":" << autoTestTime_ << "}\n";
        autoTest_ = AutoTest::None;
        PostQuitMessage(0);
        return;
    }

    const AutoTestWaypoint& waypoint = kTourWaypoints[autoTestWaypoint_];
    const myEngine::Vector3 offset{waypoint.target.x - position.x, waypoint.target.y - position.y, 0.0f};
    const float distance = myEngine::Length(offset);
    if (!autoTestSettling_ && (distance < 1.5f || autoTestLegTime_ > 10.0f)) {
        autoTestSettling_ = true;
        autoTestLegTime_ = 0.0f;
    }
    if (!autoTestSettling_) {
        player.SetDemoInput(true, {offset.x / distance, offset.y / distance}, aim, false, false);
        return;
    }
    // 止まっている間は撃ち、主砲のまわりの熱の輪が画面に写るようにする。
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, true, false);
    if (autoTestLegTime_ < 1.2f) {
        return;
    }
    const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
    if (autoTestWaypoint_ > 0) {
        autoTestLog_ << ",";
    }
    autoTestLog_ << "{\"name\":\"" << waypoint.name << "\",\"target\":[" << waypoint.target.x << "," << waypoint.target.y
                 << "],\"player\":[" << position.x << "," << position.y << "],\"camera\":[" << camera.x << "," << camera.y
                 << "],\"reached\":" << (distance < kAutoTestReachDistance ? "true" : "false")
                 << ",\"aimError\":" << MeasureAimRoundTripError() << ",\"heat\":" << player.GetHeat().GetHeat() << "}";
    RequestCapture(waypoint.name);
    autoTestCaptureQueued_ = true;
}

void SurvivorRunController::UpdatePerfAutoTest(float dt)
{
    // 0〜15秒：敵を200体まで湧かせる（自機は中央で止まり、撃たない）。
    // 15〜35秒：散らばった場面を測る（敵はほぼ画面の外）。35秒から：画面の外の図形を2秒ごとに画面の中へ移し、
    // 自機は撃ち始める。45〜65秒：画面が敵で埋まった場面を測る。
    constexpr float kSpreadFrom = 15.0f, kSpreadTo = 35.0f, kCrowdFrom = 45.0f, kCrowdTo = 65.0f;
    auto& player = *world_.resources.player_;
    autoTestTime_ += dt;
    const myEngine::Vector3 position = player.GetWorldPosition();
    myEngine::Vector3 aim = position + myEngine::Vector3{6.0f, 0.0f, 0.0f};
    if (const ExpEnemy* nearest = world_.resources.enemyManager_->FindNearestEnemy(position, 30.0f)) {
        aim = nearest->GetWorldPosition();
    }
    player.SetDemoInput(true, {0.0f, 0.0f}, aim, perfCrowded_, false);
    if (autoTestTime_ >= kSpreadTo && (!perfCrowded_ || std::fmod(autoTestTime_ - kSpreadTo, 2.0f) < dt)) {
        perfCrowded_ = true;
        const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
        const float halfHeight = arenacamera::HalfViewHeight() - 1.5f;
        const float halfWidth = arenacamera::HalfViewHeight() * arenacamera::kAspect - 1.5f;
        std::uniform_real_distribution<float> x(camera.x - halfWidth, camera.x + halfWidth), y(camera.y - halfHeight, camera.y + halfHeight);
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || enemy->IsExpeditionCombatRole() || enemy->GetType() == ExpEnemyType::Shooter) {
                continue;
            }
            const myEngine::Vector3 at = enemy->GetWorldPosition();
            if (std::abs(at.x - camera.x) <= halfWidth && std::abs(at.y - camera.y) <= halfHeight) {
                continue;
            }
            for (int attempt = 0; attempt < 8; ++attempt) {
                const myEngine::Vector3 target{x(random_), y(random_), at.z};
                const float dx = target.x - position.x, dy = target.y - position.y;
                if (dx * dx + dy * dy > 16.0f && !world_.resources.stage_->IsCollisionWithAnyBlock(target, 1.2f)) {
                    enemy->SetWorldPosition(target);
                    break;
                }
            }
        }
    }
    if (autoTestTime_ >= 30.0f && autoTestNextCapture_ == 0 && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("perf_spread");
        ++autoTestNextCapture_;
    }
    if (autoTestTime_ >= 60.0f && autoTestNextCapture_ == 1 && world_.run.tankRunCapturePath_.empty()) {
        RequestCapture("perf_crowd");
        ++autoTestNextCapture_;
    }

    // 1秒ごとに、その1秒の fps・敵の数・画面の中の敵の数を記録する。
    if (autoTestTime_ >= perfNextSample_) {
        perfNextSample_ += 1.0f;
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - sampleStart_).count();
        const double fps = seconds > 0.0 ? static_cast<double>(drawnFrames_ - sampleFrames_) / seconds : 0.0;
        sampleStart_ = now;
        sampleFrames_ = drawnFrames_;
        const myEngine::Vector3 camera = world_.resources.camera->GetTranslate();
        const float halfHeight = arenacamera::HalfViewHeight();
        const float halfWidth = halfHeight * arenacamera::kAspect;
        int alive = 0, onScreen = 0;
        for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead()) {
                continue;
            }
            ++alive;
            const myEngine::Vector3 at = enemy->GetWorldPosition();
            if (std::abs(at.x - camera.x) <= halfWidth && std::abs(at.y - camera.y) <= halfHeight) {
                ++onScreen;
            }
        }
        const float t = autoTestTime_ - 1.0f;
        const int phase = t >= kSpreadFrom && t < kSpreadTo ? 0 : t >= kCrowdFrom && t < kCrowdTo ? 1 : -1;
        if (phase >= 0) {
            PerfPhase& sum = perfPhases_[static_cast<size_t>(phase)];
            ++sum.samples;
            sum.fpsSum += fps;
            sum.fpsMin = (std::min)(sum.fpsMin, fps);
            sum.aliveSum += alive;
            sum.onScreenSum += onScreen;
        }
        perfLog_ << (perfLog_.tellp() > 0 ? "," : "") << "{\"t\":" << static_cast<int>(t) << ",\"fps\":" << std::fixed << std::setprecision(1)
                 << fps << std::defaultfloat << ",\"alive\":" << alive << ",\"onScreen\":" << onScreen
                 << ",\"bullets\":" << world_.resources.bulletManager_->GetBulletCount() << "}";
    }

    if (autoTestTime_ < kCrowdTo + 0.5f || !world_.run.tankRunCapturePath_.empty()) {
        return;
    }
    auto phaseJson = [](const char* name, const PerfPhase& sum) {
        std::ostringstream text;
        const double n = (std::max)(1, sum.samples);
        text << "{\"name\":\"" << name << "\",\"samples\":" << sum.samples << ",\"fpsAvg\":" << std::fixed << std::setprecision(1)
             << sum.fpsSum / n << ",\"fpsMin\":" << (sum.samples > 0 ? sum.fpsMin : 0.0) << ",\"aliveAvg\":" << sum.aliveSum / n
             << ",\"onScreenAvg\":" << sum.onScreenSum / n << "}";
        return text.str();
    };
    std::ofstream(std::string(kAutoTestDirectory) + "/perf.json")
        << "{\"completed\":true,\"phases\":[" << phaseJson("spread", perfPhases_[0]) << "," << phaseJson("crowd", perfPhases_[1])
        << "],\"separationMsAvg\":" << (separationCalls_ > 0 ? separationSeconds_ * 1000.0 / static_cast<double>(separationCalls_) : 0.0)
        << ",\"samples\":[" << perfLog_.str() << "]}\n";
    autoTest_ = AutoTest::None;
    PostQuitMessage(0);
}

} // namespace gameplay
