# プレイヤー機体エディター

PCまたはスマートフォンで機体JSONを編集するための試作ツールです。

## PCからゲーム用JSONへ直接保存する

File System Access APIに対応したデスクトップ版Chrome / EdgeなどのChromium系ブラウザでは、ゲーム用の `playerClasses.json` を選択し、同じファイルへ直接保存できます。

1. ローカルサーバーを起動する

   ```powershell
   cd project/tools/player_ship_editor
   python -m http.server 8000
   ```

2. 対応ブラウザで `http://localhost:8000` を開く
3. `ゲーム用JSONを開く` を押し、`project/resources/configs/playerClasses.json` を選択する
4. 機体設定を編集する
5. `ゲームへ保存` を押す
6. 起動中のCG2が変更を検知し、自動で再読み込みすることを確認する

選択したファイルのハンドルは、ページを閉じるまでのセッション内だけ保持します。ページを再読み込みした場合は、もう一度 `ゲーム用JSONを開く` から選択してください。

ブラウザがFile System Access APIに対応していない場合や、安全なコンテキストとして認識されない環境では直接保存ボタンが無効になります。その場合も、従来の `JSONを読み込む`、端末内の下書き保存、`別ファイルとして書き出す` は利用できます。

## 項目の説明を見る

主要な設定名の横にある `?` をクリックまたはタップすると、日本語の説明、基準値や増減方向、対応するJSONキーを確認できます。

- PC：`?` をクリックするか、Tabキーで選択してEnterキーを押す
- スマートフォン：`?` をタップする
- 閉じる：右上の `×`、説明の外側、またはEscapeキー

日本語化されている選択肢も、JSONへ保存する内部値は従来の英語値を維持します。

## スマホへ送るもの

`player_ship_editor` フォルダを丸ごと送ってください。

最低限必要なファイルは以下です。

```text
player_ship_editor/
  index.html
  styles.css
  app.js
  vendor/
    konva.min.js
```

編集したい `playerClasses.json` または `playerClasses_edited.json` も一緒に送ります。

## 開き方

1. スマホで `index.html` を開く
2. `JSONを読み込む` から編集したいJSONを選ぶ
3. 機体、砲塔、パラメータを編集する
4. `別ファイルとして書き出す` で保存する
5. PCに戻して `project/resources/configs/playerClasses.json` と差し替える

## 通信量

`vendor/konva.min.js` が入っていれば、通常の編集操作ではサーバー通信しません。
JSONの読み込み、ドラッグ、回転、書き出しは端末内で完結します。

## 下書き保存

編集内容はブラウザの `localStorage` に下書き保存されます。
これはクラウド同期ではなく、その端末・そのブラウザ内だけの保存です。

PCの対応ブラウザでは `ゲームへ保存`、それ以外の環境では `別ファイルとして書き出す` でファイルを保存してください。

## 注意

スマホのブラウザやファイルアプリによっては、ローカルHTMLからのファイル保存や再読み込みの挙動が少し違います。
安定確認したい場合は、PCで簡易ローカルサーバーを立てて同じWi-Fiからアクセスしてください。

```powershell
cd project/tools/player_ship_editor
python -m http.server 8000
```

