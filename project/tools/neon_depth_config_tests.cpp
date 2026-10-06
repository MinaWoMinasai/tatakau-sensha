#include "game/enemy/actor/NeonDepthConfig.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
using nlohmann::json;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool SameConfig(const neondepth::Config& a, const neondepth::Config& b) {
    auto values = [](const neondepth::Config& c) {
        return std::make_tuple(c.enabled,c.camera.tiltDegrees,c.camera.distance,c.camera.fovY,c.camera.aspect,
            c.camera.nearClip,c.camera.farClip,c.visual.worldHeight,c.visual.hoverHeight,
            c.visual.floorOffset.x,c.visual.floorOffset.y,c.visual.reducedMotion,c.visual.motionAmplitude,
            c.visual.collapseSeconds,c.visual.dissolveSeconds,c.visual.footOffsetLocal.x,
            c.visual.footOffsetLocal.y,c.visual.footOffsetLocal.z,c.visual.forwardYawOffsetRadians);
    };
    return a.combat == b.combat && values(a) == values(b);
}
json FullDocument() {
    return {
        {"schemaVersion",1},{"enabled",false},
        {"camera",{{"tiltDegrees",17},{"distance",80},{"fovY",.63},{"nearClip",.2},{"farClip",6000}}},
        {"visual",{{"worldHeight",21},{"hoverHeight",4},{"floorOffset",{-3,6}},
            {"reducedMotion",true},{"motionAmplitude",.7},{"collapseSeconds",.4},{"dissolveSeconds",1.5},
            {"footOffsetLocal",{.1,-.2,.3}},{"forwardYawOffsetRadians",.9}}},
        {"combat",{{"intro",1.1},{"reposition",.8},{"hitInterval",.6},
            {"volleyRadius",2.5},{"diveRadius",4},{"beamRange",25},{"beamHalfWidth",1.1},{"beamSweepRadians",1.5},
            {"volley",{{"telegraph",1.2},{"locked",.4},{"airborne",.6},{"active",.7},{"recovery",1.4},{"damage",31}}},
            {"dive",{{"telegraph",1.3},{"locked",.5},{"airborne",.7},{"active",.8},{"recovery",1.5},{"damage",47}}},
            {"beam",{{"telegraph",1.4},{"locked",.6},{"airborne",.8},{"active",.9},{"recovery",1.6},{"damage",63}}}}}
    };
}
neondepth::Config ExpectedFullConfig() {
    neondepth::Config result;
    result.enabled = false;
    result.camera = {17,80,.63f,1280.0f/720.0f,.2f,6000};
    result.visual.worldHeight = 21;
    result.visual.hoverHeight = 4;
    result.visual.floorOffset = {-3,6};
    result.visual.reducedMotion = true;
    result.visual.motionAmplitude = .7f;
    result.visual.collapseSeconds = .4f;
    result.visual.dissolveSeconds = 1.5f;
    result.visual.footOffsetLocal = {.1f,-.2f,.3f};
    result.visual.forwardYawOffsetRadians = .9f;
    result.combat.intro = 1.1f;
    result.combat.reposition = .8f;
    result.combat.hitInterval = .6f;
    result.combat.volleyRadius = 2.5f;
    result.combat.diveRadius = 4;
    result.combat.beamRange = 25;
    result.combat.beamHalfWidth = 1.1f;
    result.combat.beamSweepRadians = 1.5f;
    result.combat.volley = {1.2f,.4f,.6f,.7f,1.4f};
    result.combat.dive = {1.3f,.5f,.7f,.8f,1.5f};
    result.combat.beam = {1.4f,.6f,.8f,.9f,1.6f};
    result.combat.volleyDamage = 31;
    result.combat.diveDamage = 47;
    result.combat.beamDamage = 63;
    return result;
}
neondepth::Config Load(const json& document) {
    neondepth::Config config;
    std::string error = "previous failure";
    if (!neondepth::ReadConfig(document,config,error)) throw std::runtime_error("Unexpected config rejection: " + error);
    Check(error.empty(),"Successful reload must clear the previous diagnostic.");
    return config;
}
void ExpectRejected(const json& document) {
    auto config = ExpectedFullConfig();
    const auto previous = config;
    std::string error = "stale error";
    Check(!neondepth::ReadConfig(document,config,error),"Malformed or out-of-range config was accepted.");
    Check(!error.empty() && error.size() <= 256 && error != "stale error","Rejection must replace the diagnostic with bounded text.");
    Check(SameConfig(config,previous),"Rejected config must preserve every prior camera, visual and combat field.");
}
json ScalarDocument(const char* group, const char* field, const json& value) {
    return {{group,{{field,value}}}};
}
void DefaultsMappingAndReload() {
    const auto defaults = Load(json::object());
    Check(defaults.enabled && defaults.combat.intro == 2.4f && defaults.visual.worldHeight == 18.0f &&
        defaults.camera.tiltDegrees == 20 && defaults.camera.distance == 90 && defaults.camera.fovY == .55f &&
        defaults.combat.volleyDamage == 12 && defaults.combat.diveDamage == 18 && defaults.combat.beamDamage == 9,
        "Shipped encounter defaults changed unexpectedly.");
    const auto full = Load(FullDocument());
    Check(SameConfig(full,ExpectedFullConfig()),"A complete document must map each setting to its intended field.");
    auto config = full;
    std::string error;
    Check(!neondepth::ReadConfig({{"enabled",1}},config,error),"Numeric enabled flag must be rejected.");
    Check(neondepth::ReadConfig({{"schemaVersion",1},{"visual",{{"reducedMotion",true}}},
        {"combat",{{"volley",{{"damage",7},{"telegraph",1.0}}}}}},config,error),"Partial reload must succeed.");
    auto partial = defaults;
    partial.visual.reducedMotion = true;
    partial.combat.volleyDamage = 7;
    partial.combat.volley.telegraph = 1;
    Check(SameConfig(config,partial),"Omitted fields must return to shipped defaults, including previous nondefault groups.");
    Check(error.empty(),"Successful reload after failure must clear the old error.");
    Check(neondepth::ReadConfig(json::object(),config,error) && SameConfig(config,defaults),
        "An empty reload must restore the complete default profile.");
    auto disabled = defaults;
    disabled.enabled = false;
    Check(SameConfig(Load({{"enabled",false}}),disabled),"Boolean disabled profile must be accepted without changing other defaults.");
    Check(SameConfig(Load({{"schemaVersion",uint64_t{1}}}),defaults),"Schema version one must also accept JSON unsigned integer representation.");
}
void StrictTypesAndAtomicRejection() {
    const json invalid[] = {
        nullptr,json::array(),true,1,{{"schemaVersion",2}},{{"schemaVersion",1.0}},{{"schemaVersion","1"}},
        {{"enabled",nullptr}},{{"enabled",1}},{{"camera",json::array()}},{{"visual",nullptr}},{{"combat",true}},
        {{"unknownSetting",true}},{{"camera",{{"aspect",1.7}}}},{{"visual",{{"unknown",1}}}},
        {{"combat",{{"unknown",1}}}},{{"combat",{{"beam",{{"unknown",1}}}}}},{{"combat",{{"dive",json::array()}}}},
        {{"camera",{{"tiltDegrees","20"}}}},{{"visual",{{"reducedMotion",1}}}},{{"combat",{{"beamRange",true}}}},
        {{"visual",{{"floorOffset",{1,2,3}}}}},{{"visual",{{"footOffsetLocal",{0,0}}}}},
        {{"visual",{{"floorOffset",{{"x",1},{"y",2}}}}}},{{"visual",{{"footOffsetLocal",{"bad",0,0}}}}},
        {{"visual",{{"floorOffset",{0,std::numeric_limits<double>::infinity()}}}}},
        {{"visual",{{"motionAmplitude",std::numeric_limits<double>::quiet_NaN()}}}},
        {{"camera",{{"fovY",std::numeric_limits<double>::infinity()}}}},{{"combat",{{"intro",1.0e100}}}},
        {{"combat",{{"volley",{{"damage",7.5}}}}}},{{"combat",{{"dive",{{"damage",true}}}}}},
        {{"combat",{{"beam",{{"damage","9"}}}}}},
        {{"combat",{{"volley",{{"damage",(std::numeric_limits<uint64_t>::max)()}}}}}},
        {{"combat",{{"dive",{{"damage",(std::numeric_limits<int64_t>::min)()}}}}}}
    };
    for (const auto& document : invalid) ExpectRejected(document);
    auto longUnknown = FullDocument();
    longUnknown[std::string(4096,'x')] = true;
    ExpectRejected(longUnknown);
    auto lateFailure = FullDocument();
    lateFailure["combat"]["beam"]["damage"] = 0;
    ExpectRejected(lateFailure); // Valid earlier groups must never leak into the caller.
}
struct ScalarRange { const char* group; const char* field; float minimum, maximum; };
void NumericBoundaries() {
    const ScalarRange ranges[] = {
        {"camera","tiltDegrees",15,25},{"camera","distance",48,120},{"camera","fovY",.4f,.8f},
        {"visual","worldHeight",4,40},{"visual","hoverHeight",0,15},{"visual","motionAmplitude",0,1.5f},
        {"visual","collapseSeconds",.15f,.8f},{"visual","dissolveSeconds",.4f,2},
        {"visual","forwardYawOffsetRadians",-2*cg2::pi,2*cg2::pi},
        {"combat","intro",.1f,5},{"combat","reposition",.1f,5},{"combat","hitInterval",.1f,2},
        {"combat","volleyRadius",.5f,6},{"combat","diveRadius",.5f,8},{"combat","beamRange",4,40},
        {"combat","beamHalfWidth",.2f,2},{"combat","beamSweepRadians",.2f,2.4f}
    };
    for (const auto& range : ranges) {
        Load(ScalarDocument(range.group,range.field,range.minimum));
        Load(ScalarDocument(range.group,range.field,range.maximum));
        ExpectRejected(ScalarDocument(range.group,range.field,std::nextafter(range.minimum,-std::numeric_limits<float>::infinity())));
        ExpectRejected(ScalarDocument(range.group,range.field,std::nextafter(range.maximum,std::numeric_limits<float>::infinity())));
    }
    for (const char* key : {"floorOffset","footOffsetLocal"}) {
        const size_t dimensions = std::string(key) == "floorOffset" ? 2u : 3u;
        const float extent = dimensions == 2u ? 12.0f : .5f;
        for (size_t index = 0; index < dimensions; ++index) {
            auto vector = json::array();
            for (size_t component = 0; component < dimensions; ++component) vector.push_back(0);
            vector[index] = -extent;
            Load(ScalarDocument("visual",key,vector));
            vector[index] = extent;
            Load(ScalarDocument("visual",key,vector));
            vector[index] = std::nextafter(-extent,-std::numeric_limits<float>::infinity());
            ExpectRejected(ScalarDocument("visual",key,vector));
            vector[index] = std::nextafter(extent,std::numeric_limits<float>::infinity());
            ExpectRejected(ScalarDocument("visual",key,vector));
        }
    }
    const ScalarRange durationRanges[] = {
        {"","telegraph",.35f,5},{"","locked",.15f,2},{"","airborne",.05f,3},
        {"","active",.05f,5},{"","recovery",.35f,5}
    };
    for (const char* attack : {"volley","dive","beam"}) {
        for (const auto& range : durationRanges) {
            auto document = json{{"combat",{{attack,{{range.field,range.minimum}}}}}};
            Load(document);
            document["combat"][attack][range.field] = range.maximum;
            Load(document);
            document["combat"][attack][range.field] = std::nextafter(range.minimum,-std::numeric_limits<float>::infinity());
            ExpectRejected(document);
            document["combat"][attack][range.field] = std::nextafter(range.maximum,std::numeric_limits<float>::infinity());
            ExpectRejected(document);
        }
        for (const auto damage : {uint64_t{1},uint64_t{999}}) Load({{"combat",{{attack,{{"damage",damage}}}}}});
        for (const auto damage : {int64_t{1},int64_t{999}}) Load({{"combat",{{attack,{{"damage",damage}}}}}});
        for (const int damage : {-1,0,1000}) ExpectRejected({{"combat",{{attack,{{"damage",damage}}}}}});
    }
}
void CameraVisibilityEnvelope() {
    ExpectRejected({{"camera",{{"farClip",10}}}}); // Was numerically valid while the encounter floor lay outside the far plane.
    ExpectRejected({{"camera",{{"farClip",std::nextafter(112.0f,0.0f)}}}});
    ExpectRejected({{"camera",{{"nearClip",std::nextafter(.01f,0.0f)}}}});
    ExpectRejected({{"camera",{{"nearClip",std::nextafter(10.0f,11.0f)}}}});
    ExpectRejected({{"camera",{{"farClip",std::nextafter(10000.0f,11000.0f)}}}});
    const json cameras[] = {
        {{"distance",48},{"nearClip",.01f},{"farClip",88}},
        {{"distance",72},{"nearClip",.1f},{"farClip",112}},
        {{"distance",120},{"nearClip",10},{"farClip",160}},
        {{"distance",120},{"nearClip",10},{"farClip",10000}}
    };
    for (const auto& camera : cameras) {
        const auto config = Load({{"camera",camera}});
        neondepth::CameraFrame frame;
        const cg2::Vector3 focus{44,29,0};
        Check(neondepth::TryCamera(config.camera,focus,frame),"Accepted camera must construct a finite real engine matrix.");
        cg2::Vector2 pixel;
        float depth = -1;
        Check(neondepth::TryProject(focus,frame.viewProjection,{1280,720},pixel,&depth),
            "Accepted encounter camera must keep its floor focus inside the displayed depth interval.");
        Check(depth >= 0 && depth <= 1 && std::abs(pixel.x-640) < .01f && std::abs(pixel.y-360) < .01f,
            "Encounter floor focus must remain at the displayed camera center.");
        cg2::Vector3 floor;
        Check(neondepth::TryScreenToFloorFromViewProjection(pixel,{1280,720},frame.viewProjection,0,floor) &&
            std::abs(floor.x-focus.x) < .01f && std::abs(floor.y-focus.y) < .01f,
            "Accepted camera must support the same displayed floor aim mapping.");
    }
}
}
int main() {
    try {
        DefaultsMappingAndReload();
        StrictTypesAndAtomicRejection();
        NumericBoundaries();
        CameraVisibilityEnvelope();
        std::cout << "PASS: Depth config full schema mapping, defaults, transactional reload, strict types, numeric boundaries and visible floor camera.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
