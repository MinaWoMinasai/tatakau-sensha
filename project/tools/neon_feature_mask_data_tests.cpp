// Production LinearData decode/mipmap API flags, tested with actual authored PNGs and independent Pillow bytes.
#include <Windows.h>
#include "DirectXTex.h"
#include <array>
#include <cmath>
#include <cstdint>
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
void Check(HRESULT result, const char* message) {
    Require(SUCCEEDED(result), message);
}
std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    Require(stream.good(), "PNG / Pillow golden file not found");
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}
bool IsLinearByteFormat(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM
        || format == DXGI_FORMAT_B8G8R8X8_UNORM;
}
std::array<uint8_t, 4> LogicalRgba(const DirectX::Image& image, size_t x, size_t y) {
    const auto* pixel = image.pixels + y * image.rowPitch + x * 4;
    if (image.format == DXGI_FORMAT_R8G8B8A8_UNORM) return { pixel[0], pixel[1], pixel[2], pixel[3] };
    return { pixel[2], pixel[1], pixel[0], image.format == DXGI_FORMAT_B8G8R8X8_UNORM ? uint8_t{255} : pixel[3] };
}
void ValidateMask(const std::filesystem::path& pngPath, const std::filesystem::path& goldenPath,
    bool runtimeCandidate, bool constantFixture = false, bool validateMips = true) {
    const auto png = ReadBytes(pngPath);
    const auto golden = ReadBytes(goldenPath);
    DirectX::ScratchImage decoded;
    Check(DirectX::LoadFromWICMemory(png.data(), png.size(), DirectX::WIC_FLAGS_NONE, nullptr, decoded),
        "LinearData WIC decode failed");
    const auto& metadata = decoded.GetMetadata();
    std::wcout << pngPath.filename().wstring() << L": decoded DXGI_FORMAT=" << metadata.format
        << L", " << metadata.width << L"x" << metadata.height << L"\n";
    Require(!DirectX::IsSRGB(metadata.format) && IsLinearByteFormat(metadata.format),
        "Authored masks must decode to linear UNORM RGB/BGRA/BGRX, not SRGB");
    Require(metadata.dimension == DirectX::TEX_DIMENSION_TEXTURE2D && metadata.arraySize == 1,
        "Masks must be a single Texture2D");
    Require(golden.size() == metadata.width * metadata.height * 4, "Pillow golden dimensions mismatch");
    if (runtimeCandidate) Require(metadata.width == 1024 && metadata.height == 1024,
        "Runtime masks must retain the bounded 1024-square candidate resolution");
    const auto* base = decoded.GetImage(0, 0, 0);
    Require(base != nullptr, "Decoded base image missing");
    size_t intermediateR = 0, intermediateG = 0, erasePixels = 0, coveredPixels = 0;
    double sums[2]{};
    for (size_t y = 0; y < metadata.height; ++y) for (size_t x = 0; x < metadata.width; ++x) {
        const auto actual = LogicalRgba(*base, x, y);
        const auto* expected = golden.data() + (y * metadata.width + x) * 4;
        for (size_t channel = 0; channel < 4; ++channel)
            Require(actual[channel] == expected[channel], "WIC logical RGBA differs from Pillow input (orientation / gamma / premultiplication)");
        if (runtimeCandidate) Require(actual[2] == 0 && actual[3] == 255,
            "Runtime mask convention must preserve B=0 and A=255");
        intermediateR += actual[0] != 0 && actual[0] != 255;
        intermediateG += actual[1] != 0 && actual[1] != 255;
        erasePixels += actual[0] == 0 && actual[1] == 255;
        coveredPixels += actual[0] > 0 && actual[1] > 0;
        sums[0] += actual[0]; sums[1] += actual[1];
    }
    if (runtimeCandidate) Require(intermediateR > 0 && intermediateG > 0 && erasePixels > 0 && coveredPixels > 0,
        "Candidates must contain antialiased coverage / replacement, authored lines, and erase-only regions");
    if (!validateMips) {
        // Production candidates mandate A=255. Existing WIC mip filtering for A<255 can weight / quantize RGB;
        // the A64 fixture verifies decode only, and does not claim arbitrary-alpha mip preservation.
        std::cout << "PASS: A64 fixture decoded logical RGBA matches input exactly (no load-time premultiplication).\n"
            << "NOTE: A<255 mip invariance is not tested / promised; authored runtime masks require A=255.\n";
        return;
    }
    DirectX::ScratchImage mips;
    Check(DirectX::GenerateMipMaps(decoded.GetImages(), decoded.GetImageCount(), metadata,
        DirectX::TEX_FILTER_DEFAULT, 0, mips), "LinearData mip generation failed");
    Require(mips.GetMetadata().format == metadata.format && !DirectX::IsSRGB(mips.GetMetadata().format),
        "Mipmap generation must retain linear data format");
    Require(mips.GetMetadata().mipLevels > 1, "Test requires a complete mip chain");
    const auto first = LogicalRgba(*base, 0, 0);
    for (size_t mip = 0; mip < mips.GetMetadata().mipLevels; ++mip) {
        const auto* image = mips.GetImage(mip, 0, 0);
        Require(image != nullptr, "Mip image missing");
        for (size_t y = 0; y < image->height; ++y) for (size_t x = 0; x < image->width; ++x) {
            const auto actual = LogicalRgba(*image, x, y);
            if (mip == 0) {
                const auto* expected = golden.data() + (y * image->width + x) * 4;
                for (size_t channel = 0; channel < 4; ++channel)
                    Require(actual[channel] == expected[channel], "Mip generation must not modify base coverage data");
            }
            if (runtimeCandidate) Require(actual[2] == 0 && actual[3] == 255, "Mip chain must retain unused B / A values");
            if (constantFixture) Require(actual == first,
                "Constant mid-value coverage with A255 must survive every mip without gamma or premultiplication");
        }
    }
    const auto* last = mips.GetImage(mips.GetMetadata().mipLevels - 1, 0, 0);
    Require(last->width == 1 && last->height == 1, "Mip chain must reach 1x1");
    const auto mean = LogicalRgba(*last, 0, 0);
    for (size_t channel = 0; channel < 2; ++channel)
        Require(std::abs(mean[channel] - sums[channel] / (metadata.width * metadata.height)) <= 3,
            "Final RG mip must represent linear coverage averages rather than SRGB color averages");
    std::cout << "PASS: logical RGBA exactly equals independent input; linear format / MIPs, no alpha premultiplication; "
        << mips.GetMetadata().mipLevels << " mips, fractional R=" << intermediateR << ", G=" << intermediateG
        << ", erase-only=" << erasePixels << ".\n";
}
}

int wmain(int argc, wchar_t** argv) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try {
        Require(argc == 3, "Usage: neon_feature_mask_data_tests <runtime mask directory> <Pillow fixtures directory>");
        Check(com, "COM initialization failed");
        const std::filesystem::path runtime = argv[1], fixtures = argv[2];
        for (const auto* name : { L"face_candidate", L"bangs_candidate" })
            ValidateMask(runtime / (std::wstring(name) + L".png"), fixtures / (std::wstring(name) + L".rgba"), true);
        for (const auto* name : { L"midvalue_a255", L"linear_checker" })
            ValidateMask(fixtures / (std::wstring(name) + L".png"), fixtures / (std::wstring(name) + L".rgba"),
                false, std::wstring(name) != L"linear_checker");
        ValidateMask(fixtures / L"midvalue_a64.png", fixtures / L"midvalue_a64.rgba", false, false, false);
        CoUninitialize();
        std::cout << "PASS: production WIC_FLAGS_NONE / TEX_FILTER_DEFAULT LinearData PNG path with authored assets and independent midvalue fixtures.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
}
