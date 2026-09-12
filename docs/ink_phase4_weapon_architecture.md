# Ink Lab 第4段階：ブキ定義と編集基盤

更新日: 2026-09-12。この文書は完成した第4段階の実装を説明する。ブキクラスはシューターとストリンガーの2種類、初期登録は `splattershot` と `tri_stringer`。新しい同系統のブキはJSONの定義を複製して作れる。射撃方法が異なる新しいクラスにはC++側の追加が必要である。原作の数値と近似の境界は [ストリンガー調査](ink_phase4_stringer_research.md) と [レティクル調査](ink_phase4_reticle_research.md) に分けた。

## 実装した型と責務

| 実装 | 責務 |
|---|---|
| [WeaponDefinition.h](../project/game/ink/WeaponDefinition.h) | 安定ID、所有する日本語名、クラス、参考情報、クラス別の静的性能を保持 |
| [InkWeaponFields.h](../project/game/ink/InkWeaponFields.h) と [WeaponCatalog.cpp](../project/game/ink/WeaponCatalog.cpp) の項目一覧 | 型付きのメンバーポインター、保存キー、日本語名、単位、分類、範囲をJSONとエディタで共有 |
| [WeaponCatalog.h/.cpp](../project/game/ink/WeaponCatalog.h) | 順序付きの定義、ID検索、複製、検証、JSON入出力、内蔵初期値。DirectXやプレイヤー状態へ依存しない |
| [InkSimulation.Stringer.cpp](../project/game/ink/InkSimulation.Stringer.cpp) と [InkSimulation.cpp](../project/game/ink/InkSimulation.cpp) | 装備のコピー、チャージ・クールダウン、発射、投射体、塗り、ダメージを処理 |
| [InkShooterScene.Weapons.cpp](../project/game/scene/InkShooterScene.Weapons.cpp) | 選択中の下書き、有効な編集カタログ、未保存表示、適用・保存・明示的な再読込の操作 |
| [InkShooterScene.WeaponVisuals.cpp](../project/game/scene/InkShooterScene.WeaponVisuals.cpp) とレティクル | 弓、矢、チャージ、照準の表示。弾道と所有者マスクは書き換えない |

```cpp
enum class WeaponClass { Shooter, Stringer };
struct WeaponDefinition {
    std::string id;
    std::string displayNameJa;
    WeaponClass type = WeaponClass::Shooter;
    std::string reference;
    ShooterWeaponParams shooter;
    StringerWeaponParams stringer;
};
```

名前で射撃処理を分岐させない。`type` に対応するペイロードだけを保存・実行する。名前と参考情報は `std::string` が所有し、JSON解析時の一時文字列を `const char*` として保持しない。既存paramsの固定 `name` ポインターはカタログの保存対象・日本語表示名として使わない。

カタログの公開APIは `Defaults()`、`Entries()`、`Find(id)`、`DefaultId()`、`Upsert(definition,error)`、`Clone(sourceId,newName,error)`、`Load(path,error)`、`Save(path,error)`。`ValidateWeapon(definition,error)` も単独で利用できる。`Entries` と `Find` は読み取り専用で、成功した変更操作の後まで取得した参照を持ち越さない。

## 保存形式と114項目

実ファイルは [project/resources/configs/ink_weapons.json](../project/resources/configs/ink_weapons.json)。アプリケーションの実行ディレクトリからは `resources/configs/ink_weapons.json` を使う。`schemaVersion` は整数の1、クラス文字列は `shooter` / `stringer`。根拠のキーは **`reference`** であり、`sourceVersion` や `sourceUrls` ではない。未知キーはエラーになる。

次は読み込み可能な最小例。省略した性能項目には各クラスのC++初期値を使い、保存時にはそのクラスの全保存項目を書き出す。

```json
{
  "schemaVersion": 1,
  "defaultWeaponId": "splattershot",
  "weapons": [
    {
      "id": "splattershot",
      "displayNameJa": "スプラシューター参考",
      "class": "shooter",
      "reference": "Splatoon 3 11.3.0 / 塗りの形と周期はCG2近似",
      "params": {}
    },
    {
      "id": "tri_stringer",
      "displayNameJa": "トライストリンガー参考",
      "class": "stringer",
      "reference": "Splatoon 3 11.3.0 / 弾道・塗り・爆発演出にはCG2近似を含む",
      "params": {}
    }
  ]
}
```

保存項目数はシューター61、ストリンガー53、合計**114**。これは保存される性能フィールド数で、114種類のブキや114個すべての有効な調整項目を意味しない。`TypedFloatField` / `TypedIntField` / `TypedBoolField` を入出力とUIで共用し、単位・範囲も同じ定義から表示する。

`参考` グループはUIで無効化し、公開値や未採用の条件を誤って現行の調整項目として扱わない。参考値もJSONには残る。`enableChargeKeep` は `editable=false`、`true` の読込・適用は未対応エラーになる。`arrowCount` は1～3本を許容し、投射体生成、HUDの本数、レティクルの側点と装填中の見た目が同じ設定を使う。`reference` はJSONの文字列として保存するが、現在の編集画面に参考情報用のテキスト入力欄はない。

## 編集・試し撃ち・保存の実際の操作

シーンは `weaponCatalog_`、選択中の `weaponDraft_`、`selectedWeaponId_`、下書きと一覧それぞれの未保存フラグを持つ。適用済みの性能は `Simulation` 内の別コピーである。「保存済みカタログ」をもう一つメモリに常駐させる方式ではなく、ディスク上のJSONから再読込する。

| 画面の操作 | 完成実装の動作 |
|---|---|
| `編集中のブキ` | 選択変更前に現在の下書きを検証し、有効ならカタログへ反映して次の定義を編集する。別ブキの有効な変更は一覧に残る。不正な下書きでは選択変更を止め、入力を保持する |
| `名前` | 所有する日本語表示名を編集。IDとクラスはこの画面で変更しない |
| `このブキで試し撃ち` | 下書きを検証してカタログへ反映し、装備へ値をコピー。ファイル保存は行わない |
| `複製して新しいブキを作る` | 現在の有効な定義を複製。`custom_shooter_N` / `custom_stringer_N` の未使用IDを採番し、「のコピー」を付けた名前で編集対象にする。保存・装備へ自動適用しない |
| `ブキ一覧をファイルに保存` | 下書きを検証・反映してから一覧全体を保存する。選択中の1ブキだけを保存するボタンではない。成功時だけ未保存状態を解除し、装備へは自動適用しない |
| `未保存の変更を戻して再読込` | **未保存の下書き・一覧の変更を破棄して**ファイルを読み直すことを明示したボタン。成功時に編集カタログと下書きを置換する。失敗時は現在のデータと未保存状態を保持する |
| `1` / `2` / `Q` | 基準のシューター／ストリンガーへ切替、またはカタログ順に次のブキへ切替。編集中の有効な変更を確保してから装備へ適用する |

再読込は使用中の装備を勝手に変更しない。使用中のIDが新しいファイルに無くても装備コピーは残る。編集中のIDが消えた場合は `defaultWeaponId` を編集対象に選ぶ。再読込後の下書きを実際の試し撃ちへ反映するときは、適用ボタンを使う。

未保存表示は `weaponDraftDirty_ || weaponCatalogDirty_` で判定する。下書きの検証失敗は入力を保ち、保存が失敗しても有効な編集一覧を残してエラーを表示する。選択中だけを保存済み値へ戻す別ボタン、自動ファイル監視、外部変更とのマージ、履歴管理は実装していない。

## 発射時の設定と装備変更

`Simulation::Equip(const WeaponDefinition&)` はクラス、ID、表示名、両クラスの性能値をコピーする。`ActiveWeaponClass()`、`ActiveWeaponId()`、`ActiveWeaponName()` で使用中の定義を参照できる。ストリンガーは `IsCharging()`、`ChargeTime()`、`ChargeProfile()`、`EmbeddedArrows()` を描画側へ公開する。JSONの読み込みをSimulation内で行う設計ではない。

切替時はチャージと予約射撃を中断し、押しっぱなしのトリガーを一度離すまで新しいブキで射撃しない。残りのクールダウン、飛行中の弾と落下飛沫、冷却矢、タンク、位置、床や壁の塗りは保持する。

`Projectile` は発射時の `tuning` と種別を持ち、ストリンガーは直撃・自由落下・爆発の専用値も保持する。生じる飛沫と `EmbeddedArrow` にも必要な設定を引き継ぐ。後からカタログや装備を編集しても、既存弾の物理・ダメージ・塗りを現在の装備から引き直さない。`PaintKind` は主着弾／飛沫／足元／爆発などの塗り用途で、ブキクラスとは別の分類である。

発射方法はシューターの連射とストリンガーの保持・解放で分ける。ストリンガーの `ShotsFired` は斉射1回につき1増え、3矢なら投射体は3個。インクも斉射単位で1回だけ消費する。低インク時は支払える段階へ下げず不発にする。着弾・爆発塗りは従来と同じ `PaintStamp` をCPU/GPUへ渡す。

## 入力検証と原子置換による保存

`Load` は一時カタログを構築し、全件が有効なときだけ現在の一覧とIDをswapする。失敗時に有効なカタログを途中まで書き換えない。

| 検証 | 現在の条件 |
|---|---|
| ファイル・構造 | 2MiB以内、ルートobject、整数schemaVersion=1、weaponsは1～128件、深すぎる入れ子を拒否 |
| 識別子 | idは1～64文字の英小文字・数字・`_`。重複ID、存在しないdefaultWeaponIdを拒否 |
| 表示情報 | 日本語名は空でないUTF-8で240バイト以内、referenceはUTF-8で4096バイト以内 |
| JSONキー | 同一object内の重複キー、未知のroot・weapon・paramsキー、未対応classを拒否 |
| 性能 | 数値型、整数項目の整数性、変換前後の有限値・範囲、威力・精度・チャージ時間など実装済みの関係条件を検証 |
| 未対応機能 | charge keep有効、4本以上のストリンガー等を拒否 |

存在する不正な値を初期値に置き換えて成功扱いにはしない。省略可能なparams項目だけがクラス初期値へ戻る。初回起動でJSONが無い・壊れている場合は内蔵の2ブキで起動し、元ファイルを自動で上書きしない。

`Save` は全件を検証し、対象と同じディレクトリに一時ファイルを書き出す。Windowsでは `CreateFileW`、`WriteFile`、`FlushFileBuffers`、`CloseHandle` の成否を確認し、最後に `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` で置換する。既存ファイルを先に削除しない。置換失敗では一時ファイルを整理し、元のJSONとメモリ内の編集を保持する。これは書きかけファイルの公開を避ける保存処理で、外部エディタとの競合検出・バックアップ世代管理までは含まない。

## ブキを追加する手順と拡張の境界

既存のシューター／ストリンガーの処理で表現できるブキは、UIの複製、またはJSONの定義複製で追加できる。IDを重複させず、日本語名、reference、対応するparamsを変更し、検証して保存する。C++のブキ名分岐や独立した描画システムの追加は不要。ブキモデルや効果はそのクラスの共通表現を使う。

射撃方法そのものが異なる新しいクラスには、`WeaponClass`、固有params、項目一覧とJSONのclass分岐、検証、Simulationの実行・投射体処理、必要なHUD・見た目、回帰テストをC++側へ追加する。JSONへ未対応のclass名を書くのみでは新方式を実行できない。サブ・スペシャル、異なる形の複数弾配置、モデルアセット指定、汎用のプラグイン式武器処理は第4段階の範囲外。

## 確認状況

[ink_weapon_catalog_tests.cpp](../project/tools/ink_weapon_catalog_tests.cpp) はMSVC C++17でPASS。114項目の往復、日本語名と日本語パス、順序とID、複製IDの一意性、既存シューターの基準値、不正JSON・版・型・巨大数・NaN・重複キー・チャージ順序・未対応機能の拒否、保存先ロック時の元バイト列保持を確認した。

[test_ink_simulation.ps1](../project/tools/test_ink_simulation.ps1) は既存Simulation、飛沫パターン、ストリンガーパターン、ストリンガーSimulation、カタログ、レティクル数学の6実行ファイルをビルド・実行する。弾の設定保持や45F爆発は [ストリンガー統合テスト記録](ink_phase4_stringer_test_notes.md) に記載した。保存の単体テストと、画面上の操作・D3D12・ネイティブ実行の確認は別の検証であり、後者の最終結果は親側の実装レポートにまとめる。
