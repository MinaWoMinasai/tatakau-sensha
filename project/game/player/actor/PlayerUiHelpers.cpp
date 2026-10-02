#include "PlayerUiHelpers.h"

namespace playerui {
/// @brief JSONオブジェクトのx・yを読み、欠けた成分にはfallbackの値を使う。
cg2::Vector2 ReadVector2Object(const nlohmann::json& json, const cg2::Vector2& fallback)
{
    if (!json.is_object()) {
        return fallback;
    }
    cg2::Vector2 value = fallback;
    if (json.contains("x") && json["x"].is_number())
        value.x = json["x"].get<float>();
    if (json.contains("y") && json["y"].is_number())
        value.y = json["y"].get<float>();
    return value;
}

/// @brief 2成分の値をx・yを持つJSONオブジェクトへ変換する。
nlohmann::json WriteVector2Object(const cg2::Vector2& value)
{
    return {{"x", value.x}, {"y", value.y}};
}

/// @brief 文字を設定する。
void SetLabel(std::unique_ptr<cg2::TextLabel>& label, cg2::SpriteCommon* spriteCommon, const std::string& text,
              const cg2::Vector2& position, const cg2::TextStyle& style)
{
    if (!label) {
        label = std::make_unique<cg2::TextLabel>();
        label->Initialize(spriteCommon, text, style);
    } else {
        label->SetStyle(style);
        label->SetText(text);
    }
    label->SetPosition(position);
}

/// @brief ベクトル4を保存用のJSONへ変換する。
nlohmann::json Vector4ToJson(const cg2::Vector4& value)
{
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}
} // namespace playerui
