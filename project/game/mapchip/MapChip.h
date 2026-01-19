#pragma once
#include <Windows.h>
#include <vector>
#include "Struct.h"

enum class MapChipType {
	kBlank,  // 空白
	kBlock,  // ブロック
	kDamageBlock, // ダメージブロック
};

struct MapChipData {
	std::vector<std::vector<MapChipType>> data;
};

class MapChip {

public:

	/// <summary>
	/// マップチップデータをリセット
	/// </summary>
	void ResetMapChipData();

	/// <summary>
	/// csvファイルからマップを読み込む
	/// </summary>
	/// <param name="filePath"></param>
	void LoadMapChipCsv(const std::string& filePath);

	MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex);

	Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex);
	
	uint32_t GetNumBlockVirtical() { return kNumBlockVirtical; }
	uint32_t GetNumBlockHorizontal() { return kNumBlockHorizontal; }

	// 1ブロックのサイズ
	static inline const float kBlockWidth = 2.0f;
	static inline const float kBlockHeight = 2.0f;
	// ブロックの個数
	static inline const uint32_t kNumBlockVirtical = 20;
	static inline const uint32_t kNumBlockHorizontal = 30;

	MapChipData mapChipData_;
};
