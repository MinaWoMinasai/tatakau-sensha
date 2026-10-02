#pragma once
#include "PlayerClassConfig.h"
#include <unordered_map>
/// @brief 制作された機体設定と表示順を所有する。実行中の装備・HP・成長はPlayerに保持する。
class PlayerClassCatalog {
public:
    /// @brief 機体設定を読み込む。失敗時は既存の値・表示順・取得済みポインターを保つ。
    /// @param path 読み込むJSONファイルのパス。
    /// @return 読み込みと検証に成功した場合true。
    /// @note 成功時は内容を置き換えるため、取得済みポインターを再取得する。
    bool Load(const std::string& path);
    /// @brief 現在の機体設定を表示順とともにJSONへ保存する。
    /// @param path 保存先のパス。
    /// @note 保存結果は戻り値では通知しない。エラー時のログも確認する。
    void Save(const std::string& path) const;
    /// @brief 指定した機体種類の既定設定を作成して返す。
    static PlayerClassConfig CreateDefaultConfig(ClassType type);
    /// @brief 制作設定を既定の機体一覧へ戻す。取得済みの設定ポインターは再取得する。
    void ResetToDefaults();

    /// @brief 機体設定を検索する。見つからなければnullptrを返す。
    /// @note 戻り値はCatalogが所有する。Load成功・ResetToDefaults・対象のErase後は再取得する。
    const PlayerClassConfig* Find(ClassType type) const;
    /// @brief 機体設定を検索する。見つからなければnullptrを返す。
    /// @note 戻り値はCatalogが所有する。Load成功・ResetToDefaults・対象のErase後は再取得する。
    const PlayerClassConfig* Find(const std::string& id) const;
    /// @brief 編集する機体設定を検索する。見つからなければnullptrを返す。
    /// @note 寿命はFindと同じ。IDの変更は表示順と整合させて管理する。
    PlayerClassConfig* FindMutable(const std::string& id);
    /// @brief 制作データの表示順で機体ID一覧を返す。
    /// @return Catalogが所有する一覧への読み取り専用参照。
    const std::vector<std::string>& OrderedIds() const
    {
        return classOrder_;
    }
    /// @brief 設定を追加または上書きする。既存IDの表示位置を保つ。
    /// @note 追加しても既存要素へのポインターは有効。既存IDの値は上書きされる。
    void InsertOrAssign(const PlayerClassConfig& config);
    /// @brief 指定IDの設定と表示順の項目を削除する。
    /// @param id 削除する設定自身が持つIDへの参照も渡せる。
    /// @return 対象が存在して削除できた場合true。
    /// @note 削除した設定へのポインターは以降使わない。
    bool Erase(const std::string& id);
    /// @brief 機体設定と表示順の所有者をまとめて交換する。
    /// @note 要素のポインターは有効だが、その要素を所有するCatalogが入れ替わる。
    void Swap(PlayerClassCatalog& other) noexcept;

private:
    std::unordered_map<std::string, PlayerClassConfig> classConfigs_;
    std::vector<std::string> classOrder_;
};
