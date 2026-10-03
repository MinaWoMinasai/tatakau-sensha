#pragma once
#include <cstdint>

// ゲームの戦闘・シーン用の型。再利用する描画エンジンとは分けて保持する。
// シーンのフェードイン・通常進行・フェードアウトの区分。
enum Phase {
    kFadeIn,
    kMain,
    kFadeOut,
};

class Player;
/// @brief 弾の発射数・拡散・速度・耐久度・ダメージと、命中元や追加効果の設定をまとめる。
/// 発射位置と方向はメンバーに含まず、AttackControllerの呼び出しへ別に渡す。
struct AttackParam {
    // 負値（既定は-1）はドローン弾以外。自機弾の生成時だけBulletへ設定する。
    int sourceDroneIndex = -1;
    // 命中元の自機を借用する。nullptrを許容し、弾の衝突通知で参照する間は有効に保つ。所有権は移らない。
    Player* sourcePlayer = nullptr;
    // 連鎖・マーキング／起爆・帰還・撃破時破裂の有効設定と強さ。AttackControllerは自機弾へだけ渡す。
    // フラグだけで効果の発動を保証しない。子弾・ドローン弾の通知除外、対象・撃破などの条件は各処理で判定する。
    bool shooterChain = false, shooterMark = false, shooterBoomerang = false, shooterKillBurst = false;
    float shooterChainPower = 1, shooterMarkPower = 1, shooterBoomerangPower = 1, shooterKillBurstPower = 1;
    // 自機弾の衝突半径と軌跡幅の倍率。Bullet::ConfigureVisualScaleでそれぞれ0.5～2に制限する。
    float bulletVisualScale = 1, bulletTrailScale = 1;
    // 正規化した発射方向へ掛ける、60FPS相当の基準1フレーム当たりの移動量。0は静止弾、負値は逆向きになる。
    // AttackControllerは非有限値を拒否するが、負値は制限しない。
    float bulletSpeed = 0.0f;
    // 1回の発射で生成を試みる弾数。0以下なら生成しない。管理側の上限により登録されない場合もある。
    int bulletCount = 1;
    // 拡散範囲の全幅（度）。均等配置か範囲内のランダム配置を選ぶ。AttackControllerは角度範囲を制限しない。
    float spreadAngleDeg = 0.0f;
    bool randomSpread = false;

    // 壁反射の許可。実際に反射するには残り反射回数が0以外で、有効な法線を取得できる必要がある。
    bool reflect = false;
    // 現行のAttackControllerでは参照しない。アクターを貫通できる回数はactorPierceCountで別に指定する。
    bool penetrate = false;

    // 現行のAttackControllerでは参照しない。発射間隔は呼び出し側の時計で管理する。
    float cooldown = 0.0f;

    // アクターへ与える整数ダメージ。弾同士の耐久度計算に使うbulletPenetrationとは別。
    uint32_t damage = 0;
    // 弾の耐久度と、他の弾の耐久度へ与える値。AttackControllerでの生成時は、正値でなければdamageから最低1を補う。
    float bulletHp = 0.0f;
    float bulletPenetration = 0.0f;
    // 資源を取得する資格の有無。trueでも対象・所有者・撃破などの条件は別に判定する。
    // 中立のシューター弾はfalseにし、相手側の共有資源を取得させない。
    bool canClaimRunResource = true;
    // 壁反射回数の-1は無制限、0は反射なし。reflectの許可とは別にBulletで-1～32へ制限する。
    // 非負の反射回数、正の貫通／分裂回数や射撃強化は遠征の弾数上限を有効にする。既定設定だけでは有効にしない。
    int maxWallBounces = -1;
    // 帰還能力のない弾が命中後も進み続けられるアクター数（0～8へ制限）。同じ衝突IDへの重複命中は弾の履歴で防ぐ。
    // 帰還能力付き弾はこの回数を消費せず、別の経路で飛行を続ける。
    int actorPierceCount = 0;
    // 命中・壁衝突時に予約する分裂子弾数（0～2へ制限）。元弾からの分裂予約は1回、子弾は再分裂しない設定で生成する。
    int impactSplitCount = 0;
    // 分裂子弾のダメージ倍率。Bulletで有限値は0.1～0.95、非有限値は既定の0.55へ補正する。
    float impactSplitDamageScale = 0.55f;
};

// 弾の陣営。kEnemyにはボス以外の敵の弾も含む。kExpEnemyHostileはボスだけを対象にする経験値敵の弾。
enum BulletOwner {
    kPlayer,
    kEnemy,
    kExpEnemyHostile
};
