# Neon Boss Depth Encounter 素材出典メモ

既存の素材・台帳・モデル内メタデータを読み取った記録。最終収録版のモデル／source／config hash、選定8 PNG／8 WebPと動画2本、無音streamと素材単体非配布は照合済み。画像内の全texture／font binding、個別素材のorigin、公開採否は要確認。この記録は公開許諾の取得や全素材の権利確認完了を示さない。
素材・ライセンス本文は変更せず、新規素材取得・公開・配布は行っていない。

## 確認した既存根拠

リポジトリ相対パスで記す。古い台帳の件数・Release除外記述は当時の記録で
あり、現在のpackage収録範囲の断定には使わない。

- `project/resources/models/neon_hologram/README.md`
- `project/resources/models/neon_hologram/AvatarSample_B.glb` のVRM meta
- `project/resources/models/neon_hologram/line_masks/README.md`
- `project/resources/models/neon_hologram/line_masks/quality/README.md`
- `THIRD_PARTY_NOTICES.md`
- `docs/public-assets-audit.md`、`docs/public-assets-inventory.md`
- `project/resources/fonts/ZenMaruGothic-OFL.txt`
- `docs/neon-boss-depth/legacy-recording.md`
- `docs/neon-boss-depth/CODEX_NEON_BOSS_DEPTH_GOAL.md` の公開条件・引継ぎ要件

## AvatarSample_B：既知の第三者モデル

| 項目 | 実際に確認した記録 |
|---|---|
| ファイル | `project/resources/models/neon_hologram/AvatarSample_B.glb` |
| 元の形式／取り込み | 作者提供の既存VRMをbyte-for-byteコピーし、GLB名で保存 |
| 既存台帳の取得日 | 2026-10-01 |
| 既存台帳のサイズ | 28,333,772 bytes |
| 既存台帳のSHA256 | `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518` |
| 実VRM meta title / author | `AvatarSample_B` / `VRoid Project` |
| 実VRM meta licenseName | `Other` |
| 実VRM meta commercialUssageName / allowedUserName | `Allow` / `Everyone`（原データの綴り） |
| 埋込みのその他条件 | violentUssageName / sexualUssageName とも `Allow` |

otherLicenseUrl と otherPermissionUrl は同じ既存VRoid Hub利用条件URLで、
commercial/modification/redistribution等のallowを含む。原値は次のURL。

<https://hub.vroid.com/license?allowed_to_use_user=everyone&characterization_allowed_user=everyone&corporate_commercial_use=allow&credit=unnecessary&modification=allow&personal_commercial_use=profit&redistribution=allow&sexual_expression=allow&version=1&violent_expression=allow>

既存READMEは [VRoid Project AvatarSample A〜Z 利用条件](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z)
を参照し、営利／非営利利用、改変、キャラクター利用、クレジット不要等を記録
している。同時に禁止事項も記録している。2026-10-05に上記の公式Web原文を
再確認した（ページ更新表示2024-12-26）。営利・非営利の利用、画像・動画での
利用、改変、クレジット不要が記載される一方、CC0としての配布、条件に反する
素材配布、虚偽の推奨表示等は禁止される。全条項はリンク先原文で照合する。

`Other` は条件が未知という意味でもCC0という意味でもない。このモデルには
上記の具体的な既存条件がある。一方、入手した経路・ダウンロード元URLは
既存READMEでも未確認なので、公開前に作者の入手記録と現行原文を照合する。
モデルを自作、CC0、Repositoryのコードライセンス対象とは説明しない。

公開候補は作品のPNG／MP4／WebPである。モデルGLB／VRM、埋込みテクスチャ、
素材単体のダウンロードをPages用素材セットへ追加しない。画像・動画掲載と
素材ファイル再配布は別の行為として、採用する条件を記録する。

## 派生線データ・モーション・実装の区別

line_masks と quality候補は元モデルのテクスチャ・UV由来の派生データと既存
READMEに明記されている。CC0や完全独自素材として扱わず、元モデルの条件を
引き継ぐ。これらはDeveloper Preview比較候補であり、最終Depth録画で実際に
使用されたとの証拠はない。現行NeonBossVisualは共有のRecommended Line Artを
適用する。公開時は最終設定・bindingを照合し、未使用候補を作品の採用品と
説明しない。

AvatarSample_Bには埋込みanimationがない。既存Preview Idle／Attack、今回の
Depth用姿勢clip／補間はプログラム生成で、GLBに付属するモーションや外部の
モーションキャプチャ素材と説明しない。GPU Skinning、NeonSkinnedRenderer、
共有Style、Directional Dissolveは既存実装を再利用し、今回の作業ではDepthの
攻撃契約・配置・モーション対応・床予告／接続表示・本編接続を追加した。
モデルの造形制作とゲーム側の実装を区別して記載する。

## 他の既存素材：出自未確認の項目を残す

| 対象 | 既存根拠／公開前の扱い |
|---|---|
| Player.png、PlayerBullet.png、BossHP.png、HPBarCurrent.png等 | public-assets-inventoryはD分類。画面内に実際に現れる画像／材質を最終bindingで確認し、作者の制作・入手記録と画像／動画掲載条件を照合する。第三者モデルの許諾で一括して許諾済みにしない |
| その他のOBJ／MTL／画像 | 既存の参照・分類は制作経緯の証明ではない。実際に映る対象を個別に確認する。名前や同梱だけで自作とは説明しない |
| bulletShoot.mp3 | 既存Third-party台帳に入手元・再配布条件未確認と記録。今回の公開候補は無音で、音声streamもサウンドトラック追加も行わない。無音化は他の画像／モデルの公開条件確認の代わりにはならない |
| audio/tank_expedition | 既存README／生成script／測定記録に波形合成で外部録音・sampleを使わない記録。今回は動画へ音声を収録しない |
| Zen Maru Gothic | 既存OFL 1.1本文あり。文字描画の利用とフォントファイル配布を区別する。Pagesのためにフォントファイルをコピーする作業は今回行わない |
| ライブラリ／SDK DLL | Third-party noticesの対象。PNG／MP4公開とソース／実行package配布を区別する。配布テスト成功を未知素材の権利確認完了にはしない |

## 公開担当／作者が記入する確認欄

| 確認項目 | 現在の状態 | 記入する証拠 |
|---|---|---|
| 最終原本のモデル／source／config hashがこの記録と対応 | PASS（hash照合範囲） | final recording prepared／resource manifest、最終907入力／651入力の再照合。全texture／font binding網羅は要確認 |
| AvatarSample_Bの入手記録・現行利用条件原文との照合 | 要確認 | 作者確認日、原文URL／保存記録、画像動画掲載条件 |
| 最終候補に現れるD分類画像／材質の出典・掲載条件 | 要確認 | 個別素材と出典／許諾根拠、採用または除外判断 |
| 選定MP4無音、素材単体配布なし | PASS（handoff範囲） | 2 MP4の最終ffprobe／full decode、handoffの19file選定一覧。internal manifest／metadataは公開ファイルに含めない |
| 公開先へ渡す最終metadataの個人情報確認 | 要確認 | internal hash台帳と公開captionを分離し、公開時の具体的なfile一覧を確認 |
| Pages公開採否 | 未判断 | 確認者／日付／公開可とした具体的な候補一覧 |

未確認素材は公開候補から除外するか、この要確認状態をPages担当へ明示する。
今回の引継ぎは公開・push・deployの実施指示ではなく、作者による最終公開判断
のための資料である。

最終引継ぎはローカルの`portfolio-handoff.md`、fileごとの原本／圧縮版対応・caption／alt・条件とhashは`generated/neon-boss-depth/final-media/handoff_a_20261006/manifest-v2.json`。内部台帳にはworkspace pathやprocess provenanceがあるため、Pagesへ台帳一式をそのまま公開する案ではない。公式利用条件を読むためのWeb確認で新しいモデル／画像／音源を取得してはいない。
