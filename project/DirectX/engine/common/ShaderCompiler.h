#pragma once
#include <Windows.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <unordered_map>

namespace myEngine {

/// @brief DXCでHLSLをコンパイルし、結果をメモリとディスクにキャッシュする。
class ShaderCompiler {
public:
    /// @brief DXCのユーティリティ・コンパイラ・#includeの解決を用意する。
    void Initialize();
    /// @brief シェーダーをコンパイルする。同じファイルとプロファイルの2回目以降は、メモリかディスクのキャッシュを返す。
    /// @return 失敗したときは空のComPtr。
    Microsoft::WRL::ComPtr<IDxcBlob> Compile(const std::wstring& filePath, const wchar_t* profile);
    /// @brief キャッシュとDXCのオブジェクトを手放す。
    void Release();

    /// @brief DXCのユーティリティを返す。
    IDxcUtils* GetUtils() const
    {
        return dxcUtils_.Get();
    }

    /// @brief DXCのコンパイラを返す。
    IDxcCompiler3* GetCompiler() const
    {
        return dxcCompiler_.Get();
    }

    /// @brief シェーダーの #include を解決するハンドラーを返す。
    IDxcIncludeHandler* GetIncludeHandler() const
    {
        return includeHandler_.Get();
    }

private:
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
    std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<IDxcBlob>> shaderCache_;
};

} // namespace myEngine
