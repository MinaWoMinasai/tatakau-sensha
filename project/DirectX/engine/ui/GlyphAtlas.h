#pragma once
#include "Struct.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <d3d12.h>

namespace myEngine {

class DirectXCommon;
class DynamicTexture;
class SrvManager;
struct GlyphAtlasFonts;

/// @brief 文字を1文字ずつ GDI+ で描き、大きな画像（ページ）に詰めて使い回す（グリフ・アトラス）。
/// 日付や数値のように毎回変わる文字でも、新しい文字が出たときだけ描くので、テクスチャが増え続けない。
///
/// 文字の画像は「白い文字 + 黒い縁取り」で描く。表示するときに色を掛ければ、文字はその色、縁取りは黒のままになる。
/// ページは CPU 側の画素と、GPU 側の DynamicTexture の両方を持つ。新しい文字を描いたページは、Upload で GPU へ送る。
/// メインスレッドから使う。Upload は描画の途中（PreDraw〜PostDraw の間）ではなく、Update などから呼ぶ。
class GlyphAtlas {
public:
    /// @brief 文字の見た目（フォントと大きさ）。
    struct Style {
        // システムのフォント名（fontPath が空のとき使う）。
        std::string fontFamily = "Meiryo";
        // フォントファイル（UTF-8 のパス）。指定すればこちらを使う。
        std::string fontPath;
        // 文字の大きさ（em の高さ、ピクセル）。
        float size = 24.0f;
        bool bold = false;
        // 縁取りの太さ（ピクセル）。0 なら縁取りなし。
        float outline = 2.0f;
    };

    /// @brief アトラスの中の1文字。
    struct Glyph {
        // ページの番号と、ページの中の位置（UV、0〜1）。描く画素が無い文字（空白）は size が 0。
        uint32_t page = 0;
        Vector2 uvMin = {0.0f, 0.0f};
        Vector2 uvMax = {0.0f, 0.0f};
        // 画像の大きさ（ピクセル）と、ペンの位置（行の左上）から画像の左上までのずれ。
        Vector2 size = {0.0f, 0.0f};
        Vector2 offset = {0.0f, 0.0f};
        // 次の文字までの幅（ピクセル）。
        float advance = 0.0f;
        // ページの中の画素の位置（テスト・確認用）。
        uint32_t pixelX = 0;
        uint32_t pixelY = 0;
    };

    /// @brief 並べた1文字（Layout の結果）。
    struct PlacedGlyph {
        const Glyph* glyph = nullptr;
        char32_t codepoint = 0;
        // 画像の左上の位置（文字列の左上を原点とするピクセル）。
        Vector2 position = {0.0f, 0.0f};
    };

    /// @brief 文字列を並べた結果。
    struct TextLayout {
        std::vector<PlacedGlyph> glyphs;
        // 文字列全体の幅と高さ（ピクセル）。
        Vector2 size = {0.0f, 0.0f};
        int lineCount = 0;
    };

    GlyphAtlas();
    ~GlyphAtlas();
    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    /// @brief 準備する。dxCommon と srvManager が nullptr なら、GPU のページを作らない（CPU だけで使う）。
    /// @param pageSize 1ページの幅と高さ（ピクセル）。
    /// @return GDI+ を始められたらtrue。
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t pageSize = 1024);

    /// @brief 見た目を登録し、その番号を返す。同じ見た目なら同じ番号。フォントを用意できなければ -1。
    int RegisterStyle(const Style& style);

    /// @brief 文字を返す。まだ描いていなければ、ここで描いてページに詰める。見た目の番号が無効なら nullptr。
    const Glyph* GetGlyph(int style, char32_t codepoint);

    /// @brief 1行の高さ（ピクセル）。
    float LineHeight(int style) const;

    /// @brief UTF-8 の文字列を並べる。"\n" で改行し、wrapWidth が 0 より大きければ、その幅を超える前で折り返す（文字単位）。
    TextLayout Layout(int style, std::string_view utf8, float wrapWidth = 0.0f);
    /// @brief Layout の大きさだけを返す。
    Vector2 MeasureText(int style, std::string_view utf8, float wrapWidth = 0.0f);

    /// @brief 新しい文字を描いたページを GPU へ送る。
    void Upload();

    uint32_t GetPageSize() const
    {
        return pageSize_;
    }

    uint32_t GetPageCount() const
    {
        return static_cast<uint32_t>(pages_.size());
    }

    /// @brief ページの CPU 側の画素（RGBA8、上の行から）。
    const std::vector<uint8_t>& GetPagePixels(uint32_t page) const;
    /// @brief ページの GPU 側のテクスチャ。GPU のページを作らない設定なら nullptr。
    DynamicTexture* GetPageTexture(uint32_t page) const;

    /// @brief これまでに描いた文字の数（同じ文字を二度描いていないかの確認用）。
    uint32_t GetRasterizedCount() const
    {
        return rasterizedCount_;
    }

    /// @brief UTF-8 を文字（コードポイント）の並びにする。壊れた並びは U+FFFD にする。
    static std::u32string DecodeUtf8(std::string_view utf8);

private:
    struct Page;
    struct StyleData;

    /// @brief 文字の画像をページに置く場所を探す。入らなければ新しいページを作る。
    bool Allocate(uint32_t width, uint32_t height, uint32_t& page, uint32_t& x, uint32_t& y);
    void AddPage();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    uint32_t pageSize_ = 1024;
    uintptr_t gdiplusToken_ = 0;
    std::unique_ptr<GlyphAtlasFonts> fonts_;
    std::vector<std::unique_ptr<Page>> pages_;
    std::vector<std::unique_ptr<StyleData>> styles_;
    // 詰める位置（棚詰め: 左から右へ並べ、入らなければ次の段へ）。
    uint32_t shelfX_ = 0;
    uint32_t shelfY_ = 0;
    uint32_t shelfHeight_ = 0;
    uint32_t rasterizedCount_ = 0;
};

} // namespace myEngine
