# プレイヤー機体エディター

「たたかうせんしゃ」のプレイヤー機体データを、JSONを直接編集せずに視覚的に調整するためのブラウザツールです。

機体形状・射撃性能・砲塔配置をGUIから編集でき、CG2側では保存された `playerClasses.json` を自動検知してホットリロードできます。

## Web版

スマートフォンや、ローカルHTMLを開きにくい環境ではGitHub Pages版を利用できます。

**Web版**
https://minahaminanomina.github.io/PlayerEditor/

**ソースコード**
https://github.com/MinaHaMinaNoMina/PlayerEditor

> iPhone / Safariなどでは、ゲーム用JSONへの直接上書き保存が利用できない場合があります。
> その場合は `JSONを読み込む` → 編集 → `別ファイルとして書き出す` を使用してください。

## 主な機能

- 機体形状・表示名・必要ランクの編集
- リロード倍率・弾速倍率・弾ダメージ倍率・発射数・拡散角度の編集
- 砲塔の追加・複製・削除
- 砲塔のドラッグ配置
- 回転ハンドルによる砲塔角度調整
- 外周16方向へのスナップ / 自由移動
- 武器タイプ・砲身形状の変更
- 日本語の操作ガイド・パラメータ説明
- JSONの読み込み / 別ファイルへの書き出し
- 対応ブラウザでゲーム用JSONへ直接保存
- `localStorage` による下書き保存
- PC / スマートフォン対応

## 単体で確認する

Web版を開くだけでも仮データが表示され、砲塔の移動・回転や各パラメータの編集を確認できます。

サンプルJSONを使う場合は、

1. `JSONを読み込む`
2. `sample/playerClasses_sample.json` を選択
3. 機体や砲塔を編集
4. `別ファイルとして書き出す`

の順で確認できます。

## PCでCG2と連携する

PCではローカルサーバーから開くことを推奨します。

```powershell
python -m http.server 8000
```

ブラウザで次を開きます。

```text
http://localhost:8000
```

CG2との連携手順は次の通りです。

1. CG2を起動する
2. `ゲーム用JSONを開く` を押す
3. CG2の `project/resources/configs/playerClasses.json` を選択する
4. 機体・砲塔・射撃設定を編集する
5. `ゲームへ保存` を押す
6. CG2が変更を自動検知し、ゲームを再起動せず反映する

```text
プレイヤー機体エディター
        ↓
playerClasses.json へ保存
        ↓
CG2が変更を自動検知
        ↓
ホットリロード
        ↓
ゲームへ即時反映
```

## キャンバスの見かた

キャンバス内の `操作ガイドを表示` から各表示の意味を確認できます。

- 前マーク：機体の正面方向
- 破線リング：外周固定時に砲塔を置ける範囲
- 小さな点：16方向の配置候補
- シアンの枠：選択中の砲塔
- ピンクの回転マーク：ドラッグして砲塔を回転

各設定名の `?` を押すと、パラメータの意味・値を増減したときの変化・対応するJSONキーを確認できます。

## ファイル構成

```text
player_editor/
├─ index.html
├─ app.js
├─ styles.css
├─ README.md
├─ README_mobile.md
├─ sample/
│  └─ playerClasses_sample.json
└─ vendor/
   └─ konva.min.js
```

スマートフォンでの利用方法は [README_mobile.md](README_mobile.md) を参照してください。
