#pragma once
#include "game/ui/session/NeonMenuKit.h"

namespace gameplay {
/// @brief 設定の画面（計画書 p11 の R2、モックの形）。上下でえらび、左右でかえる。変えるとすぐ効き、保存する。タイトルと止める画面で使う。
class SettingsScreen {
public:
    enum class Result { None, Closed, ReplayTutorial };

    /// @param confirmReplay 「説明をもう一度」を2回きめるか（遊んでいる途中では、今の遊びが終わるため）。
    void Open(const MenuInput& input, bool confirmReplay);
    Result Update(float dt, const MenuInput& input);
    void Queue(NeonMenuKit& kit);
    bool IsOpen() const
    {
        return open_;
    }
    /// @brief 選ぶ行・値が、今の更新で変わったか（音を鳴らす）。
    bool Moved() const
    {
        return moved_;
    }
    /// @brief 確認用：選んでいる行と、その値。
    std::string Describe() const;

private:
    /// @brief 値を変える。wrap なら、端で反対へ戻る（決める操作で回す）。
    void Adjust(int row, int direction, bool wrap);
    void Apply(int row);
    bool open_ = false, moved_ = false, pad_ = false, confirmReplay_ = false, armed_ = false;
    int selected_ = 0;
    float age_ = 0.0f, shake_ = 0.0f;
};
} // namespace gameplay
