#pragma once
#include "Input.h"
#include "Struct.h"
#include "game/player/PlayerClassCatalog.h"
#include "game/player/PlayerClassConfig.h"
#include "game/ui/TankButtonUI.h"
#include <string>

/// @brief 進化画面（PlayerEvolution）が自機に対して行う操作だけをまとめた窓口。
/// @note 進化画面にPlayerの非公開メンバを直接触らせないために使う。実装はPlayerが持ち、寿命もPlayerと同じ。
class PlayerEvolutionTarget {
public:
    /// @brief 機体設定の一覧を返す。
    virtual const PlayerClassCatalog& GetClassCatalog() const = 0;
    /// @brief 現在の機体IDを返す。
    virtual const std::string& GetCurrentClassId() const = 0;
    /// @brief 現在の機体の表示名を返す。
    virtual const char* GetCurrentClassName() const = 0;
    /// @brief 機体IDから設定を借用する。見つからなければnullptr。
    virtual const PlayerClassConfig* GetClassConfig(const std::string& classId) const = 0;
    /// @brief 現在使用する機体設定を借用する。
    virtual const PlayerClassConfig* GetCurrentClassConfig() const = 0;
    /// @brief 機体の見た目を表示用に返す。見つかった場合true。
    virtual bool GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const = 0;
    /// @brief レベルからランクを返す。
    virtual int GetRankFromLevel(int level) const = 0;
    /// @brief 現在のレベルを返す。
    virtual int GetLevel() const = 0;
    /// @brief 指定機体へ進化できるか判定する。
    virtual bool CanEvolveTo(const std::string& classId) const = 0;
    /// @brief 読み込んだ進化経路にfromからtoへの接続があるか判定する。
    virtual bool HasEvolutionEdge(const std::string& from, const std::string& to) const = 0;
    /// @brief 進化画面で機体を表示対象にするか判定する。
    virtual bool IsEvolutionClassVisible(const std::string& classId) const = 0;
    /// @brief 遠征で使用できる機体か判定する。
    virtual bool IsRunCompatibleClass(const PlayerClassConfig& config) const = 0;
    /// @brief 進化条件を満たす機体へ切り替える。成功ならtrue。
    virtual bool TryConfirmEvolutionById(const std::string& classId) = 0;
    /// @brief 指定IDの機体へ切り替える。
    virtual void EvolveById(const std::string& classId) = 0;
    /// @brief 既定のパスから機体設定を読み直す。成功ならtrue。
    virtual bool LoadPlayerClassConfigs() = 0;
    /// @brief 進化画面を開いているか判定する。
    virtual bool IsChangeMode() const = 0;
    /// @brief 進化を選ばずに進化画面を閉じ、取り消しの通知を残す。
    virtual void CancelEvolutionMode() = 0;
    /// @brief 図鑑で選んでいる項目の番号を返す。
    virtual int GetEvolutionSelectedIndex() const = 0;
    /// @brief 図鑑で選んでいる項目の番号を設定する。
    virtual void SetEvolutionSelectedIndex(int index) = 0;
    /// @brief 進化画面の演出に使う経過時間（秒）を返す。
    virtual float GetEvolutionUiTimer() const = 0;
    /// @brief 進化画面の演出に使う経過時間を進める。
    virtual void AdvanceEvolutionUiTimer(float seconds) = 0;
    /// @brief 直前の更新で使った経過時間（秒）を返す。
    virtual float GetDeltaTime() const = 0;
    /// @brief 自機が読む入力を返す。初期化前はnullptr。
    virtual myEngine::Input* GetInput() const = 0;
    /// @brief 論理座標に直したマウス位置を返す。
    virtual const myEngine::Vector2& GetMousePosition() const = 0;

protected:
    virtual ~PlayerEvolutionTarget() = default;
};
