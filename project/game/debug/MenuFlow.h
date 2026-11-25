#pragma once
#include <string>
#include "DirectX/engine/common/DeveloperTools.h"

namespace myEngine {
class Input;
}

/// @brief 画面の出入りを、キーボードだけ・コントローラーだけで自動でたどる確認（計画書 p11）。
/// @note 開発用の構成だけで動く。MYENGINE_MENU_FLOW に手順を書いたファイルを指定すると、入力を手順のとおりに置き換える。
/// 1行に1つ：wait 画面 [フレーム]／expect 画面 [中身]／key キー／hold キー フレーム／pad ボタン／padhold ボタン フレーム／
/// stick L|R x y フレーム／mouse x y／click／sleep フレーム／shot 名前／blur／do 名前（画面の側の確認用の命令）／quit。
namespace menuflow {
#if MYENGINE_DEVELOPER_TOOLS
bool IsActive();
/// @brief 今の画面の名前と、選んでいる項目などの中身を知らせる。毎フレーム呼んでよい。
void SetScreen(const std::string& name, const std::string& detail = {});
/// @brief 写真を撮る要求を取り出す。無ければ false。
bool TakeShot(std::string& name);
/// @brief 確認用の命令（do）を取り出す。name の命令が来ていたら true。
bool TakeCommand(const char* name);
/// @brief 1フレーム進める。入力を読んだ直後に呼ぶ。
void Step(myEngine::Input& input);
#else
inline bool IsActive()
{
    return false;
}
inline void SetScreen(const std::string&, const std::string& = {}) {}
inline bool TakeShot(std::string&)
{
    return false;
}
inline bool TakeCommand(const char*)
{
    return false;
}
inline void Step(myEngine::Input&) {}
#endif
} // namespace menuflow
