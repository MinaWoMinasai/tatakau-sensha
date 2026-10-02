#pragma once

// Disposable DXIL cache. A cache failure must never make a valid shader fail to load.
#include <Windows.h>
#include <bcrypt.h>
#include <dxcapi.h>
#include <wrl.h>
#include <wrl/implements.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <json.hpp>

#pragma comment(lib, "bcrypt.lib")

namespace cg2 {

/// @brief シェーダーとinclude依存の内容に基づいてコンパイル済みデータを保存・再利用する。
class ShaderDiskCache {
public:
    /// @brief シェーダーのキャッシュを検証するための依存ファイルの情報を保持する。
    struct Dependency {
        std::wstring path;
        bool found = false;
        std::string digest;
    };

    // DXC calls this for each include candidate, including unsuccessful probes.
    // Recording missing candidates also catches a newly created, higher-priority include.
    /// @brief シェーダーのinclude読み込みを記録し、キャッシュの依存情報へ渡す。
    class IncludeRecorder final
        : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDxcIncludeHandler> {
    public:
        /// @brief インスタンスの初期値と利用先を設定する。
        explicit IncludeRecorder(IDxcIncludeHandler* inner) : inner_(inner) {}
        /// @brief includeファイルをDXCへ渡し、見つからなかった候補も依存情報として記録する。
        /// @param filename DXCが照合するinclude候補のパス。
        /// @param source 読み込み結果の出力先。所有権はCOMの参照カウントに従う。
        /// @return 内側のincludeハンドラーが返したHRESULT。
        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR filename, IDxcBlob** source) override
        {
            const HRESULT result = inner_->LoadSource(filename, source);
            try {
                const bool found = SUCCEEDED(result) && source && *source;
                dependencies.push_back(
                    {filename, found, found ? Hash((*source)->GetBufferPointer(), (*source)->GetBufferSize()) : std::string()});
                if (found && dependencies.back().digest.empty())
                    valid = false;
            }
            catch (...) {
                valid = false;
            }
            return result;
        }
        std::vector<Dependency> dependencies;
        bool valid = true;

    private:
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> inner_;
    };

    /// @brief この機能が有効か判定する。
    static bool Enabled()
    {
        wchar_t value[8]{};
        return GetEnvironmentVariableW(L"CG2_STARTUP_CACHE", value, 8) != 1 || value[0] != L'0';
    }

    /// @brief hが存在するか判定する。
    static std::string Hash(const void* bytes, size_t size)
    {
        if (size > (std::numeric_limits<ULONG>::max)())
            return {};
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            return {};
        std::array<unsigned char, 32> digest{};
        const NTSTATUS status = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<void*>(bytes)),
                                           static_cast<ULONG>(size), digest.data(), static_cast<ULONG>(digest.size()));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0)
            return {};
        constexpr char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(64);
        for (unsigned char byte : digest) {
            result.push_back(hex[byte >> 4]);
            result.push_back(hex[byte & 15]);
        }
        return result;
    }

    // Source bytes and the entire command line are part of the key. Compiler and
    // validator DLL contents are fingerprinted once, not merely their timestamps.
    /// @brief キーを作成して返す。
    std::string MakeKey(const std::wstring& path, const void* source, size_t size, LPCWSTR const* arguments, size_t count) noexcept
    {
        try {
            if (!Enabled())
                return {};
            if (!identityInitialized_) {
                identityInitialized_ = true;
                const std::string compiler = ModuleDigest(L"dxcompiler.dll");
                const std::string validator = ModuleDigest(L"dxil.dll");
                if (!compiler.empty() && !validator.empty())
                    identity_ = compiler + validator;
            }
            if (identity_.empty())
                return {};
            nlohmann::json metadata = {{"format", 1},
                                       {"compiler", identity_},
                                       {"sourcePath", Utf8(std::filesystem::absolute(path).lexically_normal().wstring())},
                                       {"workingDirectory", Utf8(std::filesystem::current_path().wstring())},
                                       {"source", Hash(source, size)},
                                       {"arguments", nlohmann::json::array()}};
            if (metadata["source"].get<std::string>().empty())
                return {};
            for (size_t index = 0; index < count; ++index)
                metadata["arguments"].push_back(Utf8(arguments[index]));
            const std::string serialized = metadata.dump();
            return Hash(serialized.data(), serialized.size());
        }
        catch (...) {
            return {};
        }
    }

    /// @brief 保存されたデータを読み込む。
    Microsoft::WRL::ComPtr<IDxcBlob> Load(const std::string& key, IDxcIncludeHandler* includeHandler, IDxcUtils* utils) noexcept
    {
        try {
            if (key.empty())
                return {};
            const auto bytes = ReadFile(CachePath(key), kMaximumCacheSize);
            constexpr size_t prefixSize = sizeof(kMagic) + sizeof(uint32_t);
            if (bytes.size() < prefixSize || memcmp(bytes.data(), kMagic, sizeof(kMagic)) != 0)
                return {};
            uint32_t metadataSize = 0;
            memcpy(&metadataSize, bytes.data() + sizeof(kMagic), sizeof(metadataSize));
            if (metadataSize > 4 * 1024 * 1024 || metadataSize > bytes.size() - prefixSize)
                return {};
            const auto metadata = nlohmann::json::parse(bytes.data() + prefixSize, bytes.data() + prefixSize + metadataSize);
            if (metadata.at("key").get<std::string>() != key)
                return {};
            const size_t objectOffset = prefixSize + metadataSize;
            const size_t objectSize = bytes.size() - objectOffset;
            if (objectSize == 0 || metadata.at("size").get<size_t>() != objectSize ||
                metadata.at("sha256").get<std::string>() != Hash(bytes.data() + objectOffset, objectSize))
                return {};
            const auto& dependencies = metadata.at("dependencies");
            if (!dependencies.is_array() || dependencies.size() > 4096)
                return {};
            for (const auto& dependency : dependencies) {
                Microsoft::WRL::ComPtr<IDxcBlob> included;
                const std::wstring filename = Wide(dependency.at("path").get<std::string>());
                const HRESULT status = includeHandler->LoadSource(filename.c_str(), &included);
                const bool found = SUCCEEDED(status) && included;
                if (dependency.at("found").get<bool>() != found)
                    return {};
                if (found && dependency.at("sha256").get<std::string>() != Hash(included->GetBufferPointer(), included->GetBufferSize()))
                    return {};
            }
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> result;
            if (FAILED(utils->CreateBlob(bytes.data() + objectOffset, static_cast<UINT32>(objectSize), DXC_CP_ACP, &result)))
                return {};
            return result;
        }
        catch (...) {
            return {};
        }
    }

    /// @brief 指定したデータを保存する。
    void Store(const std::string& key, const IncludeRecorder& includes, IDxcBlob* object) noexcept
    {
        try {
            if (key.empty() || !includes.valid || !object || object->GetBufferSize() > kMaximumCacheSize)
                return;
            const std::string digest = Hash(object->GetBufferPointer(), object->GetBufferSize());
            if (digest.empty())
                return;
            nlohmann::json metadata = {
                {"key", key}, {"size", object->GetBufferSize()}, {"sha256", digest}, {"dependencies", nlohmann::json::array()}};
            for (const auto& dependency : includes.dependencies) {
                metadata["dependencies"].push_back(
                    {{"path", Utf8(dependency.path)}, {"found", dependency.found}, {"sha256", dependency.digest}});
            }
            const std::string serialized = metadata.dump();
            if (serialized.size() > 4 * 1024 * 1024)
                return;
            const uint32_t size = static_cast<uint32_t>(serialized.size());
            const auto destination = CachePath(key);
            std::filesystem::create_directories(destination.parent_path());
            const auto temporary =
                destination.wstring() + L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence_) + L".tmp";
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                output.write(kMagic, sizeof(kMagic));
                output.write(reinterpret_cast<const char*>(&size), sizeof(size));
                output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
                output.write(static_cast<const char*>(object->GetBufferPointer()), static_cast<std::streamsize>(object->GetBufferSize()));
                output.close();
                if (!output) {
                    std::error_code ignored;
                    std::filesystem::remove(temporary, ignored);
                    return;
                }
            }
            if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
        }
        catch (...) { /* A read-only install or unavailable cache is harmless. */
        }
    }

    /// @brief キャッシュの保存パスを組み立てる。
    static std::filesystem::path CachePath(const std::string& key)
    {
        const DWORD length = GetEnvironmentVariableW(L"CG2_SHADER_CACHE_DIR", nullptr, 0);
        if (length > 0 && length < 32768) {
            std::vector<wchar_t> directory(length);
            const DWORD copied = GetEnvironmentVariableW(L"CG2_SHADER_CACHE_DIR", directory.data(), length);
            if (copied > 0 && copied < length)
                return std::filesystem::path(directory.data()) / (key + ".dxilcache");
        }
        return std::filesystem::path(L"generated/shader_cache/v1") / (key + ".dxilcache");
    }

private:
    static constexpr char kMagic[8] = {'C', 'G', '2', 'D', 'X', 'I', 'L', '1'};
    static constexpr size_t kMaximumCacheSize = 64 * 1024 * 1024;
    bool identityInitialized_ = false;
    std::string identity_;
    inline static std::atomic<uint64_t> sequence_{0};

    /// @brief ワイド文字列をUTF-8へ変換する。
    static std::string Utf8(const std::wstring& value)
    {
        if (value.empty())
            return {};
        const int count =
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (count <= 0)
            throw std::runtime_error("Invalid shader path");
        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr,
                            nullptr);
        return result;
    }
    /// @brief UTF-8文字列をワイド文字列へ変換する。
    static std::wstring Wide(const std::string& value)
    {
        if (value.empty())
            return {};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0)
            throw std::runtime_error("Invalid shader path");
        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
        return result;
    }
    /// @brief ファイルを読み取る。
    static std::vector<char> ReadFile(const std::filesystem::path& path, size_t limit)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
            return {};
        const auto length = input.tellg();
        if (length < 0 || static_cast<uint64_t>(length) > limit)
            return {};
        std::vector<char> bytes(static_cast<size_t>(length));
        input.seekg(0);
        if (!bytes.empty())
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return input ? bytes : std::vector<char>();
    }
    /// @brief 実行モジュールの内容をキャッシュ識別用のハッシュへ変換する。
    static std::string ModuleDigest(const wchar_t* name)
    {
        std::array<wchar_t, 32768> path{};
        const HMODULE module = GetModuleHandleW(name);
        const DWORD count = module ? GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()))
                                   : SearchPathW(nullptr, name, nullptr, static_cast<DWORD>(path.size()), path.data(), nullptr);
        if (count == 0 || count >= path.size())
            return {};
        const auto bytes = ReadFile(path.data(), 256 * 1024 * 1024);
        return bytes.empty() ? std::string() : Hash(bytes.data(), bytes.size());
    }
};

} // namespace cg2
