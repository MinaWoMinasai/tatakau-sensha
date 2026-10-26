#pragma once
// Player.cppだけが読み込む。各画面へ渡す窓口を、Playerの入れ子クラスとして定義する。
// 入れ子クラスはPlayerの非公開メンバを使えるが、各画面からは窓口のインターフェースしか見えない。
#include "Player.h"
#include "game/player/editor/PlayerClassEditorTarget.h"
#include "game/player/ui/PlayerEvolutionTarget.h"
#include "game/player/ui/PlayerHudTarget.h"

/// @brief 機体設定の制作画面に渡す窓口。必要な操作だけを自機の非公開メンバへ中継する。
class Player::ClassEditorTarget final : public PlayerClassEditorTarget {
public:
    explicit ClassEditorTarget(Player& player) : player_(player) {}

    PlayerClassCatalog& GetClassCatalog() override
    {
        return player_.classCatalog_;
    }

    const std::string& GetCurrentClassId() const override
    {
        return player_.currentClassId_;
    }

    const std::vector<float>& GetWeaponGroupCooldowns() const override
    {
        return player_.weaponGroupCooldowns_;
    }

    const char* GetCurrentClassName() const override
    {
        return player_.GetCurrentClassName();
    }

    const PlayerClassConfig* GetClassConfig(const std::string& classId) const override
    {
        return player_.GetClassConfig(classId);
    }

    PlayerClassConfig* GetMutableClassConfig(const std::string& classId) override
    {
        return player_.GetMutableClassConfig(classId);
    }

    PlayerClassConfig CreateDefaultClassConfig(ClassType type) const override
    {
        return player_.CreateDefaultClassConfig(type);
    }

    bool LoadPlayerClassConfigs() override
    {
        return player_.LoadPlayerClassConfigs();
    }

    void SavePlayerClassConfigs() const override
    {
        player_.SavePlayerClassConfigs();
    }

    void EvolveById(const std::string& classId) override
    {
        player_.EvolveById(classId);
    }

    void InitializeBarrels() override
    {
        player_.InitializeBarrels();
    }

    void UpdateBarrelLayout() override
    {
        player_.UpdateBarrelLayout();
    }

private:
    Player& player_;
};

/// @brief 強化HUDに渡す窓口。必要な値と操作だけを自機の非公開メンバへ中継する。
class Player::HudTarget final : public PlayerHudTarget {
public:
    explicit HudTarget(Player& player) : player_(player) {}

    int GetLevel() const override
    {
        return player_.level_;
    }

    int GetMaxLevel() const override
    {
        return player_.kMaxLevel;
    }

    int GetExp() const override
    {
        return player_.exp_;
    }

    int GetNextLevelExpValue() const override
    {
        return player_.nextLevelExp_;
    }

    int GetSkillPoints() const override
    {
        return player_.skillPoints_;
    }

    const std::array<int, 7>& GetUpgradeLevels() const override
    {
        return player_.upgradeLevels_;
    }

    int GetMaxEnhancePoint() const override
    {
        return player_.maxEnhancePoint;
    }

    void SetMaxEnhancePoint(int maxEnhancePoint) override
    {
        player_.maxEnhancePoint = maxEnhancePoint;
    }

    bool IsChangeMode() const override
    {
        return player_.isChangeMode_;
    }

    bool IsDead() const override
    {
        return player_.isDead_;
    }

    bool IsRunModifiersEnabled() const override
    {
        return player_.runModifiers_.enabled;
    }

    myEngine::Input* GetInput() const override
    {
        return player_.input_;
    }

    const myEngine::Vector2& GetMousePosition() const override
    {
        return player_.mousePosition_;
    }

    const char* GetCurrentClassName() const override
    {
        return player_.GetCurrentClassName();
    }

    bool ApplyStatUpgrade(int index) override
    {
        return player_.ApplyStatUpgrade(index);
    }

    bool RefundStatUpgrade(int index) override
    {
        return player_.RefundStatUpgrade(index);
    }

private:
    Player& player_;
};

/// @brief 進化画面に渡す窓口。必要な値と操作だけを自機の非公開メンバへ中継する。
class Player::EvolutionTarget final : public PlayerEvolutionTarget {
public:
    explicit EvolutionTarget(Player& player) : player_(player) {}

    const PlayerClassCatalog& GetClassCatalog() const override
    {
        return player_.classCatalog_;
    }

    const std::string& GetCurrentClassId() const override
    {
        return player_.currentClassId_;
    }

    const char* GetCurrentClassName() const override
    {
        return player_.GetCurrentClassName();
    }

    const PlayerClassConfig* GetClassConfig(const std::string& classId) const override
    {
        return player_.GetClassConfig(classId);
    }

    const PlayerClassConfig* GetCurrentClassConfig() const override
    {
        return player_.GetCurrentClassConfig();
    }

    bool GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const override
    {
        return player_.GetTankButtonVisualData(classId, output);
    }

    int GetRankFromLevel(int level) const override
    {
        return player_.GetRankFromLevel(level);
    }

    int GetLevel() const override
    {
        return player_.level_;
    }

    bool CanEvolveTo(const std::string& classId) const override
    {
        return player_.CanEvolveTo(classId);
    }

    bool HasEvolutionEdge(const std::string& from, const std::string& to) const override
    {
        return player_.HasEvolutionEdge(from, to);
    }

    bool IsEvolutionClassVisible(const std::string& classId) const override
    {
        return player_.IsEvolutionClassVisible(classId);
    }

    bool IsRunCompatibleClass(const PlayerClassConfig& config) const override
    {
        return player_.IsRunCompatibleClass(config);
    }

    bool TryConfirmEvolutionById(const std::string& classId) override
    {
        return player_.TryConfirmEvolutionById(classId);
    }

    void EvolveById(const std::string& classId) override
    {
        player_.EvolveById(classId);
    }

    bool LoadPlayerClassConfigs() override
    {
        return player_.LoadPlayerClassConfigs();
    }

    bool IsChangeMode() const override
    {
        return player_.isChangeMode_;
    }

    void CancelEvolutionMode() override
    {
        player_.isChangeMode_ = false;
        player_.evolutionCancelledEvent_ = true;
    }

    int GetEvolutionSelectedIndex() const override
    {
        return player_.evolutionSelectedIndex_;
    }

    void SetEvolutionSelectedIndex(int index) override
    {
        player_.evolutionSelectedIndex_ = index;
    }

    float GetEvolutionUiTimer() const override
    {
        return player_.evolutionUiTimer_;
    }

    void AdvanceEvolutionUiTimer(float seconds) override
    {
        player_.evolutionUiTimer_ += seconds;
    }

    float GetDeltaTime() const override
    {
        return player_.dt_;
    }

    myEngine::Input* GetInput() const override
    {
        return player_.input_;
    }

    const myEngine::Vector2& GetMousePosition() const override
    {
        return player_.mousePosition_;
    }

private:
    Player& player_;
};
