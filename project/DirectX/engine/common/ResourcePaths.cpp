#include "ResourcePaths.h"

#include <Windows.h>

namespace myEngine {

namespace {

/// @brief 設定済みのパスと、そこから組み立てた派生パスをまとめて保持する。
struct PathState {
    std::string engineRoot = "resources";
    std::wstring engineRootWide = L"resources";
    std::string defaultWhiteTexture = "resources/white512x512.png";
    std::string gameRoot = "resources";
};

/// @brief パスの状態を返す。静的初期化の順序に左右されないよう、関数内の静的変数にする。
PathState& State()
{
    static PathState state;
    return state;
}

/// @brief UTF-8の文字列をUTF-16へ変換する。
std::wstring ToWide(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

} // namespace

void ResourcePaths::SetEngineRoot(std::string_view root)
{
    if (root.empty()) {
        return;
    }
    PathState& state = State();
    state.engineRoot = std::string(root);
    state.engineRootWide = ToWide(root);
    state.defaultWhiteTexture = state.engineRoot + "/white512x512.png";
}

void ResourcePaths::SetGameRoot(std::string_view root)
{
    if (root.empty()) {
        return;
    }
    State().gameRoot = std::string(root);
}

const std::string& ResourcePaths::EngineRoot()
{
    return State().engineRoot;
}

const std::string& ResourcePaths::GameRoot()
{
    return State().gameRoot;
}

std::wstring ResourcePaths::EngineShader(std::wstring_view fileName)
{
    std::wstring path = State().engineRootWide;
    path += L"/shaders/";
    path += fileName;
    return path;
}

const std::string& ResourcePaths::DefaultWhiteTexture()
{
    return State().defaultWhiteTexture;
}

} // namespace myEngine
