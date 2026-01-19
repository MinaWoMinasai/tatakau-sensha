#pragma once
#include <cstdint>

// プレイヤー陣営
const uint32_t kCollisionAttributePlayer = 1 << 0;
// 敵陣営
const uint32_t kCollisionAttributeEnemy = 1 << 1;
// 敵の弾陣営
const uint32_t kCollisionAttributeEnemyBullet = 1 << 2;
// プレイヤー弾陣営
const uint32_t kCollisionAttributePlayerBullet = 1 << 3;
// プレイヤードローン陣営
const uint32_t kCollisionAttributePlayerDrone = 1 << 4;