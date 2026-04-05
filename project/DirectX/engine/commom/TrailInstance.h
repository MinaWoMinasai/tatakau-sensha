//#pragma once
//#include "DirectXCommon.h"
//#include "TextureManager.h"
//#include "Object3dCommon.h"
//#include <deque>
//#include <nlohmann/json.hpp>
//#include <fstream>
//#include <iomanip>
//
//// 1点分の剣の状態
//struct SwordSection {
//    Vector3 tip;  // 先端の座標
//    Vector3 base; // 根元の座標
//};
//
//struct TrailVertex {
//    Vector3 pos;   // POSITION
//    Vector4 color; // COLOR (ここを毎フレーム変えてフェードアウトさせる)
//    Vector2 uv;    // TEXCOORD
//};
//
//struct TrailConfig {
//    Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // 出現時の色
//    Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };   // 消える時の色
//    uint32_t interpolationSteps = 4;                 // 補間分割数
//    uint32_t maxPoints = 50;                         // 軌跡の長さ
//
//    // JSON変換 (ModelParticleManagerと同様に)
//    nlohmann::json ToJson() const {
//        return nlohmann::json{
//            {"startColor", {startColor.x, startColor.y, startColor.z, startColor.w}},
//            {"endColor", {endColor.x, endColor.y, endColor.z, endColor.w}},
//            {"interpolationSteps", interpolationSteps},
//            {"maxPoints", maxPoints}
//        };
//    }
//
//    void FromJson(const nlohmann::json& j) {
//        if (j.contains("startColor")) {
//            startColor = { j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3] };
//        }
//        if (j.contains("endColor")) {
//            endColor = { j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3] };
//        }
//        interpolationSteps = j.value("interpolationSteps", interpolationSteps);
//        maxPoints = j.value("maxPoints", maxPoints);
//    }
//};
//
//class TrailInstance {
//public:
//    // 武器の行列などから位置を更新
//    void Update(const Vector3& tipPos, const Vector3& basePos, const TrailConfig& config);
//
//    // ゲッター
//    const std::deque<SwordSection>& GetPoints() const { return points_; }
//    const TrailConfig& GetConfig() const { return config_; }
//    void SetConfig(const TrailConfig& config) { config_ = config; }
//    void Clear() { points_.clear(); }
//
//    void SetActive(bool active) { isActive_ = active; }
//    bool IsActive() const { return isActive_; }
//
//private:
//    std::deque<SwordSection> points_;
//    TrailConfig config_;
//    bool isActive_ = true;
//};
