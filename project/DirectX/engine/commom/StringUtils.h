#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace cg2 {
/// @brief ASCIIの英大文字を小文字へ変換する。
inline std::string ToLowerAscii(const std::string& text)
{
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}
/// @brief 入力文字列に指定したトークンが含まれるか判定する。
inline bool ContainsToken(const std::string& text, const char* token)
{
    return text.find(token) != std::string::npos;
}
} // namespace cg2
