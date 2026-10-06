#pragma once
// Strict transactional encounter settings; missing optional fields use defaults.
#include "game/enemy/actor/NeonDepthCombat.h"
#include "game/enemy/visual/NeonDepthPlacement.h"
#include "game/enemy/visual/NeonDepthPresentation.h"
#include "externals/nlohmann/json.hpp"
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace neondepth {
struct Config {
    bool enabled = true;
    Tuning combat{};
    PresentationConfig visual{};
    CameraProfile camera{20.0f,90.0f,0.55f,1280.0f/720.0f,0.1f,5000.0f};
};

inline bool ReadConfig(const nlohmann::json& document, Config& output, std::string& error) {
    Config candidate;
    try {
        if (!document.is_object()) throw std::runtime_error("Depth settings must be an object.");
        if (document.contains("schemaVersion") &&
            (!document.at("schemaVersion").is_number_integer() || document.at("schemaVersion") != 1))
            throw std::runtime_error("Unsupported Depth settings schema.");
        auto keys = [](const nlohmann::json& value, std::initializer_list<const char*> allowed) {
            if (!value.is_object()) throw std::runtime_error("Depth settings section must be an object.");
            for (auto item = value.begin(); item != value.end(); ++item)
                if (std::none_of(allowed.begin(),allowed.end(),[&](const char* key) { return item.key() == key; }))
                    throw std::runtime_error("Unknown Depth settings field: " + item.key());
        };
        keys(document,{"schemaVersion","enabled","camera","visual","combat"});
        auto scalar = [](const nlohmann::json& object, const char* key, float& value) {
            if (!object.contains(key)) return;
            if (!object.at(key).is_number()) throw std::runtime_error("Depth scalar must be numeric.");
            value = object.at(key).get<float>();
            if (!std::isfinite(value)) throw std::runtime_error("Depth scalar must be finite.");
        };
        auto flag = [](const nlohmann::json& object, const char* key, bool& value) {
            if (!object.contains(key)) return;
            if (!object.at(key).is_boolean()) throw std::runtime_error("Depth flag must be boolean.");
            value = object.at(key).get<bool>();
        };
        auto vector = [&](const nlohmann::json& object, const char* key, std::initializer_list<float*> destinations) {
            if (!object.contains(key)) return;
            const auto& array = object.at(key);
            if (!array.is_array() || array.size() != destinations.size()) throw std::runtime_error("Depth vector has invalid dimensions.");
            unsigned index = 0;
            for (float* destination : destinations) {
                if (!array.at(index).is_number()) throw std::runtime_error("Depth vector must be numeric.");
                *destination = array.at(index).get<float>();
                if (!std::isfinite(*destination)) throw std::runtime_error("Depth vector must be finite.");
                ++index;
            }
        };
        auto object = [&](const char* key) -> const nlohmann::json& {
            static const nlohmann::json empty = nlohmann::json::object();
            if (!document.contains(key)) return empty;
            if (!document.at(key).is_object()) throw std::runtime_error("Depth settings section must be an object.");
            return document.at(key);
        };
        flag(document,"enabled",candidate.enabled);
        const auto& camera = object("camera");
        keys(camera,{"tiltDegrees","distance","fovY","nearClip","farClip"});
        scalar(camera,"tiltDegrees",candidate.camera.tiltDegrees);
        scalar(camera,"distance",candidate.camera.distance);
        scalar(camera,"fovY",candidate.camera.fovY);
        scalar(camera,"nearClip",candidate.camera.nearClip);
        scalar(camera,"farClip",candidate.camera.farClip);
        CameraFrame frame;
        if (!TryCamera(candidate.camera,{},frame) ||
            !InRange(candidate.camera.tiltDegrees,15,25) || !InRange(candidate.camera.distance,48,120) ||
            !InRange(candidate.camera.fovY,.4f,.8f) ||
            candidate.camera.farClip < candidate.camera.distance + 40 ||
            candidate.camera.nearClip > candidate.camera.distance - 30) throw std::runtime_error("Depth camera settings are out of range.");
        const auto& visual = object("visual");
        keys(visual,{"worldHeight","hoverHeight","floorOffset","reducedMotion","motionAmplitude",
            "collapseSeconds","dissolveSeconds","footOffsetLocal","forwardYawOffsetRadians"});
        scalar(visual,"worldHeight",candidate.visual.worldHeight);
        scalar(visual,"hoverHeight",candidate.visual.hoverHeight);
        scalar(visual,"motionAmplitude",candidate.visual.motionAmplitude);
        scalar(visual,"collapseSeconds",candidate.visual.collapseSeconds);
        scalar(visual,"dissolveSeconds",candidate.visual.dissolveSeconds);
        scalar(visual,"forwardYawOffsetRadians",candidate.visual.forwardYawOffsetRadians);
        flag(visual,"reducedMotion",candidate.visual.reducedMotion);
        vector(visual,"floorOffset",{&candidate.visual.floorOffset.x,&candidate.visual.floorOffset.y});
        vector(visual,"footOffsetLocal",{&candidate.visual.footOffsetLocal.x,&candidate.visual.footOffsetLocal.y,&candidate.visual.footOffsetLocal.z});
        if (!Valid(candidate.visual)) throw std::runtime_error("Depth presentation settings are out of range.");
        const auto& combat = object("combat");
        keys(combat,{"intro","reposition","hitInterval","volleyRadius","diveRadius","beamRange",
            "beamHalfWidth","beamSweepRadians","volley","dive","beam"});
        scalar(combat,"intro",candidate.combat.intro);
        scalar(combat,"reposition",candidate.combat.reposition);
        scalar(combat,"hitInterval",candidate.combat.hitInterval);
        scalar(combat,"volleyRadius",candidate.combat.volleyRadius);
        scalar(combat,"diveRadius",candidate.combat.diveRadius);
        scalar(combat,"beamRange",candidate.combat.beamRange);
        scalar(combat,"beamHalfWidth",candidate.combat.beamHalfWidth);
        scalar(combat,"beamSweepRadians",candidate.combat.beamSweepRadians);
        auto attack = [&](const char* key, Durations& durations, uint32_t& damage) {
            if (!combat.contains(key)) return;
            const auto& value = combat.at(key);
            if (!value.is_object()) throw std::runtime_error("Depth attack settings must be an object.");
            keys(value,{"telegraph","locked","airborne","active","recovery","damage"});
            scalar(value,"telegraph",durations.telegraph); scalar(value,"locked",durations.locked);
            scalar(value,"airborne",durations.airborne); scalar(value,"active",durations.active);
            scalar(value,"recovery",durations.recovery);
            if (value.contains("damage")) {
                if (!value.at("damage").is_number_integer()) throw std::runtime_error("Depth damage must be an integer.");
                const auto& number = value.at("damage");
                if (number.is_number_unsigned()) {
                    const auto integer = number.get<uint64_t>();
                    if (integer < 1 || integer > 999) throw std::runtime_error("Depth damage is out of range.");
                    damage = static_cast<uint32_t>(integer);
                } else {
                    const auto integer = number.get<int64_t>();
                    if (integer < 1 || integer > 999) throw std::runtime_error("Depth damage is out of range.");
                    damage = static_cast<uint32_t>(integer);
                }
            }
        };
        attack("volley",candidate.combat.volley,candidate.combat.volleyDamage);
        attack("dive",candidate.combat.dive,candidate.combat.diveDamage);
        attack("beam",candidate.combat.beam,candidate.combat.beamDamage);
        if (!Valid(candidate.combat)) throw std::runtime_error("Depth combat settings are out of range.");
        output = candidate; error.clear(); return true;
    } catch (const std::exception& exception) {
        error = std::string(exception.what()).substr(0,256);
        return false; // Caller keeps the previous valid settings or defaults.
    }
}
} // namespace neondepth
