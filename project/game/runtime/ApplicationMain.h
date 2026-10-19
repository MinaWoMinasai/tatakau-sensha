#pragma once

class IApplicationHooks;

/// @brief コマンドライン引数とカレントディレクトリを整え、Applicationで初期化・メインループ・終了処理を行う。
/// 各ゲームのWinMainから呼ぶ。hooksはこの関数から戻るまで有効に保つ。
/// @return プロセスの終了コード。初期化に失敗した場合は-1。
int RunApplication(IApplicationHooks& hooks);
