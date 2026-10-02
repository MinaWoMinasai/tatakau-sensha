#pragma once
#define NOMINMAX
#include <memory>
#include <string>
#include "Struct.h"

namespace cg2 {

/// @brief 文字のフォント・色・縁取りなど、文字テクスチャ生成時の外観を指定する。
struct TextStyle {
    std::string fontFamily = "Meiryo";
    std::string fontPath;
    int fontWeight = 400;
    float fontSize = 42.0f;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 outlineColor = {0.0f, 0.0f, 0.0f, 0.75f};
    float outlineThickness = 2.0f;
    float padding = 8.0f;
    // 全体のフォント設定がアウトラインを上書きしている場合でも、
    // HUD上で個別に可読性を確保したい文字だけは指定値を優先する。
    bool preserveOutline = false;
};

/// @brief 指定した文字範囲に別のフォントを割り当てる設定を表す。
struct TextFontOverride {
    bool enabled = false;
    std::string fontFamily;
    std::string fontPath;
    int fontWeight = 400;
    bool overrideOutline = false;
    Vector4 outlineColor{0.0f, 0.0f, 0.0f, 0.0f};
    float outlineThickness = 0.0f;
};

struct TextRendererFontStore;

/// @brief DirectWriteで文字を画像化し、同じ内容のテクスチャを再利用する。
class TextRenderer {
public:
#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 文字テクスチャ取得処理の所要時間とキャッシュ利用状況を記録する。
    struct GetOrCreateTextureProfile {
        bool cacheFileExisted = false;
        bool generatedPng = false;
    };
#endif

    /// @brief 共有インスタンスを返す。呼び出し側は取得したポインターをdeleteしない。
    static TextRenderer* GetInstance();
    /// @brief 利用終了時の資源と状態を解放する。
    void Finalize();

    /// @brief テクスチャを取得し、存在しない場合は生成する。
    std::string GetOrCreateTexture(const std::string& utf8Text, const TextStyle& style);
    /// @brief 文字範囲ごとのフォント上書き設定を設定する。
    void SetFontOverride(const TextFontOverride& fontOverride);
    /// @brief 文字範囲ごとのフォント上書き設定を返す。
    const TextFontOverride& GetFontOverride() const
    {
        return fontOverride_;
    }
    /// @brief フォントRevisionを返す。
    unsigned long long GetFontRevision() const
    {
        return fontRevision_;
    }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 前回のGetOrCreateテクスチャ設定を返す。
    const GetOrCreateTextureProfile& GetLastGetOrCreateTextureProfile() const
    {
        return lastGetOrCreateTextureProfile_;
    }
#endif

private:
    /// @brief インスタンスの初期値と利用先を設定する。
    TextRenderer();
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    /// @brief Initializedを必要な状態を用意する。
    void EnsureInitialized();
    /// @brief UTF-8文字列をWindows API用のワイド文字列へ変換する。
    std::wstring Utf8ToWide(const std::string& text) const;
    /// @brief キャッシュパスを組み立てる。
    std::string BuildCachePath(const std::string& utf8Text, const TextStyle& style) const;
    /// @brief 文字Pngを保存する。
    bool SaveTextPng(const std::wstring& text, const TextStyle& style, const std::string& path);
    /// @brief 外観を解決する。
    TextStyle ResolveStyle(const TextStyle& style) const;

    bool initialized_ = false;
    unsigned long long gdiplusToken_ = 0;
    std::unique_ptr<TextRendererFontStore> fontStore_;
    TextFontOverride fontOverride_{};
    unsigned long long fontRevision_ = 1;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    GetOrCreateTextureProfile lastGetOrCreateTextureProfile_{};
#endif
};

} // namespace cg2
