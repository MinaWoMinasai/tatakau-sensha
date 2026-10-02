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
/// @brief この機能が有効か判定する。
inline bool Enabled()
{
    static const bool enabled = [] {
        wchar_t value[8]{};
        return GetEnvironmentVariableW(L"CG2_SUBMISSION_AUTOTEST", value, 8) > 0 && value[0] == L'1' &&
               std::filesystem::is_regular_file("generated/submission_validation/isolated-profile");
    }();
    return enabled;
}
/// @brief 提出版の実行検証で観測した進行状態と結果を保持する。
struct State {
    int titles = 0, runs = 0, repairStep = 0, repairWallet = 0, repairHp = 0, repairCost = 0;
    float repairAge = 0;
    bool firstFresh = false, secondFresh = false, tutorialSaved = false, repairDone = false, skipWorks = false;
    bool returned = false, finished = false, fullBlocked = false, poorBlocked = false;
    unsigned newEnemies = 0;
    std::vector<std::string> path, errors;
    nlohmann::json screens = nlohmann::json::object(), experience;
};
inline State state;
/// @brief 保存済みのチュートリアル履修状態を判定する。
inline bool TutorialSaved()
{
    std::ifstream in("resources/configs/expedition_user.json");
    const auto value = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json{};
    return value.is_object() && value.value("tutorialCompleted", false);
}
/// @brief 検証した状態を出力先へ書き込む。
inline void Write(bool completed = false)
{
    const nlohmann::json report = {{"completed", completed},
                                   {"testMode", true},
                                   {"isolatedProfile", true},
                                   {"titles", state.titles},
                                   {"runs", state.runs},
                                   {"firstFresh", state.firstFresh},
                                   {"secondFresh", state.secondFresh},
                                   {"tutorialSaved", state.tutorialSaved},
                                   {"repairFullBlocked", state.fullBlocked},
                                   {"repairPoorBlocked", state.poorBlocked},
                                   {"repairPurchased", state.repairDone},
                                   {"skipWorksAfterCompletion", state.skipWorks},
                                   {"returnedToTitle", state.returned},
                                   {"newEnemyMask", state.newEnemies},
                                   {"errors", state.errors},
                                   {"screens", state.screens},
                                   {"experience", state.experience},
                                   {"laterCombatForcedClear", true},
                                   {"standardNewEnemyRoomsSelectedForFixture", true},
                                   {"repairHpAndWalletBoundaryFixtures", true}};
    std::ofstream("generated/submission_validation/validation.json") << report.dump(2) << '\n';
}
} // namespace tanksubmission
