#pragma once
#include <Windows.h>
#include <mmreg.h>
#include <cstdint>

namespace myEngine {

/// @brief RIFFチャンクの種類とデータサイズを表す。
struct ChunkHeader {
    char id[4];   // チャンク毎のID
    int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
/// @brief RIFF/WAVEファイルの先頭識別子とサイズを表す。
struct RiffHeader {
    ChunkHeader chunk; // "RIFF"
    char type[4];      // "WAVE"
};

// FMTチャンク
/// @brief WAVEの音声フォーマット情報を表す。
struct FormatChunk {
    ChunkHeader chunk; // "fmt"
    WAVEFORMATEX fmt;  // 波形フォーマット
};

} // namespace myEngine
