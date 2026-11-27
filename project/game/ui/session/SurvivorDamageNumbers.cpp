#include "game/ui/session/SurvivorDamageNumbers.h"
#include "SpriteCommon.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
// ネオンの縁の色。ふつうは水色、大きな数字は金色。中は白。
const myEngine::Vector4 kNormalOutline{0.10f, 0.85f, 1.0f, 1.0f};
const myEngine::Vector4 kBigOutline{1.0f, 0.62f, 0.12f, 1.0f};
// 会心（計画書 p9 の H2）：金色の太い縁。
const myEngine::Vector4 kCriticalOutline{1.0f, 0.86f, 0.18f, 1.0f};
// 出た直後に大きく見せる秒数と、消える前に薄くなる秒数。
constexpr float kPopSeconds = 0.08f;
constexpr float kFadeSeconds = 0.25f;
// 出てから消えるまでに上へ動く量（画面の座標）。
constexpr float kRise = 28.0f;
} // namespace

void SurvivorDamageNumbers::Initialize()
{
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontWeight = 700;
    style.fontSize = 30.0f;
    style.color = {1.0f, 1.0f, 1.0f, 1.0f};
    style.outlineThickness = 3.0f;
    style.padding = 3.0f;
    style.preserveOutline = true;
    for (size_t set = 0; set < digitLabels_.size(); ++set) {
        style.outlineColor = set == 0 ? kNormalOutline : set == 1 ? kBigOutline : kCriticalOutline;
        style.outlineThickness = set == 2 ? 4.5f : 3.0f;
        style.padding = set == 2 ? 4.0f : 3.0f;
        for (size_t digit = 0; digit < 10; ++digit) {
            auto& label = digitLabels_[set][digit];
            label = std::make_unique<myEngine::TextLabel>();
            label->Initialize(myEngine::SpriteCommon::GetInstance(), std::to_string(digit), style);
            digitPaths_[set][digit] = label->GetTexturePath();
            digitSizes_[set][digit] = label->GetSprite()->GetSize();
        }
    }
    // 会心の「!」。
    style.outlineColor = kCriticalOutline;
    style.outlineThickness = 4.5f;
    style.padding = 4.0f;
    bangLabel_ = std::make_unique<myEngine::TextLabel>();
    bangLabel_->Initialize(myEngine::SpriteCommon::GetInstance(), "!", style);
    bangPath_ = bangLabel_->GetTexturePath();
    bangSize_ = bangLabel_->GetSprite()->GetSize();
    sprites_.resize(kSpriteCount);
    for (auto& sprite : sprites_) {
        sprite = std::make_unique<myEngine::Sprite>();
        sprite->Initialize(myEngine::SpriteCommon::GetInstance(), digitPaths_[0][0]);
        sprite->SetAnchorPoint({0.0f, 0.5f});
    }
    entries_.reserve(64);
}

void SurvivorDamageNumbers::Spawn(uint64_t id, const myEngine::Vector3& world, uint32_t amount, float bigThreshold, bool critical)
{
    if (!config_.enabled || amount == 0) {
        return;
    }
    criticals_ += critical ? 1u : 0u;
    // 同じ敵へ続けて当てた分は、まだ新しい数字に足して出し直す（mergeSeconds が0ならまとめない。会心は別に出す）。
    for (Entry& entry : entries_) {
        if (!critical && !entry.critical && entry.id == id && entry.age < config_.mergeSeconds) {
            entry.amount += amount;
            entry.world = world;
            entry.age = 0.0f;
            entry.big = entry.big || static_cast<float>(entry.amount) >= bigThreshold;
            return;
        }
    }
    const size_t limit = static_cast<size_t>((std::max)(1, config_.maxActive));
    if (entries_.size() >= limit) {
        // 一番古い数字を消して空ける。
        const auto oldest = std::max_element(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
            return a.age < b.age;
        });
        entries_.erase(oldest);
    }
    entries_.push_back({id, world, amount, 0.0f, static_cast<float>(amount) >= bigThreshold, critical});
    ++spawned_;
}

void SurvivorDamageNumbers::Update(float dt, const std::function<myEngine::Vector2(const myEngine::Vector3&)>& worldToScreen,
                                   const myEngine::Vector3& player)
{
    const float life = (std::max)(0.1f, config_.lifeSeconds);
    for (Entry& entry : entries_) {
        entry.age += dt;
    }
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const Entry& entry) {
                                      return entry.age >= life;
                                  }),
                   entries_.end());
    usedSprites_ = 0;
    if (!config_.enabled) {
        return;
    }
    for (const Entry& entry : entries_) {
        // 自機のまわりでは薄くする（自機が数字で隠れないように）。会心は半分より薄くしない。
        float strength = entry.critical ? 1.0f : config_.alpha;
        if (config_.nearRadius > 0.0f) {
            const float dx = entry.world.x - player.x, dy = entry.world.y - player.y;
            const float t = (std::clamp)((std::sqrt(dx * dx + dy * dy) / config_.nearRadius - 0.6f) / 0.4f, 0.0f, 1.0f);
            const float low = entry.critical ? (std::max)(0.5f, config_.nearAlpha) : config_.nearAlpha;
            strength *= low + (1.0f - low) * t * t * (3.0f - 2.0f * t);
        }
        if (strength <= 0.02f) {
            continue;
        }
        const std::string text = std::to_string((std::min)(entry.amount, 999999u));
        if (usedSprites_ + text.size() + 1 > sprites_.size()) {
            break;
        }
        // 出た直後は少し大きく、消える前は薄くする。大きな数字は1.35倍、会心は critScale 倍で、出た直後の膨らみも大きい。
        const float popSize = entry.critical ? 0.9f : 0.5f;
        const float pop = entry.age < kPopSeconds * (entry.critical ? 1.6f : 1.0f)
                              ? 1.0f + popSize * (1.0f - entry.age / (kPopSeconds * (entry.critical ? 1.6f : 1.0f)))
                              : 1.0f;
        const float scale = config_.scale * (entry.critical ? config_.critScale : entry.big ? config_.bigScale : 1.0f) * pop;
        const float alpha = (std::clamp)((life - entry.age) / kFadeSeconds, 0.0f, 1.0f) * strength;
        const myEngine::Vector4 color{1.0f, 1.0f, 1.0f, alpha};
        const size_t set = entry.critical ? 2 : entry.big ? 1 : 0;
        // 数字の間は、文字の縁取りと余白の分だけ詰める（会心は縁が太いので、もう少し詰める）。
        const float spacing = entry.critical ? 0.68f : 0.78f;
        float width = 0.0f;
        for (char c : text) {
            width += digitSizes_[set][static_cast<size_t>(c - '0')].x * spacing * scale;
        }
        if (entry.critical) {
            width += bangSize_.x * 0.6f * scale;
        }
        const myEngine::Vector2 anchor = worldToScreen(entry.world);
        float x = anchor.x - width * 0.5f;
        const float y = anchor.y - 22.0f - kRise * (entry.age / life);
        for (char c : text) {
            const size_t digit = static_cast<size_t>(c - '0');
            auto& sprite = sprites_[usedSprites_++];
            sprite->SetTexture(digitPaths_[set][digit]);
            sprite->SetSize({digitSizes_[set][digit].x * scale, digitSizes_[set][digit].y * scale});
            sprite->SetPosition({x, y});
            sprite->SetColor(color);
            sprite->Update();
            x += digitSizes_[set][digit].x * spacing * scale;
        }
        if (entry.critical) {
            auto& sprite = sprites_[usedSprites_++];
            sprite->SetTexture(bangPath_);
            sprite->SetSize({bangSize_.x * scale, bangSize_.y * scale});
            sprite->SetPosition({x - bangSize_.x * 0.12f * scale, y});
            sprite->SetColor(color);
            sprite->Update();
        }
    }
}

void SurvivorDamageNumbers::Draw()
{
    for (size_t i = 0; i < usedSprites_; ++i) {
        sprites_[i]->Draw();
    }
}

void SurvivorDamageNumbers::Clear()
{
    entries_.clear();
    usedSprites_ = 0;
}

} // namespace gameplay
