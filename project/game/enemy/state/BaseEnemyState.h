#pragma once
class Enemy;

class BaseEnemyState {
public:
	virtual ~BaseEnemyState() = default;
	// 状態ごとの初期化
	virtual void Initialize(Enemy& enemy) = 0;
	// 状態ごとの更新処理
	virtual void Update(Enemy& enemy) = 0;
};
