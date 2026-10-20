#pragma once
#include "ComponentRegistry.h"
#include "SceneSerializer.h"

#include <filesystem>
#include <string>

namespace myEngine {

/// @brief ECS のシーンを編集するエディタ（ヒエラルキー・インスペクター・保存・Play/Stop）。
/// 操作の本体（作成・削除・保存・Play/Stop）はどの構成でも使える。ImGui の画面（Draw）は USE_IMGUI のときだけ描く。
///
/// Play/Stop は Unity の「シーンだけ読み直す」と同じ考え方で、Play の時点のシーンをメモリへ保存し、
/// Stop でエンティティを消してから保存した内容を読み直す。
class EntityEditor {
public:
    /// @param sceneFile 保存先のシーンファイル（UTF-8 のパスから作ったもの）。
    EntityEditor(ComponentRegistry& registry, std::filesystem::path sceneFile);

    /// @brief ImGui のウィンドウ（シーン操作・ヒエラルキー・インスペクター）を描く。ImGui のフレームの中で呼ぶ。
    void Draw();

    /// @brief ゲームのシステムを動かしてよいか（Play 中か）。
    bool IsPlaying() const
    {
        return playing_;
    }

    /// @brief 保存していない変更があるか。
    bool IsDirty() const
    {
        return dirty_;
    }

    /// @brief 選択中のエンティティを返す（無ければ無効なエンティティ）。
    flecs::entity GetSelected() const
    {
        return selected_;
    }

    void Select(flecs::entity entity)
    {
        selected_ = entity;
    }

    /// @brief 新しいエンティティを作る。parent が有効ならその子にする。作ったものを選択する。
    flecs::entity CreateEntity(const std::string& name, flecs::entity parent = flecs::entity());
    /// @brief エンティティを子ごと消す。
    void DeleteEntity(flecs::entity entity);
    /// @brief 表のキー名でコンポーネントを既定値で追加する。
    bool AddComponent(flecs::entity entity, const std::string& componentName);
    /// @brief 表のキー名でコンポーネントを取り除く。
    bool RemoveComponent(flecs::entity entity, const std::string& componentName);

    /// @brief 値を書き換えたことを通知する（変更ありにする）。
    void MarkDirty()
    {
        dirty_ = true;
    }

    /// @brief 現在のシーンをシーンファイルへ保存する。Play 中は保存しない。
    bool Save();
    /// @brief 指定したファイルへ保存する。Play 中は保存しない。
    bool SaveAs(const std::filesystem::path& path);
    /// @brief シーンのエンティティを消して、シーンファイルから読み直す。Play 中は読み直さない。
    bool Reload();

    /// @brief Play を始める（今のシーンをメモリへ保存する）。
    void Play();
    /// @brief Play を止め、Play を始めたときのシーンへ戻す。
    void Stop();

    /// @brief 直近の操作の結果（画面の下に表示する）。
    const std::string& GetStatus() const
    {
        return status_;
    }

private:
    /// @brief EntityGuid を持つエンティティをすべて消す。
    void ClearSceneEntities();
    /// @brief 文字列からシーンを読み込み、選択を外す。
    bool LoadSceneText(const std::string& text);

#ifdef USE_IMGUI
    void DrawToolbar();
    void DrawHierarchy();
    void DrawHierarchyNode(flecs::entity entity);
    void DrawInspector();
#endif

    ComponentRegistry& registry_;
    SceneSerializer serializer_;
    std::filesystem::path sceneFile_;
    flecs::entity selected_;
    bool playing_ = false;
    bool dirty_ = false;
    std::string playSnapshot_;
    std::string status_;
    // ヒエラルキーの右クリックメニューで選んだ削除対象。描画中に消さないよう、フレームの最後に消す。
    flecs::entity pendingDelete_;
};

} // namespace myEngine
