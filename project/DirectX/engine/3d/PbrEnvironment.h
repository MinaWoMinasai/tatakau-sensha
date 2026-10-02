#pragma once

#include <cstdint>
#include <string>

#include "Struct.h"
#include "TextureManager.h"

namespace cg2 {

/// @brief PBR用の環境マップ・拡散反射・鏡面反射・BRDFテクスチャを管理する。
class PbrEnvironment {
public:
    enum class SourceType {
        ProceduralSky,
        TextureFile,
        SolidColor,
    };

    /// @brief cg2::PbrEnvironmentで使う設定値をまとめる。生成・更新される実行状態とは分けて扱う。
    struct Settings {
        SourceType sourceType = SourceType::ProceduralSky;
        std::string texturePath;
        Vector3 solidColor = {0.74f, 0.78f, 0.82f};
    };

    /// @brief 保持している設定や状態を指定対象へ反映する。
    bool Apply(const Settings& settings)
    {
        if (applied_ && IsSameSettings(settings_, settings)) {
            return loadedFromSource_;
        }

        TextureManager* textureManager = TextureManager::GetInstance();
        bool loadedFromSource = false;
        switch (settings.sourceType) {
        case SourceType::TextureFile:
            loadedFromSource = textureManager->CreatePbrEnvironmentTexturesFromCubeMap(settings.texturePath);
            sourceLabel_ = loadedFromSource ? "texture convolution" : "texture rejected, procedural fallback";
            break;
        case SourceType::SolidColor:
            textureManager->CreatePbrSolidColorEnvironmentTextures(settings.solidColor.x, settings.solidColor.y, settings.solidColor.z);
            sourceLabel_ = "solid color";
            break;
        case SourceType::ProceduralSky:
        default:
            textureManager->CreatePbrEnvironmentTexturesFromCubeMap("");
            sourceLabel_ = "procedural clean sky";
            break;
        }

        environmentSrvIndex_ =
            textureManager->GetSrvIndex(TextureManager::GetPbrEnvironmentTexturePath(), TextureManager::TextureColorSpace::LinearData);
        settings_ = settings;
        loadedFromSource_ = loadedFromSource;
        applied_ = true;
        return loadedFromSource_;
    }

    /// @brief 環境SRV添字を返す。
    uint32_t GetEnvironmentSrvIndex() const
    {
        return environmentSrvIndex_;
    }
    /// @brief Loadedからの元データであるか判定する。
    bool IsLoadedFromSource() const
    {
        return loadedFromSource_;
    }
    /// @brief 元データ文字を返す。
    const std::string& GetSourceLabel() const
    {
        return sourceLabel_;
    }

private:
    /// @brief 同一設定であるか判定する。
    static bool IsSameSettings(const Settings& lhs, const Settings& rhs)
    {
        return lhs.sourceType == rhs.sourceType && lhs.texturePath == rhs.texturePath && lhs.solidColor.x == rhs.solidColor.x &&
               lhs.solidColor.y == rhs.solidColor.y && lhs.solidColor.z == rhs.solidColor.z;
    }

    Settings settings_;
    uint32_t environmentSrvIndex_ = 0;
    bool loadedFromSource_ = false;
    bool applied_ = false;
    std::string sourceLabel_ = "not applied";
};

} // namespace cg2
