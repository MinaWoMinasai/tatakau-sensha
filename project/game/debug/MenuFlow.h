#pragma once
#include <string>
#include "DirectX/engine/common/DeveloperTools.h"

namespace myEngine {
class Input;
}

/// @brief 画面の出入りを、キーボードだけ・コントローラーだけで自動でたどる確認（計画書 p11）。
/// @note 開発用の構成だけで動く。MYENGINE_MENU_FLOW に手順を書いたファイルを指定すると、入力を手順のとおりに置き換える。
/// 1行に1つ：wait 画面 [フレーム]／expect 画面 [中身]／key キー／hold キー フレーム／pad ボタン／padhold ボタン フレーム／
/// frames 名前 コマ数（描いたコマを毎回撮る。待たずに次へ進む）／stick L|R x y フレーム／aim フレーム・aimpad フレーム（目標を狙う）／go フレーム・gopad フレーム（目標へ向かう。画面が変わったら終わる）／mouse x y／click／sleep フレーム／shot 名前／blur／do 名前（画面の側の確認用の命令）／quit。
namespace menuflow {
#if MYENGINE_DEVELOPER_TOOLS
bool IsActive();
/// @brief 今の画面の名前と、選んでいる項目などの中身を知らせる。毎フレーム呼んでよい。
void SetScreen(const std::string& name, const std::string& detail = {});
/// @brief 写真を撮る要求を取り出す。無ければ false。
bool TakeShot(std::string& name);
/// @brief 目標の画面の位置と、自機の画面の位置を知らせる（aim・go の手順で使う）。
void SetTarget(float selfX, float selfY, float targetX, float targetY);
/// @brief 確認用の命令（do）を取り出す。name の命令が来ていたら true。
bool TakeCommand(const char* name);
/// @brief 手順 frames 名前 コマ数 の間、描いたコマごとに写真の場所（generated/menu_flow/名前/000.png）を返す。描いたコマごとに1回だけ呼ぶ。
bool TakeFrameShot(std::string& path);
/// @brief frames で撮っている間だけ、記録に1行足す（各コマの値など）。
void Note(const std::string& text);
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
inline void SetTarget(float, float, float, float) {}
inline bool TakeCommand(const char*)
{
    return false;
}
inline bool TakeFrameShot(std::string&)
{
    return false;
}
inline void Note(const std::string&) {}
inline void Step(myEngine::Input&) {}
#endif
} // namespace menuflow
