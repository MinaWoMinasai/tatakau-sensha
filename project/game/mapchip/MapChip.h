#pragma once
#include <Windows.h>
#include <vector>
#include "Struct.h"

enum class MapChipType {
    kBlank,       // 空白
    kBlock,       // ブロック
    kDamageBlock, // ダメージブロック
};

/// @brief マップチップの種類と配置を保持する。
struct MapChipData {
    std::vector<std::vector<MapChipType>> data;
};

/// @brief チップ座標とワールド座標を対応させ、地形の参照を提供する。
class MapChip {

public:
    /// @brief マップチップデータをリセット
    void ResetMapChipData();

    /// @brief csvファイルからマップを読み込む
    void LoadMapChipCsv(const std::string& filePath);

    /// @brief マップChip種類By添字を返す。
    MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex);

    /// @brief マップChip位置By添字を返す。
    cg2::Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex);

    /// @brief 個数地形ブロック縦を返す。
    uint32_t GetNumBlockVirtical()
    {
        return kNumBlockVirtical;
    }
    /// @brief 個数地形ブロック横を返す。
    uint32_t GetNumBlockHorizontal()
    {
        return kNumBlockHorizontal;
    }

    // 1ブロックのサイズ
    static inline const float kBlockWidth = 2.0f;
    static inline const float kBlockHeight = 2.0f;
    // ブロックの個数
    static inline const uint32_t kNumBlockVirtical = 30;
    static inline const uint32_t kNumBlockHorizontal = 45;

    MapChipData mapChipData_;
};
