#pragma once
#include <string>
#include <string_view>

namespace myEngine {

/// @brief エンジン付属の素材（シェーダー・既定テクスチャ）と、ゲームの素材の置き場所を管理する。
/// どちらも既定は作業ディレクトリ基準の "resources"。パスの区切りは常に '/' で組み立てる。
/// 設定はエンジンの初期化前（バックグラウンド読み込みの開始前）に、メインスレッドから一度だけ行う。
class ResourcePaths {
public:
    /// @brief エンジン付属の素材を置いたディレクトリを設定する。空なら既定値のままにする。
    static void SetEngineRoot(std::string_view root);
    /// @brief ゲームの素材を置いたディレクトリを設定する。空なら既定値のままにする。
    static void SetGameRoot(std::string_view root);

    /// @brief エンジン付属の素材を置いたディレクトリを返す。
    static const std::string& EngineRoot();
    /// @brief ゲームの素材を置いたディレクトリを返す。
    static const std::string& GameRoot();

    /// @brief エンジン付属のシェーダーファイルのパスを返す。例: L"Object3d.VS.hlsl" → L"resources/shaders/Object3d.VS.hlsl"。
    static std::wstring EngineShader(std::wstring_view fileName);
    /// @brief テクスチャが無いときに使う白いテクスチャのパスを返す。
    static const std::string& DefaultWhiteTexture();
};

} // namespace myEngine
