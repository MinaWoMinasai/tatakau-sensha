#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace cg2 {
inline std::string ToLowerAscii(const std::string& text) {
    std::string result = text;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}
inline bool ContainsToken(const std::string& text, const char* token) {
    return text.find(token) != std::string::npos;
}
} // namespace cg2
