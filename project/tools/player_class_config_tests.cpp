// CPU characterization tests built by test_player_class_config.ps1.
// The real Catalog header/source is compiled normally. Player runtime wrappers
// and Editor delete/reload/apply blocks come verbatim from current Player.cpp.
// Only the GPU barrel hooks, EvolveById and ImGui interaction use adapters.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include "game/player/PlayerClassCatalog.h"

namespace ImGui {
std::string clickedButton;
int endCount = 0;
bool Button(const char* label) { return clickedButton == label; }
void End() { ++endCount; }
}

class Player {
public:
    using BodyShape = PlayerBodyShape;
    using PlayerClassConfig = ::PlayerClassConfig;
    bool LoadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");
    bool ReloadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");
    void SavePlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json") const;
    PlayerClassConfig CreateDefaultClassConfig(ClassType type) const;
    const PlayerClassConfig* GetClassConfig(ClassType type) const;
    const PlayerClassConfig* GetClassConfig(const std::string& classId) const;
    const PlayerClassConfig* GetCurrentClassConfig() const;
    PlayerClassConfig* GetMutableClassConfig(const std::string& classId);

    void InitializeBarrels() { ++barrelInitializations; }
    void UpdateBarrelLayout() { ++barrelLayouts; }
    void EvolveById(const std::string& id) { ++evolveCalls; currentClassId_ = id; }
    void ExerciseEditorToolbar();

    PlayerClassCatalog classCatalog_;
    struct { bool enabled = false; } runModifiers_;
    bool runEvolutionActive_ = false;
    bool expeditionCombatStyleSelected_ = false;
    bool runCheckpointEvolution_ = false;
    PlayerClassConfig runEvolutionConfig_{};
    PlayerClassConfig runStarterConfig_{};
    std::string currentClassId_ = "Basic";
    ClassType currentClass_ = ClassType::Basic;
    int shootBarrelIndex_ = 0;
    int shootGroupIndex_ = 0;
    std::vector<float> weaponGroupCooldowns_;
    int editorSelectedClassIndex_ = 0;
    int barrelInitializations = 0;
    int barrelLayouts = 0;
    int evolveCalls = 0;
    std::unordered_map<std::string, PlayerClassConfig> editorBaselineConfigs;
    std::unordered_map<std::string, std::string> editorBaselineLabels;
    std::string editorObservedId;
    std::string editorObservedName;
    bool editorUnsaved = true;
};

#include "player_class_config_methods.inc"

void Player::ExerciseEditorToolbar()
{
    std::string selectedId = classCatalog_.OrderedIds().at(static_cast<size_t>(editorSelectedClassIndex_));
    PlayerClassConfig* config = GetMutableClassConfig(selectedId);
    if (!config) throw std::runtime_error("Editor fixture has no selected config");
    bool rebuildBarrels = false;
    bool relayoutBarrels = false;
    bool hasUnsavedEditorChanges = editorUnsaved;
    bool editorBaselineInitialized = false;
#include "player_class_config_baselines.inc"
    refreshEditorBaselines();
    if (!editorBaselineInitialized) throw std::runtime_error("Baseline refresh did not run");
#include "player_class_config_unique_id.inc"
#include "player_class_config_create.inc"
#include "player_class_config_clone.inc"
    const bool isLegacyId = config->id == ClassTypeToString(config->type);
#include "player_class_config_delete.inc"
#include "player_class_config_reload.inc"
    // The real Editor continues reading/editing this pointer in the same frame.
    // ASan must detect the original reload use-after-free here.
    editorObservedId = config->id;
    editorObservedName = config->displayName;
    config->requiredRank = 4;
#include "player_class_config_apply.inc"
    editorUnsaved = hasUnsavedEditorChanges;
}

namespace {
using Json = nlohmann::json;
constexpr const char* configPath = "resources/configs/playerClasses.json";

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void Near(float actual, float expected, const char* message)
{
    Check(std::abs(actual - expected) < 0.00001f, message);
}
Json Classes(std::initializer_list<Json> items)
{
    Json classes = Json::array();
    for (const auto& item : items) classes.push_back(item);
    return Json{{"version", 2}, {"classes", classes}};
}
void WriteRaw(const std::string& text)
{
    std::ofstream file(configPath);
    file << text;
    Check(file.good(), "Could not write isolated fixture");
}
void Write(const Json& root) { WriteRaw(root.dump()); }
Json Read(const std::string& path)
{
    std::ifstream file(path);
    Json root;
    file >> root;
    return root;
}
Json Snapshot(const Player& player)
{
    player.SavePlayerClassConfigs("snapshot.json");
    return Read("snapshot.json");
}
Json Snapshot(const PlayerClassCatalog& catalog)
{
    catalog.Save("snapshot.json");
    return Read("snapshot.json");
}
void CheckAuthoredFields(const Json& saved, const Json& authored)
{
    if (authored.is_object()) {
        for (auto field = authored.begin(); field != authored.end(); ++field) {
            Check(saved.contains(field.key()), "Save dropped an authored field");
            CheckAuthoredFields(saved.at(field.key()), field.value());
        }
    } else if (authored.is_array()) {
        Check(saved.is_array() && saved.size() == authored.size(), "Save changed an authored array");
        for (size_t i = 0; i < authored.size(); ++i) CheckAuthoredFields(saved[i], authored[i]);
    } else if (authored.is_number()) {
        Check(saved.is_number() && std::abs(saved.get<double>() - authored.get<double>()) < 0.00001, "Save changed an authored number");
    } else {
        Check(saved == authored, "Save changed an authored value");
    }
}
Player Seed()
{
    Write(Classes({Json{{"id", "Basic"}, {"displayName", "old basic"}},
                   Json{{"id", "Twin"}, {"displayName", "old twin"}},
                   Json{{"id", "CustomTank"}, {"displayName", "old custom"}}}));
    Player player;
    Check(player.LoadPlayerClassConfigs(), "Seed load failed");
    return player;
}

void OrderedLookupAndDefaults()
{
    Write(Classes({Json{{"id", "Twin"}}, Json{{"id", "CustomTank"}}, Json{{"id", "Basic"}}}));
    Player player;
    Check(player.LoadPlayerClassConfigs(), "Valid load failed");
    Check(player.classCatalog_.OrderedIds() == std::vector<std::string>{"Twin", "CustomTank", "Basic"}, "JSON order changed");
    const auto* twin = player.GetClassConfig("Twin");
    Check(twin && twin == player.GetClassConfig(ClassType::Twin), "ID/enum lookup mismatch");
    Check(twin == player.GetMutableClassConfig("Twin"), "Mutable lookup mismatch");
    Check(twin->type == ClassType::Twin && twin->requiredRank == 2 && twin->alternateBarrels, "Twin defaults changed");
    Check(twin->barrels.size() == 2, "Twin default mount count changed");
    Near(twin->reloadScale, 0.4f, "Twin default reload changed");
    const auto* custom = player.GetClassConfig("CustomTank");
    Check(custom && custom->id == "CustomTank" && custom->type == ClassType::Basic, "Custom ID fallback type changed");
    Check(custom->displayName == "Basic" && custom->barrels.size() == 1, "Optional defaults changed");
    Check(!player.GetClassConfig("missing") && !player.GetMutableClassConfig("missing"), "Unknown ID must return nullptr");
    Check(!player.GetClassConfig(ClassType::MachineGun), "Omitted authored classes must stay absent");
    Check(player.barrelInitializations == 0 && player.barrelLayouts == 0, "Load must not apply gameplay equipment");
}
void BasicFallbackAndDuplicates()
{
    Write(Classes({Json{{"id", "Twin"}, {"displayName", "first"}}, Json{{"id", "CustomTank"}},
                   Json{{"id", "Twin"}, {"displayName", "last"}}}));
    Player player;
    Check(player.LoadPlayerClassConfigs(), "Missing Basic should be complemented");
    Check(player.classCatalog_.OrderedIds() == std::vector<std::string>{"Basic", "Twin", "CustomTank"}, "First occurrence order/Basic prefix changed");
    Check(player.GetClassConfig("Twin")->displayName == "last", "Duplicate ID last value changed");
    Check(player.GetClassConfig("Basic")->displayName == "Basic", "Basic fallback defaults changed");
}
void MountCompatibility()
{
    Write(Classes({Json{{"id", "Twin"}, {"barrels", Json::array({Json{{"weaponType", "Mine"}}})}},
        Json{{"id", "CustomTank"}, {"bulletCount", 12}, {"weaponMounts", Json::array({Json{
            {"weaponType", "Laser"}, {"barrelShape", "Heavy"}, {"offset", {1.0f, 2.0f, 3.0f}},
            {"damageScale", -2.0f}, {"projectileSpeedScale", 0.0f}, {"fireGroup", -1},
            {"reloadScale", 0.0f}, {"recoilScale", -1.0f}, {"laserRange", 0.0f},
            {"mineFuseTime", -1.0f}, {"meleeArcDeg", 900.0f}}})},
            {"barrels", Json::array({Json{{"weaponType", "Mine"}}})},
            {"specialActionCooldownScale", 0.0f}, {"specialActionStaminaRequirement", 3.0f}},
        Json{{"id", "Basic"}, {"weaponMounts", Json::array()}}}));
    Player player;
    Check(player.LoadPlayerClassConfigs(), "Compatible mount JSON failed");
    Check(player.GetClassConfig("Twin")->barrels.front().weaponType == WeaponType::Mine, "Legacy barrels ignored");
    const auto& custom = *player.GetClassConfig("CustomTank");
    Check(custom.bulletCount == 1, "Legacy bulletCount must not restore multishot");
    Check(custom.barrels.size() == 1 && custom.barrels[0].weaponType == WeaponType::Laser, "weaponMounts must take priority");
    const auto& mount = custom.barrels[0];
    Check(mount.barrelShape == BarrelShape::Heavy && mount.fireGroup == 0, "Mount enum/group changed");
    Near(mount.offset.y, 2.0f, "Mount vector changed");
    Near(mount.damageScale, 0.0f, "Damage lower bound changed");
    Near(mount.projectileSpeedScale, 0.01f, "Speed lower bound changed");
    Near(mount.reloadScale, 0.05f, "Reload lower bound changed");
    Near(mount.recoilScale, 0.0f, "Recoil lower bound changed");
    Near(mount.laserRange, 0.1f, "Laser lower bound changed");
    Near(mount.mineFuseTime, 0.0f, "Mine lower bound changed");
    Near(mount.meleeArcDeg, 360.0f, "Melee upper bound changed");
    Near(custom.specialActionCooldownScale, 0.05f, "Special cooldown lower bound changed");
    Near(custom.specialActionStaminaCost, 3.0f, "Legacy stamina alias changed");
    Check(player.GetClassConfig("Basic")->barrels.size() == 1, "Empty mount list must use class defaults");
}
void SaveRoundTrip()
{
    const Json authored = Classes({Json{{"id", "CustomTank"}, {"displayName", "試験機"}, {"requiredRank", 3},
        {"usesDrone", true}, {"maxDrones", 5}, {"reloadScale", 0.7f}, {"bulletDamageScale", 2.0f},
        {"bulletSpeedScale", 1.4f}, {"spreadAngleDeg", 15.0f}, {"randomSpread", false},
        {"reflect", true}, {"penetrate", true}, {"fireAllBarrels", true}, {"alternateBarrels", true},
        {"recoilPower", 0.02f}, {"specialActionId", "saber_counter"}, {"specialActionStaminaCost", 2.0f},
        {"bodyShape", "Pentagon"}, {"bodyScale", {1.2f, 0.8f}}, {"bodyFillColor", {0.1f, 0.2f, 0.3f, 0.4f}},
        {"weaponMounts", Json::array({Json{{"weaponType", "Melee"}, {"barrelShape", "Trapezoid"},
            {"fires", false}, {"angleDeg", 35.0f}, {"effectColor", {0.9f, 0.8f, 0.7f, 0.6f}},
            {"laserDamageInterval", 0.2f}, {"mineLifeTime", 8.0f}, {"meleeCombo3DamageScale", 2.0f},
            {"meleeCombo2Windup", 0.3f}, {"meleeCombo1Recovery", 0.4f}}})}}, Json{{"id", "Basic"}}});
    Write(authored);
    Player first;
    Check(first.LoadPlayerClassConfigs(), "Roundtrip initial load failed");
    const Json saved = Snapshot(first);
    Check(saved["version"] == 2 && saved["classes"][0]["id"] == "CustomTank", "Save schema/order changed");
    Check(saved["classes"][0].contains("weaponMounts") && !saved["classes"][0].contains("barrels"), "Save must emit v2 mounts");
    Check(saved["classes"][0]["displayName"] == "試験機", "UTF-8 name changed");
    Check(saved["classes"][0]["weaponMounts"][0]["weaponType"] == "Melee", "Save mount enum changed");
    CheckAuthoredFields(saved["classes"][0], authored["classes"][0]);
    Player second;
    Check(second.LoadPlayerClassConfigs("snapshot.json"), "Saved data could not reload");
    Check(second.classCatalog_.OrderedIds() == first.classCatalog_.OrderedIds() && Snapshot(second) == saved, "Normalized all-field roundtrip changed");
}
void InvalidLoadsAreTransactional()
{
    Player player = Seed();
    const Json before = Snapshot(player);
    const auto order = player.classCatalog_.OrderedIds();
    const auto* oldBasic = player.GetClassConfig("Basic");
    const std::vector<Json> invalid = {
        Json::array(), Json::object(), Json{{"classes", 3}}, Classes({}), Classes({Json::array()}),
        Classes({Json::object()}), Classes({Json{{"id", 42}}}), Classes({Json{{"id", ""}}}),
        Classes({Json{{"id", "Basic"}, {"requiredRank", "bad"}}}),
        Classes({Json{{"id", "Basic"}, {"weaponMounts", Json::array({42})}}}),
        Classes({Json{{"id", "Basic"}, {"weaponMounts", Json::array({Json{{"model", ""}}})}}}),
        Classes({Json{{"id", "Basic"}, {"weaponMounts", Json::array({Json{{"model", "absent.obj"}}})}}}),
        // A valid first item must not be committed when a later item fails.
        Classes({Json{{"id", "Basic"}, {"displayName", "partial"}}, Json{{"id", false}}}),
        Classes({Json{{"id", "Basic"}, {"bodyFillColor", {1, "bad", 3, 4}}}})
    };
    for (const auto& root : invalid) {
        Write(root);
        Check(!player.LoadPlayerClassConfigs(), "Invalid JSON must fail");
        Check(player.classCatalog_.OrderedIds() == order && Snapshot(player) == before, "Failed load changed catalog/order");
        Check(player.GetClassConfig("Basic") == oldBasic, "Failed load invalidated old pointer");
    }
    WriteRaw("{ broken JSON");
    Check(!player.LoadPlayerClassConfigs(), "Malformed JSON must fail");
    Check(!player.LoadPlayerClassConfigs("never-created.json"), "Missing file must fail");
    Check(Snapshot(player) == before && player.GetClassConfig("Basic") == oldBasic, "Parse/open failure changed live config");
}
void ReloadKeepsActiveClassAndResetsFiring()
{
    Player player = Seed();
    player.currentClassId_ = "Twin";
    player.currentClass_ = ClassType::Twin;
    player.shootBarrelIndex_ = 7;
    player.shootGroupIndex_ = 9;
    player.weaponGroupCooldowns_ = {0.2f, 0.5f, 0.8f};
    Write(Classes({Json{{"id", "Twin"}, {"displayName", "new twin"}, {"reloadScale", 0.9f}}}));
    Check(player.ReloadPlayerClassConfigs(), "Active class reload should succeed");
    Check(player.currentClassId_ == "Twin" && player.currentClass_ == ClassType::Twin, "Reload changed active class");
    Check(player.GetClassConfig("Twin")->displayName == "new twin", "Reload did not commit new values");
    Check(player.classCatalog_.OrderedIds() == std::vector<std::string>{"Basic", "Twin"} && !player.GetClassConfig("CustomTank"), "Omitted inactive class was retained");
    Check(player.shootBarrelIndex_ == 0 && player.shootGroupIndex_ == 0 && player.weaponGroupCooldowns_.empty(), "Successful reload firing reset changed");
    Check(player.barrelInitializations == 1 && player.barrelLayouts == 1, "Successful reload must rebuild/layout once");
}
void FailedReloadKeepsActiveState()
{
    Player player = Seed();
    player.currentClassId_ = "Twin";
    player.currentClass_ = ClassType::Twin;
    player.shootBarrelIndex_ = 7;
    player.shootGroupIndex_ = 9;
    player.weaponGroupCooldowns_ = {0.2f, 0.5f, 0.8f};
    const auto before = Snapshot(player);
    const auto order = player.classCatalog_.OrderedIds();
    const auto* oldTwin = player.GetClassConfig("Twin");
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (attempt == 0) Write(Classes({Json{{"id", "Basic"}}})); // Active Twin disappears.
        else WriteRaw("{ malformed");
        Check(!player.ReloadPlayerClassConfigs(), "Reload must reject missing active class or malformed JSON");
        Check(Snapshot(player) == before && player.classCatalog_.OrderedIds() == order && player.GetClassConfig("Twin") == oldTwin, "Rejected reload changed/invalidate catalog");
        Check(player.currentClassId_ == "Twin" && player.currentClass_ == ClassType::Twin, "Rejected reload changed active class");
        Check(player.shootBarrelIndex_ == 7 && player.shootGroupIndex_ == 9 && player.weaponGroupCooldowns_.at(2) == 0.8f, "Rejected reload changed firing state");
        Check(player.barrelInitializations == 0 && player.barrelLayouts == 0, "Rejected reload applied equipment");
    }
}
void CatalogLoadIsIndependentAndTransactional()
{
    Player player = Seed();
    PlayerClassCatalog catalog;
    Check(catalog.Load(configPath), "Standalone catalog load failed");
    const auto before = Snapshot(catalog);
    const auto order = catalog.OrderedIds();
    const auto* oldBasic = catalog.Find("Basic");
    Write(Classes({Json{{"id", "Basic"}, {"displayName", "partial"}}, Json{{"id", 9}}}));
    Check(!catalog.Load(configPath), "Standalone catalog accepted invalid later entry");
    Check(Snapshot(catalog) == before && catalog.OrderedIds() == order && catalog.Find("Basic") == oldBasic,
        "Standalone failed load changed values, order or pointer lifetime");

    player.currentClassId_ = "Twin";
    Write(Classes({Json{{"id", "Basic"}, {"displayName", "new"}}}));
    Check(catalog.Load(configPath) && !catalog.Find("Twin"), "Catalog must not enforce Player active-class policy");
    Check(!player.LoadPlayerClassConfigs() && player.GetClassConfig("Twin"), "Player must reject the same missing-active-class data");
    Check(catalog.Find("Basic")->displayName == "new", "Standalone successful load did not commit");
}
void CatalogMutationOrderAndLifetime()
{
    PlayerClassCatalog catalog;
    catalog.ResetToDefaults();
    const std::vector<std::string> defaults = {"Basic", "Twin", "MachineGun", "Overseer", "Triple", "Assassin", "Bounder", "Ninja", "Smasher", "Summoner"};
    Check(catalog.OrderedIds() == defaults, "Built-in fallback class order changed");
    const auto* twin = catalog.Find(ClassType::Twin);
    Check(twin && twin == catalog.Find("Twin"), "Standalone enum/ID lookup mismatch");
    auto replacement = *twin;
    replacement.displayName = "replacement twin";
    catalog.InsertOrAssign(replacement);
    Check(catalog.OrderedIds() == defaults && catalog.Find("Twin") == twin && twin->displayName == "replacement twin",
        "Assignment changed first position or invalidated the existing value");
    auto custom = PlayerClassCatalog::CreateDefaultConfig(ClassType::Basic);
    custom.id = "CustomTank_with_a_long_owned_class_identifier";
    catalog.InsertOrAssign(custom);
    auto* borrowed = catalog.FindMutable(custom.id);
    Check(borrowed && borrowed == catalog.Find(custom.id), "Standalone mutable lookup mismatch");
    for (int i = 0; i < 64; ++i) {
        auto added = custom;
        added.id = "Extra_" + std::to_string(i);
        catalog.InsertOrAssign(added);
    }
    Check(twin->displayName == "replacement twin" && borrowed->id == custom.id, "Insertion/rehash invalidated borrowed values");
    Check(catalog.Erase(borrowed->id), "Erase of a borrowed ID failed");
    Check(!catalog.Find(custom.id) && std::find(catalog.OrderedIds().begin(), catalog.OrderedIds().end(), custom.id) == catalog.OrderedIds().end(),
        "Erase left order and values inconsistent");
    const auto order = catalog.OrderedIds();
    Check(!catalog.Erase("missing") && catalog.OrderedIds() == order && !catalog.FindMutable("missing"), "Unknown mutation changed catalog");
    catalog.ResetToDefaults();
    Check(catalog.OrderedIds() == defaults && !catalog.Find("Extra_0"), "Default reset retained authored entries");
}
void CatalogSwapMovesValuesAndOrderTogether()
{
    PlayerClassCatalog first;
    PlayerClassCatalog second;
    auto basic = PlayerClassCatalog::CreateDefaultConfig(ClassType::Basic);
    auto twin = PlayerClassCatalog::CreateDefaultConfig(ClassType::Twin);
    first.InsertOrAssign(basic);
    second.InsertOrAssign(twin);
    const auto* basicPointer = first.Find("Basic");
    const auto* twinPointer = second.Find("Twin");
    first.Swap(second);
    Check(first.OrderedIds() == std::vector<std::string>{"Twin"} && second.OrderedIds() == std::vector<std::string>{"Basic"}, "Swap separated order from values");
    Check(first.Find("Twin") == twinPointer && second.Find("Basic") == basicPointer && !first.Find("Basic"), "Swap changed values or their ownership");
}
void CurrentConfigPriorityStaysInPlayer()
{
    Player player = Seed();
    const auto* basic = player.GetClassConfig("Basic");
    player.runEvolutionConfig_ = PlayerClassCatalog::CreateDefaultConfig(ClassType::Twin);
    player.runStarterConfig_ = PlayerClassCatalog::CreateDefaultConfig(ClassType::MachineGun);
    player.runEvolutionActive_ = true;
    player.expeditionCombatStyleSelected_ = true;
    player.runCheckpointEvolution_ = true;
    Check(player.GetCurrentClassConfig() == basic, "Disabled Run must use normal catalog config");
    player.runModifiers_.enabled = true;
    Check(player.GetCurrentClassConfig() == &player.runEvolutionConfig_, "Run evolution must have highest priority");
    player.runEvolutionActive_ = false;
    Check(player.GetCurrentClassConfig() == &player.runStarterConfig_, "Style starter priority changed");
    player.expeditionCombatStyleSelected_ = false;
    Check(player.GetCurrentClassConfig() == &player.runStarterConfig_, "Checkpoint starter priority changed");
    player.runStarterConfig_.barrels.clear();
    Check(player.GetCurrentClassConfig() == basic, "Empty checkpoint equipment must fall back to catalog");
    player.runCheckpointEvolution_ = false;
    Check(player.GetCurrentClassConfig() == basic && player.currentClassId_ == "Basic", "Ordinary fallback changed active class");
}
void EditorCreateUsesCatalog()
{
    Player player = Seed();
    player.editorSelectedClassIndex_ = 1;
    ImGui::clickedButton = "新規作成";
    player.ExerciseEditorToolbar();
    Check(player.editorObservedId == "CustomTank_1" && player.editorSelectedClassIndex_ == 3, "Create unique ID/selection changed");
    Check(player.classCatalog_.OrderedIds() == std::vector<std::string>{"Basic", "Twin", "CustomTank", "CustomTank_1"}, "Create authored order changed");
    Check(player.editorBaselineConfigs.at("CustomTank_1").requiredRank == 1 && player.editorBaselineLabels.at("CustomTank_1") == "新規作成時", "Create baseline changed");
    Check(player.GetClassConfig("CustomTank_1")->requiredRank == 4 && player.editorUnsaved && player.currentClassId_ == "Basic" && player.barrelInitializations == 0,
        "Create must edit the new catalog value without applying inactive equipment");
}
void EditorCloneUsesCatalog()
{
    Player player = Seed();
    auto existing = player.CreateDefaultClassConfig(ClassType::Basic);
    existing.id = "Twin_Copy";
    player.classCatalog_.InsertOrAssign(existing);
    player.editorSelectedClassIndex_ = 1;
    ImGui::clickedButton = "複製";
    player.ExerciseEditorToolbar();
    Check(player.editorObservedId == "Twin_Copy_1" && player.editorSelectedClassIndex_ == 4, "Clone unique ID/selection changed");
    const auto* clone = player.GetClassConfig("Twin_Copy_1");
    Check(clone && clone->type == ClassType::Twin && clone->displayName == "Twin_Copy_1" && clone->barrels.size() == 2, "Clone did not preserve source values");
    Near(clone->reloadScale, 0.4f, "Clone reload value changed");
    Check(player.editorBaselineConfigs.at("Twin_Copy_1").displayName == "old twin" && player.editorBaselineLabels.at("Twin_Copy_1") == "複製元: old twin", "Clone source baseline changed");
    Check(player.GetClassConfig("Twin")->requiredRank == 2 && player.currentClassId_ == "Basic" && player.barrelInitializations == 0 && player.editorUnsaved,
        "Clone changed source config or active equipment");
}
void EditorDeleteInactive()
{
    Player player = Seed();
    player.editorSelectedClassIndex_ = 2;
    ImGui::clickedButton = "削除";
    ImGui::endCount = 0;
    player.ExerciseEditorToolbar();
    Check(!player.GetClassConfig("CustomTank") && !player.editorBaselineConfigs.contains("CustomTank") &&
        !player.editorBaselineLabels.contains("CustomTank"), "Delete must clear catalog and both baseline caches");
    Check(player.classCatalog_.OrderedIds() == std::vector<std::string>{"Basic", "Twin"} && player.editorSelectedClassIndex_ == 1, "Delete order/index clamp changed");
    Check(player.currentClassId_ == "Basic" && player.evolveCalls == 0 && ImGui::endCount == 1, "Inactive delete must end frame without evolution");
}
void EditorDeleteActive()
{
    Player player = Seed();
    player.currentClassId_ = "CustomTank";
    player.editorSelectedClassIndex_ = 2;
    ImGui::clickedButton = "削除";
    player.ExerciseEditorToolbar();
    Check(!player.GetClassConfig("CustomTank") && player.currentClassId_ == "Basic" && player.evolveCalls == 1, "Active custom delete must request Basic once");
}
void EditorKeepsLegacyClass()
{
    Player player = Seed();
    player.editorSelectedClassIndex_ = 1;
    ImGui::clickedButton = "削除";
    player.ExerciseEditorToolbar();
    Check(player.GetClassConfig("Twin") && player.classCatalog_.OrderedIds().size() == 3 && player.evolveCalls == 0, "Legacy class deletion must stay disabled");
}
void EditorReloadCurrent()
{
    Player player = Seed();
    player.currentClassId_ = "Twin";
    player.currentClass_ = ClassType::Twin;
    player.editorSelectedClassIndex_ = 1;
    player.shootBarrelIndex_ = 3;
    player.shootGroupIndex_ = 4;
    player.weaponGroupCooldowns_ = {0.2f, 0.5f, 0.8f};
    Write(Classes({Json{{"id", "Basic"}}, Json{{"id", "Twin"}, {"displayName", "new twin"}}}));
    ImGui::clickedButton = "JSON再読み込み";
    player.ExerciseEditorToolbar();
    Check(player.editorObservedId == "Twin" && player.editorObservedName == "new twin", "Editor must use new config after reload");
    Check(player.GetClassConfig("Twin")->requiredRank == 4, "Same-frame editing must modify the live new config");
    Check(player.editorBaselineConfigs.at("Twin").requiredRank == 2 && !player.editorUnsaved, "Reload baseline/dirty behavior changed");
    Check(player.barrelInitializations == 1 && player.barrelLayouts == 1, "Editor current class reload must apply equipment once");
    Check(player.shootBarrelIndex_ == 3 && player.shootGroupIndex_ == 4 && player.weaponGroupCooldowns_.at(2) == 0.8f, "Editor must keep Load semantics, not Reload firing reset");
}
void EditorReloadRemovedSelection()
{
    Player player = Seed();
    player.editorSelectedClassIndex_ = 2;
    Write(Classes({Json{{"id", "Basic"}}, Json{{"id", "Twin"}, {"displayName", "remaining"}}}));
    ImGui::clickedButton = "JSON再読み込み";
    player.ExerciseEditorToolbar();
    Check(player.editorSelectedClassIndex_ == 1 && player.editorObservedId == "Twin" && player.editorObservedName == "remaining", "Removed selection must clamp existing index");
    Check(!player.GetClassConfig("CustomTank") && !player.editorBaselineConfigs.contains("CustomTank"), "Removed config/baseline retained");
    Check(player.currentClassId_ == "Basic" && player.barrelInitializations == 0, "Inactive selection reload must not apply equipment");
}
void EditorReloadReorderedSelection()
{
    Player player = Seed();
    player.editorSelectedClassIndex_ = 1;
    Write(Classes({Json{{"id", "Twin"}}, Json{{"id", "CustomTank"}, {"displayName", "new custom"}}, Json{{"id", "Basic"}}}));
    ImGui::clickedButton = "JSON再読み込み";
    player.ExerciseEditorToolbar();
    Check(player.editorSelectedClassIndex_ == 1 && player.editorObservedId == "CustomTank", "Editor selection must remain index based");
    Check(player.editorObservedName == "new custom" && player.GetClassConfig("CustomTank")->requiredRank == 4, "Reordered selection must edit live selected config");
}
void EditorReloadFailure()
{
    Player player = Seed();
    player.currentClassId_ = "Twin";
    player.editorSelectedClassIndex_ = 1;
    const auto* oldTwin = player.GetClassConfig("Twin");
    Write(Classes({Json{{"id", "Basic"}}}));
    ImGui::clickedButton = "JSON再読み込み";
    player.ExerciseEditorToolbar();
    Check(player.GetClassConfig("Twin") == oldTwin && player.editorObservedName == "old twin", "Failed Editor load must retain valid old config");
    Check(player.editorSelectedClassIndex_ == 1 && player.currentClassId_ == "Twin", "Failed Editor load changed selection/active class");
    Check(player.barrelInitializations == 1 && player.barrelLayouts == 1, "Failed Editor load existing apply behavior changed");
    // Failed reload currently clears the dirty flag despite not saving edits.
    // That UI defect is outside Phase 0; deliberately do not freeze it in a test.
}
}

int main(int argc, char** argv)
{
    try {
        // Keep the production resources safe even if the exe is run directly.
        const auto fixtureRoot = std::filesystem::current_path() / "player_class_config_fixture";
        std::filesystem::create_directories(fixtureRoot / "resources/configs");
        std::filesystem::current_path(fixtureRoot);
        std::ofstream("resources/gunBarrel.obj") << "# existence-only CPU fixture\n";
        const bool configOnly = argc == 2 && std::string(argv[1]) == "--config-only";
        const std::string onlyCase = argc == 3 && std::string(argv[1]) == "--case" ? argv[2] : "";
        struct Test { const char* name; void (*run)(); bool editor; };
        const Test tests[] = {
            {"ordered_lookup_defaults", OrderedLookupAndDefaults, false},
            {"basic_fallback_duplicates", BasicFallbackAndDuplicates, false},
            {"mount_compatibility", MountCompatibility, false},
            {"save_roundtrip", SaveRoundTrip, false},
            {"invalid_load_transaction", InvalidLoadsAreTransactional, false},
            {"reload_success", ReloadKeepsActiveClassAndResetsFiring, false},
            {"reload_failure", FailedReloadKeepsActiveState, false},
            {"catalog_load_transaction", CatalogLoadIsIndependentAndTransactional, false},
            {"catalog_mutation_lifetime", CatalogMutationOrderAndLifetime, false},
            {"catalog_swap", CatalogSwapMovesValuesAndOrderTogether, false},
            {"current_config_priority", CurrentConfigPriorityStaysInPlayer, false},
            {"editor_create", EditorCreateUsesCatalog, true},
            {"editor_clone", EditorCloneUsesCatalog, true},
            {"editor_delete_inactive", EditorDeleteInactive, true},
            {"editor_delete_active", EditorDeleteActive, true},
            {"editor_keep_legacy", EditorKeepsLegacyClass, true},
            {"editor_reload_current", EditorReloadCurrent, true},
            {"editor_reload_removed", EditorReloadRemovedSelection, true},
            {"editor_reload_reordered", EditorReloadReorderedSelection, true},
            {"editor_reload_failure", EditorReloadFailure, true}
        };
        int count = 0;
        for (const auto& test : tests) {
            if ((configOnly && test.editor) || (!onlyCase.empty() && onlyCase != test.name)) continue;
            test.run();
            ++count;
            std::cout << "PASS " << test.name << std::endl;
        }
        Check(count > 0, "No tests selected");
        std::cout << "Player class config/Editor: " << count << " test groups passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
