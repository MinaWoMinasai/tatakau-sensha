#pragma once
#include "Camera.h"
#include "EcsFlecs.h"

#include <memory>
#include <string>

namespace myEngine {

class Object3d;

/// @brief MeshRenderer を描画するための実行時だけの状態。シーンには保存しない。
struct RenderHandle {
    std::unique_ptr<Object3d> object;
    // object に読み込んだモデルのパス。MeshRenderer.model と違えば読み直す。
    std::string loadedModel;

    RenderHandle();
    ~RenderHandle();
    RenderHandle(RenderHandle&&) noexcept;
    RenderHandle& operator=(RenderHandle&&) noexcept;
};

/// @brief ECS のデータ（Transform・MeshRenderer・CameraComponent）を、既存の描画（Object3d・Camera）へ反映する。
/// 親子関係は ChildOf をたどってワールド行列を合成する。描画はメインスレッドから行う。
class RenderSync {
public:
    explicit RenderSync(flecs::world& world);

    /// @brief カメラと Object3d を、現在のコンポーネントの値へ合わせる。
    /// MeshRenderer が付いたエンティティには Object3d を作り、外れたものからは取り除く。
    void Update();
    /// @brief Object3d を描画する。Object3dCommon::PreDraw の後に呼ぶ。
    void Draw();

    /// @brief シーンのカメラを返す。Object3dCommon の既定のカメラとして使う。
    Camera* GetCamera()
    {
        return &camera_;
    }

private:
    void UpdateCamera();

    flecs::world& world_;
    Camera camera_;
};

/// @brief RenderHandle を flecs へ登録する（保存しない実行時のコンポーネント）。RenderSync より前に呼ぶ。
void RegisterRenderComponents(flecs::world& world);

} // namespace myEngine
