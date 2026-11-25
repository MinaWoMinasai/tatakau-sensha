#include "game/ui/session/NeonMenuKit.h"
#include "Input.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
using Color = myEngine::Vector4;
const std::string kFolder = "resources/endless_ui/";
const std::string kWhitePixel = "resources/white512x512.png";
constexpr float kWidth = 1280.0f, kHeight = 720.0f;
constexpr float kRowHeight = 49.6f, kRowGap = 6.4f, kMark = 33.3f, kMarkGap = 15.4f;
constexpr float kRowFont = 32.0f;
constexpr float kPi = 3.14159265f;

myEngine::TextStyle Style(float size, bool number)
{
    myEngine::TextStyle style{};
    style.fontFamily = number ? "Bahnschrift" : "Zen Maru Gothic";
    if (!number) {
        style.fontPath = "resources/fonts/ZenMaruGothic-Bold.ttf";
    }
    style.fontWeight = 700;
    style.fontSize = size;
    style.color = {1.0f, 1.0f, 1.0f, 1.0f};
    style.outlineThickness = 0.0f;
    style.padding = 2.0f;
    return style;
}
} // namespace

MenuInput ReadMenuInput(myEngine::Input& input, myEngine::Vector2& lastMouse)
{
    MenuInput out;
    auto key = [&](int code) {
        return input.IsKeyTriggered(static_cast<uint8_t>(code));
    };
    out.mouse = input.GetMousePosition();
    out.mouseMoved = std::abs(out.mouse.x - lastMouse.x) + std::abs(out.mouse.y - lastMouse.y) > 1.0f;
    lastMouse = out.mouse;
    out.click = input.IsTrigger(input.GetMouseState().rgbButtons[0], input.GetPreMouseState().rgbButtons[0]);
    out.up = key(DIK_W) || key(DIK_UP);
    out.down = key(DIK_S) || key(DIK_DOWN);
    out.left = key(DIK_A) || key(DIK_LEFT);
    out.right = key(DIK_D) || key(DIK_RIGHT);
    out.confirm = key(DIK_RETURN) || key(DIK_SPACE);
    out.back = key(DIK_ESCAPE);
    out.pad = input.IsGamepadActive();
    return out;
}

void NeonMenuKit::Initialize(size_t capacity)
{
    if (batch_.IsInitialized()) {
        return;
    }
    batch_.Initialize(capacity);
    batch_.Preload(kWhitePixel);
    for (const char* name : {"tube_core", "tube_glow", "shade_title", "shade_pause"}) {
        batch_.Preload(kFolder + name + ".png");
    }
}

void NeonMenuKit::Begin(float alpha)
{
    alpha_ = (std::clamp)(alpha, 0.0f, 1.0f);
    batch_.Begin();
}

void NeonMenuKit::End()
{
    batch_.End();
}

void NeonMenuKit::Draw()
{
    batch_.Draw();
}

myEngine::Vector4 NeonMenuKit::Tint(const Color& color) const
{
    auto convert = [](float v) {
        return std::pow((std::max)(0.0f, v), 2.2f);
    };
    return {convert(color.x), convert(color.y), convert(color.z), color.w * alpha_};
}

void NeonMenuKit::Darken(Shade shade)
{
    const myEngine::Vector2 center{kWidth * 0.5f, kHeight * 0.5f}, size{kWidth, kHeight};
    if (shade == Shade::Full) {
        // 画面の色で 94% の板（光の量で計算する描き方では 1-(1-0.94)^2.2）。
        batch_.Quad(SurvivorQuadBatch::Panel, kWhitePixel, center, size, 0.0f, Tint(menucolor::Alpha(menucolor::kVoid, 0.998f)));
        return;
    }
    batch_.Quad(SurvivorQuadBatch::Panel, kFolder + (shade == Shade::Title ? "shade_title.png" : "shade_pause.png"), center, size, 0.0f,
                Tint(menucolor::kVoid));
}

void NeonMenuKit::Rect(float left, float top, float width, float height, const Color& color)
{
    batch_.Quad(SurvivorQuadBatch::Panel, kWhitePixel, {left + width * 0.5f, top + height * 0.5f}, {width, height}, 0.0f, Tint(color));
}

void NeonMenuKit::Tube(const myEngine::Vector2& a, const myEngine::Vector2& b, float width, const Color& core, const Color& glowColor, float glow,
                       float reach)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    const float angle = std::atan2(dy, dx);
    const float c = std::cos(angle), s = std::sin(angle);
    const myEngine::Vector2 middle{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
    // 円の画像の真ん中の縦1列を伸ばして線に、左右の半分を両端に貼る。
    auto line = [&](SurvivorQuadBatch::Layer layer, const char* kind, float half, const Color& color) {
        const std::string path = kFolder + kind + ".png";
        batch_.Quad(layer, path, middle, {length, half * 2.0f}, angle, color, 0.5f, 0.5f);
        const float offset = half * 0.5f;
        batch_.Quad(layer, path, {b.x + c * offset, b.y + s * offset}, {half, half * 2.0f}, angle, color, 0.5f, 1.0f);
        batch_.Quad(layer, path, {a.x - c * offset, a.y - s * offset}, {half, half * 2.0f}, angle, color, 0.0f, 0.5f);
    };
    if (glow > 0.004f) {
        Color light = Tint(glowColor);
        light.w *= glow;
        line(SurvivorQuadBatch::PreviewGlow, "tube_glow", width * 0.5f + reach, light);
    }
    line(SurvivorQuadBatch::PreviewTop, "tube_core", width * 0.5f * 1.12f, Tint(core));
}

void NeonMenuKit::Ring(const myEngine::Vector2& center, float radius, float width, const Color& core, const Color& glowColor, float glow, float reach,
                       float from, float to)
{
    if (to <= from) {
        return;
    }
    const int segments = (std::max)(2, static_cast<int>(std::ceil((to - from) * 40.0f)));
    const float step = (to - from) / static_cast<float>(segments) * 2.0f * kPi;
    const float chord = 2.0f * radius * std::sin(step * 0.5f);
    const Color solid = Tint(core);
    Color light = Tint(glowColor);
    light.w *= glow;
    for (int i = 0; i < segments; ++i) {
        const float angle = -kPi * 0.5f + from * 2.0f * kPi + (static_cast<float>(i) + 0.5f) * step;
        const float r = radius * std::cos(step * 0.5f);
        const myEngine::Vector2 at{center.x + std::cos(angle) * r, center.y + std::sin(angle) * r};
        if (glow > 0.004f) {
            batch_.Quad(SurvivorQuadBatch::PreviewGlow, kFolder + "tube_glow.png", at, {chord, width + reach * 2.0f}, angle + kPi * 0.5f, light,
                        0.5f, 0.5f);
        }
        batch_.Quad(SurvivorQuadBatch::PreviewTop, kFolder + "tube_core.png", at, {chord + width * 0.2f, width * 1.12f}, angle + kPi * 0.5f, solid, 0.5f,
                    0.5f);
    }
}

float NeonMenuKit::Text(const std::string& text, float size, float x, float y, float anchorX, const Color& color, bool number)
{
    return batch_.Label(SurvivorQuadBatch::Text, text, Style(size, number), x, y, anchorX, Tint(color));
}

float NeonMenuKit::MeasureText(const std::string& text, float size, bool number)
{
    return batch_.Measure(text, Style(size, number)).x;
}

float NeonMenuKit::GlowText(const std::string& text, float size, float x, float y, float anchorX, const Color& color, const Color& glowColor, float glow)
{
    const auto style = Style(size, true);
    Color light = Tint(glowColor);
    constexpr int kRings = 3, kSides = 12;
    for (int ring = 0; ring < kRings; ++ring) {
        const float radius = size * (0.022f + 0.032f * static_cast<float>(ring));
        Color part = light;
        part.w *= glow * (ring == 0 ? 0.10f : ring == 1 ? 0.05f : 0.028f);
        for (int i = 0; i < kSides; ++i) {
            const float angle = 2.0f * kPi * (static_cast<float>(i) + 0.5f * static_cast<float>(ring % 2)) / static_cast<float>(kSides);
            batch_.Label(SurvivorQuadBatch::PreviewGlow, text, style, x + std::cos(angle) * radius, y + std::sin(angle) * radius, anchorX, part);
        }
    }
    return batch_.Label(SurvivorQuadBatch::Text, text, style, x, y, anchorX, Tint(color));
}

float NeonMenuKit::Key(const std::string& label, float x, float y, bool round, const Color& color, float scale)
{
    const float font = 16.0f * scale;
    const float text = MeasureText(label, font, true) - 4.0f;
    const float height = 25.0f * scale;
    const Color border = menucolor::Alpha(Color{0.467f, 0.502f, 0.549f, 1.0f}, color.w);
    if (round) {
        const float radius = (std::max)(text * 0.5f + 5.0f * scale, height * 0.5f);
        Ring({x + radius, y}, radius, 1.6f * scale, border, border, 0.0f, 0.0f);
        Text(label, font, x + radius, y, 0.5f, color, true);
        return radius * 2.0f;
    }
    const float width = (std::max)(15.0f * scale, text) + 15.0f * scale;
    const float l = x + 2.0f, r = x + width - 2.0f, t = y - height * 0.5f, b = y + height * 0.5f;
    const float line = 1.6f * scale;
    Tube({l + 2, t}, {r - 2, t}, line, border, border, 0.0f, 0.0f);
    Tube({l + 2, b}, {r - 2, b}, line, border, border, 0.0f, 0.0f);
    Tube({l, t + 2}, {l, b - 2}, line, border, border, 0.0f, 0.0f);
    Tube({r, t + 2}, {r, b - 2}, line, border, border, 0.0f, 0.0f);
    Text(label, font, x + width * 0.5f, y, 0.5f, color, true);
    return width;
}

void NeonMenuKit::Icon(const std::string& name, float x, float y, float size, const Color& color)
{
    batch_.Quad(SurvivorQuadBatch::IconGlow, kFolder + name + ".png", {x, y}, {size, size}, 0.0f, Tint(color));
}

void NeonMenuKit::Cell(const std::string& name, float x, float y, const Color& color, int tier, int maxTier)
{
    constexpr float kSize = 64.0f;
    if (name.empty()) {
        const Color line{0.165f, 0.184f, 0.227f, 1.0f};
        const float l = x - kSize * 0.5f, r = x + kSize * 0.5f, t = y - kSize * 0.5f, b = y + kSize * 0.5f;
        Tube({l + 3, t}, {r - 3, t}, 1.5f, line, line, 0.0f, 0.0f);
        Tube({l + 3, b}, {r - 3, b}, 1.5f, line, line, 0.0f, 0.0f);
        Tube({l, t + 3}, {l, b - 3}, 1.5f, line, line, 0.0f, 0.0f);
        Tube({r, t + 3}, {r, b - 3}, 1.5f, line, line, 0.0f, 0.0f);
        return;
    }
    Icon(name, x, y, 62.0f, color);
    const int dots = (std::clamp)(maxTier, 0, 7);
    constexpr float kDot = 6.4f, kGap = 3.2f;
    float dotX = x - (static_cast<float>(dots - 1) * (kDot + kGap)) * 0.5f;
    for (int i = 0; i < dots; ++i) {
        const Color fill = i < tier ? color : Color{0.165f, 0.184f, 0.227f, 1.0f};
        Tube({dotX - 0.1f, y + 35.2f}, {dotX + 0.1f, y + 35.2f}, kDot, fill, fill, 0.0f, 0.0f);
        dotX += kDot + kGap;
    }
}

float NeonMenuKit::Meter(float x, float y, int value, bool lit)
{
    constexpr float kDash = 24.3f, kGap = 5.8f, kLine = 5.4f;
    for (int i = 0; i < 10; ++i) {
        const float left = x + static_cast<float>(i) * (kDash + kGap);
        const bool on = i < value;
        const Color core = !on ? Color{0.149f, 0.169f, 0.204f, 1.0f} : lit ? Color{0.851f, 1.0f, 0.914f, 1.0f} : Color{0.498f, 0.639f, 0.573f, 1.0f};
        Tube({left + kLine * 0.5f, y}, {left + kDash - kLine * 0.5f, y}, kLine, core, menucolor::kGreen, on && lit ? 0.55f : 0.0f, 13.0f);
    }
    return 10.0f * (kDash + kGap) - kGap;
}

void NeonMenuKit::Hints(const std::vector<Hint>& hints, bool pad)
{
    constexpr float kY = 667.0f;
    float x = 76.8f;
    const Color key{0.875f, 0.902f, 0.910f, 1.0f};
    for (const Hint& hint : hints) {
        if (pad) {
            x += Key(hint.button, x, kY, true, key) + 8.0f;
        } else {
            for (const std::string& name : hint.keys) {
                x += Key(name, x, kY, false, key) + 8.0f;
            }
        }
        x += Text(hint.text, 17.0f, x, kY, 0.0f, menucolor::kDim) + 28.0f;
    }
}

void NeonMenu::Open(std::vector<Item> items, float top, float left, int selected)
{
    items_ = std::move(items);
    top_ = top;
    left_ = left;
    age_ = 0.0f;
    armed_ = -1;
    selected_ = (std::clamp)(selected, 0, (std::max)(0, static_cast<int>(items_.size()) - 1));
    moved_ = false;
}

int NeonMenu::Update(float dt, const MenuInput& input)
{
    age_ += dt;
    moved_ = false;
    const int count = static_cast<int>(items_.size());
    if (count == 0) {
        return -1;
    }
    int wanted = selected_;
    if (input.up) {
        wanted = (wanted + count - 1) % count;
    }
    if (input.down) {
        wanted = (wanted + 1) % count;
    }
    int hover = -1;
    for (int i = 0; i < count; ++i) {
        const float y = top_ + static_cast<float>(i) * (kRowHeight + kRowGap);
        if (input.mouse.x >= left_ - 10.0f && input.mouse.x <= left_ + 430.0f && input.mouse.y >= y && input.mouse.y < y + kRowHeight) {
            hover = i;
        }
    }
    // マウスを動かしたときだけ、合わせた項目を選ぶ（キーで選んだ項目が、置いたままのマウスに戻されないように）。
    if (hover >= 0 && (input.mouseMoved || input.click)) {
        wanted = hover;
    }
    if (wanted != selected_) {
        selected_ = wanted;
        armed_ = -1;
        moved_ = true;
    }
    constexpr float kLockSeconds = 0.12f;
    if (age_ < kLockSeconds || !(input.confirm || (input.click && hover >= 0))) {
        return -1;
    }
    if (!items_[static_cast<size_t>(selected_)].armed.empty() && armed_ != selected_) {
        armed_ = selected_;
        return -1;
    }
    armed_ = -1;
    return selected_;
}

void NeonMenu::Queue(NeonMenuKit& kit)
{
    for (size_t i = 0; i < items_.size(); ++i) {
        const bool on = static_cast<int>(i) == selected_;
        const bool warn = static_cast<int>(i) == armed_;
        const float y = top_ + static_cast<float>(i) * (kRowHeight + kRowGap) + kRowHeight * 0.5f;
        if (on) {
            const Color glow = warn ? menucolor::kRed : menucolor::kGreen;
            kit.Tube({left_ + 2.7f, y}, {left_ + kMark - 2.7f, y}, 5.4f, menucolor::Core(glow, 0.94f), glow, 0.95f, 24.0f);
        }
        kit.Text(warn ? items_[i].armed : items_[i].label, kRowFont, left_ + kMark + kMarkGap - 2.0f, y, 0.0f,
                 warn ? menucolor::kRed : on ? menucolor::kWhite : menucolor::kItem);
    }
}

} // namespace gameplay
