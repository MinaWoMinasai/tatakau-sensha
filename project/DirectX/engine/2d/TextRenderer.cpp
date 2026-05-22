#include "TextRenderer.h"
#include <Windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iomanip>
#include <sstream>
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

TextRenderer* TextRenderer::instance_ = nullptr;

TextRenderer* TextRenderer::GetInstance()
{
	if (!instance_) {
		instance_ = new TextRenderer();
	}
	return instance_;
}

TextRenderer::~TextRenderer()
{
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
	EnsureInitialized();

	const std::string path = BuildCachePath(utf8Text, style);
	if (!std::filesystem::exists(path)) {
		const std::wstring text = Utf8ToWide(utf8Text);
		const bool saved = SaveTextPng(text, style, path);
		assert(saved);
	}
	return path;
}

void TextRenderer::EnsureInitialized()
{
	if (initialized_) {
		return;
	}

	Gdiplus::GdiplusStartupInput startupInput{};
	const Gdiplus::Status status = Gdiplus::GdiplusStartup(&gdiplusToken_, &startupInput, nullptr);
	assert(status == Gdiplus::Ok);
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
		<< style.fontSize << '|'
		<< style.color.x << ',' << style.color.y << ',' << style.color.z << ',' << style.color.w << '|'
		<< style.outlineColor.x << ',' << style.outlineColor.y << ',' << style.outlineColor.z << ',' << style.outlineColor.w << '|'
		<< style.outlineThickness << '|'
		<< style.padding;

	const size_t hash = std::hash<std::string>{}(key.str());
	std::ostringstream path;
	path << "resources/generated/text/text_"
		<< std::hex << std::setw(sizeof(size_t) * 2) << std::setfill('0') << hash
		<< ".png";
	return path.str();
}

bool TextRenderer::SaveTextPng(const std::wstring& text, const TextStyle& style, const std::string& path)
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());

	const std::wstring fontName = Utf8ToWide(style.fontFamily);
	Gdiplus::FontFamily fontFamily(fontName.c_str());
	const Gdiplus::Font font(&fontFamily, style.fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
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
