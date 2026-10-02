#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "Object3dCommon.h"
#include <deque>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include "Struct.h"

// 1点分の剣の状態
namespace cg2 {

/// @brief 剣の軌跡を構成する1時点の根元・先端・経過時間を表す。
struct SwordSection {
    Vector3 tip;              // 先端の座標
    Vector3 base;             // 根元の座標
    float currentTime = 0.0f; // 追加：経過時間
};

/// @brief 軌跡の幅・色・寿命・補間・描画方法を指定する。
struct TrailConfig {
    Vector4 startColor = {1.0f, 1.0f, 1.0f, 1.0f}; // 出現時の色
    Vector4 endColor = {1.0f, 1.0f, 1.0f, 0.0f};   // 消える時の色
    uint32_t interpolationSteps = 4;               // 補間分割数
    uint32_t maxPoints = 50;                       // 軌跡の長さ
    float lifetime = 0.5f;                         // 各頂点の生存時間（秒）
    float startWidthScale = 1.0f;                  // 先頭側の太さ倍率
    float endWidthScale = 1.0f;                    // 尻尾側の太さ倍率
    float widthCurvePower = 1.0f;                  // 太さの減衰カーブ
    float colorCurvePower = 1.0f;                  // 色/透明度の減衰カーブ

    // JSON変換 (ModelParticleManagerと同様に)
    nlohmann::json ToJson() const
    {
        return nlohmann::json{{"startColor", {startColor.x, startColor.y, startColor.z, startColor.w}},
                              {"endColor", {endColor.x, endColor.y, endColor.z, endColor.w}},
                              {"interpolationSteps", interpolationSteps},
                              {"maxPoints", maxPoints},
                              {"lifetime", lifetime},
                              {"startWidthScale", startWidthScale},
                              {"endWidthScale", endWidthScale},
                              {"widthCurvePower", widthCurvePower},
                              {"colorCurvePower", colorCurvePower}};
    }

    /// @brief JSONの値を対応する設定項目へ読み取る。省略時の扱いは各項目の既定値に従う。
    void FromJson(const nlohmann::json& j)
    {
        if (j.contains("startColor")) {
            startColor = {j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3]};
        }
        if (j.contains("endColor")) {
            endColor = {j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3]};
        }
        interpolationSteps = j.value("interpolationSteps", interpolationSteps);
        maxPoints = j.value("maxPoints", maxPoints);
        lifetime = j.value("lifetime", lifetime);
        startWidthScale = j.value("startWidthScale", startWidthScale);
        endWidthScale = j.value("endWidthScale", endWidthScale);
        widthCurvePower = j.value("widthCurvePower", widthCurvePower);
        colorCurvePower = j.value("colorCurvePower", colorCurvePower);
    }
};

/// @brief 1本の軌跡の履歴を保持する。GPUへの転送と描画はTrailManagerが担当する。
class TrailInstance {
public:
    // 武器の行列などから位置を更新
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime, const Vector3& tipPos, const Vector3& basePos, const TrailConfig& config);

    // ゲッター
    const std::deque<SwordSection>& GetPoints() const
    {
        return points_;
    }
    /// @brief 設定を返す。
    const TrailConfig& GetConfig() const
    {
        return config_;
    }
    /// @brief 設定を設定する。
    void SetConfig(const TrailConfig& config);
    /// @brief 保持している要素を消去する。
    void Clear()
    {
        if (!points_.empty()) {
            points_.clear();
            ++geometryRevision_;
        }
    }
    /// @brief 形状Revisionを返す。
    uint64_t GetGeometryRevision() const
    {
        return geometryRevision_;
    }

    /// @brief 有効を設定する。
    void SetActive(bool active)
    {
        isActive_ = active;
    }
    /// @brief 有効であるか判定する。
    bool IsActive() const
    {
        return isActive_;
    }
    /// @brief 状態Permanentを設定する。
    void SetIsPermanent(bool permanent)
    {
        isPermanent_ = permanent;
    }
    /// @brief Permanentであるか判定する。
    bool IsPermanent() const
    {
        return isPermanent_;
    }

private:
    std::deque<SwordSection> points_;
    TrailConfig config_;
    uint64_t geometryRevision_ = 1;
    bool isActive_ = true;
    bool isPermanent_ = false; // 追加：trueなら中身が空でも削除しない
};

} // namespace cg2
