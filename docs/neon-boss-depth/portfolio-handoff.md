# Neon Boss Depth — Pages引継ぎ

最終Development版の実描画から選んだ8 PNG／8 WebP、45秒動画、8秒抜粋、posterを引き継ぐ。原本・圧縮版のhash、実際の撮影条件、caption／altは[`manifest-v2.json`](../../generated/neon-boss-depth/final-media/handoff_a_20261006/manifest-v2.json)に保存した。技術QAと引継ぎはPASS。素材の出典に要確認事項があり、公開採否は未決定。Pages repositoryの編集・公開は行っていない。通常Release通し確認は未実施で、現在のC18は操作対象画面取得不可のためBLOCKED、Goal全体も未完了。manifestのC18 NOT RUNは採用時checkpointとして保持し、素材19file自体は変更していない。最初のmanifestも保持し、v2で8秒captionとC11訂正receiptの参照を補った。

## 作品説明案（164字）

床上の2D判定と、GPU Skinningで動くネオン投影体を同じ攻撃計画で結び付けたボス戦です。移動・射撃・回避を含む45秒の連続録画と、Volley・Dive・Beamの予告を紹介します。映像は初期通貨1000・購入強化なしのDeveloper開始状態からの自動通常入力で、本編の自然攻略や人間操作を示すものではありません。

実HP・位置・床判定は2D Enemyが持つ。攻撃計画のcircle／capsule、予告・発動・Recovery時間を判定と表示で共有し、Visualがsnapshotを一方向に読む。3D本体は足元anchorから高さを持ち、既存NeonSkinnedRendererとGPU Skinningで動く。実GLBに埋込みanimationは0本で、本実装のC++生成motionを使う。HP0でGameplayを止め、実poseを固定してDirectional Dissolve、core消灯、資源返却、結果へ進む。詳細は[architecture.md](architecture.md)、[attack-design.md](attack-design.md)。

## ファイルと推奨配置

画像の実保存先は`generated/neon-boss-depth/final-media/stills_a_20261006/`。次表の同名PNGとWebPが対応する。全てnative **1280×720**、静止画のdurationは非該当。PNG／metadataは原本のbyte同一copy、WebPはlibwebp／yuv420p／quality86のlossy圧縮。resize、補間、AI補完、演出追加はしていない。Pagesには`assets/neon-depth/`へWebPを置く案で、PNGとmetadataは確認用として保持する。推奨相対パスは未公開であり、URLを意味しない。

| ファイル名（拡張子PNG／WebP） | 用途・実frame | PNG bytes | WebP bytes |
| --- | --- | ---: | ---: |
| `01-hero` | 本体／core／自機の関係、sequence866／completed867 | 412,083 | 73,734 |
| `02-volley-air` | 3本の空中軌道と床予告、332／333 | 462,397 | 83,152 |
| `03-dive-descending` | 降下中、545／546。着地後の画像ではない | 452,395 | 76,402 |
| `04-beam-wall-clipped` | 内壁でclipされたactive Beam、1486／1487 | 442,069 | 76,012 |
| `05-player-behind` | 本体奥側の自機、2457／2458 | 437,785 | 75,512 |
| `06-phase2-controlled` | 別fixture、completed1022 | 454,496 | 81,376 |
| `07-dissolve-remnants` | 別fixture、completed854、progress0.805555 | 404,228 | 52,042 |
| `08-finished` | 別fixture、completed869、progress1 | 379,839 | 48,530 |

動画の実保存先は`generated/neon-boss-depth/final-media/media_a_20261005/`。posterは画像と同じ`stills_a_20261006/`にある。

| ファイル名・推奨相対パス | 用途 | bytes | 解像度／duration |
| --- | --- | ---: | --- |
| `assets/neon-depth/neon-depth-gameplay.mp4` | controls付き全編 | 8,884,270 | 1280×720／45秒、60fps、2700frames |
| `assets/neon-depth/neon-depth-loop.mp4` | 反復する短い抜粋 | 1,335,065 | 1280×720／8秒、60fps、480frames |
| `assets/neon-depth/neon-depth-clean-poster.webp` | 動画poster | 83,152 | 1280×720／静止画 |

clean posterは`02-volley-air.webp`のbyte同一alias（sequence332／completed333）、SHA256 `A229FBCB2FA27267FAB829618929ED8446D1E5B7FEB74000CF8D8BAF891B4AF8`。画像枚数は8のまま。旧sequence759の被弾callout posterと初期RGB-stride不良posterは過去原本として保存し、採用から除外した。その他の全file hashはmanifestを参照する。

## Caption／alt

| 素材 | caption | alt text |
| --- | --- | --- |
| 01 | 大型ネオン本体と床core、自機の高さ関係。 | 床coreの上へ浮遊する大型ネオンボスと、近接した緑の自機。 |
| 02／poster | 本体から伸びる3本の立体軌道と、同時に残る床予告。 | 大型ボス、3本の空中弾道、3つの橙色の床予告と、自機の射撃。 |
| 03 | 降下する本体と、固定された床の着地点。 | 橙色の着地円へ降下するネオンボスと、左側から撃つ自機。 |
| 04 | 内側の障害物で短く切られた床Beamと、本体からcoreへの接続線。 | 浮遊ボスから床coreへ伸びる紫色の線と、内側の壁で止まる短い斜めの床Beam。 |
| 05 | 本体の奥側にいる自機の輪郭とHPバー。 | ボスの胸から首の投影位置へ重なる緑の自機と、読み取れる自機HPバー。 |
| 06 | HPを3333へ注入した別の無敵fixtureで、Phase 2の5つの床予告と立体軌道を確認。 | Phase 2の浮遊ボス、5つの橙色の床予告、黄色の空中軌道、自機とHUD。 |
| 07 | 強制HP0の別fixture。Dissolve進行値0.8056で胴体が消え、手脚の残片が見えます。白い演出と撃破表示が重なります。 | 胴体が消えたボスの手脚の残片と、白い死亡演出、重なった2つの撃破表示。 |
| 08 | 強制HP0の別fixtureのFinished。本体・床core・接続線が消え、資源解放を実記録で確認。 | ボス本体と床core、接続線が消えた部屋に残る自機、床とHUD。 |

全編動画caption：標準DepthカメラとHUDを使用した45秒の連続実描画。自機HP120・初期ボスHP900・初期通貨1000（購入／強化なし）のDeveloper開始状態で、自動の通常入力により移動・射撃・ダッシュを行います。途中のHP注入、無敵、強制撃破は使用していません。本編の自然攻略・人間操作を示す映像ではありません。

動画alt：ネオンボスの3攻撃と、自動の通常入力による移動・射撃・回避を示す45秒の連続実描画。抜粋caption／altには「VolleyからDiveまでの8秒抜粋。反復時に位置の巻き戻りがあります」を付ける。seamless loopとは表記しない。

## 撮影条件と確認範囲

全素材は最終frozen `build_i` Development EXE（SHA256 `931EFCF9D8E12C4D78323B769C4EF65F3B89A4BE8A3522CBD0B8BE6BCFED8480`）。標準Depth cameraはtilt20度／distance90／focus y31、通常HUD、debug UIなし、reduced motion false。camera／profile／source・resource manifestは各metadataとhandoff manifestに対応する。

- **動画・画像01～05**：同じ2700連続実frameから採取。Shooter、Developer shortcut、seed20261005、通常入力の自動move／aim／shoot／dash。初期HP120／boss900／wallet1000、購入・強化なし、無敵・途中HP注入・強制attack／defeat・comparison freezeなし。HP120→93、boss900→492、coreDamage408、primary139／dash12／perfect0。自然Phase2・死亡は動画に含まれない。encoded45秒とslow motionを含む実presentation時計43.9373秒は区別する。
- **画像06**：別controlled shortcut、boss10000→3333をcompleted901前に注入、player無敵、5床予告／5空中軌道。dynamic probe／撮影frameのSparseFreezeあり。自然Phase2や同じ45秒の続きとは説明しない。
- **画像07／08**：別shortcut、player無敵、completed775前に強制HP0、SparseFreeze。854のprogressは消失面積率ではない。白い既存death pulseと重なる2撃破ラベルがあり、境界の一部を隠す。869で本体／core／connectorが消え、実記録でowned資源返却一度を確認した。

全PNG原本と全8WebPをrootが確認し、26代表状態matrixと合法下壁400を別QA証拠へ対応した。全動画はencode／probe／strict full decodeがPASS、browserで45秒終端まで再生した（total2700／dropped28／error0）。抜粋の反復再生sampleはtotal1998／dropped3／error0で、実巻き戻りを確認。totalはdropを含むbrowser報告で、全frame表示成功や無欠点とは呼ばない。撮影条件、背後・壁際の制限は[visual-review.md](visual-review.md)。

通常Developmentの自然攻略`normal_i_style1`と、実OSマウス照準900→883のnative Developer確認は別証拠。いずれもこの公開候補動画の人間操作証明や通常Release完走に置き換えない。最終All 69/69、両build、C18未実施部分は[validation.md](validation.md)、4run性能と資源の範囲は[performance.md](performance.md)に保存した。

## 出典と要確認事項

実装部分は2D攻撃契約・一方向snapshot・3D配置・C++生成motion・既存rendererへの接続・死亡lifecycle・確認機能である。既存モデル／NeonSkinnedRenderer／GPU Skinning／Directional Dissolve／共通post処理は既存資産を利用した。モデルを自作、生成motionをモーションキャプチャと表記しない。

3Dモデルは**AvatarSample_B／VRoid Project**。実GLB SHA256は`7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`、記録上licenseName Other／commercial Allow／allowedUser Everyone。[VRoid公式のAvatarSample A～Z利用条件](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z)は画像・映像制作、商用／非商用利用と改変を認め、CC0ではないことを明記している。公式条件は確認したが、このGLBの元download経路・作者側の保存記録との対応は**要確認**。

既存Player／Bullet／HP bar等のUI画像のoriginは**要確認**。Zen Maru GothicのOFL記録は存在するが、最終frame内の全font／texture bindingを網羅したという主張はしない。選定MP4にaudio trackはなく、旧音源の再利用条件は**要確認**のまま。未使用textureを画面の出典として補わない。詳細は[asset-source-notes.md](asset-source-notes.md)。モデル／texture／font／音源の単体配布はhandoffに含めない。

## Pages担当への配置案

heroと全編controls付き動画を冒頭に置き、Volley／Dive／Beamをcaptionと並べる。Phase2とDissolveの3枚には別fixtureの強制操作を明記する。軽量WebPを通常表示し、1280×720の比率を保持する。全編は`preload="metadata"`、posterはclean333を指定する。8秒抜粋は巻き戻りのある反復素材として任意で使う。

```html
<video controls playsinline preload="metadata" width="1280" height="720"
  poster="assets/neon-depth/neon-depth-clean-poster.webp">
  <source src="assets/neon-depth/neon-depth-gameplay.mp4" type="video/mp4">
</video>
```

原本と過去FAILを保持し、別fixtureを動画へspliceしていない。外部公開URLは未作成。commit／push／merge／deployは行っていない。
