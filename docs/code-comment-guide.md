# 自作C++の読み方とコメントの基準

エンジンは`project/DirectX/engine`、ゲームは`project/game`にあります。外部ライブラリは`project/externals`です。エンジンの型は`cg2`名前空間に置き、ゲーム固有の制約はゲーム側に書きます。

まずヘッダーで型の責務と呼び出し方を読み、必要な実装へ進んでください。説明は日本語を基本とし、検索に使うAPI名・JSONキー・識別子はコードと同じ綴りを残します。

## 型と関数

すべての自作クラス・構造体の定義に`/// @brief`を付けます。前方宣言には同じ説明を繰り返しません。構造体には「何のデータか」、管理クラスには「何を所有し、何を担当するか」を書きます。設定、実行中の状態、表示用の写しを区別できる説明にします。

関数の説明は宣言のあるヘッダーに置きます。cpp内だけで使う補助関数は定義の直前に書きます。名前だけで分かりにくい引数・戻り値には`@param`・`@return`を使い、寿命・呼び出す順序・失敗時の扱いには`@note`を使います。同じ意味のオーバーロードやgetter/setterは、隣接する宣言の共通説明としてまとめても構いません。

以下は書き方の例です。

```cpp
/// @brief 制作された機体設定と表示順を所有する。
/// 実行中のHP・装備・成長状態はPlayerに保持する。
class PlayerClassCatalog {
public:
    /// @brief JSONを読み込み、全件の検証が成功した時点で設定を置き換える。
    /// @param path 読み込むJSONファイルのパス。
    /// @return 読み込みと検証に成功した場合true。
    /// @note 失敗時は既存設定を保つ。成功時は取得済みポインターを再取得する。
    bool Load(const std::string& path);
};
```

型のコピー、借用ポインター、`unique_ptr`の受け渡しを説明するときは、実際の所有関係と合わせます。返すポインターをいつまで使えるか、削除や再読み込みで無効になるかは、呼び出し側が必要とする契約です。

時間・角度・座標などは単位を明記します。このゲームの`deltaTime`は秒ですが、自機の`reloadSpeed`は60FPS相当の基準フレーム数です。名前から単位を推測させず、変換する箇所の理由も残します。

## 実装中の説明

処理のまとまりの直前に、順序・制約・計算方法を選んだ理由を書きます。弾の走査中に子弾を追加しない理由、GPUフェンス完了まで旧バッファを保持する理由、ボイスを破棄してからPCM領域を解放する理由などが対象です。

```cpp
// 元の弾を走査しきるまで子弾を別配列へ集め、
// bullets_の再確保で走査が壊れるのを防ぐ。
std::vector<std::unique_ptr<Bullet>> children;
```

`i`を増やす、値をそのまま代入する、といったコードから直接分かる説明は増やしません。無効にした旧コードは削除し、復元にはGit履歴を使います。必要な制作専用処理は`USE_IMGUI`などの既存のビルド条件に従います。

## 書式と確認

リポジトリ直下の[.clang-format](../.clang-format)が自作C++の基準です。インデントは4スペース、参照・ポインターは型側に付けます。includeの順番、文字列リテラル、コメント本文は自動で組み替えません。外部ライブラリと生成コードは対象に含めません。

Visual Studioに同梱されたclang-formatを、編集した自作ファイルへ適用できます。変更後は差分を読み、コメントの単位・引数名・参照の寿命が実装と一致することを確認します。

[コメント監査ツール](../project/tools/audit_source_comments.py)で型・関数の説明不足と存在しない`@param`を検出できます。実行方法と検査の限界は[単元3の報告](source-review-unit3/README.md)を参照してください。機械検査が通った後も、説明の内容を確認する必要があります。

この基準は、[C++ Core Guidelinesの意図を説明するコメント](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#nl2-state-intent-in-comments)、[名前付きの関数への分割](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f1-package-meaningful-operations-as-carefully-named-functions)、[Doxygenのコメント形式](https://www.doxygen.nl/manual/docblocks.html)を参考にしています。
