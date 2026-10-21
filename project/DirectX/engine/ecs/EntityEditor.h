#pragma once
#include "ComponentRegistry.h"
#include "EditorCamera.h"
#include "EditorHistory.h"
#include "SceneSerializer.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace myEngine {

class Camera;

/// @brief ギズモの操作の種類。
enum class GizmoOperation {
    Translate,
    Rotate,
    Scale,
};

/// @brief ECS のシーンを編集するエディタ（ヒエラルキー・インスペクター・保存・Play/Stop・元に戻す/やり直す）。
/// 操作の本体（作成・削除・保存・Play/Stop・元に戻す）はどの構成でも使える。ImGui の画面（Draw）は USE_IMGUI のときだけ描く。
///
/// 編集中は、シーンのカメラとは別のエディタ用カメラ（GetCamera）で見る。右ドラッグで向き、右ドラッグ中の WASD / QE で移動、
/// 中ドラッグで平行移動、ホイールで前後、F で選んだエンティティに寄る。
/// 選んだエンティティにはギズモ（ImGuizmo）を出す。W / E / R で移動・回転・拡大縮小、Ctrl でスナップ。
/// 1回のドラッグは1つの操作として履歴に残る。画面の右下の視点キューブで、カメラの向きを変えられる。
///
/// Play/Stop は Unity の「シーンだけ読み直す」と同じ考え方で、Play の時点のシーンをメモリへ保存し、
/// Stop でエンティティを消してから保存した内容を読み直す。Play 中の操作は履歴に残さない。
class EntityEditor {
public:
    /// @param sceneFile 保存先のシーンファイル（UTF-8 のパスから作ったもの）。
    EntityEditor(ComponentRegistry& registry, std::filesystem::path sceneFile);

    /// @brief ImGui のウィンドウ（シーン操作・ヒエラルキー・インスペクター）を描き、ショートカットを処理する。ImGui のフレームの中で呼ぶ。
    void Draw();

    /// @brief ゲームのシステムを動かしてよいか（Play 中か）。
    bool IsPlaying() const
    {
        return playing_;
    }

    /// @brief 保存していない変更があるか。元に戻して保存した時点まで戻れば false になる。
    bool IsDirty() const
    {
        return externalDirty_ || !history_.IsAtSavedPoint();
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
    /// @brief 名前を変える。
    bool SetName(flecs::entity entity, const std::string& name);
    /// @brief 親を変える。parent が無効ならルートへ移す。自分や自分の子孫は親にできない。
    bool SetParent(flecs::entity entity, flecs::entity parent);
    /// @brief エンティティを change で書き換え、1つの操作として履歴に残す。値の変更などに使う。
    /// @return 状態が変わったらtrue。
    bool ModifyEntity(const std::string& label, flecs::entity entity, const std::function<void(flecs::entity)>& change);

    /// @brief 履歴を通さずに値を書き換えたことを通知する（変更ありにする）。
    void MarkDirty()
    {
        externalDirty_ = true;
    }

    /// @brief 直前の操作を元に戻す。Play 中は何もしない。
    bool Undo();
    /// @brief 元に戻した操作をやり直す。Play 中は何もしない。
    bool Redo();

    const EditorHistory& GetHistory() const
    {
        return history_;
    }

    /// @brief エディタ用カメラを返す。編集中の表示に使う（Play 中はシーンの主カメラで見る）。
    EditorCamera& GetCamera()
    {
        return camera_;
    }

    /// @brief エディタ用カメラを、シーンの主カメラ（primary の CameraComponent）と同じ位置・向きにする。
    /// 主カメラの z 軸まわりの回転は使わない。主カメラが無ければ何もしない。
    /// @return 主カメラが見つかればtrue。
    bool ResetCameraToSceneCamera();
    /// @brief 選んでいるエンティティが画面の中央に来るよう、エディタ用カメラを寄せる。
    /// @return 選んでいるエンティティがあればtrue。
    bool FocusSelected();

    /// @brief ギズモで使う視野角とクリップ距離を持つカメラ（シーンの描画に使うカメラ）を設定する。
    /// 位置と向きはエディタ用カメラのものを使う。nullptr ならギズモと視点キューブを出さない。
    void SetSceneCamera(const Camera* camera)
    {
        sceneCamera_ = camera;
    }

    GizmoOperation GetGizmoOperation() const
    {
        return gizmoOperation_;
    }

    void SetGizmoOperation(GizmoOperation operation)
    {
        gizmoOperation_ = operation;
    }

    /// @brief ギズモの軸をエンティティの向き（ローカル）に合わせるか。false ならワールドの軸。
    bool IsGizmoLocal() const
    {
        return gizmoLocal_;
    }

    void SetGizmoLocal(bool local)
    {
        gizmoLocal_ = local;
    }

    /// @brief Transform の連続した変更（ギズモのドラッグ）を始める。変更前の状態を覚える。Play 中は何もしない。
    /// @return 始められたらtrue（すでに始めていればfalse）。
    bool BeginTransformEdit(flecs::entity entity);
    /// @brief ワールド行列を、操作の種類に合う Transform の値（親から見たローカルの値）へ書き戻す。履歴には残さない。
    /// 移動は translate だけ、回転は rotate だけ（直前の角度に近い値を選ぶ）、拡大縮小は scale だけ（符号は元のまま）を変える。
    /// @return 書き戻せたらtrue。Transform が無い、行列が分解できないときはfalse。
    bool ApplyGizmoMatrix(flecs::entity entity, const Matrix4x4& world, GizmoOperation operation);
    /// @brief BeginTransformEdit から今までの変更を、1つの操作として履歴に残す。
    /// @return 変化があって記録したらtrue。
    bool EndTransformEdit(const std::string& label);
    /// @brief Transform の連続した変更の途中か。途中は元に戻す・やり直すをしない。
    bool IsEditingTransform() const
    {
        return transformEditBefore_.has_value();
    }

    /// @brief 現在のシーンをシーンファイルへ保存する。Play 中は保存しない。
    bool Save();
    /// @brief 指定したファイルへ保存する。Play 中は保存しない。
    bool SaveAs(const std::filesystem::path& path);
    /// @brief シーンのエンティティを消して、シーンファイルから読み直す。履歴も消える。Play 中は読み直さない。
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
    /// @brief 文字列からシーンを読み込み、選択を GUID で選び直す。
    bool LoadSceneText(const std::string& text);
    /// @brief 操作を履歴へ記録する。Play 中は記録しない。
    void Record(EditorCommand command);
    /// @brief entity と、EntityGuid を持つ子孫を、親が先になる順に集める。
    std::vector<flecs::entity> CollectSubtree(flecs::entity entity) const;
    /// @brief 元に戻す・やり直すの後で、選択を GUID で選び直す。
    void ReselectAfterHistory(const std::optional<EntityGuid>& selectedGuid);

#ifdef USE_IMGUI
    void HandleShortcuts();
    void HandleCameraInput();
    void DrawGizmo();
    void DrawToolbar();
    void DrawHierarchy();
    void DrawHierarchyNode(flecs::entity entity);
    void DrawInspector();
#endif

    ComponentRegistry& registry_;
    SceneSerializer serializer_;
    EditorHistory history_;
    EditorCamera camera_;
    // ImGui のウィンドウの外で右・中ボタンを押して、カメラを動かしている途中か。
    bool cameraDragging_ = false;
    const Camera* sceneCamera_ = nullptr;
    GizmoOperation gizmoOperation_ = GizmoOperation::Translate;
    bool gizmoLocal_ = false;
    // ギズモのドラッグ（BeginTransformEdit〜EndTransformEdit）の変更前の状態。
    std::optional<EntityState> transformEditBefore_;
    std::filesystem::path sceneFile_;
    flecs::entity selected_;
    bool playing_ = false;
    // 履歴を通さない変更（MarkDirty）があったか。保存か読み直しで消える。
    bool externalDirty_ = false;
    std::string playSnapshot_;
    std::string status_;
    // ヒエラルキーの右クリックメニューで選んだ削除対象。描画中に消さないよう、フレームの最後に消す。
    flecs::entity pendingDelete_;
    // インスペクターでの連続した変更（ドラッグ・文字入力）を1つの操作にまとめるための状態。
    std::optional<EntityState> inspectorFrameBefore_;
    std::optional<EntityState> inspectorEditBefore_;
};

} // namespace myEngine
