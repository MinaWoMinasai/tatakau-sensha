#include "NeonWindmillScene.h"
#include "NeonGridRenderer.h"
#include "Object3dCommon.h"
#include "Input.h"
#include "TextLabel.h"
#include "SpriteCommon.h"
#include "WinApp.h"
#include "RuntimeProfiler.h"
#include "game/render/WindmillEmojiRenderer.h"
#include "externals/nlohmann/json.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/NeonShowcaseCapture.h"
#endif

namespace {
using cg2::Vector3;
using cg2::Vector4;
constexpr float kPi = neonwindmill::kPi;
constexpr std::array<Vector4, 4> kHandColors{{{.05f, 1.35f, 1.8f, 1}, {1.7f, .12f, .7f, 1}, {1.8f, .8f, .08f, 1}, {.4f, .3f, 1.9f, 1}}};

/// @brief エンジン座標へ生成データの点を変換する。
Vector3 V(neonwindmill::Point p)
{
    return {p.x, p.y, p.z};
}
/// @brief 線形RGBを指定強度で減衰する。Alphaは別に保持する。
Vector4 Scale(Vector4 c, float strength)
{
    return {c.x * strength, c.y * strength, c.z * strength, c.w};
}
} // namespace

NeonWindmillScene::NeonWindmillScene() = default;

NeonWindmillScene::~NeonWindmillScene()
{
    if (cg2::Object3dCommon::GetInstance()->GetDefaultCamera() == &camera_)
        cg2::Object3dCommon::GetInstance()->SetDefaultCamera(previousCamera_);
}

void NeonWindmillScene::Initialize()
{
    auto* common = cg2::Object3dCommon::GetInstance();
    previousCamera_ = common->GetDefaultCamera();
    common->SetDefaultCamera(&camera_);
    common->SetIsDebugCamera(false);
    common->SetDebugUiEnabled(false);
    input_ = cg2::Input::GetInstance();
    renderer_ = std::make_unique<cg2::NeonGridRenderer>();
    renderer_->Initialize(common->GetDxCommon(), "resources/white512x512.png");
    if (!renderer_->InitializeSceneSolidPipeline())
        throw std::runtime_error("Neon windmill HDR solid pipeline could not initialize.");
    std::ifstream atlasFile("../generated/neon_windmill/iphone_atlas.json");
    if (!atlasFile)
        throw std::runtime_error("iPhone emoji atlas is missing. Run project/tools/prepare_windmill_emoji.py first.");
    const auto atlas = nlohmann::json::parse(atlasFile);
    for (const auto& item : atlas.at("glyphs").items()) {
        Glyph glyph;
        glyph.uv = item.value().at("uv").get<std::array<float, 4>>();
        for (const auto& p : item.value().at("contour"))
            glyph.contour.push_back({p.at(0).get<float>(), p.at(1).get<float>()});
        glyphs_.emplace(item.key(), std::move(glyph));
    }
    emojiRenderer_ = std::make_unique<WindmillEmojiRenderer>();
    emojiRenderer_->Initialize(common->GetDxCommon(), "../generated/neon_windmill/iphone_atlas.png");
    camera_.SetAspectRatio(float(cg2::WinApp::kClientWidth) / float(cg2::WinApp::kClientHeight));
    camera_.SetFovY(.7f);
    camera_.SetNearClip(.1f);
    camera_.SetFarClip(80);
    const auto label = [this](const std::string& text, float size, cg2::Vector2 position, Vector4 color) {
        cg2::TextStyle style{};
        style.fontFamily = "Meiryo";
        style.fontSize = size;
        style.fontWeight = 400;
        style.color = color;
        style.padding = 3;
        auto item = std::make_unique<cg2::TextLabel>();
        item->Initialize(cg2::SpriteCommon::GetInstance(), text, style);
        item->SetAnchorPoint({0, 0});
        item->SetPosition(position);
        labels_.push_back(std::move(item));
    };
    label("NEON WINDMILL", 30, {42, 30}, {.82f, .94f, 1, 1});
    label("MinaWoMinasai  /  C++・DirectX 12 自作エンジン", 14, {44, 77}, {.42f, .66f, .76f, 1});
    label("iPhone絵文字の板ポリゴン × ネオン輪郭 × HDRブルーム", 15, {44, 102}, {.45f, .79f, .84f, 1});
    label("回転", 18, {44, 145}, {1, .68f, .25f, 1});
    label("SPACE 停止 / R 再生し直す / B ブルーム比較 / H 表示切替", 14, {42, 646}, {.52f, .74f, .82f, 1});
    label("← → 視点 / ↑ ↓ 距離 / 1 回転 / 2 停止 / 3 認識 / + - 光量 / ESC 終了", 13, {42, 674}, {.38f, .55f, .64f, 1});
    previousTime_ = std::chrono::steady_clock::now();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    capture_ = std::make_unique<NeonShowcaseCapture>();
    char flag[8]{};
    verification_ = GetEnvironmentVariableA("CG2_WINDMILL_AUTOTEST", flag, sizeof(flag)) == 1 && flag[0] == '1';
#endif
    pose_ = neonwindmill::Evaluate(seconds_);
    UpdateCamera();
    BuildGeometry();
    UpdateLabels();
}

void NeonWindmillScene::Update()
{
    const auto now = std::chrono::steady_clock::now();
    const float dt = (std::clamp)(std::chrono::duration<float>(now - previousTime_).count(), 0.0f, .05f);
    previousTime_ = now;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    capture_->Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    if (verification_)
        UpdateVerification();
    else
#endif
    {
        if (input_->IsKeyTriggered(DIK_ESCAPE))
            PostQuitMessage(0);
        if (input_->IsKeyTriggered(DIK_SPACE))
            paused_ = !paused_;
        if (input_->IsKeyTriggered(DIK_B)) {
            bloomEnabled_ = !bloomEnabled_;
            lastStatus_ = -1;
        }
        if (input_->IsKeyTriggered(DIK_H))
            showUi_ = !showUi_;
        if (input_->IsKeyTriggered(DIK_R)) {
            seconds_ = 0;
            paused_ = false;
        }
        if (input_->IsKeyTriggered(DIK_1)) {
            seconds_ = 3.35;
            paused_ = true;
        }
        if (input_->IsKeyTriggered(DIK_2)) {
            seconds_ = 13.3;
            paused_ = true;
        }
        if (input_->IsKeyTriggered(DIK_3)) {
            seconds_ = 13.95;
            paused_ = true;
        }
        if (input_->IsKeyTriggered(DIK_EQUALS) || input_->IsKeyTriggered(DIK_ADD))
            bloomGain_ = (std::min)(1.8f, bloomGain_ + .1f);
        if (input_->IsKeyTriggered(DIK_MINUS) || input_->IsKeyTriggered(DIK_SUBTRACT))
            bloomGain_ = (std::max)(0.0f, bloomGain_ - .1f);
        const auto held = [this](int key) {
            return input_->IsPress(input_->GetKey()[key]);
        };
        yaw_ += (float(held(DIK_RIGHT)) - float(held(DIK_LEFT))) * dt * .7f;
        distance_ = (std::clamp)(distance_ + (float(held(DIK_DOWN)) - float(held(DIK_UP))) * dt * 4, 7.5f, 19.0f);
        if (!paused_)
            seconds_ += dt * speed_;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (input_->IsKeyTriggered(DIK_P) && !capture_->IsBusy())
            capture_->Request("../generated/neon_windmill", "manual_" + std::to_string(++manualCaptureIndex_), {});
#endif
    }
    pose_ = neonwindmill::Evaluate(seconds_);
    UpdateCamera();
    BuildGeometry();
    UpdateLabels();
    cg2::RuntimeProfiler::Get().SetCounter("Windmill generated vertices", renderer_->GetVertexCount());
    cg2::RuntimeProfiler::Get().SetCounter("Windmill 3D draw calls", 3);
}

void NeonWindmillScene::UpdateCamera()
{
    const Vector3 target{0, 2.75f, -.7f};
    const float cameraY = 4.3f;
    camera_.SetTranslate({std::sin(yaw_) * distance_, cameraY, target.z - std::cos(yaw_) * distance_});
    camera_.SetRotate({std::atan2(cameraY - target.y, distance_), -yaw_, 0});
    camera_.SetProjectionJitter({0, 0});
    camera_.Update();
    cameraForward_ = cg2::Normalize(target - camera_.GetTranslate());
}

void NeonWindmillScene::Fill(const Vector3* points, uint32_t count, Vector4 color)
{
    renderer_->QueueBeveledPolygonFill(points, count, color, {0, 0, 0, 1}, {0, 1, 0});
}

void NeonWindmillScene::Line(Vector3 a, Vector3 b, float width, Vector4 color)
{
    cg2::NeonContourStyle style{};
    style.coreIntensity = 2.1f;
    style.coreWhiteMix = .12f;
    style.shoulderIntensity = 1.0f;
    style.haloIntensity = .4f;
    style.haloAlpha = .11f;
    style.roundCaps = false;
    renderer_->QueueContourLine(a, b, width, color, cameraForward_, style);
}

void NeonWindmillScene::BuildGeometry()
{
    renderer_->BeginFrame();
    emojiRenderer_->BeginFrame();
    BuildRoom(true);
    BuildCharacter(true);
    solidEnd_ = renderer_->GetVertexCount();
    BuildRoom(false);
    BuildCharacter(false);
}

void NeonWindmillScene::BuildRoom(bool solid)
{
    if (solid) {
        // ブルームは画面上のにじみなので、床の照明は独立した点光源の減衰で求める。
        // N=(0,1,0)のLambert項と距離減衰を使い、5光源の床への照り返しを毎フレーム更新する。
        constexpr float step = .65f;
        for (int iz = -14; iz < 22; ++iz)
            for (int ix = -18; ix < 18; ++ix) {
                const float x = float(ix) * step, z = float(iz) * step;
                const Vector3 sample{x + step * .5f, 0, z + step * .5f};
                Vector4 color{.0028f, .006f, .013f, 1};
                for (int source = 0; source < 5; ++source) {
                    const Vector3 light = source == 4 ? V(pose_.center) : V(pose_.hands[static_cast<std::size_t>(source)]);
                    const Vector3 d = light - sample;
                    const float d2 = cg2::Dot(d, d), lambert = (std::max)(0.0f, d.y / std::sqrt((std::max)(.001f, d2)));
                    const float amount = pose_.visibility * .045f * lambert / (1 + d2 * .30f);
                    const Vector4 tint = source == 4 ? Vector4{1.8f, .8f, .08f, 1} : kHandColors[static_cast<std::size_t>(source)];
                    color.x += tint.x * amount;
                    color.y += tint.y * amount;
                    color.z += tint.z * amount;
                }
                const Vector3 points[] = {{x, 0, z}, {x + step, 0, z}, {x + step, 0, z + step}, {x, 0, z + step}};
                Fill(points, 4, color);
            }
        const Vector3 wall[] = {{-13, 0, 14}, {13, 0, 14}, {13, 12, 14}, {-13, 12, 14}};
        Fill(wall, 4, {.003f, .007f, .018f, 1});
        const Vector3 door[] = {{3.4f, 0, 5}, {5.0f, 0, 5}, {5.0f, 3.3f, 5}, {3.4f, 3.3f, 5}};
        Fill(door, 4, {.001f, .001f, .004f, 1});
        return;
    }
    for (int i = -11; i <= 11; ++i) {
        const float p = float(i) * 1.3f;
        Line({p, .008f, -9}, {p, .008f, 14}, .014f, {.018f, .08f, .14f, .65f});
        Line({-12, .008f, p}, {12, .008f, p}, .014f, {.018f, .08f, .14f, .65f});
    }
    // 発光の強い輪は展示台の外周に限定し、床全体が白くなるのを避ける。
    for (int i = 0; i < 128; ++i) {
        const float a = float(i) * 2 * kPi / 128, b = float(i + 1) * 2 * kPi / 128;
        Line({std::cos(a) * 3.4f, .018f, std::sin(a) * 3.4f - .8f}, {std::cos(b) * 3.4f, .018f, std::sin(b) * 3.4f - .8f}, .025f,
             {.02f, .4f, .55f, .65f});
    }
    for (float x : {-8.5f, 8.5f})
        for (float z : {5.0f, 11.0f}) {
            Line({x, 0, z}, {x, 8, z}, .035f, {.05f, .15f, .32f, .8f});
            Line({x, 8, z}, {-x, 8, z}, .025f, {.02f, .09f, .2f, .6f});
        }
    Line({3.4f, 0, 4.98f}, {3.4f, 3.3f, 4.98f}, .035f, {.45f, .03f, .75f, 1});
    Line({3.4f, 3.3f, 4.98f}, {5, 3.3f, 4.98f}, .035f, {.45f, .03f, .75f, 1});
    Line({5, 3.3f, 4.98f}, {5, 0, 4.98f}, .035f, {.45f, .03f, .75f, 1});
}

void NeonWindmillScene::BuildCharacter(bool solid)
{
    if (pose_.visibility < .001f)
        return;
    const bool recognized = pose_.recognition >= .5f;
    const auto add = [this, solid](const std::string& codepoint, Vector3 center, float size, float angle, Vector4 tint) {
        const auto& glyph = glyphs_.at(codepoint);
        if (solid) {
            // 頂点はワールドXY平面のまま。CameraRight/Upで再構成しないためビルボードにならない。
            emojiRenderer_->Queue(center, size, angle, glyph.uv, pose_.visibility);
            return;
        }
        std::vector<Vector3> outline;
        outline.reserve(glyph.contour.size());
        const float c = std::cos(angle), s = std::sin(angle);
        for (const auto& p : glyph.contour)
            outline.push_back(center + Vector3{size * (p.x * c - p.y * s), size * (p.x * s + p.y * c), -.012f});
        cg2::NeonContourStyle style{};
        style.coreIntensity = 2.1f;
        style.coreWhiteMix = .12f;
        style.shoulderIntensity = 1.0f;
        style.haloIntensity = .4f;
        style.haloAlpha = .11f;
        tint = Scale(tint, pose_.visibility);
        tint.w = pose_.visibility;
        renderer_->QueueContourPolygon(outline.data(), uint32_t(outline.size()), .027f, tint, cameraForward_, style);
    };
    add(recognized ? "1faea" : "1f601", V(pose_.center), 1.5f, 0, {1.8f, .8f, .08f, 1});
    constexpr std::array<const char*, 4> directions{{"1f449", "1f446", "1f448", "1f447"}};
    for (std::size_t hand = 0; hand < 4; ++hand) {
        // 四方向は独立した絵文字を使う。通常時は位置の公転と板のZ回転を同じ角度で進める。
        // 認識時は板をカメラへ回さず、画像を🫵へ切り替える。
        add(recognized ? "1faf5" : directions[hand], V(pose_.hands[hand]), 1.05f, recognized ? 0 : pose_.angle, kHandColors[hand]);
        if (!solid) {
            const float trail = 1 - neonwindmill::Smooth(12, 13.4f, pose_.seconds);
            for (int i = 0; i < 18; ++i) {
                const float a = pose_.angle + float(hand) * kPi * .5f - float(i) * .045f, b = a - .045f;
                const float alpha = trail * pose_.visibility * (1 - float(i) / 18) * .22f;
                const auto color = kHandColors[hand];
                Line(V(pose_.center) + Vector3{1.65f * std::cos(a), 1.65f * std::sin(a), .05f},
                     V(pose_.center) + Vector3{1.65f * std::cos(b), 1.65f * std::sin(b), .05f}, .024f, {color.x, color.y, color.z, alpha});
            }
        }
    }
}

void NeonWindmillScene::DrawPostEffect3D()
{
    const auto& vp = camera_.GetViewProjectionMatrix();
    renderer_->DrawRangeSceneSolid(0, solidEnd_, vp);
    emojiRenderer_->Draw(vp);
    renderer_->DrawRange(solidEnd_, renderer_->GetVertexCount() - solidEnd_, vp);
}

IScene::DeveloperShowcaseState NeonWindmillScene::GetDeveloperShowcaseState()
{
    DeveloperShowcaseState state;
    state.active = true;
    state.camera = &camera_;
    state.threshold = .8f;
    state.exposure = .85f;
    state.intensity = .75f;
    state.bloomComparisonMode = bloomEnabled_ ? 2 : 0;
    state.bloomSoftKnee = .5f;
    state.bloomScatter = .65f;
    state.bloomRadius = 1.0f;
    state.bloomGain = bloomGain_;
    state.toneMappingMode = 2;
    state.comparisonFreeze = true;
    return state;
}

IScene::ScreenEffectState NeonWindmillScene::GetScreenEffectState() const
{
    ScreenEffectState state;
    state.active = true;
    state.suppressOutlines = true;
    state.suppressTemporal = true;
    state.suppressPostEffectDebugUi = true;
    return state;
}

void NeonWindmillScene::UpdateLabels()
{
    const int status = int(pose_.phase) * 4 + (bloomEnabled_ ? 1 : 0) + (paused_ ? 2 : 0);
    if (lastStatus_ == status)
        return;
    const char* phases[] = {"回転 / ORBIT",   "接近 / APPROACH", "減速・停止 / LOCK", "認識・正面への指差し / RECOGNIZED",
                            "急接近 / SURGE", "再構成 / RESET"};
    labels_[3]->SetText(std::string(phases[int(pose_.phase)]) + "  |  BLOOM " + (bloomEnabled_ ? "ON" : "OFF") +
                        (paused_ ? "  |  PAUSED" : ""));
    lastStatus_ = status;
}

void NeonWindmillScene::DrawSprite()
{
    if (showUi_)
        for (auto& label : labels_)
            label->Draw();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (capture_->HasRequest()) {
        nlohmann::json metadata = {{"scene", "NEON_WINDMILL"},
                                   {"author", "MinaWoMinasai"},
                                   {"proceduralMotion", true},
                                   {"emojiVendor", "Apple iOS 26.4"},
                                   {"billboard", false},
                                   {"emojiQuads", emojiRenderer_->GetQuadCount()},
                                   {"timeSeconds", pose_.seconds},
                                   {"phase", int(pose_.phase)},
                                   {"recognition", pose_.recognition},
                                   {"center", {pose_.center.x, pose_.center.y, pose_.center.z}},
                                   {"camera", {camera_.GetTranslate().x, camera_.GetTranslate().y, camera_.GetTranslate().z}},
                                   {"yaw", yaw_},
                                   {"vertices", renderer_->GetVertexCount()},
                                   {"solidVertices", solidEnd_},
                                   {"drawCalls", 3},
                                   {"post",
                                    {{"bloomMode", renderedPost_.bloomMode},
                                     {"threshold", renderedPost_.threshold},
                                     {"gain", renderedPost_.bloomGain},
                                     {"exposure", renderedPost_.exposure},
                                     {"temporal", renderedPost_.temporalEnabled},
                                     {"toneMapping", renderedPost_.toneMappingMode}}}};
        capture_->SetFrameMetadata(std::move(metadata));
        capture_->Record(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    }
#endif
}

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
void NeonWindmillScene::UpdateVerification()
{
    constexpr std::array<const char*, 6> names{{"orbit_off", "orbit_on", "side_on", "lock_on", "recognized_on", "showcase"}};
    constexpr std::array<double, 6> times{{3.35, 3.35, 3.35, 13.3, 13.95, 4.45}};
    ++verificationFrames_;
    if (verificationPending_ && !capture_->IsBusy()) {
        if (capture_->WasLastCaptureSuccessful())
            capturedNames_.push_back(names[verificationIndex_]);
        else
            verificationErrors_.push_back(capture_->GetStatus());
        verificationPending_ = false;
        ++verificationIndex_;
    }
    if (verificationIndex_ >= names.size() || verificationFrames_ > 600) {
        if (verificationIndex_ < names.size())
            verificationErrors_.push_back("GPU capture verification timed out.");
        std::filesystem::create_directories("../generated/neon_windmill");
        std::ofstream report("../generated/neon_windmill/runtime_report.json");
        report << nlohmann::json({{"captures", capturedNames_},
                                  {"errors", verificationErrors_},
                                  {"frames", verificationFrames_},
                                  {"emojiQuads", emojiRenderer_->GetQuadCount()},
                                  {"maxVertices", uint32_t(cg2::NeonGridRenderer::kMaxVertices)},
                                  {"completed", verificationIndex_ == names.size() && verificationErrors_.empty()}})
                      .dump(2)
               << '\n';
        PostQuitMessage(verificationErrors_.empty() && report.good() ? 0 : 8);
        return;
    }
    seconds_ = times[verificationIndex_];
    paused_ = true;
    bloomEnabled_ = verificationIndex_ != 0;
    yaw_ = verificationIndex_ == 2 ? .72f : .12f;
    showUi_ = verificationIndex_ == 5;
    if (!verificationPending_ && verificationFrames_ % 8 == 0) {
        capture_->Request("../generated/neon_windmill", names[verificationIndex_], {});
        verificationPending_ = true;
    }
}
#endif
