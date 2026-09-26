#include "TextRenderer.h"
#include "StartupTrace.h"
#include <Windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

Gdiplus::Color ToGdiColor(const Vector4& color)
{
	auto toByte = [](float value) -> BYTE {
		return static_cast<BYTE>((std::clamp)(value, 0.0f, 1.0f) * 255.0f + 0.5f);
	};
	return Gdiplus::Color(toByte(color.w), toByte(color.x), toByte(color.y), toByte(color.z));
}

bool GetPngEncoderClsid(CLSID* clsid)
{
	UINT num = 0;
	UINT size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) {
		return false;
	}

	std::vector<BYTE> buffer(size);
	auto* imageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
	Gdiplus::GetImageEncoders(num, size, imageCodecInfo);
	for (UINT i = 0; i < num; ++i) {
		if (wcscmp(imageCodecInfo[i].MimeType, L"image/png") == 0) {
			*clsid = imageCodecInfo[i].Clsid;
			return true;
		}
	}
	return false;
}

std::wstring ToWidePath(const std::string& path)
{
	const int size = MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), result.data(), size);
	return result;
}

} // namespace

struct TextRendererFontStore {
	Gdiplus::PrivateFontCollection privateFonts;
	std::unordered_set<std::wstring> registeredPaths;
};

TextRenderer* TextRenderer::instance_ = nullptr;

TextRenderer::TextRenderer() = default;

TextRenderer* TextRenderer::GetInstance()
{
	if (!instance_) {
		instance_ = new TextRenderer();
	}
	return instance_;
}

TextRenderer::~TextRenderer()
{
	fontStore_.reset();
	if (initialized_) {
		Gdiplus::GdiplusShutdown(gdiplusToken_);
	}
}

void TextRenderer::Finalize()
{
	delete instance_;
	instance_ = nullptr;
}

std::string TextRenderer::GetOrCreateTexture(const std::string& utf8Text, const TextStyle& style)
{
#if defined(USE_IMGUI) && !defined(NDEBUG)
	lastGetOrCreateTextureProfile_ = {};
#endif
	EnsureInitialized();
	const TextStyle resolvedStyle = ResolveStyle(style);

	const std::string path = BuildCachePath(utf8Text, resolvedStyle);
	const bool cacheFileExisted = std::filesystem::exists(path);
#if defined(USE_IMGUI) && !defined(NDEBUG)
	lastGetOrCreateTextureProfile_.cacheFileExisted = cacheFileExisted;
#endif
	if (!cacheFileExisted) {
		const std::wstring text = Utf8ToWide(utf8Text);
		const bool saved = SaveTextPng(text, resolvedStyle, path);
#if defined(USE_IMGUI) && !defined(NDEBUG)
		lastGetOrCreateTextureProfile_.generatedPng = saved;
#endif
		assert(saved);
	}
	return path;
}

void TextRenderer::SetFontOverride(const TextFontOverride& fontOverride)
{
	if (fontOverride_.enabled == fontOverride.enabled &&
		fontOverride_.fontFamily == fontOverride.fontFamily &&
		fontOverride_.fontPath == fontOverride.fontPath &&
		fontOverride_.fontWeight == fontOverride.fontWeight &&
		fontOverride_.overrideOutline == fontOverride.overrideOutline &&
		fontOverride_.outlineColor.x == fontOverride.outlineColor.x &&
		fontOverride_.outlineColor.y == fontOverride.outlineColor.y &&
		fontOverride_.outlineColor.z == fontOverride.outlineColor.z &&
		fontOverride_.outlineColor.w == fontOverride.outlineColor.w &&
		fontOverride_.outlineThickness == fontOverride.outlineThickness) {
		return;
	}
	fontOverride_ = fontOverride;
	++fontRevision_;
}

TextStyle TextRenderer::ResolveStyle(const TextStyle& style) const
{
	TextStyle resolved = style;
	if (fontOverride_.enabled) {
		resolved.fontFamily = fontOverride_.fontFamily;
		resolved.fontPath = fontOverride_.fontPath;
		resolved.fontWeight = fontOverride_.fontWeight;
		if (fontOverride_.overrideOutline && !resolved.preserveOutline) {
			resolved.outlineColor = fontOverride_.outlineColor;
			resolved.outlineThickness = (std::max)(0.0f, fontOverride_.outlineThickness);
		}
	}
	return resolved;
}

void TextRenderer::EnsureInitialized()
{
	if (initialized_) {
		return;
	}

	Gdiplus::GdiplusStartupInput startupInput{};
	const Gdiplus::Status status = Gdiplus::GdiplusStartup(&gdiplusToken_, &startupInput, nullptr);
	assert(status == Gdiplus::Ok);
	fontStore_ = std::make_unique<TextRendererFontStore>();
	initialized_ = true;
}

std::wstring TextRenderer::Utf8ToWide(const std::string& text) const
{
	if (text.empty()) {
		return L"";
	}

	const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
	return result;
}

std::string TextRenderer::BuildCachePath(const std::string& utf8Text, const TextStyle& style) const
{
	std::ostringstream key;
	key << utf8Text << '|'
		<< style.fontFamily << '|'
		<< style.fontPath << '|'
		<< style.fontWeight << '|'
		<< style.fontSize << '|'
		<< style.color.x << ',' << style.color.y << ',' << style.color.z << ',' << style.color.w << '|'
		<< style.outlineColor.x << ',' << style.outlineColor.y << ',' << style.outlineColor.z << ',' << style.outlineColor.w << '|'
		<< style.outlineThickness << '|'
		<< style.padding << '|'
		<< style.preserveOutline;
	if (!style.fontPath.empty()) {
		std::error_code error{};
		const auto modified = std::filesystem::last_write_time(
			std::filesystem::path(style.fontPath),
			error);
		if (!error) {
			key << '|' << modified.time_since_epoch().count();
		}
	}

	const size_t hash = std::hash<std::string>{}(key.str());
	std::ostringstream path;
	path << "resources/generated/text/text_"
		<< std::hex << std::setw(sizeof(size_t) * 2) << std::setfill('0') << hash
		<< ".png";
	return path.str();
}

bool TextRenderer::SaveTextPng(const std::wstring& text, const TextStyle& style, const std::string& path)
{
	StartupTrace::Scope startupScope("Text.GeneratePng");
	StartupTrace::Count("Text.GeneratedPng");
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());

	const std::wstring fontName = Utf8ToWide(style.fontFamily);
	std::unique_ptr<Gdiplus::FontFamily> fontFamily;
	if (!style.fontPath.empty() && fontStore_) {
		std::error_code error{};
		const std::filesystem::path absolutePath = std::filesystem::absolute(
			std::filesystem::path(style.fontPath),
			error).lexically_normal();
		if (!error && std::filesystem::exists(absolutePath)) {
			const std::wstring widePath = absolutePath.wstring();
			if (!fontStore_->registeredPaths.contains(widePath)) {
				if (fontStore_->privateFonts.AddFontFile(widePath.c_str()) == Gdiplus::Ok) {
					fontStore_->registeredPaths.insert(widePath);
				}
			}
			fontFamily = std::make_unique<Gdiplus::FontFamily>(
				fontName.c_str(),
				&fontStore_->privateFonts);
		}
	}
	if (!fontFamily || fontFamily->GetLastStatus() != Gdiplus::Ok) {
		fontFamily = std::make_unique<Gdiplus::FontFamily>(fontName.c_str());
	}
	if (fontFamily->GetLastStatus() != Gdiplus::Ok) {
		fontFamily = std::make_unique<Gdiplus::FontFamily>(L"Meiryo");
	}

	INT fontStyle = style.fontWeight >= 700
		? Gdiplus::FontStyleBold
		: Gdiplus::FontStyleRegular;
	if (!fontFamily->IsStyleAvailable(fontStyle)) {
		fontStyle = fontFamily->IsStyleAvailable(Gdiplus::FontStyleRegular)
			? Gdiplus::FontStyleRegular
			: Gdiplus::FontStyleBold;
	}
	const Gdiplus::Font font(fontFamily.get(), style.fontSize, fontStyle, Gdiplus::UnitPixel);
	Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
	format.SetFormatFlags(format.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);

	Gdiplus::Bitmap measureBitmap(1, 1, PixelFormat32bppPARGB);
	Gdiplus::Graphics measureGraphics(&measureBitmap);
	measureGraphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

	Gdiplus::RectF bounds{};
	measureGraphics.MeasureString(text.c_str(), -1, &font, Gdiplus::PointF(0.0f, 0.0f), &format, &bounds);

	const int padding = static_cast<int>(std::ceil(style.padding + style.outlineThickness));
	const int width = (std::max)(1, static_cast<int>(std::ceil(bounds.Width)) + padding * 2);
	const int height = (std::max)(1, static_cast<int>(std::ceil(bounds.Height)) + padding * 2);

	Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppPARGB);
	Gdiplus::Graphics graphics(&bitmap);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	Gdiplus::SolidBrush fillBrush(ToGdiColor(style.color));
	Gdiplus::SolidBrush outlineBrush(ToGdiColor(style.outlineColor));
	const Gdiplus::PointF origin(static_cast<float>(padding), static_cast<float>(padding));

	const int outlineSteps = static_cast<int>(std::ceil(style.outlineThickness));
	if (outlineSteps > 0 && style.outlineColor.w > 0.0f) {
		for (int y = -outlineSteps; y <= outlineSteps; ++y) {
			for (int x = -outlineSteps; x <= outlineSteps; ++x) {
				if (x == 0 && y == 0) {
					continue;
				}
				const float distanceSq = static_cast<float>(x * x + y * y);
				if (distanceSq > style.outlineThickness * style.outlineThickness) {
					continue;
				}
				graphics.DrawString(text.c_str(), -1, &font,
					Gdiplus::PointF(origin.X + static_cast<float>(x), origin.Y + static_cast<float>(y)),
					&format, &outlineBrush);
			}
		}
	}

	graphics.DrawString(text.c_str(), -1, &font, origin, &format, &fillBrush);

	CLSID pngClsid{};
	if (!GetPngEncoderClsid(&pngClsid)) {
		return false;
	}

	const std::wstring widePath = ToWidePath(path);
	return bitmap.Save(widePath.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}
