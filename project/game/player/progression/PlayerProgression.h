#pragma once
#include "Player.h"

/// @brief 自機の装備・機体設定・成長と性能の再計算を担当する。
class PlayerProgression {
public:
    /// @brief 戦闘主体である自機を借用する。
    explicit PlayerProgression(Player& player) : player_(player) {}
    using PlayerClassConfig = ::PlayerClassConfig;
    using PlayerStats = Player::PlayerStats;
    using BodyShape = Player::BodyShape;
    using SpecialCombatEvent = Player::SpecialCombatEvent;
    using TargetLockVisual = Player::TargetLockVisual;
    using BarrelModel = Player::BarrelModel;
    using BalanceConfig = Player::BalanceConfig;
    using SpecialCombatStats = Player::SpecialCombatStats;
    using MineDropEvent = Player::MineDropEvent;
    using WallSmashTarget = Player::WallSmashTarget;
    using DroneLaserLink = Player::DroneLaserLink;
    using MeleeSlashEvent = Player::MeleeSlashEvent;
    using DashImpactEvent = Player::DashImpactEvent;
    using UiProfileStats = Player::UiProfileStats;
    using NeonBodyLayout = Player::NeonBodyLayout;
    using RunCombatSnapshot = Player::RunCombatSnapshot;
    using DroneAbilityVisual = Player::DroneAbilityVisual;
    using LaserShotEvent = Player::LaserShotEvent;
    using NeonBarrelLayout = Player::NeonBarrelLayout; /// @brief 遠征の戦闘系統を選び、性能・装備・特殊状態を切り替える。
    /// @return 有効な遠征で切り替えられた場合、または同じ系統が選択済みならtrue。
    /// @note 無効な系統、遠征外、死亡中、必要なBasic設定がない場合はfalse。切り替えでHP・スタミナを補充しない。
    bool SetExpeditionCombatStyle(tankbuild::Style style);

    /// @brief 性能調整設定を現在の状態へ適用する。
    void ApplyBalanceConfig(const BalanceConfig& config);

    /// @brief 戦闘系統ごとの調整値を補正して反映し、性能とドローン設定を更新する。
    /// @note HP・スタミナは新しい上限内に制限し、補充しない。
    void ApplyCombatStyleBalance(const TankCombatStyleBalances& profiles);

    /// @brief 遠征補正をコピーして性能・装備へ反映する。補正の有効化・無効化に伴う状態も初期化する。
    void SetRunModifiers(const TankRunModifiers& modifiers);

    /// @brief 遠征戦闘状態の写しを返す。
    RunCombatSnapshot GetRunCombatSnapshot() const;

    /// @brief 遠征の初期装備を準備し、整備・進化状態を初期化する。遠征補正無効なら何もしない。
    /// @param archetype 初期系統。0: Twin、1: MachineGun、2: Overseer。範囲外は0～2へ制限する。
    /// @note 部屋ごとの進化方式ではBasicから開始し、レベル・強化・ドローンをリセットしてHPを全回復する。
    void ConfigurePrototypeLoadout(int archetype);

    // 0: Twin, 1: MachineGun, 2: Overseer
    /// @brief 遠征中の自機のHPを回復する。
    /// @note amountが正で生存中の場合、最大HPまで回復する。遠征補正無効なら何もしない。
    void HealRunPlayer(int amount);

    /// @brief 遠征中の自機のHPを費用として消費する。
    /// @return 正のamountを支払ってHPが1以上残る場合true。遠征補正無効・死亡中・支払不可なら変更せずfalse。
    bool SpendRunHealth(int amount);

    /// @brief 現在選択できる遠征の進化候補を値で返す。方式・装備系統・準備状態に応じて空になる。
    std::vector<RunEvolutionChoice> GetRunEvolutionChoices() const;

    /// @brief 遠征の進化候補を選べる状態へ進め、進化画面と確定・取消イベントを解除する。
    /// @note 部屋ごとの方式では準備フラグを設定する。旧方式では次の機体に必要なランクまでレベルを上げる。
    void PrepareRunEvolution();

    /// @brief 現在の遠征進化候補から指定IDを選択し、装備へ反映して確定イベントを記録する。
    /// @return 選択できればtrue。候補外・遠征補正無効・死亡中などはfalse。
    bool ChooseRunEvolution(const std::string& id);

    /// @brief 部屋ごとの進化方式で、未付与のクリア部屋に対して整備ポイントを1付与する。
    /// @param clearedRoom 部屋番号（1～4）。
    /// @return 付与した場合true。付与済み・範囲外・遠征補正無効・方式外・死亡中はfalse。
    bool AwardRunMaintenancePoint(int clearedRoom);

    /// @brief 遠征整備の3項目について、現在の段階と支払可否を値で返す。
    std::array<RunMaintenanceChoice, 3> GetRunMaintenanceChoices() const;

    /// @brief 整備ポイントを1消費し、指定項目を1段階強化して性能を再計算する。
    /// @param stat 0: 機動、1: 装填、2: 装甲。
    /// @return 強化した場合true。方式外・死亡中・範囲外・ポイント不足・上限到達ならfalse。
    /// @note この再計算ではHPを回復しない。
    bool SpendRunMaintenancePoint(int stat);

    /// @brief 指定整備を1段階戻してポイントを1返却し、性能を再計算する。
    /// @param stat 0: 機動、1: 装填、2: 装甲。
    /// @return 返却した場合true。方式外・死亡中・範囲外・未強化ならfalse。
    bool RefundRunMaintenancePoint(int stat);

    /// @brief 部屋移動時に一時的な戦闘状態・ドローン・イベントをリセットし、指定ワールド座標へ移す。
    /// @note 遠征補正有効かつ生存中に行う。HP・成長は保ち、スタミナを全回復して短い無敵時間を設定する。
    /// 通常入力はマウスボタンを解放するまで攻撃を待つ。生成済みの弾はここでは消去しない。
    void ResetRunRoomState(const cg2::Vector3& position);

    /// @brief 経験値を加算してレベル・強化ポイントを更新する。通貨方式では獲得通貨として保留する。
    /// @param amount 獲得した経験値。呼び出し側では非負の値を渡す。
    /// @note 通常方式ではレベル上限で加算を止める。通貨方式の保留上限は1000000。
    void AddExp(int amount);

    /// @brief 撃破報酬をレベルへ加算せず、マップ所有の財布へ渡す通貨として保留する方式を切り替える。
    /// @note 方式が変わると保留通貨を消去する。有効化時はレベル・経験値・強化ポイントを初期化し進化画面を閉じる。
    void SetRunCurrencyMode(bool enabled);

    /// @brief 制作データから遠征で使う機体設定を登録する。
    /// @note Catalogの検証失敗なら変更しない。成功時は登録一覧を置き換え、適合する選択中の制作機体も更新する。
    void InstallRunAuthoredClasses(const tankcontent::Catalog& catalog);

    /// @brief 登録した制作機体から、現在の装備系統に合う未選択の候補を値で返す。遠征補正無効・死亡中は空。
    std::vector<RunEvolutionChoice> GetRunAuthoredEvolutionChoices() const;

    /// @brief 登録した制作機体を選び、装備と性能を更新して進化確定イベントを記録する。
    /// @return 遠征補正有効・生存中でIDと装備系統が適合すればtrue。それ以外は変更せずfalse。
    /// @note HPは新しい上限内に保ち、補充しない。ドローンと一部の特殊戦闘状態を作り直す。
    bool ChooseRunAuthoredClass(const std::string& id);

    /// @brief 強化レベルを返す。
    /// @param index 強化項目の添字（0～6）。範囲外なら0。
    int GetUpgradeLevel(int index) const;

    /// @brief 現在の機体の表示名を借用して返す。設定がなければ旧機体名またはUnknown。
    /// @note 設定の編集・再読込・機体切り替えをまたいで保持せず、必要なら文字列をコピーする。
    const char* GetCurrentClassName() const;

    /// @brief 通常方式の強化ポイントを1消費し、指定項目を1段階上げて性能と実行イベントを更新する。
    /// @param index 0: スタミナ回復、1: 最大HP、2: 接触ダメージ、3: 弾速、4: 弾の威力、5: 発射間隔、6: 移動速度。
    /// @return 強化した場合true。遠征補正有効・範囲外・ポイント不足・上限到達なら変更せずfalse。
    bool ApplyStatUpgrade(int index);

    /// @brief 通常方式の指定強化を1段階戻し、ポイントを1返却して性能を再計算する。
    /// @return 返却した場合true。遠征補正有効・添字の範囲外・未強化なら変更せずfalse。
    bool RefundStatUpgrade(int index);

    /// @brief 機体設定を再読込し、現在の機体の砲塔と配置に反映する。
    /// @return 現在の機体を保ったまま再読込できた場合true。
    /// @note HPと成長状態を初期化しない。
    /// 読み込み失敗や現在の機体IDが欠けている場合は既存設定を保持する。成功時は設定の借用ポインターを再取得する。
    bool ReloadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");

    /// @brief レベルから解放ランク（1～4）を返す。5・10・15以上でそれぞれランク2・3・4になる。
    int GetRankFromLevel(int level) const;

    /// @brief 現在のレベルから、次のレベルアップに必要な経験値を計算する。
    int GetNextLevelExp() const;

    /// @brief 発射間隔の基準値を60FPS相当のフレーム数で返す。選択済みの戦闘系統、または基礎値を使う。
    float GetRunBaseReloadFrames() const;

    /// @brief ドローン系統の護衛機数を現在の上限に揃える。超過分を削除し、不足分を生成する。
    /// @note 本体・弾管理先の未設定、死亡中、ドローン系統以外なら何もしない。
    void EnsureExpeditionDrones();

    /// @brief 遠征ドローン上限を返す。
    int GetExpeditionDroneLimit() const;

    /// @brief 旧機体種類をIDへ変換し、EvolveByIdへ渡す。
    void Evolve(ClassType newClass);

    /// @brief 指定IDの機体へ進化し、装備と外観を反映する。
    /// @note 設定がないか遠征で使用できない機体なら何もしない。ランク・進化経路の判定は呼び出し側で行う。
    void EvolveById(const std::string& classId);

    /// @brief 進化条件を満たす機体へ切り替え、履歴と確定イベントを更新する。成功ならtrue。
    bool TryConfirmEvolutionById(const std::string& classId);

    /// @brief 指定機体が次ランクの接続先で、使用条件と必要ランクを満たすか判定する。
    bool CanEvolveTo(const std::string& classId) const;

    /// @brief 遠征で使用できるドローン機体または射撃砲塔を持つ機体か判定する。遠征補正無効ならtrue。
    /// @note 遠征ではSmasherを対象外とする。
    bool IsRunCompatibleClass(const PlayerClassConfig& config) const;

    /// @brief 進化画面で機体を表示対象にするか判定する。遠征補正無効、または現在の機体IDならtrue。
    bool IsEvolutionClassVisible(const std::string& classId) const;

    /// @brief 読み込んだ進化経路にfromからtoへの接続があるか判定する。
    bool HasEvolutionEdge(const std::string& from, const std::string& to) const;

    /// @brief 機体設定を一時Catalogへ読み、現在の機体IDを検証してから置き換える。
    /// @return 読み込みと現在の機体の検証に成功した場合true。
    /// @note 失敗時は現在の設定を保つ。実行中のHPや装備の再初期化は行わない。
    bool LoadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");

    /// @brief 機体Catalogを指定パスのJSONへ保存する。保存の成否は戻り値で通知しない。
    void SavePlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json") const;

    /// @brief 指定した旧機体種類の既定設定を値で返す。
    PlayerClassConfig CreateDefaultClassConfig(ClassType type) const;

    /// @brief 機体種類からCatalogの設定を借用する。見つからなければnullptr。
    /// @note 読み込み成功・既定値へのリセット・対象の削除後は再取得する。
    const PlayerClassConfig* GetClassConfig(ClassType type) const;

    /// @brief 機体IDからCatalogの設定を借用する。見つからなければnullptr。
    /// @note 読み込み成功・既定値へのリセット・対象の削除後は再取得する。
    const PlayerClassConfig* GetClassConfig(const std::string& classId) const;

    /// @brief 現在使用する機体設定を借用する。遠征の進化設定・初期装備を優先し、なければCatalogを参照する。
    /// @note 設定切り替えや再読込後は再取得する。Catalogに現在IDがなければnullptrになり得る。
    const PlayerClassConfig* GetCurrentClassConfig() const;

    /// @brief 指定IDのCatalog設定を編集用に借用する。見つからなければnullptr。寿命はGetClassConfigと同じ。
    PlayerClassConfig* GetMutableClassConfig(const std::string& classId);

    /// @brief 基礎性能または選択した戦闘系統に、強化・遠征・整備の補正を掛けて性能を再計算する。
    /// @param healToFull HPを新しい最大値まで回復するか。
    /// @note falseでも再計算前が満タンなら新しい最大HPにする。それ以外は新しい上限内に保つ。
    void RecalculateStatsFromBase(bool healToFull);

private:
    Player& player_;
};
