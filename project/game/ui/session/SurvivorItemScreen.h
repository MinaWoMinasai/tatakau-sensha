#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"

namespace gameplay {
/// @brief 育つ戦車の試作の、アイコン中心の持ち物の画面（計画書 p3 の X4）。持ち物・拠点・休憩部屋で共通に使う。
/// @note 左に武器3枚（下にチップのマス）とシールド・補助、右に素材の列と持ち物のマス。マウスを合わせると説明の札が出て、
/// ドラッグで移し、右クリックやボタンは事象として呼ぶ側へ返す（何が起きるかは呼ぶ側が決める）。
/// 文字と板を変えるのは更新の段階だけ（Show と UpdatePointer）。Draw では作り直さない。
class SurvivorItemScreen {
public:
    static constexpr size_t kWeaponCards = 3;
    static constexpr size_t kMaxSockets = 6;
    static constexpr size_t kColumns = 10;
    static constexpr size_t kRows = 5;

    /// @brief マス1つの中身。kind と id は呼ぶ側が事象を受けるときに使う。title が空なら説明の札は出ない。
    struct Item {
        std::string icon;
        myEngine::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
        /// @brief マスの右下の数（負なら出さない）。
        int count = -1;
        /// @brief 暗く出す（作れない・もう通った段など）。
        bool dim = false;
        /// @brief ドラッグできるか。
        bool draggable = false;
        int kind = 0;
        int id = -1;
        std::string title;
        std::string body;
        std::string actions;
    };
    struct WeaponCard {
        bool empty = true;
        bool active = false;
        std::string name;
        std::string stats;
        Item item;
        /// @brief チップのマスの数と、入っているチップ。
        int sockets = 0;
        std::vector<Item> chips;
    };
    struct Button {
        std::string text;
        int id = 0;
        bool enabled = true;
    };
    struct View {
        std::string title;
        std::string help;
        std::string gridTitle;
        std::array<WeaponCard, kWeaponCards> cards{};
        Item shield;
        Item support;
        /// @brief 右の上の列（お金・素材・弾の数）。
        std::vector<Item> header;
        std::vector<Item> grid;
        /// @brief 空のマスの枠も出すか（派生の木では出さない）。
        bool showEmptyCells = true;
        std::vector<Button> tabs;
        int activeTab = -1;
        std::vector<Button> buttons;
    };

    /// @brief 画面の中のどこか。
    enum class Zone {
        None,
        Card,
        Socket,
        Shield,
        Support,
        Header,
        Grid,
        Tab,
        Button
    };
    struct Hit {
        Zone zone = Zone::None;
        int index = -1;
        int sub = -1;

        bool operator==(const Hit&) const = default;
    };
    enum class EventType {
        None,
        /// @brief 押して同じ所で放した（source）。
        Click,
        /// @brief ドラッグして別の所（target）で放した。
        Drop,
        /// @brief 右クリックした（source）。
        RightClick,
        /// @brief タブかボタンを押した（source）。
        Press
    };
    struct Event {
        EventType type = EventType::None;
        Hit source;
        Hit target;
    };

    /// @brief 板と文字の入れ物を作り、アイコンの画像を読み込む。
    void Initialize(const std::vector<std::string>& icons);
    /// @brief 中身を入れ替える。説明の札は作り直す。更新の段階で、中身が変わったときだけ呼ぶ。
    void Show(const View& view);
    /// @brief マウスの状態を受けて、選んでいるマス・説明の札・ドラッグを更新し、起きた事象を返す。更新の段階で毎フレーム呼ぶ。
    Event UpdatePointer(const myEngine::Vector2& mouse, bool leftDown, bool rightTrigger, int wheel);
    void Draw();
    /// @brief 今マウスが合っている所。
    const Hit& GetHover() const
    {
        return hover_;
    }
    /// @brief 中身の Item（無ければ nullptr）。
    const Item* ItemAt(const Hit& hit) const;
    /// @brief 所の中心の座標（自動確認でマウスの代わりに使う）。
    myEngine::Vector2 CenterOf(const Hit& hit) const;

private:
    Hit HitTest(const myEngine::Vector2& mouse) const;
    void LayoutGrid();
    void RefreshTooltip();
    static void SetLabel(myEngine::TextLabel& label, const std::string& text, const myEngine::Vector4& color, float size, bool bold);

    /// @brief マス1つの見た目（枠・アイコン・数）。
    struct Cell {
        std::unique_ptr<myEngine::Sprite> frame;
        std::unique_ptr<myEngine::Sprite> icon;
        std::unique_ptr<myEngine::TextLabel> count;
        std::string countShown;
        bool showIcon = false;
        bool showFrame = true;
        bool showCount = false;
    };
    Cell MakeCell(const myEngine::Vector2& position, float size);
    void FillCell(Cell& cell, const Item* item, const myEngine::Vector2& position, float size, bool emptyFrame);
    void DrawCell(Cell& cell);

    View view_{};
    Hit hover_{};
    Hit pressed_{};
    bool leftWas_ = false;
    bool dragging_ = false;
    bool tooltipDirty_ = true;
    int scroll_ = 0;
    myEngine::Vector2 pressAt_{};

    std::unique_ptr<myEngine::Sprite> backdrop_;
    std::unique_ptr<myEngine::Sprite> rightPanel_;
    std::unique_ptr<myEngine::TextLabel> title_;
    std::unique_ptr<myEngine::TextLabel> help_;
    std::unique_ptr<myEngine::TextLabel> gridTitle_;
    std::array<std::unique_ptr<myEngine::Sprite>, kWeaponCards> cardPanels_;
    std::array<std::unique_ptr<myEngine::Sprite>, kWeaponCards> cardStripes_;
    std::array<Cell, kWeaponCards> cardIcons_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kWeaponCards> cardNames_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kWeaponCards> cardStats_;
    std::array<std::array<Cell, kMaxSockets>, kWeaponCards> sockets_;
    std::array<std::unique_ptr<myEngine::Sprite>, 2> smallPanels_;
    Cell shieldCell_;
    Cell supportCell_;
    std::array<std::unique_ptr<myEngine::TextLabel>, 2> smallNames_;
    std::array<Cell, 10> headerCells_;
    std::array<Cell, kColumns * kRows> gridCells_;
    std::array<std::unique_ptr<myEngine::Sprite>, 4> tabPanels_;
    std::array<std::unique_ptr<myEngine::TextLabel>, 4> tabLabels_;
    std::array<std::unique_ptr<myEngine::Sprite>, 3> buttonPanels_;
    std::array<std::unique_ptr<myEngine::TextLabel>, 3> buttonLabels_;
    std::unique_ptr<myEngine::Sprite> hoverMark_;
    bool showHoverMark_ = false;
    std::unique_ptr<myEngine::Sprite> ghost_;
    std::unique_ptr<myEngine::Sprite> tipPanel_;
    std::unique_ptr<myEngine::Sprite> tipStripe_;
    std::unique_ptr<myEngine::TextLabel> tipTitle_;
    std::unique_ptr<myEngine::TextLabel> tipBody_;
    std::unique_ptr<myEngine::TextLabel> tipActions_;
    bool showTip_ = false;
    float tipHeight_ = 0.0f;
    // アイコンの画像を読み込んでおくための板（使い回さない）。
    std::vector<std::unique_ptr<myEngine::Sprite>> preloaded_;
};
} // namespace gameplay
