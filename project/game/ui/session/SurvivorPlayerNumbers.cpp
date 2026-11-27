#include "game/ui/session/SurvivorPlayerNumbers.h"
#include "SpriteCommon.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
constexpr size_t kSign = 10;
// 続けて受けた被弾を1つにまとめる秒、数字が出ている秒、縁の赤が出ている秒。
constexpr float kMergeSeconds = 0.10f;
constexpr float kLifeSeconds = 0.9f;
constexpr float kFadeSeconds = 0.35f;
constexpr float kEdgeSeconds = 0.28f;
// この割合の被弾で、数字と縁の赤が一番大きくなる。
constexpr float kFullRatio = 0.2f;
constexpr float kRise = 34.0f;
constexpr float kAbove = 52.0f;
} // namespace

void SurvivorPlayerNumbers::Initialize()
{
    // 中は白に近い色、縁取りが種類の色（敵の数字と同じネオンの作り）。被弾は読み間違えないよう、中も赤みを付ける。
    static constexpr std::array<myEngine::Vector4, kKinds> kOutlines{{{1.0f, 0.10f, 0.24f, 1.0f}, {0.12f, 0.85f, 0.42f, 1.0f}, {1.0f, 0.78f, 0.15f, 1.0f}}};
    static constexpr std::array<myEngine::Vector4, kKinds> kFills{{{1.0f, 0.86f, 0.88f, 1.0f}, {0.90f, 1.0f, 0.94f, 1.0f}, {1.0f, 0.98f, 0.88f, 1.0f}}};
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontWeight = 700;
    style.fontSize = 30.0f;
    style.outlineThickness = 4.0f;
    style.padding = 4.0f;
    style.preserveOutline = true;
    for (size_t kind = 0; kind < kKinds; ++kind) {
        style.color = kFills[kind];
        style.outlineColor = kOutlines[kind];
        for (size_t glyph = 0; glyph < kGlyphs; ++glyph) {
            const std::string text = glyph < 10 ? std::to_string(glyph) : kind == static_cast<size_t>(Kind::Hurt) ? "-" : "+";
            auto& label = labels_[kind][glyph];
            label = std::make_unique<myEngine::TextLabel>();
            label->Initialize(myEngine::SpriteCommon::GetInstance(), text, style);
            paths_[kind][glyph] = label->GetTexturePath();
            sizes_[kind][glyph] = label->GetSprite()->GetSize();
        }
    }
    sprites_.resize(kSpriteCount);
    for (auto& sprite : sprites_) {
        sprite = std::make_unique<myEngine::Sprite>();
        sprite->Initialize(myEngine::SpriteCommon::GetInstance(), paths_[0][0]);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }
    // 縁の赤：上が白く下へ黒く消える画像を4つの縁へ向けて置き、光を足す描き方で出す（透け方で消すと、薄い所が描かれず段差が出る）。
    const float width = static_cast<float>(myEngine::WinApp::kClientWidth), height = static_cast<float>(myEngine::WinApp::kClientHeight);
    constexpr float kDepth = 170.0f;
    for (size_t i = 0; i < edges_.size(); ++i) {
        auto& edge = edges_[i];
        edge = std::make_unique<myEngine::Sprite>();
        edge->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/edgeFade.png");
        edge->SetAnchorPoint({0.5f, 0.5f});
        const bool vertical = i < 2;
        edge->SetSize({vertical ? width : height, kDepth});
        edge->SetRotation(i == 0 ? 0.0f : i == 1 ? 3.1415927f : i == 2 ? -1.5707964f : 1.5707964f);
        edge->SetPosition(i == 0   ? myEngine::Vector2{width * 0.5f, kDepth * 0.5f}
                          : i == 1 ? myEngine::Vector2{width * 0.5f, height - kDepth * 0.5f}
                          : i == 2 ? myEngine::Vector2{kDepth * 0.5f, height * 0.5f}
                                   : myEngine::Vector2{width - kDepth * 0.5f, height * 0.5f});
        edge->Update();
    }
    entries_.reserve(kMaxEntries);
}

void SurvivorPlayerNumbers::Spawn(Kind kind, int amount, float ratio)
{
    ratio = (std::clamp)(ratio, 0.0f, 1.0f);
    if (kind == Kind::Hurt) {
        // まとまった被弾ほど縁を赤く。小さい当たりでも少しは出す。
        const float power = 0.3f + 0.7f * (std::min)(1.0f, ratio / kFullRatio);
        const float left = edgePower_ * (edgeTimer_ / kEdgeSeconds);
        edgePower_ = (std::max)(power, left);
        edgeTimer_ = kEdgeSeconds;
        for (Entry& entry : entries_) {
            if (entry.kind == Kind::Hurt && entry.age < kMergeSeconds) {
                entry.amount += amount;
                entry.ratio = (std::min)(1.0f, entry.ratio + ratio);
                return;
            }
        }
    }
    if (entries_.size() >= kMaxEntries) {
        entries_.erase(entries_.begin());
    }
    // 続けて出た数字が重ならないよう、左右へ少しずつずらす。
    static constexpr std::array<float, 5> kOffsets{0.0f, 20.0f, -20.0f, 34.0f, -34.0f};
    entries_.push_back({kind, amount, ratio, 0.0f, kind == Kind::Hurt ? kOffsets[sequence_++ % kOffsets.size()] : 0.0f});
    ++spawned_[static_cast<size_t>(kind)];
}

void SurvivorPlayerNumbers::Update(float dt, const myEngine::Vector2& anchor)
{
    edgeTimer_ = (std::max)(0.0f, edgeTimer_ - dt);
    for (Entry& entry : entries_) {
        entry.age += dt;
    }
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [](const Entry& entry) {
                                      return entry.age >= kLifeSeconds * (entry.kind == Kind::Revive ? 1.6f : 1.0f);
                                  }),
                   entries_.end());
    if (edgeTimer_ > 0.0f) {
        const float fade = edgeTimer_ / kEdgeSeconds;
        for (auto& edge : edges_) {
            const float light = (std::clamp)(config_.edge * edgePower_ * fade, 0.0f, 1.0f);
            edge->SetColor({1.0f * light, 0.05f * light, 0.14f * light, 1.0f});
            edge->Update();
        }
    }
    usedSprites_ = 0;
    for (const Entry& entry : entries_) {
        const size_t kind = static_cast<size_t>(entry.kind);
        const std::string digits = std::to_string((std::clamp)(entry.amount, 0, 99999));
        if (usedSprites_ + digits.size() + 1 > sprites_.size()) {
            break;
        }
        const float life = kLifeSeconds * (entry.kind == Kind::Revive ? 1.6f : 1.0f);
        // 出た瞬間に膨らみ、0.15秒で元の大きさへ。大きい被弾ほど大きい。
        const float pop = 1.0f + (std::max)(0.0f, 0.15f - entry.age) * (entry.kind == Kind::Hurt ? 4.5f : 3.0f);
        const float base = entry.kind == Kind::Hurt     ? 0.75f + 0.45f * (std::min)(1.0f, entry.ratio / kFullRatio)
                           : entry.kind == Kind::Revive ? 1.3f
                                                        : 0.95f;
        const float scale = config_.scale * base * pop;
        const float alpha = (std::clamp)((life - entry.age) / kFadeSeconds, 0.0f, 1.0f);
        const float spacing = 0.68f * scale;
        float width = sizes_[kind][kSign].x * spacing;
        for (char c : digits) {
            width += sizes_[kind][static_cast<size_t>(c - '0')].x * spacing;
        }
        float x = anchor.x + entry.offsetX * config_.scale - width * 0.5f;
        // 立ち直りは、直前の被弾の数字と重ならないように上へ出す。
        const float y = anchor.y - kAbove - (entry.kind == Kind::Revive ? 44.0f * config_.scale : 0.0f) - kRise * (1.0f - std::pow(1.0f - entry.age / life, 2.0f));
        auto put = [&](size_t glyph) {
            auto& sprite = sprites_[usedSprites_++];
            sprite->SetTexture(paths_[kind][glyph]);
            sprite->SetSize({sizes_[kind][glyph].x * scale, sizes_[kind][glyph].y * scale});
            sprite->SetPosition({x, y});
            sprite->SetColor({1.0f, 1.0f, 1.0f, alpha});
            sprite->Update();
            x += sizes_[kind][glyph].x * spacing;
        };
        put(kSign);
        for (char c : digits) {
            put(static_cast<size_t>(c - '0'));
        }
    }
}

void SurvivorPlayerNumbers::Draw()
{
    if (edgeTimer_ > 0.0f && config_.edge > 0.0f) {
        myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kAdd);
        for (auto& edge : edges_) {
            edge->Draw();
        }
        myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    }
    for (size_t i = 0; i < usedSprites_; ++i) {
        sprites_[i]->Draw();
    }
}

void SurvivorPlayerNumbers::Clear()
{
    entries_.clear();
    usedSprites_ = 0;
    edgeTimer_ = 0.0f;
}

float SurvivorPlayerNumbers::GetNewestAge(Kind kind) const
{
    float newest = -1.0f;
    for (const Entry& entry : entries_) {
        if (entry.kind == kind && (newest < 0.0f || entry.age < newest)) {
            newest = entry.age;
        }
    }
    return newest;
}

} // namespace gameplay
