#include "GlyphAtlas.h"
#include "DynamicTexture.h"
#include "Utf8Path.h"

#include <Windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#pragma comment(lib, "gdiplus.lib")

namespace myEngine {

/// @brief GDI+ のフォントの寿命をまとめて持つ（GDI+ を終える前に消すため）。
struct GlyphAtlasFonts {
    std::vector<std::unique_ptr<Gdiplus::PrivateFontCollection>> collections;
};

/// @brief 1ページ分の画素と、GPU のテクスチャ。
struct GlyphAtlas::Page {
    std::vector<uint8_t> pixels;
    std::unique_ptr<DynamicTexture> texture;
    bool dirty = false;
};

/// @brief 登録した見た目ごとのフォントと、描いた文字。
struct GlyphAtlas::StyleData {
    Style style;
    std::unique_ptr<Gdiplus::FontFamily> family;
    std::unique_ptr<Gdiplus::Font> font;
    INT fontStyle = Gdiplus::FontStyleRegular;
    float lineHeight = 0.0f;
    std::unordered_map<char32_t, Glyph> glyphs;
};

namespace {

std::wstring ToWide(char32_t codepoint)
{
    std::wstring text;
    if (codepoint >= 0x10000) {
        const char32_t value = codepoint - 0x10000;
        text.push_back(static_cast<wchar_t>(0xD800 + (value >> 10)));
        text.push_back(static_cast<wchar_t>(0xDC00 + (value & 0x3FF)));
    } else {
        text.push_back(static_cast<wchar_t>(codepoint));
    }
    return text;
}

bool IsBlank(char32_t codepoint)
{
    return codepoint == U' ' || codepoint == U'\t' || codepoint == 0x3000;
}

/// @brief 測るときと描くときで同じ書式を使う（文字の前後の余白を付けず、末尾の空白も測る）。
std::unique_ptr<Gdiplus::StringFormat> MakeFormat()
{
    // StringFormat はコピーできないので、ヒープに作って返す。
    auto format = std::make_unique<Gdiplus::StringFormat>(Gdiplus::StringFormat::GenericTypographic());
    format->SetFormatFlags(format->GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    return format;
}

} // namespace

GlyphAtlas::GlyphAtlas() = default;

GlyphAtlas::~GlyphAtlas()
{
    // GDI+ の物は、GDI+ を終える前に消す。
    styles_.clear();
    fonts_.reset();
    if (gdiplusToken_ != 0) {
        Gdiplus::GdiplusShutdown(gdiplusToken_);
    }
}

bool GlyphAtlas::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t pageSize)
{
    if (gdiplusToken_ != 0 || pageSize == 0) {
        return false;
    }
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    pageSize_ = pageSize;
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        return false;
    }
    gdiplusToken_ = token;
    fonts_ = std::make_unique<GlyphAtlasFonts>();
    return true;
}

int GlyphAtlas::RegisterStyle(const Style& style)
{
    if (gdiplusToken_ == 0) {
        return -1;
    }
    for (size_t index = 0; index < styles_.size(); ++index) {
        const Style& other = styles_[index]->style;
        if (other.fontFamily == style.fontFamily && other.fontPath == style.fontPath && other.size == style.size &&
            other.bold == style.bold && other.outline == style.outline) {
            return static_cast<int>(index);
        }
    }

    auto data = std::make_unique<StyleData>();
    data->style = style;
    if (!style.fontPath.empty()) {
        // フォントファイルから読む。ファイルに入っている最初のフォントを使う。
        auto collection = std::make_unique<Gdiplus::PrivateFontCollection>();
        const std::wstring path = Utf8ToPath(style.fontPath).wstring();
        if (collection->AddFontFile(path.c_str()) != Gdiplus::Ok || collection->GetFamilyCount() <= 0) {
            return -1;
        }
        Gdiplus::FontFamily family;
        INT found = 0;
        collection->GetFamilies(1, &family, &found);
        if (found <= 0) {
            return -1;
        }
        data->family.reset(family.Clone());
        fonts_->collections.push_back(std::move(collection));
    } else {
        // システムのフォント名（UTF-8 をワイド文字へ）。
        data->family = std::make_unique<Gdiplus::FontFamily>(Utf8ToPath(style.fontFamily).wstring().c_str());
    }
    if (!data->family || data->family->GetLastStatus() != Gdiplus::Ok || !data->family->IsAvailable()) {
        return -1;
    }
    data->fontStyle =
        style.bold && data->family->IsStyleAvailable(Gdiplus::FontStyleBold) ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular;
    data->font = std::make_unique<Gdiplus::Font>(data->family.get(), style.size, data->fontStyle, Gdiplus::UnitPixel);
    if (data->font->GetLastStatus() != Gdiplus::Ok) {
        return -1;
    }
    data->lineHeight = style.size * static_cast<float>(data->family->GetLineSpacing(data->fontStyle)) /
                       static_cast<float>(data->family->GetEmHeight(data->fontStyle));
    styles_.push_back(std::move(data));
    return static_cast<int>(styles_.size() - 1);
}

float GlyphAtlas::LineHeight(int style) const
{
    return style >= 0 && style < static_cast<int>(styles_.size()) ? styles_[style]->lineHeight : 0.0f;
}

void GlyphAtlas::AddPage()
{
    auto page = std::make_unique<Page>();
    page->pixels.assign(size_t(pageSize_) * pageSize_ * 4, 0);
    page->dirty = true;
    pages_.push_back(std::move(page));
    shelfX_ = 0;
    shelfY_ = 0;
    shelfHeight_ = 0;
}

bool GlyphAtlas::Allocate(uint32_t width, uint32_t height, uint32_t& page, uint32_t& x, uint32_t& y)
{
    constexpr uint32_t kGap = 1; // 隣の文字がにじまないよう、1画素あける。
    if (width > pageSize_ || height > pageSize_) {
        return false;
    }
    if (pages_.empty()) {
        AddPage();
    }
    if (shelfX_ + width > pageSize_) {
        // 次の段へ。
        shelfX_ = 0;
        shelfY_ += shelfHeight_ + kGap;
        shelfHeight_ = 0;
    }
    if (shelfY_ + height > pageSize_) {
        AddPage();
    }
    page = static_cast<uint32_t>(pages_.size() - 1);
    x = shelfX_;
    y = shelfY_;
    shelfX_ += width + kGap;
    shelfHeight_ = (std::max)(shelfHeight_, height);
    return true;
}

const GlyphAtlas::Glyph* GlyphAtlas::GetGlyph(int style, char32_t codepoint)
{
    if (style < 0 || style >= static_cast<int>(styles_.size())) {
        return nullptr;
    }
    StyleData& data = *styles_[style];
    const auto cached = data.glyphs.find(codepoint);
    if (cached != data.glyphs.end()) {
        return &cached->second;
    }

    Glyph glyph;
    const std::wstring text = ToWide(codepoint);
    const std::unique_ptr<Gdiplus::StringFormat> format = MakeFormat();

    // 送り幅を測る。
    {
        Gdiplus::Bitmap scratch(1, 1, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(&scratch);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
        Gdiplus::RectF bounds;
        graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), data.font.get(), Gdiplus::PointF(0.0f, 0.0f), format.get(),
                               &bounds);
        glyph.advance = bounds.Width;
    }

    if (!IsBlank(codepoint) && glyph.advance > 0.0f) {
        // 縁取りが欠けないよう、周りに余白を付けて描く。
        const float pad = std::ceil(data.style.outline) + 2.0f;
        const uint32_t width = static_cast<uint32_t>(std::ceil(glyph.advance + pad * 2.0f));
        const uint32_t height = static_cast<uint32_t>(std::ceil(data.lineHeight + pad * 2.0f));
        uint32_t page = 0, x = 0, y = 0;
        if (Allocate(width, height, page, x, y)) {
            Gdiplus::Bitmap bitmap(static_cast<INT>(width), static_cast<INT>(height), PixelFormat32bppARGB);
            {
                Gdiplus::Graphics graphics(&bitmap);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
                Gdiplus::GraphicsPath path;
                path.AddString(text.c_str(), static_cast<INT>(text.size()), data.family.get(), data.fontStyle, data.style.size,
                               Gdiplus::PointF(pad, pad), format.get());
                if (data.style.outline > 0.0f) {
                    Gdiplus::Pen pen(Gdiplus::Color(255, 0, 0, 0), data.style.outline * 2.0f);
                    pen.SetLineJoin(Gdiplus::LineJoinRound);
                    graphics.DrawPath(&pen, &path);
                }
                Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
                graphics.FillPath(&brush, &path);
            }
            // GDI+ の BGRA を、ページの RGBA へ写す。
            Gdiplus::BitmapData locked{};
            Gdiplus::Rect rect(0, 0, static_cast<INT>(width), static_cast<INT>(height));
            if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &locked) == Gdiplus::Ok) {
                Page& target = *pages_[page];
                for (uint32_t row = 0; row < height; ++row) {
                    const uint8_t* source = static_cast<const uint8_t*>(locked.Scan0) + size_t(row) * locked.Stride;
                    uint8_t* destination = &target.pixels[(size_t(y + row) * pageSize_ + x) * 4];
                    for (uint32_t column = 0; column < width; ++column) {
                        destination[column * 4 + 0] = source[column * 4 + 2];
                        destination[column * 4 + 1] = source[column * 4 + 1];
                        destination[column * 4 + 2] = source[column * 4 + 0];
                        destination[column * 4 + 3] = source[column * 4 + 3];
                    }
                }
                bitmap.UnlockBits(&locked);
                target.dirty = true;
            }
            glyph.page = page;
            glyph.pixelX = x;
            glyph.pixelY = y;
            glyph.size = {static_cast<float>(width), static_cast<float>(height)};
            glyph.offset = {-pad, -pad};
            const float scale = 1.0f / static_cast<float>(pageSize_);
            glyph.uvMin = {x * scale, y * scale};
            glyph.uvMax = {(x + width) * scale, (y + height) * scale};
        }
        ++rasterizedCount_;
    }
    return &data.glyphs.emplace(codepoint, glyph).first->second;
}

GlyphAtlas::TextLayout GlyphAtlas::Layout(int style, std::string_view utf8, float wrapWidth)
{
    TextLayout layout;
    const float lineHeight = LineHeight(style);
    const std::u32string text = DecodeUtf8(utf8);
    if (text.empty() || lineHeight <= 0.0f) {
        return layout;
    }
    float penX = 0.0f;
    float penY = 0.0f;
    float width = 0.0f;
    layout.lineCount = 1;
    for (const char32_t codepoint : text) {
        if (codepoint == U'\r') {
            continue;
        }
        if (codepoint == U'\n') {
            penX = 0.0f;
            penY += lineHeight;
            ++layout.lineCount;
            continue;
        }
        const Glyph* glyph = GetGlyph(style, codepoint);
        if (!glyph) {
            continue;
        }
        // 折り返し: 行の先頭でなく、この文字で幅を超えるなら次の行へ（日本語向けに文字単位）。
        if (wrapWidth > 0.0f && penX > 0.0f && penX + glyph->advance > wrapWidth) {
            penX = 0.0f;
            penY += lineHeight;
            ++layout.lineCount;
        }
        if (glyph->size.x > 0.0f) {
            layout.glyphs.push_back({glyph, codepoint, {penX + glyph->offset.x, penY + glyph->offset.y}});
        }
        penX += glyph->advance;
        width = (std::max)(width, penX);
    }
    layout.size = {width, lineHeight * layout.lineCount};
    return layout;
}

Vector2 GlyphAtlas::MeasureText(int style, std::string_view utf8, float wrapWidth)
{
    return Layout(style, utf8, wrapWidth).size;
}

void GlyphAtlas::Upload()
{
    for (const std::unique_ptr<Page>& page : pages_) {
        if (!page->dirty) {
            continue;
        }
        if (!page->texture && dxCommon_ && srvManager_) {
            page->texture = std::make_unique<DynamicTexture>();
            if (!page->texture->Initialize(dxCommon_, srvManager_, pageSize_, pageSize_)) {
                page->texture.reset();
            }
        }
        if (page->texture) {
            page->texture->Update(page->pixels);
        }
        page->dirty = false;
    }
}

const std::vector<uint8_t>& GlyphAtlas::GetPagePixels(uint32_t page) const
{
    static const std::vector<uint8_t> empty;
    return page < pages_.size() ? pages_[page]->pixels : empty;
}

DynamicTexture* GlyphAtlas::GetPageTexture(uint32_t page) const
{
    return page < pages_.size() ? pages_[page]->texture.get() : nullptr;
}

std::u32string GlyphAtlas::DecodeUtf8(std::string_view utf8)
{
    constexpr char32_t kReplacement = 0xFFFD;
    std::u32string result;
    size_t index = 0;
    while (index < utf8.size()) {
        const uint8_t lead = static_cast<uint8_t>(utf8[index]);
        int length = 0;
        char32_t codepoint = 0;
        if (lead < 0x80) {
            length = 1;
            codepoint = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            length = 2;
            codepoint = lead & 0x1F;
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            codepoint = lead & 0x0F;
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            codepoint = lead & 0x07;
        } else {
            result.push_back(kReplacement);
            ++index;
            continue;
        }
        if (index + length > utf8.size()) {
            result.push_back(kReplacement);
            break;
        }
        bool valid = true;
        for (int offset = 1; offset < length; ++offset) {
            const uint8_t next = static_cast<uint8_t>(utf8[index + offset]);
            if ((next & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        // 冗長な表し方、サロゲートの範囲、範囲外も壊れた並びとして扱う。
        static constexpr char32_t kMinimum[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (!valid || codepoint < kMinimum[length] || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            result.push_back(kReplacement);
            index += valid ? length : 1;
            continue;
        }
        result.push_back(codepoint);
        index += length;
    }
    return result;
}

} // namespace myEngine
