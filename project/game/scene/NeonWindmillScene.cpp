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
#include "externals/imgui/imgui.h"
#include <iomanip>
#include <sstream>
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
    appearances_[1].baseBrightness = .035f;
    appearances_[1].surfaceEmission = .015f;
    appearances_[1].innerIntensity = 1.1f;
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
    char run[128]{}, output[1024]{};
    if (verification_ && GetEnvironmentVariableA("CG2_WINDMILL_RUN", run, sizeof(run)) > 0)
        qualityRun_ = run;
    if (GetEnvironmentVariableA("CG2_WINDMILL_OUTPUT", output, sizeof(output)) > 0)
        qualityOutput_ = output;
    if (qualityOutput_.empty())
        qualityOutput_ = "../generated/neon_windmill_quality/" + qualityRun_;
    if (!qualityRun_.empty())
        std::filesystem::create_directories(qualityOutput_);
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
    if (verification_) {
        if (qualityRun_.empty())
            UpdateVerification();
        else
            UpdateQualityVerification();
    } else
#endif
    {
        bool keyboard = true;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (input_->IsKeyTriggered(DIK_F4))
            settingsUi_ = !settingsUi_;
        keyboard = !(settingsUi_ && ImGui::GetIO().WantCaptureKeyboard);
#endif
        if (keyboard) {
            if (input_->IsKeyTriggered(DIK_M))
                SelectMode(WindmillRenderMode((int(renderMode_) + 1) % 3));
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
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
            if (input_->IsKeyTriggered(DIK_P) && !capture_->IsBusy())
                capture_->Request("../generated/neon_windmill", "manual_" + std::to_string(++manualCaptureIndex_), {});
#endif
        }
        if (!paused_)
            seconds_ += dt * speed_;
    }
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (!verification_ && settingsUi_)
        DrawSettingsUi();
#endif
    pose_ = neonwindmill::Evaluate(seconds_);
    UpdateCamera();
    {
        cg2::RuntimeProfiler::CpuScope scope("Windmill geometry update");
        BuildGeometry();
    }
    UpdateLabels();
    cg2::RuntimeProfiler::Get().SetCounter("Windmill generated vertices", renderer_->GetVertexCount());
    cg2::RuntimeProfiler::Get().SetCounter("Windmill 3D draw calls", (solidEnd_ ? 1 : 0) + (emojiRenderer_->GetQuadCount() ? 1 : 0) +
                                                                         (renderer_->GetVertexCount() > solidEnd_ ? 1 : 0));
}

void NeonWindmillScene::SelectMode(WindmillRenderMode mode)
{
    if (mode == renderMode_)
        return;
    renderMode_ = mode;
    lastStatus_ = -1;
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
    auto settings = appearances_[int(renderMode_)];
    // An optional brief warm/pink accent, not a change in animation timing.
    settings.recognitionAccent =
        renderMode_ == WindmillRenderMode::Legacy ? 0 : .45f * pose_.recognition * (1 - neonwindmill::Smooth(14.2f, 14.9f, pose_.seconds));
    emojiRenderer_->SetAppearance(renderMode_, settings);
    renderer_->BeginFrame();
    emojiRenderer_->BeginFrame();
    if (showBackground_)
        BuildRoom(true);
    BuildCharacter(true);
    solidEnd_ = renderer_->GetVertexCount();
    if (showBackground_)
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
                    const auto& settings = appearances_[int(renderMode_)];
                    const float radiance = settings.coreIntensity * .4f + settings.surfaceEmission + settings.haloIntensity * .2f;
                    const Vector4 tint =
                        renderMode_ == WindmillRenderMode::Legacy
                            ? (source == 4 ? Vector4{1.8f, .8f, .08f, 1} : kHandColors[static_cast<std::size_t>(source)])
                            : Vector4{settings.haloColor.x * radiance, settings.haloColor.y * radiance, settings.haloColor.z * radiance, 1};
                    color.x += tint.x * amount;
                    color.y += tint.y * amount;
                    color.z += tint.z * amount;
                }
                const Vector3 points[] = {{x, 0, z}, {x + step, 0, z}, {x + step, 0, z + step}, {x, 0, z + step}};
                Fill(points, 4, color);
            }
        const Vector3 wall[] = {{-13, 0, 14}, {13, 0, 14}, {13, 12, 14}, {-13, 12, 14}};
        if (renderMode_ == WindmillRenderMode::Legacy)
            Fill(wall, 4, {.003f, .007f, .018f, 1});
        else {
            // Local diffuse approximation: 78 cells, five moving sources.
            // Shared lighting, model shaders and gameplay defaults stay intact.
            const auto& settings = appearances_[int(renderMode_)];
            const float strength = settings.coreIntensity * .4f + settings.surfaceEmission + settings.haloIntensity * .2f;
            for (int iy = 0; iy < 6; ++iy)
                for (int ix = 0; ix < 13; ++ix) {
                    const float x = -13 + float(ix) * 2, y = float(iy) * 2;
                    Vector4 color{.003f, .007f, .018f, 1};
                    for (int source = 0; source < 5; ++source) {
                        const auto light = source == 4 ? V(pose_.center) : V(pose_.hands[std::size_t(source)]);
                        const auto d = light - Vector3{x + 1, y + 1, 14};
                        const float d2 = cg2::Dot(d, d);
                        const float lambert = (std::max)(0.0f, -d.z / std::sqrt((std::max)(d2, .001f)));
                        const float amount = pose_.visibility * strength * .045f * lambert / (1 + d2 * .30f);
                        color.x += settings.haloColor.x * amount;
                        color.y += settings.haloColor.y * amount;
                        color.z += settings.haloColor.z * amount;
                    }
                    const Vector3 cell[] = {{x, y, 14}, {x + 2, y, 14}, {x + 2, y + 2, 14}, {x, y + 2, 14}};
                    Fill(cell, 4, color);
                }
        }
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
        if (renderMode_ != WindmillRenderMode::Legacy)
            return; // Shader owns the sole Core/Halo source.
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
        if (!solid && showBackground_ && appearances_[int(renderMode_)].diagnostic == 0) {
            const float trail = 1 - neonwindmill::Smooth(12, 13.4f, pose_.seconds);
            for (int i = 0; i < 18; ++i) {
                const float a = pose_.angle + float(hand) * kPi * .5f - float(i) * .045f, b = a - .045f;
                const float alpha = trail * pose_.visibility * (1 - float(i) / 18) * .22f;
                const auto color = renderMode_ == WindmillRenderMode::Legacy ? kHandColors[hand] : Vector4{1, .38f, .025f, 1};
                Line(V(pose_.center) + Vector3{1.65f * std::cos(a), 1.65f * std::sin(a), .05f},
                     V(pose_.center) + Vector3{1.65f * std::cos(b), 1.65f * std::sin(b), .05f}, .024f, {color.x, color.y, color.z, alpha});
            }
        }
    }
}

void NeonWindmillScene::DrawPostEffect3D()
{
    cg2::RuntimeProfiler::GpuScope scope("Windmill scene");
    const auto& vp = camera_.GetViewProjectionMatrix();
    renderer_->DrawRangeSceneSolid(0, solidEnd_, vp);
    // Quality is premultiplied and includes transparent halo. Draw room lines
    // before the five disjoint plates so their source opacity covers the grid.
    // Legacy retains its original cutout/depth/contour ordering byte for byte.
    if (renderMode_ != WindmillRenderMode::Legacy)
        renderer_->DrawRange(solidEnd_, renderer_->GetVertexCount() - solidEnd_, vp);
    {
        cg2::RuntimeProfiler::GpuScope plates("Windmill plates");
        emojiRenderer_->Draw(vp);
    }
    if (renderMode_ == WindmillRenderMode::Legacy)
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
    const int status = int(pose_.phase) * 12 + int(renderMode_) * 4 + (bloomEnabled_ ? 1 : 0) + (paused_ ? 2 : 0);
    if (lastStatus_ == status)
        return;
    const char* phases[] = {"回転 / ORBIT",   "接近 / APPROACH", "減速・停止 / LOCK", "認識・正面への指差し / RECOGNIZED",
                            "急接近 / SURGE", "再構成 / RESET"};
    const char* modes[] = {"LEGACY", "LINE ART", "HYBRID GOLD"};
    labels_[3]->SetText(std::string(phases[int(pose_.phase)]) + "  |  " + modes[int(renderMode_)] + "  |  BLOOM " +
                        (bloomEnabled_ ? "ON" : "OFF") + (paused_ ? "  |  PAUSED" : ""));
    labels_[4]->SetText("SPACE 停止 / R 再生 / M 描画モード / B ブルーム / H 表示"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
                        " / F4 開発設定"
#endif
    );
    lastStatus_ = status;
}

void NeonWindmillScene::DrawSprite()
{
    if (showUi_)
        for (auto& label : labels_)
            label->Draw();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (capture_->HasRequest()) {
        nlohmann::json metadata = {
            {"scene", "NEON_WINDMILL"},
            {"author", "MinaWoMinasai"},
            {"proceduralMotion", true},
            {"emojiVendor", "Apple iOS 26.4"},
            {"billboard", false},
            {"emojiQuads", emojiRenderer_->GetQuadCount()},
            {"timeSeconds", pose_.seconds},
            {"phase", int(pose_.phase)},
            {"recognition", pose_.recognition},
            {"glyphSet", pose_.recognition >= .5f ? "1faea/1faf5" : "1f601/1f449/1f446/1f448/1f447"},
            {"recognitionAccent", renderMode_ == WindmillRenderMode::Legacy
                                      ? 0
                                      : .45f * pose_.recognition * (1 - neonwindmill::Smooth(14.2f, 14.9f, pose_.seconds))},
            {"center", {pose_.center.x, pose_.center.y, pose_.center.z}},
            {"camera", {camera_.GetTranslate().x, camera_.GetTranslate().y, camera_.GetTranslate().z}},
            {"yaw", yaw_},
            {"vertices", renderer_->GetVertexCount()},
            {"solidVertices", solidEnd_},
            {"drawCalls",
             (solidEnd_ ? 1 : 0) + (emojiRenderer_->GetQuadCount() ? 1 : 0) + (renderer_->GetVertexCount() > solidEnd_ ? 1 : 0)},
            {"renderMode", int(renderMode_)},
            {"angle", pose_.angle},
            {"visibility", pose_.visibility},
            {"background", showBackground_},
            {"resolution", {cg2::WinApp::kClientWidth, cg2::WinApp::kClientHeight}},
            {"appearance",
             {{"base", appearances_[int(renderMode_)].baseBrightness},
              {"coreWidth", appearances_[int(renderMode_)].coreWidthPixels},
              {"coreIntensity", appearances_[int(renderMode_)].coreIntensity},
              {"innerThreshold", appearances_[int(renderMode_)].innerThreshold},
              {"innerIntensity", appearances_[int(renderMode_)].innerIntensity},
              {"surface", appearances_[int(renderMode_)].surfaceEmission},
              {"haloWidth", appearances_[int(renderMode_)].haloWidthPixels},
              {"haloIntensity", appearances_[int(renderMode_)].haloIntensity},
              {"coreColor",
               {appearances_[int(renderMode_)].coreColor.x, appearances_[int(renderMode_)].coreColor.y,
                appearances_[int(renderMode_)].coreColor.z}},
              {"haloColor",
               {appearances_[int(renderMode_)].haloColor.x, appearances_[int(renderMode_)].haloColor.y,
                appearances_[int(renderMode_)].haloColor.z}},
              {"diagnostic", appearances_[int(renderMode_)].diagnostic}}},
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
void NeonWindmillScene::DrawSettingsUi()
{
    ImGui::SetNextWindowSize({365, 570}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Windmill appearance / F4", &settingsUi_)) {
        int mode = int(renderMode_);
        if (ImGui::Combo("Mode / M", &mode, "Legacy Neon\0Neon Line Art\0Hybrid Neon (Gold)\0"))
            SelectMode(WindmillRenderMode(mode));
        ImGui::Checkbox("Pause / Space", &paused_);
        float time = float(seconds_);
        if (ImGui::SliderFloat("Seek (seconds)", &time, 0, 15.99f)) {
            seconds_ = time;
            paused_ = true;
        }
        ImGui::Checkbox("Background", &showBackground_);
        ImGui::Checkbox("Global Bloom / B", &bloomEnabled_);
        ImGui::SliderFloat("Bloom gain", &bloomGain_, 0, 1.8f);
        ImGui::TextWrapped("Bloom OFF retains the source Core and Local Halo. Set Local Halo intensity to 0 for Core-only comparison.");
        ImGui::Separator();
        auto& s = appearances_[mode];
        ImGui::BeginDisabled(mode == 0);
        if (ImGui::Button("Restore golden preset")) {
            s = WindmillNeonSettings{};
            if (mode == 1) {
                s.baseBrightness = .035f;
                s.surfaceEmission = .015f;
                s.innerIntensity = 1.1f;
            }
        }
        ImGui::SliderFloat("Base brightness", &s.baseBrightness, 0, 1.2f);
        ImGui::SliderFloat("Outer Core width (px)", &s.coreWidthPixels, 0, 4);
        ImGui::SliderFloat("Outer Core intensity", &s.coreIntensity, 0, 8);
        ImGui::SliderFloat("Inner contrast threshold", &s.innerThreshold, .04f, .65f);
        ImGui::SliderFloat("Inner intensity", &s.innerIntensity, 0, 3);
        ImGui::SliderFloat("Surface emission", &s.surfaceEmission, 0, .8f);
        ImGui::SliderFloat("Local Halo width (px)", &s.haloWidthPixels, 0, 6);
        ImGui::SliderFloat("Local Halo intensity", &s.haloIntensity, 0, 2);
        ImGui::ColorEdit3("Core color (linear)", &s.coreColor.x);
        ImGui::ColorEdit3("Halo / inner color", &s.haloColor.x);
        int diagnostic = int(s.diagnostic);
        if (ImGui::Combo("Source diagnostic", &diagnostic, "Combined\0Base\0Outer Core\0Inner Lines\0Local Halo\0Surface\0"))
            s.diagnostic = uint32_t(diagnostic);
        ImGui::EndDisabled();
        ImGui::TextWrapped(
            "Legacy preserves the original source shader, contour geometry, palette and Bloom defaults. Texture contrast lines are approximate, not semantic detection.");
    }
    ImGui::End();
}

void NeonWindmillScene::UpdateQualityVerification()
{
    ++verificationFrames_;
    const bool motion = qualityRun_ == "motion";
    const bool measure = qualityRun_.starts_with("perf");
    const unsigned count = motion ? 480u : 48u; // 5 states x 3 modes x Bloom OFF/ON + diagnostics/controls.
    if (verificationPending_ && !capture_->IsBusy()) {
        if (capture_->WasLastCaptureSuccessful())
            capturedNames_.push_back(qualityCaptureName_);
        else
            verificationErrors_.push_back(capture_->GetStatus());
        verificationPending_ = false;
        ++verificationIndex_;
    }
    const auto report = [this, motion, measure]() {
        auto* dx = cg2::Object3dCommon::GetInstance()->GetDxCommon();
        UINT64 frequency = 0;
        dx->GetQueue()->GetTimestampFrequency(&frequency);
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 description{};
        std::string adapterName = "Unavailable";
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->EnumAdapterByLuid(dx->GetDevice()->GetAdapterLuid(), IID_PPV_ARGS(&adapter))) &&
            SUCCEEDED(adapter->GetDesc1(&description))) {
            const int length = WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, nullptr, 0, nullptr, nullptr);
            std::vector<char> name(std::size_t((std::max)(length, 1)));
            WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, name.data(), int(name.size()), nullptr, nullptr);
            adapterName = name.data();
        }
        std::ofstream file(std::filesystem::path(qualityOutput_) / "runtime_report.json");
        file << nlohmann::json({{"run", qualityRun_},
                                {"captures", capturedNames_},
                                {"errors", verificationErrors_},
                                {"frames", verificationFrames_},
                                {"completed", verificationErrors_.empty()},
                                {"motion", motion},
                                {"performance", measure},
                                {"timestampFrequency", frequency},
                                {"adapter", adapterName},
                                {"renderMode", int(renderMode_)},
                                {"timeSeconds", seconds_},
                                {"yaw", yaw_},
                                {"distance", distance_},
                                {"bloomGain", bloomGain_},
                                {"bloomEnabled", bloomEnabled_},
                                {"resolution", {cg2::WinApp::kClientWidth, cg2::WinApp::kClientHeight}},
                                {"debugLayer", dx->IsD3D12DebugLayerEnabled()},
                                {"gpuValidation", dx->IsGpuBasedValidationEnabled()}})
                    .dump(2)
             << '\n';
        PostQuitMessage(verificationErrors_.empty() && file.good() ? 0 : 8);
    };
    if (verificationFrames_ > 5000) {
        verificationErrors_.push_back("Quality run timeout");
        report();
        return;
    }
    paused_ = true;
    showUi_ = false;
    showBackground_ = true;
    yaw_ = .12f;
    distance_ = 11.5f;
    bloomGain_ = .65f;
    bloomEnabled_ = true;
    if (measure) {
        if (qualityRun_ != "perf0" && qualityRun_ != "perf1" && qualityRun_ != "perf2") {
            verificationErrors_.push_back("Unknown performance mode");
            report();
            return;
        }
        SelectMode(WindmillRenderMode(qualityRun_.back() - '0'));
        seconds_ = 3.35;
        auto& profiler = cg2::RuntimeProfiler::Get();
        if (!qualityMeasureStarted_) {
            qualityMeasureStarted_ = profiler.StartCapture(qualityOutput_ + "/gpu.csv", 300, 60);
            if (!qualityMeasureStarted_) {
                verificationErrors_.push_back("GPU profiler unavailable");
                report();
            }
        } else if (profiler.IsCaptureComplete())
            report();
        return;
    }
    if (verificationIndex_ >= count) {
        report();
        return;
    }
    if (motion) {
        SelectMode(WindmillRenderMode::Hybrid);
        seconds_ = double(verificationIndex_) / 30;
        std::ostringstream name;
        name << "frame_" << std::setw(4) << std::setfill('0') << verificationIndex_;
        qualityCaptureName_ = name.str();
    } else if (qualityRun_ == "comparison") {
        constexpr std::array<double, 5> times{3.35, 10.5, 13.3, 13.95, 14.65};
        constexpr std::array<const char*, 5> states{"orbit", "approach", "before", "after", "surge"};
        const unsigned i = verificationIndex_;
        if (i < 30) {
            const unsigned state = i / 6, mode = (i / 2) % 3;
            SelectMode(WindmillRenderMode(mode));
            seconds_ = times[state];
            bloomEnabled_ = i % 2 != 0;
            qualityCaptureName_ = std::string(states[state]) + "_" + std::to_string(mode) + (bloomEnabled_ ? "_on" : "_off");
        } else {
            SelectMode(WindmillRenderMode::Hybrid);
            seconds_ = 13.3;
            bloomEnabled_ = false;
            auto& settings = appearances_[2];
            settings = WindmillNeonSettings{};
            settings.diagnostic = i < 36 ? i - 30 : 0;
            showBackground_ = false;
            constexpr std::array<const char*, 18> names{"combined",        "base",     "core",         "inner",          "halo",
                                                        "surface",         "no_halo",  "wide_halo",    "narrow_core",    "wide_core",
                                                        "zero_emission",   "side",     "far",          "core_wide_halo", "core_no_halo",
                                                        "bloom_zero_gain", "dim_base", "core_bloom_on"};
            qualityCaptureName_ = std::string("diagnostic_") + names[i - 30];
            if (i == 36)
                settings.haloIntensity = 0;
            if (i == 37)
                settings.haloWidthPixels = 6;
            if (i == 38)
                settings.coreWidthPixels = .75f;
            if (i == 39)
                settings.coreWidthPixels = 2.5f;
            if (i == 40)
                settings.coreIntensity = settings.innerIntensity = settings.surfaceEmission = settings.haloIntensity = 0;
            if (i == 41)
                yaw_ = .72f;
            if (i == 42)
                distance_ = 19;
            if (i == 43) {
                settings.diagnostic = 2;
                settings.haloWidthPixels = 6;
            }
            if (i == 44) {
                settings.diagnostic = 2;
                settings.haloIntensity = 0;
            }
            if (i == 45) {
                bloomEnabled_ = true;
                bloomGain_ = 0;
            }
            if (i == 46)
                settings.baseBrightness = .20f;
            if (i == 47) {
                settings.diagnostic = 2;
                bloomEnabled_ = true;
            }
        }
    } else {
        verificationErrors_.push_back("Unknown quality run");
        report();
        return;
    }
    // Static cases settle for eight frames: post metadata and texture commands
    // belong to the selected case. Motion keeps post parameters constant.
    if (!verificationPending_ && (motion || verificationFrames_ % 8 == 0)) {
        capture_->Request(qualityOutput_, qualityCaptureName_, {});
        verificationPending_ = true;
    }
}

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
