#pragma once
#include "Struct.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace myEngine {

/// @brief シーンの中でエンティティを識別する128bitのID。保存したシーンの親子関係や参照はこのIDでつなぐ。
/// flecsの数値IDは実行のたびに変わりうるため、保存にはこちらを使う。
struct EntityGuid {
    uint64_t high = 0;
    uint64_t low = 0;

    /// @brief 新しいIDを乱数で作る。
    static EntityGuid Generate();
    /// @brief "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 形式の文字列から読み取る。形式が違えば値を返さない。
    static std::optional<EntityGuid> Parse(std::string_view text);
    /// @brief "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" 形式の小文字の文字列にする。
    std::string ToString() const;

    bool IsValid() const
    {
        return high != 0 || low != 0;
    }

    bool operator==(const EntityGuid&) const = default;
};

/// @brief モデルを描画する。model はゲームの素材ディレクトリ（ResourcePaths::GameRoot）からの相対パス。
struct MeshRenderer {
    std::string model;
    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

/// @brief カメラの設定。primary が true のものを、シーンの描画に使うカメラにする。
struct CameraComponent {
    float fovY = 0.45f;
    float nearClip = 0.1f;
    float farClip = 5000.0f;
    bool primary = true;
};

} // namespace myEngine
