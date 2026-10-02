#pragma once

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include "externals/DirectXTex/DirectXTex.h"
#include "StartupTrace.h"

// Lossless CPU-generated PBR data, independent of the graphics adapter/driver.
// Bump the directory version whenever the procedural environment, integration
// algorithm, sample count or output layout changes. The payload is DDS, with a
// small integrity header so a truncated/corrupt cache always rebuilds safely.
namespace cg2 {

namespace GeneratedTextureCache {

/// @brief この機能が有効か判定する。
inline bool Enabled()
{
    char value[8]{};
    const DWORD length = GetEnvironmentVariableA("CG2_STARTUP_CACHE", value, sizeof(value));
    return length != 1 || value[0] != '0';
}

/// @brief この処理で使うファイルパスを返す。
inline std::filesystem::path Path(const wchar_t* name)
{
    wchar_t configured[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"CG2_TEXTURE_CACHE_DIR", configured, static_cast<DWORD>(_countof(configured)));
    const std::filesystem::path root =
        length > 0 && length < _countof(configured) ? std::filesystem::path(configured) : std::filesystem::path(L"generated/texture_cache");
    return root / L"v1" / (std::wstring(name) + L".dds.cache");
}

/// @brief hが存在するか判定する。
inline uint64_t Hash(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 14695981039346656037ull;
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

inline constexpr uint64_t kMagic = 0x3130305854473243ull;
inline constexpr uint64_t kMaxPayloadBytes = 8 * 1024 * 1024;

/// @brief 入力が照合条件に一致するか判定する。
inline bool Matches(const DirectX::TexMetadata& metadata, size_t size, size_t mipLevels, bool cube)
{
    return metadata.width == size && metadata.height == size && metadata.depth == 1 && metadata.mipLevels == mipLevels &&
           metadata.arraySize == (cube ? 6u : 1u) && metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D &&
           metadata.format == DXGI_FORMAT_R32G32B32A32_FLOAT && metadata.IsCubemap() == cube;
}

/// @brief 保存されたデータを読み込む。
inline bool Load(const wchar_t* name, size_t size, size_t mipLevels, bool cube, DirectX::ScratchImage& image)
{
    if (!Enabled())
        return false;
    StartupTrace::Scope scope("Textures.GeneratedCache.Load");
    try {
        std::ifstream file(Path(name), std::ios::binary | std::ios::ate);
        if (!file) {
            StartupTrace::Count("Textures.GeneratedCache.Miss");
            return false;
        }
        const auto fileSize = file.tellg();
        std::array<uint64_t, 3> header{};
        file.seekg(0);
        file.read(reinterpret_cast<char*>(header.data()), sizeof(header));
        if (!file || header[0] != kMagic || header[2] == 0 || header[2] > kMaxPayloadBytes ||
            fileSize != static_cast<std::streamoff>(sizeof(header) + header[2])) {
            StartupTrace::Count("Textures.GeneratedCache.Rejected");
            return false;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(header[2]));
        file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!file || Hash(bytes.data(), bytes.size()) != header[1]) {
            StartupTrace::Count("Textures.GeneratedCache.Rejected");
            return false;
        }
        DirectX::TexMetadata metadata{};
        if (FAILED(DirectX::GetMetadataFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, metadata)) ||
            !Matches(metadata, size, mipLevels, cube) ||
            FAILED(DirectX::LoadFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, image))) {
            StartupTrace::Count("Textures.GeneratedCache.Rejected");
            return false;
        }
        StartupTrace::Count("Textures.GeneratedCache.Hit");
        return true;
    }
    catch (...) {
        StartupTrace::Count("Textures.GeneratedCache.ReadFailure");
        return false;
    }
}

/// @brief 指定したデータを保存する。
inline void Store(const wchar_t* name, const DirectX::ScratchImage& image)
{
    if (!Enabled())
        return;
    StartupTrace::Scope scope("Textures.GeneratedCache.Store");
    std::filesystem::path temporary;
    try {
        DirectX::Blob dds;
        if (FAILED(DirectX::SaveToDDSMemory(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::DDS_FLAGS_NONE, dds)) ||
            dds.GetBufferSize() > kMaxPayloadBytes)
            return;
        const auto path = Path(name);
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return;
        static std::atomic<uint64_t> sequence{0};
        temporary = path;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(sequence++) + L".tmp";
        const auto* bytes = static_cast<const uint8_t*>(dds.GetBufferPointer());
        const std::array<uint64_t, 3> header{kMagic, Hash(bytes, dds.GetBufferSize()), dds.GetBufferSize()};
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(header.data()), sizeof(header));
        file.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(dds.GetBufferSize()));
        file.close();
        if (file && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            StartupTrace::Count("Textures.GeneratedCache.Written");
            return;
        }
    }
    catch (...) {
    }
    if (!temporary.empty()) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
    }
    StartupTrace::Count("Textures.GeneratedCache.WriteFailure");
}

} // namespace GeneratedTextureCache

} // namespace cg2
