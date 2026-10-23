#pragma once
#include "game/player/PlayerClassCatalog.h"
#include "game/player/PlayerClassConfig.h"
#include <string>
#include <vector>

/// @brief 機体設定の制作画面（PlayerClassEditor）が自機に対して行う操作だけをまとめた窓口。
/// @note 制作画面にPlayerの非公開メンバを直接触らせないために使う。実装はPlayerが持ち、寿命もPlayerと同じ。
class PlayerClassEditorTarget {
public:
    /// @brief 機体設定の一覧を編集用に返す。
    virtual PlayerClassCatalog& GetClassCatalog() = 0;
    /// @brief 現在の機体IDを返す。
    virtual const std::string& GetCurrentClassId() const = 0;
    /// @brief 砲身グループごとの発射待ち時間（秒）を返す。
    virtual const std::vector<float>& GetWeaponGroupCooldowns() const = 0;
    /// @brief 現在の機体の表示名を返す。
    virtual const char* GetCurrentClassName() const = 0;
    /// @brief 機体IDから設定を借用する。見つからなければnullptr。
    virtual const PlayerClassConfig* GetClassConfig(const std::string& classId) const = 0;
    /// @brief 機体IDから設定を編集用に借用する。見つからなければnullptr。
    virtual PlayerClassConfig* GetMutableClassConfig(const std::string& classId) = 0;
    /// @brief 指定した旧機体種類の既定設定を値で返す。
    virtual PlayerClassConfig CreateDefaultClassConfig(ClassType type) const = 0;
    /// @brief 既定のパスから機体設定を読み直す。成功ならtrue。
    virtual bool LoadPlayerClassConfigs() = 0;
    /// @brief 既定のパスへ機体設定を保存する。
    virtual void SavePlayerClassConfigs() const = 0;
    /// @brief 指定IDの機体へ切り替える。
    virtual void EvolveById(const std::string& classId) = 0;
    /// @brief 現在の機体設定から砲塔を作り直す。
    virtual void InitializeBarrels() = 0;
    /// @brief 砲塔の配置を更新する。
    virtual void UpdateBarrelLayout() = 0;

protected:
    virtual ~PlayerClassEditorTarget() = default;
};
