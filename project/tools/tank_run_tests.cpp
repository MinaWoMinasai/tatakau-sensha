#include "../game/run/TankRunDirector.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace tankrun;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << message << '\n';
        std::exit(1);
    }
}

void Start(RunDirector& run, int loadout = 0, int core = 0) {
    Check(run.ChooseLoadout(loadout), "choose loadout");
    Check(run.GetPhase() == Phase::CoreChoice && !run.IsCombat(), "loadout leads to core choice");
    Check(run.ChooseCore(core), "choose core");
    Check(run.GetPhase() == Phase::Combat && run.IsCombat(), "core starts contest");
}

void CheckNoOffers(const RunDirector& run) {
    Check(run.GetOfferCount() == 0, "no stale offer count");
    for (const auto card : run.GetOffers()) Check(card == CardId::Count, "no stale offer slots");
}

void CheckOffers(const RunDirector& run) {
    std::size_t unowned = 0;
    for (std::size_t i=0;i<CardCount;++i) {
        const int count=run.GetCardCounts()[i];
        Check(count == 0 || count == 1, "cards never stack beyond one");
        if (IsDraftCard(static_cast<CardId>(i))&&count == 0) ++unowned;
    }
    Check(run.GetPhase() == Phase::Draft, "offers belong to draft phase");
    Check(run.GetOfferCount() == (std::min)(unowned, run.GetOffers().size()), "fill every available offer slot");
    Check(run.GetOfferCount() > 0, "draft cannot be empty");
    for (std::size_t i = 0; i < run.GetOfferCount(); ++i) {
        const auto card = run.GetOffers()[i];
        Check(static_cast<std::size_t>(card) < CardCount, "real offered card");
        Check(IsDraftCard(card), "retired multi-shot is never offered");
        Check(run.GetCardCount(card) == 0, "owned card never offered");
        for (std::size_t j = 0; j < i; ++j) Check(run.GetOffers()[j] != card, "offered cards are distinct");
    }
    for (std::size_t i = run.GetOfferCount(); i < run.GetOffers().size(); ++i)
        Check(run.GetOffers()[i] == CardId::Count, "unused offer slots cleared");
}

void OpenFundedDraft(RunDirector& run) {
    Check(run.AddSalvage(run.GetRefitCost()), "fund next refit");
    Check(run.CanOpenDraft() && run.TryOpenDraft(), "funded refit opens");
    CheckOffers(run);
}

std::size_t PreferCommon(const RunDirector& run) {
    for (std::size_t i = 0; i < run.GetOfferCount(); ++i) if (!IsRare(run.GetOffers()[i])) return i;
    return 0;
}

bool HasUnownedRare(const RunDirector& run) {
    for (std::size_t i = 0; i < CardCount; ++i) {
        const auto card = static_cast<CardId>(i);
        if (IsDraftCard(card) && IsRare(card) && run.GetCardCount(card) == 0) return true;
    }
    return false;
}

std::size_t FindRareOffer(const RunDirector& run) {
    for (std::size_t i = 0; i < run.GetOfferCount(); ++i) if (IsRare(run.GetOffers()[i])) return i;
    return run.GetOfferCount();
}

void TestSelectionAndInvalidInputs() {
    RunDirector run(17);
    Check(run.GetPhase() == Phase::Loadout && run.GetLoadout() == -1, "initial loadout phase");
    Check(run.GetCore() == CoreId::Count && run.GetSeed() == 17, "initial core and seed");
    Check(run.GetCombatSeconds() == 150 && run.GetMaxDrafts() == 6, "default contest and draft limits");
    Check(run.GetSalvage() == 0 && run.GetRefitCost() == 12, "initial resource balance and cost");
    CheckNoOffers(run);
    Check(!run.ChooseLoadout(-1) && !run.ChooseLoadout(3), "invalid loadouts rejected");
    Check(!run.ChooseLoadout((std::numeric_limits<int>::max)()), "large loadout index rejected");
    Check(!run.ChooseCore(0) && !run.ChooseCard(0), "wrong phase selections rejected");
    Check(!run.AddSalvage(100) && !run.ClaimResource(true) && !run.ClaimResource(false), "no resources before contest");
    Check(!run.TryOpenDraft() && !run.MarkDead() && !run.CompleteBoss(), "no combat actions before contest");
    Check(!run.Update(1000) && run.GetRunElapsedSeconds() == 0, "loadout pauses time");
    Check(run.ChooseLoadout(1), "valid loadout selected");
    Check(!run.ChooseLoadout(0) && !run.ChooseCore(-1) && !run.ChooseCore(3), "core menu rejects wrong input");
    Check(!run.Update(1000) && !run.AddSalvage(12) && !run.ClaimResource(true), "core menu pauses contest");
    Check(!run.MarkDead() && !run.CompleteBoss() && !run.TryOpenDraft(), "core menu rejects combat actions");
    Check(run.GetRunElapsedSeconds() == 0 && run.GetPlayerClaims() == 0, "core menu leaves counters untouched");
    Check(run.ChooseCore(2) && run.GetCore() == CoreId::Drone, "core choice retained");
    Check(!run.ChooseCore(1) && !run.ChooseCard(0), "combat rejects menu choices");
    Check(!run.AddSalvage(0) && !run.AddSalvage(-1) && !run.AddSalvage((std::numeric_limits<int>::min)()), "nonpositive salvage rejected");
    Check(!run.CanOpenDraft() && !run.TryOpenDraft(), "unfunded refit rejected");
    Check(run.GetCardCount(CardId::Count) == 0 && run.GetCardCount(static_cast<CardId>(-1)) == 0, "invalid card lookup is safe");
    Check(!IsRare(CardId::Pierce) && !IsRare(CardId::ScatterShot) && IsRare(CardId::Overdrive), "rare category boundaries");
    Check(!IsDraftCard(CardId::ScatterShot)&&IsDraftCard(CardId::MeleeBlade)&&IsRare(CardId::PerfectDodge), "new modules replace retired spread");
    const auto before=run.GetCardCounts();
    Check(!run.GrantExpeditionModules({CardId::Rapid,CardId::ScatterShot})&&run.GetCardCounts()==before, "retired effects reject whole bundle atomically");
    Check(!IsRare(CardId::Count) && !IsRare(static_cast<CardId>(-1)), "invalid cards are not rare");
}

void TestResourceEconomyAndDraftValidation() {
    RunDirector run(21);
    Start(run);
    Check(run.ClaimResource(false), "rival can claim resource");
    Check(run.GetRivalClaims() == 1 && run.GetPlayerClaims() == 0 && run.GetSalvage() == 0, "rival claim does not pay player");
    Check(run.AddSalvage(11) && !run.TryOpenDraft(), "insufficient refit balance rejected");
    Check(run.GetSalvage() == 11 && run.GetDraftCount() == 0, "failed refit never charges");
    Check(run.AddSalvage(1) && run.CanOpenDraft(), "exact price enables refit");
    Check(run.GetPhase() == Phase::Combat, "earning resources does not force a menu");
    Check(run.TryOpenDraft() && run.GetSalvage() == 0, "opening charges exact price once");
    CheckOffers(run);
    const auto offers = run.GetOffers();
    const auto cards = run.GetCardCounts();
    Check(!run.TryOpenDraft() && !run.CanOpenDraft(), "draft cannot reopen itself");
    Check(!run.ChooseCard(run.GetOfferCount()) && !run.ChooseCard((std::numeric_limits<std::size_t>::max)()), "out of range card input rejected");
    Check(!run.AddSalvage(100) && !run.ClaimResource(true) && !run.ClaimResource(false), "draft cannot collect resources");
    Check(!run.CompleteBoss() && !run.MarkDead(), "draft cannot resolve combat");
    Check(run.GetOffers() == offers && run.GetCardCounts() == cards && run.GetDraftCount() == 0, "invalid draft actions preserve selection");
    Check(run.GetSalvage() == 0 && run.GetRivalClaims() == 1 && run.GetPlayerClaims() == 0, "invalid draft actions preserve economy");
    Check(run.ChooseCard(1) && run.GetCardCount(offers[1]) == 1, "chosen card owned once");
    Check(run.GetDraftCount() == 1 && run.GetRefitCost() == 16 && run.GetPhase() == Phase::Combat, "refit advances price and resumes contest");
    CheckNoOffers(run);
    Check(!run.ChooseCard(1) && run.GetDraftCount() == 1, "repeated input cannot grant another card");
    Check(run.ClaimResource(true), "player can claim resource");
    Check(run.GetPlayerClaims() == 1 && run.GetSalvage() == 12 && !run.CanOpenDraft(), "cache pays twelve and does not bypass next price");
    Check(run.AddSalvage(4) && run.TryOpenDraft(), "cache plus salvage funds refit");
    CheckOffers(run);
    Check(FindRareOffer(run) < run.GetOfferCount(), "player cache guarantees unowned rare offer");
    Check(run.GetSalvage() == 0 && run.GetDraftCount() == 1, "second refit price paid before card selection");
    Check(run.ChooseCard(FindRareOffer(run)), "choose guaranteed rare");
    Check(run.AddSalvage((std::numeric_limits<int>::max)()) && run.GetSalvage() == 100, "large salvage input saturates without overflow");
    Check(run.AddSalvage(1) && run.GetSalvage() == 100, "resource bank remains capped");
    Check(run.ClaimResource(true) && run.GetSalvage() == 100 && run.GetPlayerClaims() == 2, "claim still recorded at full bank");
}

void TestCombatTimerAndPausedRefits() {
    Config config;
    config.combatSeconds = 30;
    RunDirector run(4, config);
    Start(run);
    const double invalidTimes[] = {0, -1, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (const double dt : invalidTimes) Check(!run.Update(dt), "invalid time never transitions phase");
    Check(run.GetRunElapsedSeconds() == 0 && run.GetContestSecondsRemaining() == 30, "invalid time leaves timers untouched");
    Check(!run.Update(29.5) && run.GetContestSecondsRemaining() == 0.5, "contest lasts configured time");
    OpenFundedDraft(run);
    const auto offers = run.GetOffers();
    Check(!run.Update(1000), "refit menu does not advance phase");
    Check(run.GetRunElapsedSeconds() == 29.5 && run.GetContestSecondsRemaining() == 0.5 && run.GetOffers() == offers, "contest refit freezes time and offers");
    Check(run.ChooseCard(0) && run.GetPhase() == Phase::Combat, "contest refit resumes contest");
    Check(run.Update(0.5) && run.GetPhase() == Phase::Boss, "deadline starts boss confrontation");
    Check(run.GetRunElapsedSeconds() == 30 && run.GetBossElapsedSeconds() == 0 && run.GetContestSecondsRemaining() == 0, "deadline timers are consistent");
    CheckNoOffers(run);
    Check(!run.Update(2) && run.GetBossElapsedSeconds() == 2, "boss phase continues without another deadline transition");
    OpenFundedDraft(run);
    Check(!run.Update(1000) && run.GetRunElapsedSeconds() == 32 && run.GetBossElapsedSeconds() == 2, "boss refit freezes both clocks");
    Check(run.ChooseCard(0) && run.GetPhase() == Phase::Boss, "boss refit resumes boss phase");
    Check(!run.Update(3) && run.GetRunElapsedSeconds() == 35 && run.GetBossElapsedSeconds() == 5, "boss time continues after refit");
    for (const double dt : invalidTimes) Check(!run.Update(dt), "boss rejects invalid time");
    Check(run.GetRunElapsedSeconds() == 35 && run.GetBossElapsedSeconds() == 5, "invalid boss time leaves counters untouched");
    RunDirector idle(4, config);
    Start(idle);
    Check(idle.Update(100000) && idle.GetPhase() == Phase::Boss, "large frame crosses deadline once");
    Check(!idle.Update(100000) && idle.GetPhase() == Phase::Boss, "large later frame cannot repeat transition");
    Check(idle.GetDraftCount() == 0 && idle.GetSalvage() == 0 && !idle.CanOpenDraft(), "waiting never awards resources or cards");
    CheckNoOffers(idle);
}

void TestRareCreditBankAndExhaustion() {
    Config config;
    config.maxDrafts = static_cast<int>(DraftCardCount);
    bool observedCreditCap = false;
    for (std::uint32_t seed = 0; seed < 64; ++seed) {
        RunDirector ordinary(seed, config), rivalClaim(seed, config);
        Start(ordinary); Start(rivalClaim);
        Check(rivalClaim.ClaimResource(false), "rival cache is claimed");
        OpenFundedDraft(ordinary); OpenFundedDraft(rivalClaim);
        Check(ordinary.GetOffers() == rivalClaim.GetOffers(), "rival cache grants no player rare credit");
        RunDirector banked(seed, config), justInTime(seed, config);
        Start(banked); Start(justInTime);
        for (int i = 0; i < 3; ++i) Check(banked.ClaimResource(true), "bank rare credits");
        for (int round = 0; round < config.maxDrafts; ++round) {
            if (round < 3) Check(justInTime.ClaimResource(true), "earn one credit before refit");
            const bool rareAvailable = HasUnownedRare(banked);
            OpenFundedDraft(banked); OpenFundedDraft(justInTime);
            Check(banked.GetOffers() == justInTime.GetOffers(), "banked credits persist and exactly one is spent per refit");
            if (round < 3 && rareAvailable) Check(FindRareOffer(banked) < banked.GetOfferCount(), "each banked credit guarantees rare while available");
            const auto choice = PreferCommon(banked);
            Check(banked.ChooseCard(choice) && justInTime.ChooseCard(choice), "choose same card with banked and fresh credit");
        }
        RunDirector capped(seed, config), excess(seed, config), refill(seed, config);
        Start(capped); Start(excess); Start(refill);
        for (int i = 0; i < 20; ++i) {
            if (i < 6) Check(capped.ClaimResource(true) && refill.ClaimResource(true), "fill credit bank");
            Check(excess.ClaimResource(true), "extra claims remain valid");
        }
        for (int round = 0; round < config.maxDrafts; ++round) {
            const bool rareAvailable = HasUnownedRare(capped);
            if (round == 6) Check(refill.ClaimResource(true), "earn new credit after bank is spent");
            OpenFundedDraft(capped); OpenFundedDraft(excess);
            if (round <= 6) OpenFundedDraft(refill);
            Check(capped.GetOffers() == excess.GetOffers(), "credit bank is capped at six despite extra claims");
            if (round < 6 && rareAvailable) Check(FindRareOffer(capped) < capped.GetOfferCount(), "six banked credits retain their guarantees");
            if (round < 6) Check(capped.GetOffers() == refill.GetOffers(), "refill control starts with same credit state");
            if (round == 6 && rareAvailable) {
                Check(FindRareOffer(refill) < refill.GetOfferCount(), "new claim restores rare guarantee");
                if (capped.GetOffers() != refill.GetOffers()) observedCreditCap = true;
            }
            const auto choice = PreferCommon(capped);
            if (round < 6) Check(refill.ChooseCard(choice), "keep refill control in sync");
            Check(capped.ChooseCard(choice) && excess.ChooseCard(choice), "continue capped credit run");
        }
    }
    Check(observedCreditCap, "credit-cap test includes a case where seventh credit changes the offer");
    RunDirector exhausted(14, config);
    Start(exhausted);
    while (HasUnownedRare(exhausted)) {
        Check(exhausted.ClaimResource(true), "earn rare acquisition credit");
        OpenFundedDraft(exhausted);
        Check(FindRareOffer(exhausted) < exhausted.GetOfferCount(), "unowned rare is guaranteed");
        Check(exhausted.ChooseCard(FindRareOffer(exhausted)), "acquire guaranteed rare");
    }
    Check(!HasUnownedRare(exhausted), "all available rare cards acquired");
    Check(exhausted.ClaimResource(true), "resource claim remains valid after rare exhaustion");
    OpenFundedDraft(exhausted);
    for (std::size_t i = 0; i < exhausted.GetOfferCount(); ++i)
        Check(!IsRare(exhausted.GetOffers()[i]), "exhausted rare guarantee falls back to unowned common cards");
}

void TestSeedReproducibilityAndCaps() {
    Config config;
    config.maxDrafts = static_cast<int>(DraftCardCount);
    CardOffers firstSeedOffers{};
    bool differentSeedsDiffer = false;
    for (std::uint32_t seed = 0; seed < 128; ++seed) {
        RunDirector a(seed, config), b(seed, config);
        Start(a, 0, 0);
        Start(b, 2, 2); // Chassis, core, and frame timing do not perturb the draft RNG.
        for (int round = 0; round < config.maxDrafts; ++round) {
            if (round % 3 == 0) Check(a.ClaimResource(true) && b.ClaimResource(true), "same cache events");
            b.Update(0.125);
            Check(a.GetRefitCost() == (std::min)(12 + round * 4, 28), "refit price curve and cap");
            OpenFundedDraft(a); OpenFundedDraft(b);
            Check(a.GetOffers() == b.GetOffers(), "same seed and card choices reproduce offers");
            if (round == 0) {
                if (seed == 0) firstSeedOffers = a.GetOffers();
                else differentSeedsDiffer |= a.GetOffers() != firstSeedOffers;
            }
            const auto choice = (static_cast<std::size_t>(seed) + static_cast<std::size_t>(round)) % a.GetOfferCount();
            const auto chosen = a.GetOffers()[choice];
            Check(a.ChooseCard(choice) && b.ChooseCard(choice), "reproducible card choice");
            Check(a.GetCardCount(chosen) == 1 && a.GetCardCounts() == b.GetCardCounts(), "chosen card stays capped and reproducible");
            CheckNoOffers(a);
        }
        Check(a.GetDraftCount() == static_cast<int>(DraftCardCount), "config permits acquiring every available card");
        for (std::size_t i=0;i<CardCount;++i) Check(a.GetCardCounts()[i] == (IsDraftCard(static_cast<CardId>(i))?1:0), "available pool exhausts while retired slot stays empty");
        const int salvage = a.GetSalvage();
        Check(!a.AddSalvage(100) && !a.CanOpenDraft() && !a.TryOpenDraft(), "completed draft cap blocks further refits");
        Check(a.ClaimResource(true) && a.ClaimResource(false), "contest claims still count after all refits");
        Check(a.GetSalvage() == salvage, "claims cannot add unusable salvage after draft cap");
    }
    Check(differentSeedsDiffer, "different seeds can produce different offers");
    RunDirector defaults(11);
    Start(defaults);
    for (int i = 0; i < 6; ++i) { OpenFundedDraft(defaults); Check(defaults.ChooseCard(0), "choose default cap card"); }
    Check(defaults.GetDraftCount() == 6 && !defaults.AddSalvage(100) && !defaults.TryOpenDraft(), "default six-refit cap works before card pool exhaustion");
}

void TestTerminalStatesAndReset() {
    for (const bool bossPhase : {false, true}) {
        for (const bool victory : {false, true}) {
            Config config;
            config.combatSeconds = 10;
            RunDirector run(73, config);
            Start(run, 1, 1);
            Check(run.ClaimResource(true), "claim before outcome");
            OpenFundedDraft(run);
            Check(run.ChooseCard(0), "own card before outcome");
            Check(run.ClaimResource(true), "retain an unspent rare credit for reset test");
            run.Update(bossPhase ? 12 : 3);
            Check(victory ? run.CompleteBoss() : run.MarkDead(), "both combat phases can clear or die");
            Check(run.GetPhase() == (victory ? Phase::Clear : Phase::Dead) && !run.IsCombat(), "outcome enters correct terminal state");
            const auto cards = run.GetCardCounts();
            const auto elapsed = run.GetRunElapsedSeconds();
            const auto bossElapsed = run.GetBossElapsedSeconds();
            const int salvage = run.GetSalvage();
            const int claims = run.GetPlayerClaims();
            Check(!run.Update(1000) && !run.AddSalvage(100) && !run.ClaimResource(true) && !run.ClaimResource(false), "terminal state rejects time and economy changes");
            Check(!run.TryOpenDraft() && !run.CanOpenDraft() && !run.ChooseCard(0), "terminal state rejects drafts");
            Check(!run.ChooseLoadout(0) && !run.ChooseCore(0) && !run.CompleteBoss() && !run.MarkDead(), "terminal state cannot change outcome");
            Check(run.GetCardCounts() == cards && run.GetRunElapsedSeconds() == elapsed && run.GetBossElapsedSeconds() == bossElapsed, "terminal cards and timers stay frozen");
            Check(run.GetSalvage() == salvage && run.GetPlayerClaims() == claims && run.GetRivalClaims() == 0 && run.GetDraftCount() == 1, "terminal counters stay frozen");
            run.Reset();
            Check(run.GetPhase() == Phase::Loadout && run.GetLoadout() == -1 && run.GetCore() == CoreId::Count, "reset clears setup choices");
            Check(run.GetRunElapsedSeconds() == 0 && run.GetBossElapsedSeconds() == 0 && run.GetContestSecondsRemaining() == 10, "reset restores timers and preserves config");
            Check(run.GetSalvage() == 0 && run.GetDraftCount() == 0 && run.GetRefitCost() == 12, "reset clears refits and resources");
            Check(run.GetPlayerClaims() == 0 && run.GetRivalClaims() == 0, "reset clears resource contest");
            for (const int count : run.GetCardCounts()) Check(count == 0, "reset clears cards");
            CheckNoOffers(run);
            RunDirector fresh(73, config);
            Start(run); Start(fresh);
            OpenFundedDraft(run); OpenFundedDraft(fresh);
            Check(run.GetOffers() == fresh.GetOffers(), "retry restores RNG and removes previous rare credit");
            run.Reset(999); // Reset also works with an unchosen draft outstanding.
            Check(run.GetSeed() == 999 && run.GetPhase() == Phase::Loadout, "explicit seed reset works from draft");
            CheckNoOffers(run);
            RunDirector reseeded(999, config);
            Start(run); Start(reseeded);
            Check(run.ClaimResource(true) && reseeded.ClaimResource(true), "same event after explicit reset");
            OpenFundedDraft(run); OpenFundedDraft(reseeded);
            Check(run.GetOffers() == reseeded.GetOffers(), "explicit seed reset reproduces fresh run");
        }
    }
}

void TestInvalidConfig() {
    const float invalidSeconds[] = {0, -1, std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    for (const float seconds : invalidSeconds) {
        Config config;
        config.combatSeconds = seconds;
        config.maxDrafts = (std::numeric_limits<int>::min)();
        RunDirector run(0, config);
        Check(run.GetCombatSeconds() == 150 && run.GetMaxDrafts() == 1, "invalid config uses safe duration and minimum one refit");
        Start(run);
        OpenFundedDraft(run);
        Check(run.ChooseCard(0) && !run.TryOpenDraft() && !run.AddSalvage(100), "sanitized minimum cap is enforced");
        run.Reset();
        Check(run.GetCombatSeconds() == 150 && run.GetMaxDrafts() == 1, "reset retains sanitized config");
    }
    Config config;
    config.maxDrafts = (std::numeric_limits<int>::max)();
    config.combatSeconds = 0.5f;
    RunDirector upper(1, config);
    Check(upper.GetMaxDrafts() == static_cast<int>(DraftCardCount) && upper.GetCombatSeconds() == 0.5f, "large draft cap clamps to available pool and valid fractional duration is preserved");
    Start(upper);
    Check(!upper.Update(0.25) && upper.Update(0.25), "fractional duration reaches boss exactly");
}
} // namespace

int main() {
    TestSelectionAndInvalidInputs();
    TestResourceEconomyAndDraftValidation();
    TestCombatTimerAndPausedRefits();
    TestRareCreditBankAndExhaustion();
    TestSeedReproducibilityAndCaps();
    TestTerminalStatesAndReset();
    TestInvalidConfig();
    std::cout << "Tank run PASS: resource economy, rare credit bank/cap/exhaustion, phases and paused refits, seeded unique offers, card/refit caps, terminal outcomes, reset and invalid inputs\n";
}
