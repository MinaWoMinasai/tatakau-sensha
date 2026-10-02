#pragma once
#include "TextLabel.h"
#include "SpriteCommon.h"
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

// PlayerのHUDと進化画面で共有する、状態を持たない表示・JSON変換関数。
namespace playerui {
/// @brief JSONオブジェクトのx・yを読み、欠けた成分にはfallbackの値を使う。
cg2::Vector2 ReadVector2Object(const nlohmann::json& json, const cg2::Vector2& fallback);

/// @brief 2成分の値をx・yを持つJSONオブジェクトへ変換する。
nlohmann::json WriteVector2Object(const cg2::Vector2& value);

/// @brief 未生成の文字表示を作り、既存の文字表示には内容・外観・位置を反映する。
/// @param label 呼び出し側が所有する文字表示。空なら生成した表示を格納する。
/// @param spriteCommon 初回生成で使うスプライト描画の共通基盤。
void SetLabel(std::unique_ptr<cg2::TextLabel>& label, cg2::SpriteCommon* spriteCommon, const std::string& text,
              const cg2::Vector2& position, const cg2::TextStyle& style);

/// @brief 4成分を順番がx・y・z・wのJSON配列へ変換する。
nlohmann::json Vector4ToJson(const cg2::Vector4& value);
} // namespace playerui
