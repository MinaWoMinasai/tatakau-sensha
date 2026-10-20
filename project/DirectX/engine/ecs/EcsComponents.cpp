#include "EcsComponents.h"

#include <cstdio>
#include <mutex>
#include <random>

namespace myEngine {

namespace {

/// @brief 16進数1文字を値にする。16進数でなければ-1を返す。
int HexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

EntityGuid EntityGuid::Generate()
{
    // エディタやシーン読み込みは複数のスレッドから呼ばれうるので、乱数生成器は排他して使う。
    static std::mutex mutex;
    static std::mt19937_64 engine{std::random_device{}() ^ (static_cast<uint64_t>(std::random_device{}()) << 32)};
    std::lock_guard lock(mutex);
    EntityGuid guid;
    do {
        guid.high = engine();
        guid.low = engine();
    } while (!guid.IsValid());
    return guid;
}

std::optional<EntityGuid> EntityGuid::Parse(std::string_view text)
{
    // 8-4-4-4-12 桁の形式だけを受け付ける。
    constexpr size_t kLength = 36;
    if (text.size() != kLength || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-') {
        return std::nullopt;
    }
    EntityGuid guid;
    int digits = 0;
    for (const char c : text) {
        if (c == '-') {
            continue;
        }
        const int value = HexValue(c);
        if (value < 0) {
            return std::nullopt;
        }
        uint64_t& part = digits < 16 ? guid.high : guid.low;
        part = (part << 4) | static_cast<uint64_t>(value);
        ++digits;
    }
    if (digits != 32) {
        return std::nullopt;
    }
    return guid;
}

std::string EntityGuid::ToString() const
{
    char buffer[37]{};
    std::snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx", static_cast<unsigned>(high >> 32),
                  static_cast<unsigned>((high >> 16) & 0xffffu), static_cast<unsigned>(high & 0xffffu), static_cast<unsigned>(low >> 48),
                  static_cast<unsigned long long>(low & 0xffffffffffffull));
    return buffer;
}

} // namespace myEngine
