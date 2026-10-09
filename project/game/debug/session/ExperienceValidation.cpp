#include "game/debug/session/ExperienceValidation.h"
#include "game/session/GameplaySystems.h"
#include "Enemy.h"
#include "game/run/TankSubmissionValidation.h"
#include <functional>
#include <fstream>
#include <iomanip>

namespace gameplay {

namespace {
constexpr uint32_t kExperienceSeed = 20260926;
/// @brief 実行検証の出力先ディレクトリーを返す。
std::string ExperienceDirectory(int variant)
{
    if (tanksubmission::Enabled())
        return "generated/submission_validation/";
    return std::string("generated/experience_validation/") + (variant == 1 ? "upper/" : "lower/");
}
} // namespace

void ExperienceValidation::InitializeExperienceValidation()
{
    wchar_t flag[8]{};
    if (world_.demo.titleDemo_)
        return;
    const bool submission = tanksubmission::Enabled();
    if (!submission && (GetEnvironmentVariableW(L"CG2_TANK_EXPERIENCE_AUTOTEST", flag, 8) == 0 || (flag[0] != L'1' && flag[0] != L'2')))
        return;
    if (submission) {
        flag[0] = L'1';
        ++tanksubmission::state.runs;
        if (tanksubmission::state.runs == 2) {
            world_.validation.experienceValidationVariant_ = 1;
            return;
        }
    }
    world_.validation.experienceValidationVariant_ = flag[0] == L'1' ? 1 : 2;
    wchar_t styleFlag[8]{};
    if (GetEnvironmentVariableW(L"CG2_TANK_EXPERIENCE_STYLE", styleFlag, 8) > 0 && styleFlag[0] >= L'0' && styleFlag[0] <= L'2')
        world_.validation.experienceValidationStyle_ = styleFlag[0] - L'0';
    world_.run.expeditionSeed_ = kExperienceSeed;
    world_.run.expeditionMapDefinition_ = tankexp::GenerateExpeditionMap(world_.run.expeditionSeed_, 30, 8);
    if (submission) {
        // Use the real generated graph. Pick a route with repair/workshop stops,
        // then select existing standard rooms to guarantee all three enemy views.
        std::function<int(const std::string&)> score = [&](const std::string& id) {
            const auto* n = tankexp::FindMapNode(world_.run.expeditionMapDefinition_, id);
            if (!n)
                return 0;
            int next = 0;
            for (const auto& to : n->next)
                next = (std::max)(next, score(to));
            return next + (n->kind == tankexp::NodeKind::Heal ? 50 : n->kind == tankexp::NodeKind::Upgrade ? 10 : 1);
        };
        std::string id = "tutorial_combat";
        int enemyRoom = 0;
        const char* rooms[] = {"command_post", "emp_patrol", "reflect_bastion"};
        const int firstColumns[] = {7, 10, 13};
        while (!id.empty()) {
            tanksubmission::state.path.push_back(id);
            auto it = std::find_if(world_.run.expeditionMapDefinition_.nodes.begin(), world_.run.expeditionMapDefinition_.nodes.end(),
                                   [&](const auto& n) {
                                       return n.id == id;
                                   });
            if (it == world_.run.expeditionMapDefinition_.nodes.end())
                break;
            if (enemyRoom < 3 && it->kind == tankexp::NodeKind::Combat && it->column >= firstColumns[enemyRoom])
                it->roomTemplate = rooms[enemyRoom++];
            std::string next;
            int best = -1;
            for (const auto& to : it->next)
                if (const int value = score(to); value > best) {
                    best = value;
                    next = to;
                }
            id = next;
        }
    }
    world_.run.expeditionIntroOffers_ = tankcontent::IntroUpgradeIds(world_.run.expeditionContent_, world_.run.expeditionSeed_);
    world_.run.expeditionMapAutoTest_ = world_.run.tankRunAutoTest_ = world_.run.combatValidationEnabled_ = false;
    world_.combat.tutorialConfig_.enabled = false;
    world_.presentation.debugPlayerNoDamage_ = false;
    world_.run.tankExpeditionMusicEnabled_ = world_.run.tankExpeditionEffectsEnabled_ = false;
    world_.run.tankExpeditionAudio_.SetMusicVolume(0);
    world_.run.tankExpeditionAudio_.SetEffectsVolume(0);
    std::filesystem::create_directories(ExperienceDirectory(world_.validation.experienceValidationVariant_));
    WriteExperienceValidationReport(false);
}

void ExperienceValidation::CaptureExperienceValidation(const std::string& name)
{
    if (!world_.run.tankRunCapturePath_.empty() ||
        std::find(world_.validation.experienceValidationCaptures_.begin(), world_.validation.experienceValidationCaptures_.end(), name) !=
            world_.validation.experienceValidationCaptures_.end())
        return;
    world_.validation.experienceValidationCaptures_.push_back(name);
    world_.run.tankRunCapturePath_ = ExperienceDirectory(world_.validation.experienceValidationVariant_) + name + ".png";
    if (tanksubmission::Enabled())
        RecordSubmissionUi(name);
}

void ExperienceValidation::WriteExperienceValidationReport(bool completed)
{
    const auto& map = world_.run.expeditionMapRun_.GetDefinition();
    nlohmann::json report = {{"completed", completed},
                             {"testMode", true},
                             {"variant", world_.validation.experienceValidationVariant_},
                             {"seed", world_.run.expeditionSeed_},
                             {"elapsed", world_.validation.experienceValidationElapsed_},
                             {"state", world_.validation.experienceValidationState_},
                             {"forcedTutorialClear", false},
                             {"forcedLaterCombat", true},
                             {"forcedLaterClears", world_.validation.experienceForcedLaterClears_},
                             {"tutorialInvulnerable", true},
                             {"laterInvulnerable", true},
                             {"meleeModuleGrantedForProbe", false},
                             {"meleeFixtureReplacesFirstNormalRoomEnemies", true},
                             {"buildStyle", tankbuild::Id(static_cast<tankbuild::Style>(world_.validation.experienceValidationStyle_))},
                             {"buildPreserved", world_.validation.experienceBuildPreserved_},
                             {"droneSamples", world_.validation.experienceDroneSamples_},
                             {"rarityScreensAreVisualFixtures", !tanksubmission::Enabled()},
                             {"additiveGrowthVerified", world_.validation.experienceEvolutionVerified_},
                             {"growthOfferSeedIsFixture", true},
                             {"initialKills", world_.validation.experienceInitialKills_},
                             {"initialPlayerBulletSamples", world_.validation.experiencePlayerBulletSamples_},
                             {"guideStageMask", world_.validation.experienceGuideStageMask_},
                             {"successfulDashes", world_.validation.experienceSuccessfulDashes_},
                             {"introWallet", world_.validation.experienceIntroWallet_},
                             {"afterIntroWallet", world_.validation.experienceAfterIntroWallet_},
                             {"introOffers", world_.validation.experienceValidationIntroOffers_},
                             {"introOfferDetails", world_.validation.experienceIntroOfferDetails_},
                             {"earlyFlightSamples", world_.validation.experienceEarlyFlightSamples_},
                             {"prematureCredits", world_.validation.experiencePrematureCredits_},
                             {"groundOrbSamples", world_.validation.experienceGroundOrbSamples_},
                             {"creditsCollected", world_.run.expeditionCreditsCollected_},
                             {"meleeProbeCompleted", world_.validation.experienceMeleeDone_},
                             {"meleeTargetHp", world_.validation.experienceMeleeMinHp_},
                             {"meleeSlashSamples", world_.validation.experienceMeleeSlashSamples_},
                             {"meleePlayerBulletSamples", world_.validation.experienceMeleeBulletSamples_},
                             {"meleeTargetDisplacement", world_.validation.experienceMeleeDisplacement_},
                             {"visited", world_.run.expeditionMapRun_.GetVisitedNodeIds()},
                             {"mapNodeCount", map.nodes.size()},
                             {"credits", world_.run.expeditionMapRun_.GetCurrency()},
                             {"level", world_.resources.player_->GetLevel()},
                             {"experience", world_.resources.player_->GetExp()},
                             {"errors", world_.validation.experienceValidationErrors_},
                             {"captures", world_.validation.experienceValidationCaptures_}};
    if (tanksubmission::Enabled()) {
        tanksubmission::state.experience = std::move(report);
        tanksubmission::Write();
    } else
        std::ofstream(ExperienceDirectory(world_.validation.experienceValidationVariant_) + "validation.json")
            << std::setw(2) << report << '\n';
}

void ExperienceValidation::BeginExperienceMeleeProbe()
{
    world_.validation.experienceMeleeStarted_ = true;
    world_.validation.experienceMeleeAge_ = 0;
    // A single passive high-HP target isolates real melee collision/knockback.
    // This fixture substitution happens only after the normal introductory flow.
    world_.resources.enemyManager_->ClearRunActors();
    world_.resources.bulletManager_->ClearAll();
    if (!world_.resources.player_->HasExpeditionCombatStyle() ||
        world_.resources.player_->GetExpeditionCombatStyle() != static_cast<tankbuild::Style>(world_.validation.experienceValidationStyle_))
        world_.validation.experienceValidationErrors_.push_back("Chosen base style was not equipped after introduction");
    world_.arenaRunController->ApplyTankRunCards();
    const cg2::Vector3 position = world_.resources.player_->GetWorldPosition();
    const std::array<cg2::Vector3, 4> directions{cg2::Vector3{1, 0, 0}, cg2::Vector3{0, 1, 0}, cg2::Vector3{-1, 0, 0},
                                                 cg2::Vector3{0, -1, 0}};
    bool spawned = false;
    for (const auto& direction : directions) {
        const cg2::Vector3 target = position + direction * 3.1f;
        if (world_.resources.stage_->IsCollisionWithAnyBlock(target, 1.0f) ||
            world_.resources.stage_->IsCollisionWithAnyBlock(position + direction * 1.6f, 0.8f))
            continue;
        if (world_.resources.enemyManager_->SpawnLevelEnemy(target, "tutorial_target", 500)) {
            world_.validation.experienceMeleeTargetStart_ = target;
            spawned = true;
            break;
        }
    }
    if (!spawned)
        world_.validation.experienceValidationErrors_.push_back("No safe melee probe placement");
}

bool ExperienceValidation::UpdateExperienceValidation(float dt)
{
    if (!world_.validation.experienceValidationVariant_)
        return false;
    if (tanksubmission::Enabled()) {
        if (tanksubmission::state.runs == 2) {
            UpdateSubmissionValidation(dt);
            return false;
        }
        if (UpdateSubmissionValidation(dt))
            return true;
    }
    using S = tankexp::GuidedCombatTutorial::Stage;
    world_.validation.experienceValidationElapsed_ += dt;
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    const auto stage = world_.run.expeditionGuide_.GetStage();
    const std::string state = (world_.run.expeditionBuildChoice_           ? "build_choice"
                               : world_.run.expeditionMapRun_.IsChoosing() ? "map_" + world_.run.expeditionMapRun_.GetCurrentNodeId()
                                                                           : world_.run.expeditionMapRun_.GetActiveNodeId()) +
                              (world_.run.expeditionGuideActive_ ? "/guide_" + std::to_string(static_cast<int>(stage)) : "");
    if (state != world_.validation.experienceValidationState_) {
        world_.validation.experienceValidationState_ = state;
        world_.validation.experienceValidationStateAge_ = 0;
        WriteExperienceValidationReport(false);
    }
    world_.validation.experienceValidationStateAge_ += dt;
    world_.resources.player_->SetDemoInput(true, {}, world_.resources.player_->GetWorldPosition() + cg2::Vector3{1, 0, 0}, false, false);
    if (world_.run.expeditionGuideActive_)
        world_.validation.experienceGuideStageMask_ |= 1u << static_cast<unsigned>(stage);
    for (const auto& orb : world_.run.expeditionCredits_) {
        if (!orb.flying)
            ++world_.validation.experienceGroundOrbSamples_;
        if (orb.flying && orb.flight >= 0.50f)
            world_.validation.experienceCreditArrivalEligible_ = true;
        if (orb.flying && orb.flight > 0.08f && orb.flight < 0.45f) {
            ++world_.validation.experienceEarlyFlightSamples_;
            CaptureExperienceValidation("credits_flight");
        }
    }
    if (!world_.validation.experienceCreditDelivered_ && world_.run.expeditionMapRun_.GetCurrency() > 20) {
        if (!world_.validation.experienceCreditArrivalEligible_)
            ++world_.validation.experiencePrematureCredits_;
        world_.validation.experienceCreditDelivered_ = true;
    }
    const float timeLimit = tanksubmission::Enabled() ? 160.0f : 115.0f;
    if (world_.validation.experienceValidationElapsed_ > timeLimit || world_.resources.player_->IsDead() ||
        !world_.validation.experienceValidationErrors_.empty()) {
        if (world_.validation.experienceValidationElapsed_ > timeLimit)
            world_.validation.experienceValidationErrors_.push_back("Experience runtime timed out");
        if (world_.resources.player_->IsDead())
            world_.validation.experienceValidationErrors_.push_back("Tutorial player died");
        WriteExperienceValidationReport(false);
        PostQuitMessage(7);
        return true;
    }
    if (world_.combat.combatFlow_.GetState() == GameFlowState::StageClear) {
        CaptureExperienceValidation("complete");
        if (world_.validation.experienceValidationStateAge_ < 1.0f || !world_.run.tankRunCapturePath_.empty())
            return false;
        const size_t visits = world_.run.expeditionMapRun_.GetVisitedNodeIds().size();
        if (!world_.run.expeditionMapRun_.IsComplete() || visits < 18 || visits > 22)
            world_.validation.experienceValidationErrors_.push_back("Procedural route did not reach 18..22 visited nodes and final boss");
        if (world_.validation.experienceIntroWallet_ != 58 ||
            world_.validation.experienceAfterIntroWallet_ != (world_.validation.experienceValidationVariant_ == 1 ? 43 : 58))
            world_.validation.experienceValidationErrors_.push_back("Intro paths have incorrect grant or purchase/skip economy");
        if (world_.validation.experienceValidationIntroOffers_.size() != 3 || world_.validation.experienceIntroOfferDetails_.size() != 3)
            world_.validation.experienceValidationErrors_.push_back("Intro did not offer exactly three modules");
        if (world_.validation.experiencePrematureCredits_ || world_.validation.experienceEarlyFlightSamples_ == 0)
            world_.validation.experienceValidationErrors_.push_back("Credits did not wait for flight arrival");
        if (world_.validation.experienceValidationStyle_ == 2 &&
            (!world_.validation.experienceMeleeDone_ || world_.validation.experienceMeleeMinHp_ >= 500 ||
             world_.validation.experienceMeleeSlashSamples_ == 0 || world_.validation.experienceMeleeBulletSamples_ != 0 ||
             world_.validation.experienceMeleeDisplacement_ < 0.10f))
            world_.validation.experienceValidationErrors_.push_back("Actual melee hit/knockback evidence incomplete");
        if (world_.validation.experienceValidationStyle_ != 2 &&
            (!world_.validation.experienceMeleeDone_ || world_.validation.experienceMeleeMinHp_ >= 500 ||
             world_.validation.experienceMeleeSlashSamples_ != 0 || world_.validation.experienceMeleeBulletSamples_ == 0))
            world_.validation.experienceValidationErrors_.push_back("Actual projectile attack evidence incomplete");
        if (world_.validation.experienceValidationStyle_ == 1 && world_.validation.experienceDroneSamples_ == 0)
            world_.validation.experienceValidationErrors_.push_back("Drone style did not create immediate companions");
        if (!world_.validation.experienceBuildPreserved_)
            world_.validation.experienceValidationErrors_.push_back("Build selection preservation not verified");
        if (!world_.validation.experienceEvolutionVerified_)
            world_.validation.experienceValidationErrors_.push_back("Additive workshop upgrade and preservation not verified");
        if (world_.validation.experienceValidationVariant_ == 1 &&
            (world_.validation.experienceInitialKills_ < 2 || world_.validation.experiencePlayerBulletSamples_ == 0 ||
             world_.validation.experienceSuccessfulDashes_ < 3 || world_.validation.experienceGroundOrbSamples_ == 0))
            world_.validation.experienceValidationErrors_.push_back("Real tutorial shooting/pickup/dash evidence incomplete");
        if (world_.resources.player_->GetLevel() != 1 || world_.resources.player_->GetExp() != 0)
            world_.validation.experienceValidationErrors_.push_back("Expedition leaked level/EXP progression");
        WriteExperienceValidationReport(world_.validation.experienceValidationErrors_.empty());
        if (tanksubmission::Enabled() && world_.validation.experienceValidationErrors_.empty()) {
            auto& check = tanksubmission::state;
            check.tutorialSaved = tanksubmission::TutorialSaved();
            if (!check.tutorialSaved || !check.repairDone || check.newEnemies != 7)
                check.errors.push_back("Missing persisted tutorial, repair transaction, or standard enemy rooms");
            check.finished = true;
            world_.combat.resultSelection_ = 1;
            world_.combatFlow->ConfirmResultSelection();
            tanksubmission::Write();
            return true;
        }
        PostQuitMessage(world_.validation.experienceValidationErrors_.empty() ? 0 : 7);
        return true;
    }
    if (world_.run.expeditionTransition_.IsActive()) {
        if (world_.run.expeditionTransition_.Cover() > 0.65f)
            CaptureExperienceValidation("transition");
        if (world_.run.expeditionBuildChoice_)
            world_.validation.experienceValidationStateAge_ = 0;
        return false;
    }
    if (world_.run.expeditionBuildChoice_) {
        if (world_.validation.experienceAfterIntroWallet_ < 0)
            world_.validation.experienceAfterIntroWallet_ = world_.run.expeditionMapRun_.GetCurrency();
        world_.validation.experiencePreviewRarity_ = (std::min)(4, static_cast<int>(world_.validation.experienceValidationStateAge_));
        if (world_.validation.experienceValidationStateAge_ > 0.30f)
            CaptureExperienceValidation("build_choice");
        if (std::fmod(world_.validation.experienceValidationStateAge_, 1.0f) > 0.65f)
            CaptureExperienceValidation("rarity_" + std::to_string(world_.validation.experiencePreviewRarity_));
        if (world_.validation.experienceValidationStateAge_ > 5.1f && world_.run.tankRunCapturePath_.empty())
            world_.expeditionBuildSelection->SelectExpeditionBuildStyle(world_.validation.experienceValidationStyle_);
        return false;
    }
    if (world_.run.expeditionMapRun_.IsChoosing()) {
        if (world_.run.expeditionMapRun_.GetVisitedNodeIds().empty())
            CaptureExperienceValidation("map");
        if (world_.run.expeditionMapRun_.GetVisitedNodeIds().size() == 2 && world_.validation.experienceAfterIntroWallet_ < 0) {
            world_.validation.experienceAfterIntroWallet_ = world_.run.expeditionMapRun_.GetCurrency();
            WriteExperienceValidationReport(false);
        }
        if (world_.validation.experienceValidationStateAge_ < 0.45f || !world_.run.tankRunCapturePath_.empty())
            return false;
        auto options = world_.run.expeditionMapRun_.GetAvailableNodeIds();
        if (options.empty())
            return false;
        std::string id = options.front();
        if (tanksubmission::Enabled()) {
            const auto index = world_.run.expeditionMapRun_.GetVisitedNodeIds().size();
            if (index < tanksubmission::state.path.size())
                id = tanksubmission::state.path[index];
            world_.expeditionMapController->RequestExpeditionMapNode(id);
            return false;
        }
        if (world_.run.expeditionMapRun_.GetVisitedNodeIds().empty())
            id = world_.validation.experienceValidationVariant_ == 1 ? "tutorial_combat" : "tutorial_skip";
        else
            for (const auto& option : options) {
                const auto* candidate = tankexp::FindMapNode(world_.run.expeditionMapRun_.GetDefinition(), option);
                if (candidate && candidate->kind == tankexp::NodeKind::Upgrade && !world_.validation.experienceEvolutionVerified_) {
                    id = option;
                    break;
                }
                if (candidate)
                    id = option;
            }
        world_.expeditionMapController->RequestExpeditionMapNode(id);
        return false;
    }
    if (!node)
        return false;
    if (node->role == tankexp::NodeRole::TutorialCombat) {
        world_.validation.experiencePlayerBulletSamples_ += static_cast<int>(world_.resources.bulletManager_->GetBulletCounts().player);
        if (stage == S::Briefing || stage == S::Vitals) {
            CaptureExperienceValidation(stage == S::Briefing ? "briefing" : "hp_stamina");
            if (world_.validation.experienceValidationStateAge_ > 0.75f && world_.run.tankRunCapturePath_.empty())
                world_.expeditionExperience->AcknowledgeGuidedExpedition();
            return false;
        }
        if (stage == S::Dash) {
            if (world_.resources.player_->IsDashing())
                CaptureExperienceValidation("dash");
            const auto bullets = world_.resources.bulletManager_->GetBulletCounts();
            const bool request = world_.validation.experienceValidationStateAge_ > 0.6f && !world_.resources.player_->IsDashing();
            const cg2::Vector2 move = (request || world_.resources.player_->IsDashing()) ? cg2::Vector2{0, 1} : cg2::Vector2{};
            world_.resources.player_->SetDemoInput(true, move, world_.resources.player_->GetWorldPosition() + cg2::Vector3{0, 1, 0}, false,
                                                   request);
            return false;
        }
        if (stage == S::Upgrade) {
            world_.validation.experienceSuccessfulDashes_ = world_.run.expeditionGuide_.GetCompletedDashes();
            if (world_.resources.player_->GetHp() != world_.resources.player_->GetMaxHp())
                world_.validation.experienceValidationErrors_.push_back("Tutorial practice reduced HP");
        }
        if (stage == S::Collect) {
            CaptureExperienceValidation("collect");
            if (!world_.run.expeditionCredits_.empty()) {
                cg2::Vector3 move = world_.run.expeditionCredits_.front().position - world_.resources.player_->GetWorldPosition();
                if (cg2::Length(move) > 0.1f)
                    move = cg2::Normalize(move);
                world_.resources.player_->SetDemoInput(true, {move.x, move.y}, world_.run.expeditionCredits_.front().position, false,
                                                       false);
            }
            return false;
        }
        const auto actors = world_.resources.enemyManager_->GetEnemyPtrs();
        if (!actors.empty() && actors.front() && !actors.front()->IsDead())
            world_.resources.player_->SetDemoInput(true, {}, actors.front()->GetWorldPosition(), true, false);
        return false;
    }
    if (tankexp::IsIntroUpgrade(node->role)) {
        CaptureExperienceValidation("intro_upgrades");
        if (world_.validation.experienceIntroWallet_ < 0) {
            world_.validation.experienceIntroWallet_ = world_.run.expeditionMapRun_.GetCurrency();
            world_.validation.experienceInitialKills_ = world_.combat.defeatedEnemies_;
            world_.validation.experienceValidationIntroOffers_ = world_.run.expeditionServiceOffers_;
            for (const auto& id : world_.validation.experienceValidationIntroOffers_) {
                const auto* offer = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
                const int price = world_.expeditionMapController->ExpeditionServicePrice(id);
                if (!offer || offer->rarity != 0 || offer->effects.size() != 1 || price != 15)
                    world_.validation.experienceValidationErrors_.push_back("Intro offers are not weak single-effect modules priced 15");
                world_.validation.experienceIntroOfferDetails_.push_back(
                    {{"id", id}, {"price", price}, {"rarity", offer ? offer->rarity : -1}});
            }
            WriteExperienceValidationReport(false);
        }
        if (world_.validation.experienceValidationStateAge_ > 1.0f && world_.run.tankRunCapturePath_.empty())
            world_.expeditionMapController->SelectExpeditionService(world_.validation.experienceValidationVariant_ == 1 ? 0 : 3);
        return false;
    }
    if (node->kind == tankexp::NodeKind::Currency)
        return false;
    world_.presentation.debugPlayerNoDamage_ = true;
    if (tankexp::IsCombatNode(node->kind)) {
        if (!world_.validation.experienceMeleeDone_) {
            if (!world_.validation.experienceMeleeStarted_)
                BeginExperienceMeleeProbe();
            world_.validation.experienceMeleeAge_ += dt;
            const auto actors = world_.resources.enemyManager_->GetEnemyPtrs();
            if (actors.empty()) {
                world_.validation.experienceValidationErrors_.push_back("Melee target disappeared");
                return false;
            }
            auto* target = actors.front();
            world_.validation.experienceMeleeMinHp_ = (std::min)(world_.validation.experienceMeleeMinHp_, target->GetHp());
            world_.validation.experienceMeleeDisplacement_ =
                (std::max)(world_.validation.experienceMeleeDisplacement_,
                           cg2::Length(target->GetWorldPosition() - world_.validation.experienceMeleeTargetStart_));
            world_.validation.experienceMeleeSlashSamples_ += static_cast<int>(world_.presentation.playerMeleeSlashes_.size());
            world_.validation.experienceMeleeBulletSamples_ += static_cast<int>(world_.resources.bulletManager_->GetBulletCounts().player);
            world_.validation.experienceDroneSamples_ += static_cast<int>(world_.resources.player_->GetDronePtrs().size());
            world_.resources.player_->SetDemoInput(true, {}, target->GetWorldPosition(), world_.validation.experienceMeleeAge_ > 0.45f,
                                                   false);
            if (world_.validation.experienceValidationStyle_ != 2 && world_.validation.experienceMeleeAge_ > 1.0f)
                CaptureExperienceValidation("build_attack");
            for (const auto& slash : world_.presentation.playerMeleeSlashes_)
                if (slash.elapsed >= slash.windupDuration + slash.swingDuration * 0.35f &&
                    slash.elapsed < slash.windupDuration + slash.swingDuration)
                    CaptureExperienceValidation(slash.finisher ? "melee_finisher" : "melee");
            if (world_.validation.experienceMeleeAge_ > 3.0f) {
                world_.validation.experienceMeleeDone_ = true;
                world_.resources.player_->SetDemoInput(true, {}, target->GetWorldPosition(), false, false);
                WriteExperienceValidationReport(false);
            }
            return false;
        }
        if (world_.validation.experienceValidationStateAge_ > (tanksubmission::Enabled() ? 2.0f : 0.4f)) {
            bool cleared = false;
            for (auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
                if (actor && !actor->IsDead()) {
                    actor->TakeDamageFromPlayer(100000);
                    cleared = true;
                }
            if (node->kind == tankexp::NodeKind::Boss && !world_.resources.enemy_->IsDead()) {
                CaptureExperienceValidation("boss");
                if (!tanksubmission::Enabled() || world_.run.tankRunCapturePath_.empty()) {
                    world_.resources.enemy_->TakeDamage(100000);
                    cleared = true;
                }
            }
            if (cleared)
                ++world_.validation.experienceForcedLaterClears_;
        }
    } else {
        if (node->kind == tankexp::NodeKind::Upgrade)
            for (const auto& id : world_.run.expeditionServiceOffers_) {
                const auto* offer = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
                if (!offer || !tankcontent::EligibleUpgrade(*offer, world_.run.expeditionBuildStyle_, world_.run.tankRun_.GetCardCounts()))
                    world_.validation.experienceValidationErrors_.push_back("Incompatible upgrade leaked into runtime shop: " + id);
            }
        if (node->kind == tankexp::NodeKind::Upgrade && !world_.validation.experienceEvolutionVerified_ &&
            node->role == tankexp::NodeRole::None) {
            // Deterministic offer seed isolates purchase preservation from luck.
            // The real weighted generator, eligibility and card transaction run.
            if (world_.validation.experienceValidationStateAge_ < 0.7f)
                for (uint32_t fixtureSeed = 1; fixtureSeed < 512; ++fixtureSeed) {
                    auto offers =
                        tankcontent::BuildShopOffers(world_.run.expeditionContent_, world_.run.expeditionBuildStyle_,
                                                     world_.run.tankRun_.GetCardCounts(), world_.run.expeditionPurchases_, fixtureSeed);
                    const bool additive = std::any_of(offers.begin(), offers.end(), [&](const std::string& id) {
                        const auto* u = tankcontent::FindUpgrade(world_.run.expeditionContent_, id);
                        return u && std::any_of(u->effects.begin(), u->effects.end(), [](auto effect) {
                                   return effect >= tankrun::CardId::ExtraBarrel1;
                               });
                    });
                    if (additive) {
                        world_.run.expeditionServiceOffers_ = std::move(offers);
                        world_.expeditionBuildSelection->RefreshExpeditionBuildCards();
                        break;
                    }
                }
            for (size_t i = 0; i < world_.run.expeditionServiceOffers_.size(); ++i) {
                const auto* u = tankcontent::FindUpgrade(world_.run.expeditionContent_, world_.run.expeditionServiceOffers_[i]);
                if (!u || !std::any_of(u->effects.begin(), u->effects.end(), [](auto effect) {
                        return effect >= tankrun::CardId::ExtraBarrel1;
                    }))
                    continue;
                CaptureExperienceValidation("additive_upgrade");
                if (world_.validation.experienceValidationStateAge_ > 0.8f && world_.run.tankRunCapturePath_.empty() &&
                    world_.run.expeditionMapRun_.CanAfford(world_.expeditionMapController->ExpeditionServicePrice(u->id))) {
                    world_.expeditionMapController->SelectExpeditionService(static_cast<int>(i));
                    return false;
                }
            }
        }
        if (world_.validation.experienceValidationStateAge_ > 0.9f)
            world_.expeditionMapController->SelectExpeditionService(3);
    }
    return false;
}

void ExperienceValidation::RecordSubmissionUi(const std::string& screen)
{
    auto& check = tanksubmission::state;
    auto text = nlohmann::json::array();
    auto add = [&](const cg2::TextLabel* label) {
        if (label && !label->GetText().empty())
            text.push_back(label->GetText());
    };
    const bool mapScreen =
        world_.run.expeditionMapRun_.IsChoosing() || world_.expeditionBuildSelection->IsExpeditionBuildCardScreen() ||
        (world_.run.expeditionMapRun_.GetActiveNode() && !tankexp::IsCombatNode(world_.run.expeditionMapRun_.GetActiveNode()->kind));
    add(world_.run.tankRunHud_.get());
    add(world_.run.tankRunObjectiveText_.get());
    if (mapScreen) {
        add(world_.run.expeditionMapTitle_.get());
        add(world_.run.expeditionMapSubtitle_.get());
        add(world_.run.expeditionMapInfo_.get());
        add(world_.run.expeditionMapHelp_.get());
    }
    if (!world_.run.expeditionMapRun_.IsChoosing()) {
        add(world_.run.tankRunHeading_.get());
        add(world_.run.tankRunDescription_.get());
    }
    if (world_.run.tankExpeditionRivalActive_)
        add(world_.run.tankRunBossText_.get());
    if (world_.run.expeditionMapRun_.IsChoosing()) {
        add(world_.run.expeditionMapLegend_.get());
        for (const auto& n : world_.run.expeditionMapVisuals_) {
            add(n.label.get());
            add(n.state.get());
        }
    }
    const auto* active = world_.run.expeditionMapRun_.GetActiveNode();
    if (world_.run.expeditionGuideActive_) {
        add(world_.combat.tutorialInputText_.get());
        add(world_.combat.tutorialDescriptionText_.get());
    }
    if (world_.expeditionBuildSelection->IsExpeditionBuildCardScreen()) {
        if (!world_.run.expeditionBuildChoice_)
            add(world_.run.expeditionSkipText_.get());
        for (int i = 0; i < 3; ++i)
            if (world_.run.expeditionBuildChoice_ ||
                static_cast<size_t>(world_.run.expeditionServicePage_ * 3 + i) < world_.run.expeditionServiceOffers_.size()) {
                const auto& model = world_.run.expeditionRewardCards_[i]->GetModel();
                text.push_back(model.title);
                text.push_back(model.description);
                text.push_back(model.footer);
            }
    } else if (active && active->kind == tankexp::NodeKind::Heal) {
        add(world_.run.tankRunCardTitles_[0].get());
        add(world_.run.tankRunCardBodies_[0].get());
        add(world_.run.expeditionSkipText_.get());
    } else if (world_.combat.combatFlow_.GetState() == GameFlowState::StageClear) {
        add(world_.run.tankRunFooter_.get());
        for (int i = 0; i < 2; ++i) {
            add(world_.run.tankRunCardTitles_[i].get());
            add(world_.run.tankRunCardBodies_[i].get());
        }
    }
    if (world_.run.expeditionTransition_.IsActive()) {
        add(world_.run.expeditionTransitionTitle_.get());
        add(world_.run.expeditionTransitionDetail_.get());
    }
    for (const auto& entry : text) {
        const auto value = entry.get<std::string>();
        for (const char* forbidden : {"候補なし", "進化", "換装", "キット", "DEPLOY", "DOCK", "INSTALL", "REPAIR", "ROUTE",
                                      "システム更新中", "AUTOTEST", "SPECIAL VALIDATION", "F2 数値"})
            if (value.find(forbidden) != std::string::npos)
                check.errors.push_back(screen + ": visible obsolete/developer text: " + value);
    }
    check.screens[screen] = std::move(text);
    tanksubmission::Write();
}

bool ExperienceValidation::UpdateSubmissionValidation(float dt)
{
    auto& check = tanksubmission::state;
    if (check.runs == 2) {
        world_.validation.experienceValidationElapsed_ += dt;
        if (world_.validation.experienceValidationElapsed_ > 0.5f && !check.secondFresh) {
            check.secondFresh =
                world_.run.expeditionMapRun_.GetCurrency() == 20 && world_.run.expeditionMapRun_.GetVisitedNodeIds().empty() &&
                !world_.run.expeditionBuildChosen_ &&
                std::all_of(world_.run.tankRun_.GetCardCounts().begin(), world_.run.tankRun_.GetCardCounts().end(), [](int n) {
                    return n == 0;
                });
            if (!check.secondFresh || !tanksubmission::TutorialSaved() || !world_.run.expeditionTutorialPreviouslyCompleted_)
                check.errors.push_back("New expedition did not retain completion and reset run progress");
            CaptureExperienceValidation("new_expedition");
        }
        if (world_.validation.experienceValidationElapsed_ > 1.2f && world_.run.expeditionMapRun_.GetVisitedNodeIds().empty() &&
            world_.run.expeditionMapRun_.IsChoosing() && !world_.run.expeditionTransition_.IsActive() &&
            world_.run.tankRunCapturePath_.empty())
            world_.expeditionMapController->RequestExpeditionMapNode("tutorial_skip");
        if (world_.run.expeditionMapRun_.GetCurrentNodeId() == "tutorial_skip" && world_.run.expeditionMapRun_.IsChoosing() &&
            !world_.run.expeditionTransition_.IsActive()) {
            check.skipWorks = world_.run.expeditionMapRun_.GetCurrency() == 58 && !world_.run.expeditionGuideActive_;
            CaptureExperienceValidation("completed_user_skip");
            if (!world_.run.tankRunCapturePath_.empty())
                return true;
            if (!check.skipWorks)
                check.errors.push_back("Completed user skip or equivalent currency grant failed");
            tanksubmission::Write(check.errors.empty() && check.firstFresh && check.secondFresh && check.returned);
            PostQuitMessage(check.errors.empty() ? 0 : 9);
        }
        if (world_.validation.experienceValidationElapsed_ > 20) {
            check.errors.push_back("New expedition verification timeout");
            tanksubmission::Write();
            PostQuitMessage(9);
        }
        return true;
    }
    if (check.finished)
        return true;
    if (!check.firstFresh && world_.run.expeditionMapRun_.IsChoosing()) {
        check.firstFresh = world_.run.expeditionMapRun_.GetCurrency() == 20 && !tanksubmission::TutorialSaved() &&
                           world_.run.expeditionMapSelection_ == "tutorial_combat" &&
                           std::all_of(world_.run.tankRun_.GetCardCounts().begin(), world_.run.tankRun_.GetCardCounts().end(), [](int n) {
                               return n == 0;
                           });
        if (!check.firstFresh)
            check.errors.push_back("First profile did not start empty with tutorial selected");
    }
    if (world_.run.expeditionTransition_.IsActive())
        return false;
    if (check.repairStep == 4 && !check.repairDone && world_.run.expeditionMapRun_.IsChoosing()) {
        check.repairDone = world_.resources.player_->GetHp() == check.repairHp &&
                           world_.run.expeditionMapRun_.GetCurrency() == check.repairWallet - check.repairCost;
        if (!check.repairDone)
            check.errors.push_back("Repair did not restore exactly 50% max HP or charge its price");
        tanksubmission::Write();
    }
    const auto* node = world_.run.expeditionMapRun_.GetActiveNode();
    if (!node)
        return false;
    if (tankexp::IsCombatNode(node->kind)) {
        for (const auto* actor : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!actor || actor->IsDead())
                continue;
            const auto type = actor->GetType();
            const unsigned bit = type == ExpEnemyType::SummonerCommander ? 1u
                                 : type == ExpEnemyType::EMPJammer       ? 2u
                                 : type == ExpEnemyType::ReflectArmor    ? 4u
                                                                         : 0u;
            if (bit && world_.validation.experienceValidationStateAge_ > 0.8f) {
                check.newEnemies |= bit;
                CaptureExperienceValidation("enemy_" + std::to_string(bit));
            }
        }
    }
    if (node->kind != tankexp::NodeKind::Heal || check.repairDone)
        return false;
    check.repairAge += dt;
    if (check.repairStep == 0) {
        world_.resources.player_->HealRunPlayer(world_.resources.player_->GetMaxHp());
        check.repairWallet = world_.run.expeditionMapRun_.GetCurrency();
        check.repairCost = node->serviceCost;
        world_.expeditionMapController->SelectExpeditionService(0);
        check.fullBlocked =
            !world_.run.expeditionTransition_.IsActive() && world_.run.expeditionMapRun_.GetCurrency() == check.repairWallet;
        if (!check.fullBlocked)
            check.errors.push_back("Full HP repair was purchasable");
        check.repairStep = 1;
        check.repairAge = 0;
    } else if (check.repairStep == 1 && check.repairAge > 0.6f) {
        CaptureExperienceValidation("repair_full");
        if (!world_.run.tankRunCapturePath_.empty())
            return true;
        world_.resources.player_->SpendRunHealth((std::max)(1, world_.resources.player_->GetMaxHp() * 3 / 5));
        world_.run.expeditionMapRun_.TrySpendCurrency(world_.run.expeditionMapRun_.GetCurrency());
        world_.expeditionMapController->SelectExpeditionService(0);
        check.poorBlocked = !world_.run.expeditionTransition_.IsActive() && world_.run.expeditionMapRun_.GetCurrency() == 0;
        if (!check.poorBlocked)
            check.errors.push_back("Repair accepted insufficient Cr");
        check.repairStep = 2;
        check.repairAge = 0;
    } else if (check.repairStep == 2 && check.repairAge > 0.6f) {
        CaptureExperienceValidation("repair_insufficient");
        if (!world_.run.tankRunCapturePath_.empty())
            return true;
        world_.run.expeditionMapRun_.EarnCurrency(check.repairWallet);
        world_.run.expeditionMapStatus_.clear();
        check.repairStep = 3;
        check.repairAge = 0;
    } else if (check.repairStep == 3 && check.repairAge > 0.6f) {
        CaptureExperienceValidation("repair_available");
        if (!world_.run.tankRunCapturePath_.empty())
            return true;
        check.repairHp = (std::min)(world_.resources.player_->GetMaxHp(),
                                    world_.resources.player_->GetHp() + (std::max)(1, world_.resources.player_->GetMaxHp() / 2));
        world_.expeditionMapController->SelectExpeditionService(0);
        check.repairStep = 4;
    }
    world_.expeditionController->RefreshTankExpeditionUi();
    return check.repairStep < 4;
}

} // namespace gameplay
