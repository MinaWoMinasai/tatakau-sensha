// Actual production WIC_FLAGS_NONE / TEX_FILTER_DEFAULT, against independent Pillow logical RGBA.
#include <Windows.h>
#include "DirectXTex.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Check(HRESULT result, const char* message) { Require(SUCCEEDED(result), message); }
std::vector<uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    Require(stream.good(), "Quality PNG / golden / distance reference is missing");
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}
std::array<uint8_t, 4> Rgba(const DirectX::Image& image, size_t x, size_t y) {
    const auto* p = image.pixels + y * image.rowPitch + x * 4;
    if (image.format == DXGI_FORMAT_R8G8B8A8_UNORM) return {p[0], p[1], p[2], p[3]};
    return {p[2], p[1], p[0], image.format == DXGI_FORMAT_B8G8R8X8_UNORM ? uint8_t{255} : p[3]};
}
void Validate(const std::filesystem::path& source, const std::filesystem::path& fixtures,
    const std::wstring& name, bool runtime, bool sdf, bool constant = false) {
    const auto png = Read(source / (name + L".png"));
    const auto golden = Read(fixtures / (name + L".rgba"));
    std::vector<float> distance;
    if (sdf) {
        const auto bytes = Read(fixtures / (name + L".distance.f32"));
        Require(bytes.size() == golden.size(), "Signed-distance reference dimensions mismatch");
        distance.resize(bytes.size() / sizeof(float));
        memcpy(distance.data(), bytes.data(), bytes.size());
    }
    DirectX::ScratchImage decoded;
    Check(DirectX::LoadFromWICMemory(png.data(), png.size(), DirectX::WIC_FLAGS_NONE, nullptr, decoded), "LinearData decode failed");
    const auto& metadata = decoded.GetMetadata();
    Require(!DirectX::IsSRGB(metadata.format) && (metadata.format == DXGI_FORMAT_R8G8B8A8_UNORM
        || metadata.format == DXGI_FORMAT_B8G8R8A8_UNORM || metadata.format == DXGI_FORMAT_B8G8R8X8_UNORM),
        "Quality data must retain a linear byte texture format");
    Require(metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D && metadata.arraySize == 1,
        "Expected one Texture2D");
    Require(golden.size() == metadata.width * metadata.height * 4, "Pillow golden dimensions mismatch");
    if (runtime) Require(metadata.width == 1024 && metadata.height == 1024, "Unexpected asset resolution");
    const auto* base = decoded.GetImage(0, 0, 0);
    Require(base != nullptr, "Base image missing");
    std::array<double, 3> sums{};
    size_t positiveDistance = 0, eraseOnly = 0, haloPixels = 0, fractionalChannels = 0;
    double maxError = 0;
    for (size_t y = 0; y < metadata.height; ++y) for (size_t x = 0; x < metadata.width; ++x) {
        const auto actual = Rgba(*base, x, y);
        const auto offset = (y * metadata.width + x) * 4;
        for (size_t c = 0; c < 4; ++c) Require(actual[c] == golden[offset + c], "WIC decoded channels differ from Pillow (UV/gamma/premultiplication)");
        Require(actual[3] == 255, "Quality PNG must retain opaque data alpha");
        for (size_t c = 0; c < 3; ++c) {
            sums[c] += actual[c];
            fractionalChannels += actual[c] != 0 && actual[c] != 255;
        }
        if (sdf) {
            const auto expected = distance[y * metadata.width + x];
            Require(std::isfinite(expected) && expected >= -16.00001f && expected <= 16.00001f,
                "Reference distance is not finite or violates its declared texel range");
            const double value = (actual[0] / 255.0 - .5) * 32.0;
            maxError = (std::max)(maxError, std::abs(value - expected));
            Require(std::abs(value - expected) <= 16.0 / 255.0 + 1e-5,
                "Actual LinearData signed distance violates its declared half-step precision");
            positiveDistance += actual[0] > 128;
        } else {
            eraseOnly += actual[0] == 0 && actual[1] == 255;
            haloPixels += actual[2] > 0;
        }
    }
    if (runtime) Require(fractionalChannels > 0, "Expected fractional AA/data channel values");
    if (runtime && sdf) Require(positiveDistance > 0, "Signed-distance geometry vanished");
    if (runtime && !sdf) Require(eraseOnly > 0 && haloPixels > 0, "Coverage must retain erase-only G and an independent halo");
    DirectX::ScratchImage mips;
    Check(DirectX::GenerateMipMaps(decoded.GetImages(), decoded.GetImageCount(), metadata, DirectX::TEX_FILTER_DEFAULT, 0, mips),
        "Production LinearData mip generation failed");
    Require(mips.GetMetadata().format == metadata.format && !DirectX::IsSRGB(mips.GetMetadata().format), "Mips changed linear format");
    if (runtime) Require(mips.GetMetadata().mipLevels == 11, "1024 data requires eleven ordinary mips");
    const auto first = Rgba(*base, 0, 0);
    for (size_t level = 0; level < mips.GetMetadata().mipLevels; ++level) {
        const auto* image = mips.GetImage(level, 0, 0);
        Require(image != nullptr, "Mip image missing");
        for (size_t y = 0; y < image->height; ++y) for (size_t x = 0; x < image->width; ++x) {
            const auto actual = Rgba(*image, x, y);
            Require(actual[3] == 255, "Generated mip alpha changed");
            if (level == 0) {
                const auto offset = (y * image->width + x) * 4;
                for (size_t c = 0; c < 4; ++c) Require(actual[c] == golden[offset + c], "Mipmap generation changed LOD0 data");
            }
            if (constant) Require(actual == first, "Linear constant RGB values changed with mip/gamma/alpha weighting");
        }
    }
    const auto* last = mips.GetImage(mips.GetMetadata().mipLevels - 1, 0, 0);
    Require(last->width == 1 && last->height == 1, "Mip chain does not reach 1x1");
    const auto finalMean = Rgba(*last, 0, 0);
    for (size_t c = 0; c < 3; ++c)
        Require(std::abs(finalMean[c] - sums[c] / (metadata.width * metadata.height)) <= 3,
            "Ordinary RGB mip must preserve linear mean rather than sRGB mean");
    if (runtime && sdf) {
        Require(finalMean[0] < 128, "Bounded line atlas fixture should expose why averaged distance cannot preserve tiny geometry");
        std::cout << "NOTE: Ordinary averaged SDF mips are allocated by TextureManager but are not valid shape reconstruction; shader uses LOD0 distance and minification coverage fallback.\n";
    }
    std::wcout << L"PASS: " << name << L", DXGI=" << metadata.format << L", mips=" << mips.GetMetadata().mipLevels
        << L", independent RGBA exact, A255 / RGB linear means, max distance error=" << maxError << L" texels\n";
}
}

int wmain(int argc, wchar_t** argv) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try {
        Require(argc == 3, "Usage: neon_quality_mask_data_tests <quality directory> <Pillow fixtures>");
        Check(com, "COM initialization failed");
        const std::filesystem::path source = argv[1], fixtures = argv[2];
        for (const auto* role : {L"face", L"bangs"}) for (const auto* version : {L"v1", L"v2"}) {
            const auto stem = std::wstring(role) + L"_" + version;
            Validate(source, fixtures, stem + L"_coverage", true, false);
            Validate(source, fixtures, stem + L"_sdf", true, true);
        }
        Validate(fixtures, fixtures, L"linear_midvalues", false, false, true);
        Validate(fixtures, fixtures, L"linear_checker", false, false);
        CoUninitialize();
        std::cout << "PASS: all eight quality PNGs use actual production WIC / MIP API flags; SDF LOD0 precision and coverage fallback inputs validated.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
}
