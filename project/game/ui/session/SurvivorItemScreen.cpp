#include "game/ui/session/SurvivorItemScreen.h"
#include "SpriteCommon.h"
#include <algorithm>

namespace gameplay {
namespace {
// 1280×720 の画面での並び。左に武器の札、右に持ち物のマス。画面の端は CRT で曲がるので余白を取る。
constexpr float kLeft = 48.0f;
constexpr float kCardWidth = 500.0f;
constexpr float kCardHeight = 124.0f;
constexpr float kCardTop = 92.0f;
constexpr float kCardGap = 6.0f;
constexpr float kCardIcon = 72.0f;
constexpr float kSocket = 34.0f;
constexpr float kSmallTop = kCardTop + 3.0f * (kCardHeight + kCardGap);
constexpr float kSmallHeight = 76.0f;
constexpr float kSmallWidth = (kCardWidth - kCardGap) * 0.5f;
constexpr float kRightLeft = 566.0f;
constexpr float kRightWidth = 666.0f;
constexpr float kPanelTop = 92.0f;
constexpr float kHeaderCell = 44.0f;
constexpr float kHeaderStep = 64.0f;
constexpr float kGridLeft = kRightLeft + 18.0f;
constexpr float kGridTop = 212.0f;
constexpr float kCell = 58.0f;
constexpr float kCellStep = 63.0f;
constexpr float kTabTop = 152.0f;
constexpr float kTabWidth = 96.0f;
constexpr float kTabHeight = 30.0f;
constexpr float kButtonTop = 544.0f;
constexpr float kButtonHeight = 44.0f;
constexpr float kTipWidth = 430.0f;
const myEngine::Vector4 kPanelColor{0.008f, 0.016f, 0.03f, 0.96f};
const myEngine::Vector4 kCellColor{0.02f, 0.035f, 0.06f, 0.98f};

myEngine::TextStyle Style(float size, bool bold, const myEngine::Vector4& color)
{
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontWeight = bold ? 700 : 400;
    style.fontSize = size;
    style.color = color;
    style.outlineThickness = 0;
    style.padding = 3.0f;
    return style;
}

std::unique_ptr<myEngine::Sprite> Panel(const myEngine::Vector2& position, const myEngine::Vector2& size, const myEngine::Vector4& color)
{
    auto sprite = std::make_unique<myEngine::Sprite>();
    sprite->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetColor(color);
    sprite->Update();
    return sprite;
}

std::unique_ptr<myEngine::TextLabel> Label(const myEngine::Vector2& position, float size, bool bold, const myEngine::Vector4& color)
{
    auto label = std::make_unique<myEngine::TextLabel>();
    label->Initialize(myEngine::SpriteCommon::GetInstance(), " ", Style(size, bold, color));
    label->SetPosition(position);
    return label;
}

bool Inside(const myEngine::Vector2& p, float x, float y, float w, float h)
{
    return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
}

float CardY(size_t i)
{
    return kCardTop + static_cast<float>(i) * (kCardHeight + kCardGap);
}

myEngine::Vector2 SocketPosition(size_t card, size_t socket)
{
    return {kLeft + 94.0f + static_cast<float>(socket) * (kSocket + 6.0f), CardY(card) + kCardHeight - kSocket - 6.0f};
}

myEngine::Vector2 GridPosition(size_t visible)
{
    return {kGridLeft + static_cast<float>(visible % SurvivorItemScreen::kColumns) * kCellStep,
            kGridTop + static_cast<float>(visible / SurvivorItemScreen::kColumns) * kCellStep};
}

size_t LineCount(const std::string& text)
{
    return text.empty() ? 0 : 1 + static_cast<size_t>(std::count(text.begin(), text.end(), '\n'));
}
} // namespace

SurvivorItemScreen::Cell SurvivorItemScreen::MakeCell(const myEngine::Vector2& position, float size)
{
    Cell cell;
    cell.frame = Panel(position, {size, size}, kCellColor);
    cell.icon = Panel(position, {size, size}, {1.0f, 1.0f, 1.0f, 1.0f});
    // 数はアイコンに重なるので、黒い縁を付ける。
    myEngine::TextStyle countStyle = Style(15.0f, true, {1.0f, 1.0f, 1.0f, 1.0f});
    countStyle.outlineColor = {0.0f, 0.0f, 0.0f, 1.0f};
    countStyle.outlineThickness = 3.0f;
    countStyle.preserveOutline = true;
    cell.count = std::make_unique<myEngine::TextLabel>();
    cell.count->Initialize(myEngine::SpriteCommon::GetInstance(), " ", countStyle);
    cell.count->SetAnchorPoint({1.0f, 1.0f});
    return cell;
}

void SurvivorItemScreen::FillCell(Cell& cell, const Item* item, const myEngine::Vector2& position, float size, bool emptyFrame)
{
    cell.showFrame = item != nullptr || emptyFrame;
    cell.showIcon = item != nullptr && !item->icon.empty();
    cell.showCount = item != nullptr && item->count >= 0;
    cell.frame->SetPosition(position);
    cell.frame->SetSize({size, size});
    // 枠はレア度の色を薄く敷く。空のマスは暗いまま。
    const myEngine::Vector4 tint = item ? item->color : myEngine::Vector4{0.0f, 0.0f, 0.0f, 0.0f};
    const float shade = item && !item->dim ? 0.07f : 0.0f;
    cell.frame->SetColor({kCellColor.x + tint.x * shade, kCellColor.y + tint.y * shade, kCellColor.z + tint.z * shade, kCellColor.w});
    cell.frame->Update();
    if (cell.showIcon) {
        cell.icon->SetTexture(item->icon);
        cell.icon->SetPosition(position);
        cell.icon->SetSize({size, size});
        const float dim = item->dim ? 0.35f : 1.0f;
        cell.icon->SetColor({item->color.x * dim, item->color.y * dim, item->color.z * dim, 1.0f});
        cell.icon->Update();
    }
    if (cell.showCount) {
        const std::string text = std::to_string(item->count);
        if (text != cell.countShown) {
            cell.count->SetText(text);
            cell.countShown = text;
        }
        cell.count->SetPosition({position.x + size - 1.0f, position.y + size + 1.0f});
    }
}

void SurvivorItemScreen::DrawCell(Cell& cell)
{
    if (cell.showFrame) {
        cell.frame->Draw();
    }
    if (cell.showIcon) {
        cell.icon->Draw();
    }
    if (cell.showCount) {
        cell.count->Draw();
    }
}

void SurvivorItemScreen::Initialize(const std::vector<std::string>& icons)
{
    for (const std::string& icon : icons) {
        auto sprite = std::make_unique<myEngine::Sprite>();
        sprite->Initialize(myEngine::SpriteCommon::GetInstance(), icon);
        preloaded_.push_back(std::move(sprite));
    }
    backdrop_ = Panel({0.0f, 0.0f}, {1280.0f, 720.0f}, {0.0f, 0.005f, 0.012f, 0.95f});
    rightPanel_ = Panel({kRightLeft, kPanelTop}, {kRightWidth, 520.0f}, kPanelColor);
    title_ = Label({kLeft, 18.0f}, 32.0f, true, {0.82f, 0.97f, 1.0f, 1.0f});
    help_ = Label({kLeft, 64.0f}, 16.0f, false, {0.62f, 0.74f, 0.86f, 1.0f});
    gridTitle_ = Label({kGridLeft, kTabTop + 2.0f}, 20.0f, true, {1.0f, 0.86f, 0.36f, 1.0f});
    for (size_t i = 0; i < kWeaponCards; ++i) {
        const float y = CardY(i);
        cardPanels_[i] = Panel({kLeft, y}, {kCardWidth, kCardHeight}, kPanelColor);
        cardStripes_[i] = Panel({kLeft, y}, {6.0f, kCardHeight}, {1.0f, 1.0f, 1.0f, 0.9f});
        cardIcons_[i] = MakeCell({kLeft + 14.0f, y + 8.0f}, kCardIcon);
        cardNames_[i] = Label({kLeft + 94.0f, y + 4.0f}, 20.0f, true, {1.0f, 1.0f, 1.0f, 1.0f});
        cardStats_[i] = Label({kLeft + 94.0f, y + 34.0f}, 15.0f, false, {0.80f, 0.88f, 0.96f, 1.0f});
        for (size_t s = 0; s < kMaxSockets; ++s) {
            sockets_[i][s] = MakeCell(SocketPosition(i, s), kSocket);
        }
    }
    for (size_t i = 0; i < smallPanels_.size(); ++i) {
        const float x = kLeft + static_cast<float>(i) * (kSmallWidth + kCardGap);
        smallPanels_[i] = Panel({x, kSmallTop}, {kSmallWidth, kSmallHeight}, kPanelColor);
        smallNames_[i] = Label({x + 72.0f, kSmallTop + 24.0f}, 16.0f, true, {0.85f, 0.92f, 1.0f, 1.0f});
    }
    shieldCell_ = MakeCell({kLeft + 8.0f, kSmallTop + 8.0f}, 60.0f);
    supportCell_ = MakeCell({kLeft + kSmallWidth + kCardGap + 8.0f, kSmallTop + 8.0f}, 60.0f);
    for (size_t i = 0; i < headerCells_.size(); ++i) {
        headerCells_[i] = MakeCell({kGridLeft + static_cast<float>(i) * kHeaderStep, kPanelTop + 8.0f}, kHeaderCell);
    }
    for (size_t i = 0; i < gridCells_.size(); ++i) {
        gridCells_[i] = MakeCell(GridPosition(i), kCell);
    }
    for (size_t i = 0; i < tabPanels_.size(); ++i) {
        const float x = kRightLeft + kRightWidth - 12.0f - static_cast<float>(tabPanels_.size() - i) * (kTabWidth + 4.0f);
        tabPanels_[i] = Panel({x, kTabTop}, {kTabWidth, kTabHeight}, kCellColor);
        tabLabels_[i] = Label({x + kTabWidth * 0.5f, kTabTop + kTabHeight * 0.5f}, 16.0f, true, {0.85f, 0.92f, 1.0f, 1.0f});
        tabLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
    }
    for (size_t i = 0; i < buttonPanels_.size(); ++i) {
        const float width = (kRightWidth - 36.0f - 12.0f) / 2.0f;
        const float x = kGridLeft + static_cast<float>(i) * (width + 12.0f);
        const float y = kButtonTop + (i == 2 ? kButtonHeight + 6.0f : 0.0f);
        buttonPanels_[i] = Panel({i == 2 ? kGridLeft : x, y}, {width, kButtonHeight}, {0.08f, 0.26f, 0.30f, 0.95f});
        buttonLabels_[i] = Label({(i == 2 ? kGridLeft : x) + width * 0.5f, y + kButtonHeight * 0.5f}, 18.0f, true, {1.0f, 1.0f, 1.0f, 1.0f});
        buttonLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
    }
    hoverMark_ = Panel({0.0f, 0.0f}, {kCell, kCell}, {0.5f, 1.0f, 0.95f, 0.28f});
    ghost_ = Panel({0.0f, 0.0f}, {kCell, kCell}, {1.0f, 1.0f, 1.0f, 0.75f});
    ghost_->SetAnchorPoint({0.5f, 0.5f});
    tipPanel_ = Panel({0.0f, 0.0f}, {kTipWidth, 100.0f}, {0.0f, 0.008f, 0.018f, 1.0f});
    tipStripe_ = Panel({0.0f, 0.0f}, {5.0f, 100.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    tipTitle_ = Label({0.0f, 0.0f}, 18.0f, true, {1.0f, 1.0f, 1.0f, 1.0f});
    tipBody_ = Label({0.0f, 0.0f}, 15.0f, false, {0.85f, 0.92f, 1.0f, 1.0f});
    tipActions_ = Label({0.0f, 0.0f}, 15.0f, true, {1.0f, 0.86f, 0.36f, 1.0f});
}

void SurvivorItemScreen::SetLabel(myEngine::TextLabel& label, const std::string& text, const myEngine::Vector4& color, float size, bool bold)
{
    label.SetStyle(Style(size, bold, color));
    label.SetText(text.empty() ? std::string(" ") : text);
}

void SurvivorItemScreen::Show(const View& view)
{
    view_ = view;
    SetLabel(*title_, view_.title, {0.82f, 0.97f, 1.0f, 1.0f}, 32.0f, true);
    SetLabel(*help_, view_.help, {0.62f, 0.74f, 0.86f, 1.0f}, 16.0f, false);
    SetLabel(*gridTitle_, view_.gridTitle, {1.0f, 0.86f, 0.36f, 1.0f}, 20.0f, true);
    for (size_t i = 0; i < kWeaponCards; ++i) {
        const WeaponCard& card = view_.cards[i];
        cardPanels_[i]->SetColor(card.active ? myEngine::Vector4{0.05f, 0.11f, 0.16f, 0.95f} : kPanelColor);
        cardPanels_[i]->Update();
        const myEngine::Vector4 color = card.empty ? myEngine::Vector4{0.5f, 0.56f, 0.64f, 0.25f} : card.item.color;
        cardStripes_[i]->SetColor({color.x, color.y, color.z, card.empty ? 0.25f : 0.95f});
        cardStripes_[i]->SetSize({card.active ? 10.0f : 6.0f, kCardHeight});
        cardStripes_[i]->Update();
        FillCell(cardIcons_[i], card.empty ? nullptr : &card.item, {kLeft + 14.0f, CardY(i) + 8.0f}, kCardIcon, true);
        SetLabel(*cardNames_[i], card.name, card.empty ? myEngine::Vector4{0.5f, 0.56f, 0.64f, 1.0f} : card.item.color, 20.0f, true);
        SetLabel(*cardStats_[i], card.stats, {0.80f, 0.88f, 0.96f, 1.0f}, 15.0f, false);
        for (size_t s = 0; s < kMaxSockets; ++s) {
            const bool open = !card.empty && static_cast<int>(s) < card.sockets;
            const Item* chip = s < card.chips.size() ? &card.chips[s] : nullptr;
            FillCell(sockets_[i][s], chip, SocketPosition(i, s), kSocket, open);
        }
    }
    FillCell(shieldCell_, view_.shield.icon.empty() ? nullptr : &view_.shield, {kLeft + 8.0f, kSmallTop + 8.0f}, 60.0f, true);
    FillCell(supportCell_, view_.support.icon.empty() ? nullptr : &view_.support, {kLeft + kSmallWidth + kCardGap + 8.0f, kSmallTop + 8.0f}, 60.0f, true);
    SetLabel(*smallNames_[0], view_.shield.title, {0.85f, 0.92f, 1.0f, 1.0f}, 16.0f, true);
    SetLabel(*smallNames_[1], view_.support.title, {0.5f, 0.56f, 0.64f, 1.0f}, 16.0f, true);
    for (size_t i = 0; i < headerCells_.size(); ++i) {
        FillCell(headerCells_[i], i < view_.header.size() ? &view_.header[i] : nullptr, {kGridLeft + static_cast<float>(i) * kHeaderStep, kPanelTop + 8.0f},
                 kHeaderCell, false);
    }
    for (size_t i = 0; i < tabPanels_.size(); ++i) {
        if (i < view_.tabs.size()) {
            const bool active = view_.activeTab == view_.tabs[i].id;
            tabPanels_[i]->SetColor(active ? myEngine::Vector4{0.10f, 0.34f, 0.38f, 0.98f} : kCellColor);
            tabPanels_[i]->Update();
            SetLabel(*tabLabels_[i], view_.tabs[i].text, {0.85f, 0.92f, 1.0f, 1.0f}, 16.0f, true);
        }
    }
    for (size_t i = 0; i < buttonPanels_.size(); ++i) {
        if (i < view_.buttons.size()) {
            const bool enabled = view_.buttons[i].enabled;
            buttonPanels_[i]->SetColor(enabled ? myEngine::Vector4{0.08f, 0.26f, 0.30f, 0.95f} : myEngine::Vector4{0.04f, 0.06f, 0.08f, 0.9f});
            buttonPanels_[i]->Update();
            SetLabel(*buttonLabels_[i], view_.buttons[i].text, enabled ? myEngine::Vector4{1.0f, 1.0f, 1.0f, 1.0f} : myEngine::Vector4{0.5f, 0.56f, 0.64f, 1.0f}, 18.0f, true);
        }
    }
    const int maxScroll = view_.grid.size() <= kColumns * kRows ? 0 : static_cast<int>((view_.grid.size() - kColumns * kRows + kColumns - 1) / kColumns);
    scroll_ = (std::clamp)(scroll_, 0, maxScroll);
    LayoutGrid();
    tooltipDirty_ = true;
}

void SurvivorItemScreen::LayoutGrid()
{
    const size_t first = static_cast<size_t>(scroll_) * kColumns;
    for (size_t i = 0; i < gridCells_.size(); ++i) {
        const size_t index = first + i;
        FillCell(gridCells_[i], index < view_.grid.size() ? &view_.grid[index] : nullptr, GridPosition(i), kCell, view_.showEmptyCells);
    }
}

SurvivorItemScreen::Hit SurvivorItemScreen::HitTest(const myEngine::Vector2& mouse) const
{
    for (size_t i = 0; i < kWeaponCards; ++i) {
        for (size_t s = 0; s < kMaxSockets; ++s) {
            const myEngine::Vector2 p = SocketPosition(i, s);
            if (!view_.cards[i].empty && static_cast<int>(s) < view_.cards[i].sockets && Inside(mouse, p.x, p.y, kSocket, kSocket)) {
                return {Zone::Socket, static_cast<int>(i), static_cast<int>(s)};
            }
        }
        if (Inside(mouse, kLeft, CardY(i), kCardWidth, kCardHeight)) {
            return {Zone::Card, static_cast<int>(i), -1};
        }
    }
    if (Inside(mouse, kLeft, kSmallTop, kSmallWidth, kSmallHeight)) {
        return {Zone::Shield, 0, -1};
    }
    if (Inside(mouse, kLeft + kSmallWidth + kCardGap, kSmallTop, kSmallWidth, kSmallHeight)) {
        return {Zone::Support, 0, -1};
    }
    for (size_t i = 0; i < view_.header.size() && i < headerCells_.size(); ++i) {
        if (Inside(mouse, kGridLeft + static_cast<float>(i) * kHeaderStep, kPanelTop + 8.0f, kHeaderCell, kHeaderCell)) {
            return {Zone::Header, static_cast<int>(i), -1};
        }
    }
    for (size_t i = 0; i < gridCells_.size(); ++i) {
        const myEngine::Vector2 p = GridPosition(i);
        const size_t index = static_cast<size_t>(scroll_) * kColumns + i;
        if (index < view_.grid.size() && Inside(mouse, p.x, p.y, kCell, kCell)) {
            return {Zone::Grid, static_cast<int>(index), -1};
        }
    }
    for (size_t i = 0; i < tabPanels_.size() && i < view_.tabs.size(); ++i) {
        const myEngine::Vector2& p = tabPanels_[i]->GetPosition();
        if (Inside(mouse, p.x, p.y, kTabWidth, kTabHeight)) {
            return {Zone::Tab, static_cast<int>(i), -1};
        }
    }
    for (size_t i = 0; i < buttonPanels_.size() && i < view_.buttons.size(); ++i) {
        const myEngine::Vector2& p = buttonPanels_[i]->GetPosition();
        const myEngine::Vector2& size = buttonPanels_[i]->GetSize();
        if (Inside(mouse, p.x, p.y, size.x, size.y)) {
            return {Zone::Button, static_cast<int>(i), -1};
        }
    }
    return {};
}

const SurvivorItemScreen::Item* SurvivorItemScreen::ItemAt(const Hit& hit) const
{
    switch (hit.zone) {
    case Zone::Card:
        return hit.index >= 0 && !view_.cards[static_cast<size_t>(hit.index)].empty ? &view_.cards[static_cast<size_t>(hit.index)].item : nullptr;
    case Zone::Socket: {
        const auto& chips = view_.cards[static_cast<size_t>(hit.index)].chips;
        return static_cast<size_t>(hit.sub) < chips.size() ? &chips[static_cast<size_t>(hit.sub)] : nullptr;
    }
    case Zone::Shield:
        return &view_.shield;
    case Zone::Support:
        return &view_.support;
    case Zone::Header:
        return &view_.header[static_cast<size_t>(hit.index)];
    case Zone::Grid:
        return static_cast<size_t>(hit.index) < view_.grid.size() ? &view_.grid[static_cast<size_t>(hit.index)] : nullptr;
    default:
        return nullptr;
    }
}

myEngine::Vector2 SurvivorItemScreen::CenterOf(const Hit& hit) const
{
    switch (hit.zone) {
    case Zone::Card:
        // 札の真ん中はチップのマスを避ける。
        return {kLeft + kCardWidth - 40.0f, CardY(static_cast<size_t>(hit.index)) + 20.0f};
    case Zone::Socket: {
        const myEngine::Vector2 p = SocketPosition(static_cast<size_t>(hit.index), static_cast<size_t>(hit.sub));
        return {p.x + kSocket * 0.5f, p.y + kSocket * 0.5f};
    }
    case Zone::Grid: {
        const size_t visible = static_cast<size_t>(hit.index) - static_cast<size_t>(scroll_) * kColumns;
        const myEngine::Vector2 p = GridPosition(visible);
        return {p.x + kCell * 0.5f, p.y + kCell * 0.5f};
    }
    case Zone::Tab: {
        const myEngine::Vector2& p = tabPanels_[static_cast<size_t>(hit.index)]->GetPosition();
        return {p.x + kTabWidth * 0.5f, p.y + kTabHeight * 0.5f};
    }
    case Zone::Button: {
        const myEngine::Vector2& p = buttonPanels_[static_cast<size_t>(hit.index)]->GetPosition();
        const myEngine::Vector2& size = buttonPanels_[static_cast<size_t>(hit.index)]->GetSize();
        return {p.x + size.x * 0.5f, p.y + size.y * 0.5f};
    }
    default:
        return {};
    }
}

void SurvivorItemScreen::RefreshTooltip()
{
    const Item* item = ItemAt(hover_);
    showTip_ = item != nullptr && !item->title.empty() && !dragging_;
    if (!showTip_) {
        return;
    }
    SetLabel(*tipTitle_, item->title, item->color, 18.0f, true);
    SetLabel(*tipBody_, item->body, {0.85f, 0.92f, 1.0f, 1.0f}, 15.0f, false);
    SetLabel(*tipActions_, item->actions, {1.0f, 0.86f, 0.36f, 1.0f}, 15.0f, true);
    tipStripe_->SetColor({item->color.x, item->color.y, item->color.z, 1.0f});
    const size_t bodyLines = LineCount(item->body);
    const size_t actionLines = LineCount(item->actions);
    tipHeight_ = 36.0f + static_cast<float>(bodyLines) * 22.0f + static_cast<float>(actionLines) * 22.0f + (actionLines > 0 ? 8.0f : 0.0f);
}

SurvivorItemScreen::Event SurvivorItemScreen::UpdatePointer(const myEngine::Vector2& mouse, bool leftDown, bool rightTrigger, int wheel)
{
    Event event{};
    if (wheel != 0) {
        const int maxScroll = view_.grid.size() <= kColumns * kRows ? 0 : static_cast<int>((view_.grid.size() - kColumns * kRows + kColumns - 1) / kColumns);
        const int next = (std::clamp)(scroll_ + (wheel > 0 ? -1 : 1), 0, maxScroll);
        if (next != scroll_) {
            scroll_ = next;
            LayoutGrid();
            tooltipDirty_ = true;
        }
    }
    const Hit hit = HitTest(mouse);
    if (!(hit == hover_)) {
        hover_ = hit;
        tooltipDirty_ = true;
    }
    if (leftDown && !leftWas_) {
        if (hit.zone == Zone::Tab) {
            event = {EventType::Press, hit, {}};
        } else if (hit.zone == Zone::Button) {
            if (view_.buttons[static_cast<size_t>(hit.index)].enabled) {
                event = {EventType::Press, hit, {}};
            }
        } else {
            pressed_ = hit;
            pressAt_ = mouse;
        }
    }
    if (leftDown && pressed_.zone != Zone::None && !dragging_) {
        const Item* item = ItemAt(pressed_);
        const float dx = mouse.x - pressAt_.x, dy = mouse.y - pressAt_.y;
        if (item && item->draggable && dx * dx + dy * dy > 36.0f) {
            dragging_ = true;
            ghost_->SetTexture(item->icon);
            ghost_->SetColor({item->color.x, item->color.y, item->color.z, 0.8f});
            tooltipDirty_ = true;
        }
    }
    if (!leftDown && leftWas_ && pressed_.zone != Zone::None) {
        if (dragging_) {
            if (!(hit == pressed_) && hit.zone != Zone::None) {
                event = {EventType::Drop, pressed_, hit};
            }
        } else if (hit == pressed_) {
            event = {EventType::Click, pressed_, {}};
        }
        dragging_ = false;
        pressed_ = {};
        tooltipDirty_ = true;
    }
    if (rightTrigger && !dragging_ && hit.zone != Zone::None) {
        event = {EventType::RightClick, hit, {}};
    }
    leftWas_ = leftDown;

    if (tooltipDirty_) {
        RefreshTooltip();
        tooltipDirty_ = false;
    }
    // 合わせているマスの印。
    showHoverMark_ = false;
    if (hit.zone == Zone::Grid || hit.zone == Zone::Socket) {
        const myEngine::Vector2 center = CenterOf(hit);
        const float size = hit.zone == Zone::Grid ? kCell : kSocket;
        hoverMark_->SetPosition({center.x - size * 0.5f, center.y - size * 0.5f});
        hoverMark_->SetSize({size, size});
        hoverMark_->Update();
        showHoverMark_ = true;
    }
    if (dragging_) {
        ghost_->SetPosition(mouse);
        ghost_->Update();
    }
    if (showTip_) {
        // マウスの右下に出し、画面からはみ出すなら反対側へ寄せる。
        const float x = mouse.x + 18.0f + kTipWidth > 1250.0f ? mouse.x - 18.0f - kTipWidth : mouse.x + 18.0f;
        const float y = (std::clamp)(mouse.y + 12.0f, 20.0f, 700.0f - tipHeight_);
        tipPanel_->SetPosition({x, y});
        tipPanel_->SetSize({kTipWidth, tipHeight_});
        tipPanel_->Update();
        tipStripe_->SetPosition({x, y});
        tipStripe_->SetSize({5.0f, tipHeight_});
        tipStripe_->Update();
        tipTitle_->SetPosition({x + 14.0f, y + 6.0f});
        tipBody_->SetPosition({x + 14.0f, y + 34.0f});
        const Item* item = ItemAt(hover_);
        tipActions_->SetPosition({x + 14.0f, y + 40.0f + static_cast<float>(item ? LineCount(item->body) : 0) * 22.0f});
    }
    return event;
}

void SurvivorItemScreen::Draw()
{
    backdrop_->Draw();
    rightPanel_->Draw();
    title_->Draw();
    help_->Draw();
    gridTitle_->Draw();
    for (size_t i = 0; i < kWeaponCards; ++i) {
        cardPanels_[i]->Draw();
        cardStripes_[i]->Draw();
        DrawCell(cardIcons_[i]);
        cardNames_[i]->Draw();
        cardStats_[i]->Draw();
        for (Cell& socket : sockets_[i]) {
            DrawCell(socket);
        }
    }
    for (auto& panel : smallPanels_) {
        panel->Draw();
    }
    DrawCell(shieldCell_);
    DrawCell(supportCell_);
    for (auto& name : smallNames_) {
        name->Draw();
    }
    for (size_t i = 0; i < view_.header.size() && i < headerCells_.size(); ++i) {
        DrawCell(headerCells_[i]);
    }
    for (Cell& cell : gridCells_) {
        DrawCell(cell);
    }
    if (showHoverMark_) {
        hoverMark_->Draw();
    }
    for (size_t i = 0; i < view_.tabs.size() && i < tabPanels_.size(); ++i) {
        tabPanels_[i]->Draw();
        tabLabels_[i]->Draw();
    }
    for (size_t i = 0; i < view_.buttons.size() && i < buttonPanels_.size(); ++i) {
        buttonPanels_[i]->Draw();
        buttonLabels_[i]->Draw();
    }
    if (dragging_) {
        ghost_->Draw();
    }
    if (showTip_) {
        tipPanel_->Draw();
        tipStripe_->Draw();
        tipTitle_->Draw();
        tipBody_->Draw();
        if (ItemAt(hover_) && !ItemAt(hover_)->actions.empty()) {
            tipActions_->Draw();
        }
    }
}
} // namespace gameplay
