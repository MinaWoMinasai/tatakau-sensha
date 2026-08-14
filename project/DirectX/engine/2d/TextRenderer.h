#pragma once
#define NOMINMAX
#include <memory>
#include <string>
#include "Struct.h"

struct TextStyle {
	std::string fontFamily = "Meiryo";
	std::string fontPath;
	int fontWeight = 400;
	float fontSize = 42.0f;
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 outlineColor = { 0.0f, 0.0f, 0.0f, 0.75f };
	float outlineThickness = 2.0f;
	float padding = 8.0f;
	// 全体のフォント設定がアウトラインを上書きしている場合でも、
	// HUD上で個別に可読性を確保したい文字だけは指定値を優先する。
	bool preserveOutline = false;
};

struct TextFontOverride {
	bool enabled = false;
	std::string fontFamily;
	std::string fontPath;
	int fontWeight = 400;
	bool overrideOutline = false;
	Vector4 outlineColor{ 0.0f, 0.0f, 0.0f, 0.0f };
	float outlineThickness = 0.0f;
};

struct TextRendererFontStore;

class TextRenderer {
public:
	static TextRenderer* GetInstance();
	void Finalize();

	std::string GetOrCreateTexture(const std::string& utf8Text, const TextStyle& style);
	void SetFontOverride(const TextFontOverride& fontOverride);
	const TextFontOverride& GetFontOverride() const { return fontOverride_; }
	unsigned long long GetFontRevision() const { return fontRevision_; }

private:
	TextRenderer();
	~TextRenderer();
	TextRenderer(const TextRenderer&) = delete;
	TextRenderer& operator=(const TextRenderer&) = delete;

	void EnsureInitialized();
	std::wstring Utf8ToWide(const std::string& text) const;
	std::string BuildCachePath(const std::string& utf8Text, const TextStyle& style) const;
	bool SaveTextPng(const std::wstring& text, const TextStyle& style, const std::string& path);
	TextStyle ResolveStyle(const TextStyle& style) const;

	static TextRenderer* instance_;
	bool initialized_ = false;
	unsigned long long gdiplusToken_ = 0;
	std::unique_ptr<TextRendererFontStore> fontStore_;
	TextFontOverride fontOverride_{};
	unsigned long long fontRevision_ = 1;
};
