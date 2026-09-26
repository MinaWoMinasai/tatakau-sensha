#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tankrun {
enum class Phase { Loadout, CoreChoice, Combat, Draft, Boss, Clear, Dead };
enum class CoreId { Ricochet, Assault, Drone, Count };
enum class CardId { Ricochet, Heavy, Rapid, Thrusters, Capacitor, Repair, Drones, Pierce,
    ScatterShot, Homing, DashBurst, Overdrive, MeleeBlade, BladeReach, ImpactDrive, PerfectDodge,
    DroneFocus, DroneGuard, MeleeTempo, FinisherCharge, RailCannon, DroneLaserLink, SlashWave, ParryBlade, Count };
constexpr std::size_t CardCount=static_cast<std::size_t>(CardId::Count);
// Preserve legacy numeric IDs, but never offer the retired multi-shot module.
constexpr bool IsAvailableCard(CardId card) { return card>=CardId::Ricochet&&card<CardId::Count&&card!=CardId::ScatterShot; }
constexpr std::size_t AvailableCardCount=CardCount-1;
// The old arena/prototype draft has no combat-style filter. Its original pool
// stays unchanged; expedition workshops grant the new effects through catalog
// eligibility and GrantExpeditionModules instead.
constexpr bool IsDraftCard(CardId card) {return IsAvailableCard(card)&&card<=CardId::FinisherCharge;}
constexpr std::size_t DraftCardCount=static_cast<std::size_t>(CardId::FinisherCharge);
using CardCounts=std::array<int,CardCount>;
using CardOffers=std::array<CardId,3>;
constexpr bool IsRare(CardId card) { return IsAvailableCard(card)&&card>=CardId::Homing&&card!=CardId::MeleeBlade; }
struct Config { float combatSeconds=150.0f; int maxDrafts=6; };

// Resources earn optional refits; waiting never awards a card.
// All menu states freeze the contest and preserve the current combat phase.
class RunDirector {
public:
    explicit RunDirector(std::uint32_t seed=1,Config config={}) : config_(config) {
        if(!std::isfinite(config_.combatSeconds)||config_.combatSeconds<=0) config_.combatSeconds=150;
        config_.maxDrafts=(std::clamp)(config_.maxDrafts,1,static_cast<int>(DraftCardCount));
        Reset(seed);
    }
    void Reset(std::uint32_t seed) {
        seed_=seed; randomState_=seed?seed:0x9e3779b9u;
        phase_=Phase::Loadout; resumePhase_=Phase::Combat; loadout_=-1; core_=CoreId::Count;
        elapsed_=bossElapsed_=0; salvage_=0; drafts_=0; rareCredits_=0;
        playerClaims_=rivalClaims_=0; cards_.fill(0); ClearOffers();
    }
    void Reset() { Reset(seed_); }
    bool ChooseLoadout(int index) {
        if(phase_!=Phase::Loadout||index<0||index>2) return false;
        loadout_=index; phase_=Phase::CoreChoice; return true;
    }
    bool ChooseCore(int index) {
        if(phase_!=Phase::CoreChoice||index<0||index>2) return false;
        core_=static_cast<CoreId>(index); phase_=Phase::Combat; return true;
    }
    bool Update(double dt) {
        if(!IsCombat()||!std::isfinite(dt)||dt<=0) return false;
        elapsed_+=dt;
        if(phase_==Phase::Boss) { bossElapsed_+=dt; return false; }
        if(elapsed_>=config_.combatSeconds) { phase_=Phase::Boss; return true; }
        return false;
    }
    bool AddSalvage(int amount) {
        if(!IsCombat()||amount<=0||drafts_>=config_.maxDrafts) return false;
        salvage_=(std::min)(salvage_+(std::min)(amount,100),100);
        return true;
    }
    bool ClaimResource(bool playerOwned) {
        if(!IsCombat()) return false;
        if(playerOwned) { ++playerClaims_; AddSalvage(12); rareCredits_=(std::min)(rareCredits_+1,6); }
        else ++rivalClaims_;
        return true;
    }
    bool CanOpenDraft() const { return IsCombat() && drafts_<config_.maxDrafts && salvage_>=GetRefitCost(); }
    bool TryOpenDraft() {
        if(!CanOpenDraft()) return false;
        resumePhase_=phase_; phase_=Phase::Draft;
        salvage_-=GetRefitCost();
        // A contested cache guarantees one unowned rare option.
        BuildOffers(rareCredits_>0,CardId::Count);
        if(rareCredits_>0) --rareCredits_;
        return true;
    }
    // Expedition room rewards use the same card pool without spending salvage
    // or consuming the arena's banked rare credits. The scene controls the rooms.
    bool OpenRewardDraft(bool rare,CardId preferred=CardId::Count) {
        if(!IsCombat()||drafts_>=config_.maxDrafts) return false;
        resumePhase_=Phase::Combat; phase_=Phase::Draft;
        BuildOffers(rare,preferred);
        return true;
    }
    // The first room always offers a noticeable foundation; special behavior
    // enters the pool after the player has learned movement and single shots.
    bool OpenExpeditionRewardDraft(int roomIndex,bool rare,CardId preferred=CardId::Count) {
        if(!OpenRewardDraft(rare,preferred)) return false;
        if(roomIndex==0 && drafts_==0) {
            offers_={CardId::Rapid,CardId::Heavy,CardId::Thrusters}; offerCount_=3;
        }
        return true;
    }
    bool ChooseCard(std::size_t index) {
        if(phase_!=Phase::Draft||index>=offerCount_) return false;
        const auto card=static_cast<std::size_t>(offers_[index]);
        if(card>=CardCount||cards_[card]) return false;
        ++cards_[card]; ++drafts_; phase_=resumePhase_; ClearOffers(); return true;
    }
    // A catalog entry can bundle several existing modules. Grant the complete
    // bundle atomically; already owned modules never stack or consume a slot.
    bool GrantExpeditionModules(const std::vector<CardId>& effects) {
        if(!IsCombat()||effects.empty()) return false;
        for(auto id:effects) if(!IsAvailableCard(id)) return false;
        bool changed=false;
        for(auto id:effects) {
            auto& count=cards_[static_cast<std::size_t>(id)];
            if(!count) {count=1;changed=true;}
        }
        if(changed) ++drafts_;
        return changed;
    }
    bool CompleteBoss() { if(!IsCombat()) return false; phase_=Phase::Clear; return true; }
    bool MarkDead() { if(!IsCombat()) return false; phase_=Phase::Dead; return true; }
    bool IsCombat() const { return phase_==Phase::Combat||phase_==Phase::Boss; }
    Phase GetPhase() const { return phase_; }
    CoreId GetCore() const { return core_; }
    int GetLoadout() const { return loadout_; }
    int GetSalvage() const { return salvage_; }
    int GetRefitCost() const { return (std::min)(12+drafts_*4,28); }
    int GetDraftCount() const { return drafts_; }
    int GetMaxDrafts() const { return config_.maxDrafts; }
    int GetPlayerClaims() const { return playerClaims_; }
    int GetRivalClaims() const { return rivalClaims_; }
    float GetCombatSeconds() const { return config_.combatSeconds; }
    double GetRunElapsedSeconds() const { return elapsed_; }
    double GetBossElapsedSeconds() const { return bossElapsed_; }
    double GetContestSecondsRemaining() const { return (std::max)(0.0,config_.combatSeconds-elapsed_); }
    const CardOffers& GetOffers() const { return offers_; }
    std::size_t GetOfferCount() const { return offerCount_; }
    const CardCounts& GetCardCounts() const { return cards_; }
    int GetCardCount(CardId card) const { const auto i=static_cast<std::size_t>(card); return i<CardCount?cards_[i]:0; }
    std::uint32_t GetSeed() const { return seed_; }
private:
    void BuildOffers(bool guaranteeRare,CardId preferred) {
        ClearOffers();
        const auto preferredIndex=static_cast<std::size_t>(preferred);
        if(IsDraftCard(preferred)&&!cards_[preferredIndex]) offers_[offerCount_++]=preferred;
        bool rarePresent=false;
        for(std::size_t i=0;i<offerCount_;++i) rarePresent|=IsRare(offers_[i]);
        if(guaranteeRare&&!rarePresent) {
            std::array<CardId,CardCount> rare{}; std::size_t count=0;
            for(std::size_t i=0;i<CardCount;++i) {
                const auto card=static_cast<CardId>(i);
                if(IsDraftCard(card)&&IsRare(card)&&!cards_[i]) rare[count++]=card;
            }
            if(count) offers_[offerCount_++]=rare[RandomIndex(count)];
        }
        while(offerCount_<offers_.size()) {
            std::array<CardId,CardCount> eligible{}; std::size_t count=0;
            for(std::size_t i=0;i<CardCount;++i) {
                const auto card=static_cast<CardId>(i); bool present=false;
                for(std::size_t j=0;j<offerCount_;++j) present|=offers_[j]==card;
                if(IsDraftCard(card)&&!cards_[i]&&!present) eligible[count++]=card;
            }
            if(!count) break;
            offers_[offerCount_++]=eligible[RandomIndex(count)];
        }
    }
    std::uint32_t NextRandom() { randomState_^=randomState_<<13; randomState_^=randomState_>>17; randomState_^=randomState_<<5; return randomState_; }
    std::size_t RandomIndex(std::size_t count) {
        const auto bound=static_cast<std::uint32_t>(count); const auto threshold=(0u-bound)%bound;
        std::uint32_t value; do { value=NextRandom(); } while(value<threshold); return value%bound;
    }
    void ClearOffers() { offers_.fill(CardId::Count); offerCount_=0; }
    Config config_;
    Phase phase_=Phase::Loadout,resumePhase_=Phase::Combat;
    CoreId core_=CoreId::Count;
    std::uint32_t seed_=1,randomState_=1;
    int loadout_=-1,salvage_=0,drafts_=0,rareCredits_=0,playerClaims_=0,rivalClaims_=0;
    double elapsed_=0,bossElapsed_=0;
    CardCounts cards_{}; CardOffers offers_{}; std::size_t offerCount_=0;
};
}
