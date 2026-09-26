#include "GeneratedTextureCache.h"
#include <cassert>
#include <cstring>
#include <iostream>

void Fill(DirectX::ScratchImage& image) {
    for (size_t index = 0; index < image.GetImageCount(); ++index) {
        const auto& part = image.GetImages()[index];
        for (size_t row = 0; row < part.height; ++row) {
            auto* pixel = reinterpret_cast<float*>(part.pixels + row * part.rowPitch);
            for (size_t column = 0; column < part.width * 4; ++column)
                pixel[column] = static_cast<float>(index * 37 + row * 11 + column) * 0.03125f;
        }
    }
}

void Equal(const DirectX::ScratchImage& a, const DirectX::ScratchImage& b) {
    assert(a.GetImageCount() == b.GetImageCount());
    for (size_t index = 0; index < a.GetImageCount(); ++index) {
        const auto& left = a.GetImages()[index];
        const auto& right = b.GetImages()[index];
        assert(left.width == right.width && left.height == right.height);
        for (size_t row = 0; row < left.height; ++row)
            assert(std::memcmp(left.pixels + row * left.rowPitch,
                right.pixels + row * right.rowPitch, left.width * sizeof(float) * 4) == 0);
    }
}

int main() {
    SetEnvironmentVariableW(L"CG2_TEXTURE_CACHE_DIR", L"generated_texture_cache_fixture");
    SetEnvironmentVariableA("CG2_STARTUP_CACHE", "1");
    DirectX::ScratchImage source, loaded;
    assert(SUCCEEDED(source.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16, 1, 1)));
    Fill(source);
    GeneratedTextureCache::Store(L"roundtrip-2d", source);
    assert(GeneratedTextureCache::Load(L"roundtrip-2d", 16, 1, false, loaded));
    Equal(source, loaded);
    assert(!GeneratedTextureCache::Load(L"roundtrip-2d", 32, 1, false, loaded));
    assert(!GeneratedTextureCache::Load(L"roundtrip-2d", 16, 1, true, loaded));
    assert(!GeneratedTextureCache::Load(L"roundtrip-2d", 16, 2, false, loaded));

    assert(SUCCEEDED(source.InitializeCube(DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16, 1, 5)));
    Fill(source);
    GeneratedTextureCache::Store(L"roundtrip-cube", source);
    assert(GeneratedTextureCache::Load(L"roundtrip-cube", 16, 5, true, loaded));
    Equal(source, loaded);

    const auto cachePath = GeneratedTextureCache::Path(L"roundtrip-cube");
    {
        std::fstream file(cachePath, std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(-1, std::ios::end);
        char last{};
        file.read(&last, 1);
        last ^= 0x40;
        file.seekp(-1, std::ios::end);
        file.write(&last, 1);
    }
    assert(!GeneratedTextureCache::Load(L"roundtrip-cube", 16, 5, true, loaded));
    GeneratedTextureCache::Store(L"roundtrip-cube", source);
    assert(GeneratedTextureCache::Load(L"roundtrip-cube", 16, 5, true, loaded));
    Equal(source, loaded);
    { std::ofstream file(cachePath, std::ios::binary | std::ios::trunc); file << "truncated"; }
    assert(!GeneratedTextureCache::Load(L"roundtrip-cube", 16, 5, true, loaded));

    SetEnvironmentVariableA("CG2_STARTUP_CACHE", "0");
    assert(!GeneratedTextureCache::Load(L"roundtrip-2d", 16, 1, false, loaded));
    const auto before = std::filesystem::file_size(cachePath);
    GeneratedTextureCache::Store(L"roundtrip-cube", source);
    assert(std::filesystem::file_size(cachePath) == before);
    std::cout << "Generated texture cache: exact 2D/cube+mip roundtrip, metadata rejection, corruption/truncation recovery and bypass PASS\n";
}
