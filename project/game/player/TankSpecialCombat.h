#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// 実戦闘・カードの実演・描画を使わない回帰テストで共有する計算と状態。
// テクスチャやUI、実時間に依存する時計、GPU資源の確保はここへ持ち込まない。
namespace tankspecial {
// 敵の実射撃と近接迎撃で共有する耐久度の基準。
// 通常パリィでは通常弾まで破壊できるが、ボス弾・装甲弾には耐久ダメージを与えない。
inline constexpr float kOrdinaryEnemyBulletHp = 6.0f;
inline constexpr float kBossEnemyBulletHp = 12.0f;
inline constexpr float kArmoredEnemyBulletHp = 24.0f;
inline constexpr float kRailMaxChargeSeconds = 1.0f;
inline constexpr float kRailTapDamageScale = 0.65f;
inline constexpr float kRailChargeDamageGain = 4.35f;
inline constexpr float kRailTapSpeedScale = 1.8f;
inline constexpr float kRailChargeSpeedGain = 1.8f;
inline constexpr float kMinAbilityPower = 0.1f;
inline constexpr float kMaxAbilityPower = 5.0f;
inline constexpr float kRailRecoveryReloadScale = 0.8f;
inline constexpr float kRailMinRecoverySeconds = 0.12f;
inline constexpr float kRailMaxRecoverySeconds = 0.35f;
inline constexpr float kLinkHitIntervalSeconds = 0.20f;
inline constexpr float kSlashWaveDamageScale = 0.55f;
inline constexpr float kPerfectParryWindowSeconds = 0.12f;
inline constexpr float kPerfectParryDurabilityDamage = 8.0f;
/// @brief レール砲の蓄積時間（秒）と押下継続状態を保持し、入力を離したときに発射用の時間を取り出す。
struct RailCharge {
    float seconds = 0.0f;
    bool held = false;
    /// @brief 蓄積時間を0秒にし、押下継続状態を解除する。
    void Reset()
    {
        seconds = 0;
        held = false;
    }
    /// @brief 押している間に蓄積し、離したときに蓄積時間を返して状態をリセットする。
    /// @param pressed 今回の更新で発射入力が押されているか。押下開始の瞬間だけを示す値ではない。
    /// @param dt 経過時間（秒）。負値は0として扱い、蓄積はkRailMaxChargeSecondsまでに制限する。
    /// @param ready チャージ・発射を受け付けられるか。falseなら入力によらずリセットする。
    /// @return 発射なしは-1。押下後に離した場合は蓄積時間（秒）。0秒も有効な発射結果。
    /// @note 弾の生成は呼び出し側で行う。最大時間まで押しても、離すまでは発射結果を返さない。
    float Step(bool pressed, float dt, bool ready)
    {
        if (!ready) {
            Reset();
            return -1.0f;
        }
        if (pressed) {
            // 入力を離すまで発射を保留する。dtが0でも押下履歴を残すので、離したときの0秒は有効。
            held = true;
            seconds = (std::min)(kRailMaxChargeSeconds, seconds + (std::max)(0.0f, dt));
            return -1.0f;
        }
        if (!held)
            return -1.0f;
        const float shot = seconds;
        Reset();
        return shot;
    }
};
/// @brief レール砲のダメージ倍率を返す。
/// @param charge 蓄積時間（秒）。計算では0～1に制限し、最大チャージ時間による正規化は行わない。
/// @param power チャージによる加算倍率の強さ。kMinAbilityPower～kMaxAbilityPowerに制限する。
inline float RailDamageScale(float charge, float power = 1)
{
    return kRailTapDamageScale +
           kRailChargeDamageGain * (std::clamp)(charge, 0.0f, 1.0f) * (std::clamp)(power, kMinAbilityPower, kMaxAbilityPower);
}
/// @brief レール砲の弾速倍率を返す。
/// @param charge 蓄積時間（秒）。計算では0～1に制限する。
inline float RailSpeedScale(float charge)
{
    return kRailTapSpeedScale + kRailChargeSpeedGain * (std::clamp)(charge, 0.0f, 1.0f);
}
/// @brief レール砲発射後、次のチャージを受け付けるまでの待ち時間（秒）を返す。
/// @param reload 基準の発射間隔（秒）。倍率を掛け、最小・最大の待ち時間に制限する。
inline float RailRecovery(float reload)
{
    return (std::clamp)(reload * kRailRecoveryReloadScale, kRailMinRecoverySeconds, kRailMaxRecoverySeconds);
}
/// @brief ドローン数からレーザーの接続本数を計算する。2機未満は0本、2機は1本、3機以上は機数と同じ。
inline int LinkCount(int drones)
{
    return drones < 2 ? 0 : drones == 2 ? 1 : drones;
}
/// @brief XY平面で点Pに最も近い線分AB上の位置を、AからBへの比率（0～1）で返す。
/// @note 線分の長さの2乗が0.00001以下なら、始点を表す0を返す。
inline float SegmentClosestFraction(float ax, float ay, float bx, float by, float px, float py)
{
    const float dx = bx - ax, dy = by - ay, len = dx * dx + dy * dy;
    return len > .00001f ? (std::clamp)(((px - ax) * dx + (py - ay) * dy) / len, 0.0f, 1.0f) : 0.0f;
}
/// @brief XY平面で線分ABと、中心P・半径radiusの円が接触するか判定する。境界の接触もtrue。
/// @param radius 円の半径。呼び出し側では非負の値を渡す。
inline bool SegmentTouches(float ax, float ay, float bx, float by, float px, float py, float radius)
{
    const float t = SegmentClosestFraction(ax, ay, bx, by, px, py);
    const float x = ax + (bx - ax) * t - px, y = ay + (by - ay) * t - py;
    return x * x + y * y <= radius * radius;
}
/// @brief XY平面で線分ABと軸平行の矩形が交差するか判定する。境界の接触もtrue。
/// @note 矩形は各軸のminがmax以下となる座標を渡す。
inline bool SegmentCrossesBox(float ax, float ay, float bx, float by, float minX, float minY, float maxX, float maxY)
{
    // 線分上の比率[0,1]を各軸の矩形内区間で絞り、共通区間が残るかを調べる。
    float nearT = 0, farT = 1;
    auto slab = [&](float start, float delta, float lo, float hi) {
        // この軸にほぼ動かない線分では、始点が軸の範囲内なら他方の軸の判定へ進める。
        if (std::abs(delta) < .00001f)
            return start >= lo && start <= hi;
        float a = (lo - start) / delta, b = (hi - start) / delta;
        if (a > b)
            std::swap(a, b);
        nearT = (std::max)(nearT, a);
        farT = (std::min)(farT, b);
        return nearT <= farT;
    };
    return slab(ax, bx - ax, minX, maxX) && slab(ay, by - ay, minY, maxY);
}
/// @brief 接続レーザーが同じ対象へ与えるダメージの間隔を管理する。
struct LinkDamageClock {
    /// @brief 対象の衝突IDと、次にダメージを許可する内部時刻（秒）を保持する。idの0は空き枠。
    struct Target {
        uint64_t id = 0;
        float next = 0;
    };
    std::array<Target, 256> targets{};
    float seconds = 0;
    /// @brief ダメージ間隔の判定に使う内部時刻を進める。
    /// @param dt 経過時間（秒）。負値は0として扱う。
    void Advance(float dt)
    {
        seconds += (std::max)(0.0f, dt);
    }
    /// @brief 対象へのダメージを許可できれば、次に許可する時刻を予約する。
    /// @param id 対象の衝突ID。空き枠を表す0以外を渡す。
    /// @return 許可して記録した場合true。前回からの間隔不足、または256枠に再利用できる枠がなければfalse。
    /// @note 同じ更新内の他のレーザーからも共有する。空き枠または待ち時間が終わった枠を再利用する。
    bool Claim(uint64_t id)
    {
        Target* free = nullptr;
        for (auto& target : targets) {
            // 同じIDの枠を先に探し、許可時刻を更新する。誤差許容はこの既存枠の比較だけに使う。
            if (target.id == id) {
                if (seconds + 1e-6f < target.next)
                    return false;
                target.next = seconds + kLinkHitIntervalSeconds;
                return true;
            }
            // 別IDの待ち時間が終わった枠も再利用できるが、同じIDが後ろにある可能性があるので走査を続ける。
            if (!free && (target.id == 0 || target.next <= seconds))
                free = &target;
        }
        if (!free)
            return false;
        *free = {id, seconds + kLinkHitIntervalSeconds};
        return true;
    }
};
/// @brief コンボ段階が斬撃波の発生対象か判定する。comboStepが2（3段目）の場合だけtrue。
/// @note 判定だけを行い、弾の生成や状態変更は行わない。
inline bool EmitsSlashWave(int comboStep)
{
    return comboStep == 2;
}
/// @brief フィニッシュ斬撃のダメージから、飛ばす斬撃波のダメージを計算する。四捨五入し、最低1とする。
inline uint32_t SlashDamage(uint32_t finisherDamage, float power = 1)
{
    return static_cast<uint32_t>((std::max)(1.0f, std::round(static_cast<float>(finisherDamage) * kSlashWaveDamageScale * power)));
}
/// @brief 斬撃の有効時間開始からの経過秒が、ジャストパリィの受付範囲内か判定する。
/// @param activeElapsed 有効時間開始からの秒数。0～kPerfectParryWindowSecondsの両端を含めてtrue。
inline bool IsPerfectParry(float activeElapsed)
{
    return activeElapsed >= 0 && activeElapsed <= kPerfectParryWindowSeconds;
}
/// @brief パリィで弾の耐久度へ与えるダメージを計算する。
/// @param hp 対象弾の現在の耐久度。
/// @param perfect ジャストパリィか。trueならpowerを制限して固定基準の耐久ダメージに掛ける。
/// @return 通常パリィは通常弾の耐久度以下ならhp、それより強い弾には0。実際の適用は呼び出し側で行う。
inline float ParryDurabilityDamage(float hp, bool perfect, float power = 1)
{
    if (perfect)
        return kPerfectParryDurabilityDamage * (std::clamp)(power, kMinAbilityPower, kMaxAbilityPower);
    return hp <= kOrdinaryEnemyBulletHp ? hp : 0.0f;
}

// 遠征内の状態を実際のアクターと描画なしのテストで共有する。
enum class DronePhase {
    Escort,
    Warning,
    Charging,
    Returning,
    Rebuilding
};
/// @brief ドローンの護衛・予告・突撃・帰還・再構築を状態で管理する。
class DroneMission {
public:
    static constexpr float kWarningSeconds = 0.30f;
    static constexpr float kBombWarningSeconds = 0.65f;
    static constexpr float kChargingTimeoutSeconds = 1.0f;
    static constexpr float kReturnTimeoutSeconds = 2.5f;
    static constexpr float kRebuildSeconds = 5.5f;
    /// @brief 現在の任務段階を返す。
    DronePhase GetPhase() const
    {
        return phase_;
    }
    /// @brief 現在の段階へ移ってからの経過時間（秒）を返す。
    float GetElapsed() const
    {
        return elapsed_;
    }
    /// @brief 通常突撃または自爆突撃の予告時間（秒）を返す。
    float GetWarningDuration() const
    {
        return bomb_ ? kBombWarningSeconds : kWarningSeconds;
    }
    /// @brief 最後に開始した任務が自爆突撃かを返す。
    bool IsBomb() const
    {
        return bomb_;
    }
    /// @brief Arriveで記録した未消費の到着イベントがあるかを返す。
    bool HasImpact() const
    {
        return impact_;
    }
    /// @brief 到着イベントの有無を返し、保留フラグを解除する。未消費の到着があればtrue。
    bool ConsumeImpact()
    {
        const bool hit = impact_;
        impact_ = false;
        return hit;
    }
    /// @brief 護衛中なら突撃の予告を開始し、到着イベントを解除する。他の段階では変更せずfalse。
    /// @param explosive 到着後に再構築へ進む自爆突撃か。falseなら帰還する通常突撃。
    /// @return 予告を開始した場合true。
    bool Start(bool explosive)
    {
        if (phase_ != DronePhase::Escort)
            return false;
        Enter(DronePhase::Warning);
        bomb_ = explosive;
        impact_ = false;
        return true;
    }
    /// @brief 突撃中なら到着イベントを記録し、通常突撃は帰還、自爆突撃は再構築へ進める。
    /// @note 突撃中以外は何もしない。位置の移動やダメージ適用は行わない。
    void Arrive()
    {
        if (phase_ != DronePhase::Charging)
            return;
        impact_ = true;
        Enter(bomb_ ? DronePhase::Rebuilding : DronePhase::Returning);
    }
    /// @brief 経過時間と帰還条件に従って任務を進める。段階が変わると経過時間を0に戻す。
    /// @param dt 経過時間（秒）。負値は0として扱う。
    /// @param home 帰還中に自機付近へ到着したか。帰還以外の段階では使わない。
    /// @return この呼び出しで再構築が完了し、護衛へ戻った場合だけtrue。
    bool Step(float dt, bool home = false)
    {
        // 1回の呼び出しでは現在段階だけを更新する。遷移時に経過時間を戻し、超過時間を次段階へ持ち越さない。
        elapsed_ += (std::max)(0.0f, dt);
        const State& state = GetState(phase_);
        return state.Update(*this, home);
    }
    /// @brief 再構築中でなければtrueを返す。新しい任務を開始できるかはStartが別に判定する。
    bool Available() const
    {
        return phase_ != DronePhase::Rebuilding;
    }

private:
    /// @brief 任務の段階ごとの遷移判定と、再構築完了の通知を定義する。
    class State {
    public:
        /// @brief 派生した状態型を基底型経由で破棄できるようにする。
        virtual ~State() = default;
        /// @brief Stepで進めた経過時間を使って遷移を判定し、再構築完了時だけtrueを返す。
        virtual bool Update(DroneMission& mission, bool home) const = 0;
    };
    /// @brief 次の任務開始を待つ護衛状態を表す。
    class EscortState final : public State {
    public:
        /// @brief 護衛状態を保ち、falseを返す。
        bool Update(DroneMission&, bool) const override
        {
            return false;
        }
    };
    /// @brief 突撃開始までの予告状態を表す。
    class WarningState final : public State {
    public:
        /// @brief 予告時間が終われば突撃へ移る。戻り値はfalse。
        bool Update(DroneMission& mission, bool) const override
        {
            if (mission.elapsed_ >= mission.GetWarningDuration())
                mission.Enter(DronePhase::Charging);
            return false;
        }
    };
    /// @brief 到着通知または時間切れを待つ突撃状態を表す。
    class ChargingState final : public State {
    public:
        /// @brief 突撃が時間切れなら到着イベントを作らず帰還へ移る。戻り値はfalse。
        bool Update(DroneMission& mission, bool) const override
        {
            if (mission.elapsed_ >= kChargingTimeoutSeconds)
                mission.Enter(DronePhase::Returning);
            return false;
        }
    };
    /// @brief 自機への帰還状態を表す。
    class ReturningState final : public State {
    public:
        /// @brief homeがtrue、または帰還が時間切れなら護衛へ移る。戻り値はfalse。
        bool Update(DroneMission& mission, bool home) const override
        {
            if (home || mission.elapsed_ >= kReturnTimeoutSeconds)
                mission.Enter(DronePhase::Escort);
            return false;
        }
    };
    /// @brief 自爆突撃後、再使用できるまでの再構築状態を表す。
    class RebuildingState final : public State {
    public:
        /// @brief 再構築時間が終われば護衛へ移ってtrueを返す。それまではfalse。
        bool Update(DroneMission& mission, bool) const override
        {
            if (mission.elapsed_ < kRebuildSeconds)
                return false;
            mission.Enter(DronePhase::Escort);
            return true;
        }
    };
    /// @brief 段階に対応する共有の状態オブジェクトを返す。範囲外の値は護衛状態として扱う。
    static const State& GetState(DronePhase phase)
    {
        static const EscortState escort;
        static const WarningState warning;
        static const ChargingState charging;
        static const ReturningState returning;
        static const RebuildingState rebuilding;
        static const std::array<const State*, 5> states{&escort, &warning, &charging, &returning, &rebuilding};
        static_assert(states.size() == static_cast<std::size_t>(DronePhase::Rebuilding) + 1);
        const auto index = static_cast<std::size_t>(phase);
        return *states[index < states.size() ? index : 0];
    }
    /// @brief 指定した段階へ移り、段階内の経過時間だけを0に戻す。
    void Enter(DronePhase next)
    {
        phase_ = next;
        elapsed_ = 0;
    }
    DronePhase phase_ = DronePhase::Escort;
    float elapsed_ = 0;
    bool bomb_ = false, impact_ = false;
};
/// @brief 対象の衝突ID、命中したドローンのビット集合、命中数、蓄積・ロックの残り秒数を保持する。
struct PainterLock {
    uint64_t id = 0;
    uint32_t droneMask = 0;
    int hits = 0;
    float remaining = 0, buildup = 0;
    /// @brief 蓄積とロックの残り時間を減らし、両方が切れたら命中履歴を解除する。idは保持する。
    /// @param dt 経過時間（秒）。呼び出し側で非負の値を渡す。
    void Advance(float dt)
    {
        remaining = (std::max)(0.0f, remaining - dt);
        buildup = (std::max)(0.0f, buildup - dt);
        if (buildup == 0 && remaining == 0) {
            droneMask = 0;
            hits = 0;
        }
    }
    /// @brief ドローンの命中を蓄積し、2機以上から累計4回以上命中したときに4秒間ロックする。
    /// @param drone ドローンの添字（0～31）。範囲外、またはロック継続中なら変更しない。
    /// @return 今回の命中でロックが成立した場合true。蓄積だけの場合もfalse。
    bool Hit(int drone)
    {
        if (remaining > 0 || drone < 0 || drone >= 32)
            return false;
        droneMask |= uint32_t{1} << drone;
        ++hits;
        buildup = 2.0f;
        // 立っているビットを1つ落としてもビットが残るなら、2機以上から命中している。
        const bool distinct = (droneMask & (droneMask - 1)) != 0;
        if (distinct && hits >= 4) {
            remaining = 4.0f;
            return true;
        }
        return false;
    }
    /// @brief ロック中の対象へ掛けるダメージ倍率を返す。ロック外では1。
    /// @param boss ボス用の加算倍率を使うか。
    /// @param power 加算倍率に掛ける能力の強さ。この関数では範囲を制限しない。
    float DamageScale(bool boss, float power = 1) const
    {
        return remaining > 0 ? 1.0f + (boss ? .18f : .35f) * power : 1.0f;
    }
};
/// @brief 未使用の対象を優先して距離が最小の添字を選ぶ。全て使用済みなら使用済みから選ぶ。
/// @note countが正ならdistances・usedはcount要素以上を参照できる配列を渡す。配列は変更しない。
/// @return 選択した添字。同距離なら先の添字。countが0以下など、選べる距離がなければ-1。
inline int ChooseSpreadTarget(const float* distances, const bool* used, int count)
{
    int selected = -1;
    float best = 1e30f;
    // まず未使用の対象だけを比較する。選べなかった場合だけ使用済みも含めて比較し直す。
    for (int pass = 0; pass < 2 && selected < 0; ++pass)
        for (int i = 0; i < count; ++i) {
            if (pass == 0 && used[i])
                continue;
            if (distances[i] < best) {
                best = distances[i];
                selected = i;
            }
        }
    return selected;
}
/// @brief 回転攻撃の残り秒数と、最大4回の攻撃タイミングを管理する。
struct SpinCycle {
    float remaining = 0, nextTick = 0;
    int tickCount = 0;
    /// @brief 実行中でなく、入力継続・フィニッシュ後・スタミナ1以上の条件を満たせば、0.8秒の回転攻撃を開始する。
    /// @param stamina 現在のスタミナ。開始に成功した場合だけ1を消費する。
    /// @return 開始した場合true。条件を満たさなければ状態を変えずfalse。
    bool Start(bool held, bool afterFinisher, float& stamina)
    {
        if (remaining > 0 || !held || !afterFinisher || stamina < 1)
            return false;
        stamina -= 1;
        remaining = .8f;
        nextTick = 0;
        tickCount = 0;
        return true;
    }
    /// @brief 残り時間と攻撃間隔を更新し、攻撃タイミングなら回数を進める。
    /// @param dt 経過時間（秒）。呼び出し側で非負の値を渡す。
    /// @return この呼び出しで攻撃を1回行う場合true。1回の呼び出しで複数回分は返さない。
    bool Step(float dt)
    {
        if (remaining <= 0)
            return false;
        remaining = (std::max)(0.0f, remaining - dt);
        nextTick -= dt;
        if (nextTick <= 0 && tickCount < 4) {
            // 遅れを残したまま0.20秒を足す。大きなdtでも、この呼び出しでは1回分だけ通知する。
            nextTick += .20f;
            ++tickCount;
            return true;
        }
        return false;
    }
};
/// @brief 現在の残り秒数と、0～3秒に制限した要求秒数の大きい方を返す。現在値は制限しない。
inline float EmpDuration(float current, float requested)
{
    return (std::max)(current, (std::clamp)(requested, 0.0f, 3.0f));
}
/// @brief 能力が有効で、ダッシュ中または直近のダッシュ受付時間（recentDash、秒）が残っていればtrue。
inline bool CanDashSlash(bool enabled, bool dashing, float recentDash)
{
    return enabled && (dashing || recentDash > 0);
}
/// @brief 壁への体当たりで与えるダメージを計算する。
inline float WallSmashDamage(float melee, float power = 1)
{
    return (std::max)(1.0f, std::round(melee * 1.75f * power));
}
} // namespace tankspecial
