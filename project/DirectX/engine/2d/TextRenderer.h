#pragma once
#define NOMINMAX
#include <string>
#include "Struct.h"

struct TextStyle {
	std::string fontFamily = "Meiryo";
	float fontSize = 42.0f;
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 outlineColor = { 0.0f, 0.0f, 0.0f, 0.75f };
	float outlineThickness = 2.0f;
	float padding = 8.0f;
};

class TextRenderer {
public:
	static TextRenderer* GetInstance();
	void Finalize();

	std::string GetOrCreateTexture(const std::string& utf8Text, const TextStyle& style);

private:
	TextRenderer() = default;
	~TextRenderer();
	TextRenderer(const TextRenderer&) = delete;
	TextRenderer& operator=(const TextRenderer&) = delete;

	void EnsureInitialized();
	std::wstring Utf8ToWide(const std::string& text) const;
	std::string BuildCachePath(const std::string& utf8Text, const TextStyle& style) const;
	bool SaveTextPng(const std::wstring& text, const TextStyle& style, const std::string& path);

	static TextRenderer* instance_;
	bool initialized_ = false;
	unsigned long long gdiplusToken_ = 0;
};
