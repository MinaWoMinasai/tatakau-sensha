#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Test-only shared progress across title -> expedition -> title -> new expedition.
// The runner must create an isolated-profile marker in its staging directory.
namespace tanksubmission {
inline bool Enabled() {
    static const bool enabled=[] {
        wchar_t value[8]{};
        return GetEnvironmentVariableW(L"CG2_SUBMISSION_AUTOTEST",value,8)>0&&value[0]==L'1'&&
            std::filesystem::is_regular_file("generated/submission_validation/isolated-profile");
    }();
    return enabled;
}
struct State {
    int titles=0,runs=0,repairStep=0,repairWallet=0,repairHp=0,repairCost=0;
    float repairAge=0;
    bool firstFresh=false,secondFresh=false,tutorialSaved=false,repairDone=false,skipWorks=false;
    bool returned=false,finished=false,fullBlocked=false,poorBlocked=false;
    unsigned newEnemies=0;
    std::vector<std::string> path,errors;
    nlohmann::json screens=nlohmann::json::object(),experience;
};
inline State state;
inline bool TutorialSaved() {
    std::ifstream in("resources/configs/expedition_user.json");
    const auto value=in?nlohmann::json::parse(in,nullptr,false):nlohmann::json{};
    return value.is_object()&&value.value("tutorialCompleted",false);
}
inline void Write(bool completed=false) {
    const nlohmann::json report={{"completed",completed},{"testMode",true},{"isolatedProfile",true},
        {"titles",state.titles},{"runs",state.runs},{"firstFresh",state.firstFresh},
        {"secondFresh",state.secondFresh},{"tutorialSaved",state.tutorialSaved},
        {"repairFullBlocked",state.fullBlocked},{"repairPoorBlocked",state.poorBlocked},
        {"repairPurchased",state.repairDone},{"skipWorksAfterCompletion",state.skipWorks},
        {"returnedToTitle",state.returned},{"newEnemyMask",state.newEnemies},
        {"errors",state.errors},{"screens",state.screens},{"experience",state.experience},
        {"laterCombatForcedClear",true},{"standardNewEnemyRoomsSelectedForFixture",true},
        {"repairHpAndWalletBoundaryFixtures",true}};
    std::ofstream("generated/submission_validation/validation.json")<<report.dump(2)<<'\n';
}
}
