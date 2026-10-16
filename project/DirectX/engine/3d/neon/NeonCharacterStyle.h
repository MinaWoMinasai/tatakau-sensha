#pragma once
#include "NeonSkinnedRenderer.h"
#include "SkinCluster.h"

namespace myEngine {

// プレビューで推奨とした線画の既存値をゲームプレイと共有する。テクスチャマスク候補は
// 明示的に有効化したときだけ使い、シーン全体のBloom設定は変更しない。
inline void ApplyRecommendedNeonCharacterStyle(NeonSkinnedParams& params, const SkinnedModel* model,
                                               std::vector<NeonSkinnedSubmeshParams>& surfaces)
{
    params.bodyColor = {0.018f, 0.0025f, 0.012f, 1.0f};
    params.bodyEmissionIntensity = 1.5f;
    params.outlineEnabled = 1;
    params.outlineWidthPixels = 1.15f;
    params.emissiveIntensity = 6.0f;
    params.emissiveColor = {1.0f, 0.025f, 0.35f};
    params.internalLineEnabled = 1;
    params.internalLineWidthPixels = 0.7f;
    params.internalLineIntensity = 5.0f;
    params.internalLineThreshold = 0.16f;
    params.rimStrength = 0.0f;
    params.geometryLineEnabled = 0;
    if (!model) {
        return;
    }

    struct MaterialSetting {
        const char* name;
        float strength;
        float thresholdScale;
    };

    constexpr MaterialSetting settings[] = {
        {"N00_000_00_FaceMouth_00_FACE (Instance)", 1.4f, 0.8f},    {"N00_000_00_EyeIris_00_EYE (Instance)", 1.15f, 1.0f},
        {"N00_000_00_EyeHighlight_00_EYE (Instance)", 0.45f, 1.0f}, {"N00_000_00_Face_00_SKIN (Instance)", 1.25f, 0.35f},
        {"N00_000_Hair_00_HAIR_01 (Instance)", 0.75f, 1.15f},       {"N00_000_00_Body_00_SKIN (Instance)", 0.12f, 1.8f},
        {"N00_005_01_Shoes_01_CLOTH (Instance)", 0.45f, 1.4f}};
    for (size_t index = 0; index < surfaces.size(); ++index) {
        auto& surface = surfaces[index];
        surface.lineStrength = 0.15f;
        surface.internalLineThresholdScale = 1.5f;
        surface.geometryLineStrength = 0.0f;
        for (const auto& setting : settings) {
            if (model->GetSubmesh(index).materialName == setting.name) {
                surface.lineStrength = setting.strength;
                surface.internalLineThresholdScale = setting.thresholdScale;
                break;
            }
        }
    }
}

} // namespace myEngine
