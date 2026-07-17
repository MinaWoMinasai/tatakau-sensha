# Player Ship Editor Prototype

スマートフォンで機体JSONを編集するための試作ツールです。

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
4. `JSONを書き出す` で保存する
5. PCに戻して `project/resources/configs/playerClasses.json` と差し替える

## 通信量

`vendor/konva.min.js` が入っていれば、通常の編集操作ではサーバー通信しません。
JSONの読み込み、ドラッグ、回転、書き出しは端末内で完結します。

## 下書き保存

編集内容はブラウザの `localStorage` に下書き保存されます。
これはクラウド同期ではなく、その端末・そのブラウザ内だけの保存です。

正式にゲームへ反映するには、必ず `JSONを書き出す` でファイルを保存してください。

## 注意

スマホのブラウザやファイルアプリによっては、ローカルHTMLからのファイル保存や再読み込みの挙動が少し違います。
安定確認したい場合は、PCで簡易ローカルサーバーを立てて同じWi-Fiからアクセスしてください。

```powershell
cd C:\Users\k024g\OneDrive\デスクトップ\自作エンジン2\project\tools\player_ship_editor
python -m http.server 8000
```

