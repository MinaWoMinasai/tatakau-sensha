#pragma once

#include "PostEffectSettings.h"

#include <string>
#include <string_view>

class GameModuleRegistry;

namespace myEngine {
class DirectXCommon;
class Input;
} // namespace myEngine

/// @brief Applicationの起動とメインループへ、実行ファイルごとの処理を差し込むための窓口。
/// 既定の実装は何もしない。呼び出しはすべてメインスレッドから行う。
class IApplicationHooks {
public:
    virtual ~IApplicationHooks() = default;

    /// @brief この実行ファイルで選べるゲームモジュールを登録する。
    /// @return 必要な登録を完了できた場合true。falseならメインループへ入らない。
    virtual bool RegisterGameModules(GameModuleRegistry& registry) = 0;
    /// @brief プロジェクトで指定したモジュールを作れなかったときに使うモジュールIDを返す。
    virtual std::string_view GetFallbackGameModuleId() const = 0;
    /// @brief 起動シーンを作れなかったときに使うシーン名を返す。
    virtual std::string_view GetFallbackSceneName() const = 0;

    /// @brief 先に1フレーム表示してから残りの初期化を少しずつ進めるシーン名を返す。空なら一度に初期化する。
    virtual std::string_view GetProgressiveStartupSceneName() const
    {
        return {};
    }

    /// @brief MYENGINE_STARTUP_AUTOTESTで、表示できたら終了とみなすシーン名を返す。空なら自動終了しない。
    virtual std::string_view GetStartupValidationSceneName() const
    {
        return {};
    }

    /// @brief エンジン付属の素材（シェーダー・既定テクスチャ）を置いたディレクトリを返す。空なら作業ディレクトリの "resources" を使う。
    virtual std::string_view GetEngineResourceRoot() const
    {
        return {};
    }

    /// @brief D3D12デバイスの作成前に呼ぶ。デバイスを必要としないファイル読み込みを先に始められる。
    virtual void OnBeforeEngineInitialize(const std::string&) {}

    /// @brief 音声の初期化が終わった後に、ゲーム全体で使う素材（音声）を読み込む。
    /// D3D12の初期化と並行させるため別スレッドで呼ぶ。描画やシーンには触れない。
    virtual void OnLoadSharedAssets() {}

    /// @brief ウィンドウが非アクティブでも止めずに進める自動検証の実行中か判定する。メインループの開始時に一度だけ呼ぶ。
    virtual bool IsBackgroundValidationActive() const
    {
        return false;
    }

    /// @brief メインループに入る直前に呼ぶ。
    virtual void OnMainLoopStarting() {}

    /// @brief メインループを抜けた直後、エンジンの終了処理の前に呼ぶ。GPU資源を持つ場合はここで解放する。
    virtual void OnMainLoopFinished() {}

    /// @brief フレームの入力状態を確定した直後に呼ぶ。検証用の入力の差し替えに使う。
    virtual void OnFrameInputReady(myEngine::Input&) {}

    /// @brief シーンの更新の直前に呼ぶ。
    virtual void OnBeforeSceneUpdate() {}

    /// @brief シーンの更新の直後に呼ぶ。
    virtual void OnAfterSceneUpdate() {}

    /// @brief 画面のグレースケール表示の有無を、必要なら上書きする。
    /// @param deltaTime シーンの経過時間。
    /// @param showcase 現在の開発者用ショーケースの状態。
    /// @param grayscale エンジンが決めた値。上書きする場合は書き換える。
    virtual void AdjustGrayscale(float, const myEngine::DeveloperShowcaseState&, bool&) {}

    /// @brief 段階的な起動中の簡易描画で、画面を確定する直前に呼ぶ。
    /// @param firstFrame 起動後に最初に表示するフレームならtrue。
    virtual void OnStartupFrameRecorded(myEngine::DirectXCommon&, bool) {}

    /// @brief 段階的な起動中の簡易描画で、画面を確定した直後に呼ぶ。
    virtual void OnStartupFramePresented(myEngine::DirectXCommon&) {}
};
