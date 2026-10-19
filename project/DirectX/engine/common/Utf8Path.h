#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace myEngine {

/// @brief UTF-8の文字列からパスを作る。
/// std::filesystem::path(std::string) は文字列をANSI（日本語環境ではCP932）として解釈するため、
/// 日本語などを含むUTF-8のパスでは文字化けや例外になる。エンジンのパス文字列はUTF-8なので、必ずこれを通す。
inline std::filesystem::path Utf8ToPath(std::string_view utf8)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

/// @brief パスを '/' 区切りのUTF-8文字列にする。path::generic_string() のUTF-8版。
inline std::string PathToUtf8Generic(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

/// @brief パスをUTF-8文字列にする（区切り文字はそのまま）。path::string() のUTF-8版。
inline std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

} // namespace myEngine
