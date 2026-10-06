# Depth 実装前の旧ボス連続録画

`generated/neon-boss-depth/m0-recorder/legacy_20261005_a` は、Depth の本体・AI・カメラ変更前に、録画計測だけを追加した Development バイナリで収録した旧 `neon_boss` の実描画である。初期の無変更 baseline バイナリとは異なる。`results.json` は completed=true、収録中 sourceChanges=[] を記録する。収録は 2026-10-05 10:14:02–10:15:04 UTC。最終の使用対象は `media_b` である。

この映像は旧表示の基準証拠であり、Depth の改善、通常ルートでの攻略、自然なボス撃破の証拠には使用しない。

## 元画像と実際の時計

実際の完了 Update 1–480 を、native 1280×720 の PNG 480 枚と各フレームの JSON として保存した。元画像は `evidence/recording/frames/frame_00000.png`–`frame_00479.png`。sequenceFrame 0–479 と simulationFrame 1–480 の対応があり、comparisonFreeze=false、heldDrawCount=0。元画像と JSON の合計は 169,599,985 bytes。設定の上限は 12 GiB、native worst-case preflight は 1,895,301,120 bytes、開始時空き容量は 109,189,595,136 bytes。

設定の fixedDeltaTime は 1/60、保存された float 値は 0.01666666753590107。480 枚を 60 fps で符号化した映像の長さは 8 秒。最終フレームに記録された baseElapsed は 8.000000417232513 秒、actual gameplayElapsed は 6.983333697542548 秒、boss presentationElapsed は 7.773317884653807 秒である。開始演出や時間倍率を含むため、8 秒の映像を「8 秒の通常速度戦闘」とは表示しない。PNG 保存にかかった約 62 秒の実時間をゲーム内時計へ足していない。

元画像・JSON の SHA256 は `media_b/internal-source-manifest.json` に各 480 件保存されている。この文書作成時に実ファイル 960 件を読み直し、すべてのハッシュと連続した sequence/simulation frame 対応が一致した。

## 収録条件

- Development、旧 `neon_boss`、seed=20261005、Shooter style=0、authored `final_duel`。Developer の shortcut fixture=true、normalMainRoute=false。
- 固定の自動入力。射撃・ダッシュ入力なし、初期 HP900 を設定したボス、無敵の自機 HP120/120。初期の追加敵・追加弾・upgrade は 0。自機 HP の設定注入、強制攻撃、強制撃破は行っていない。固定入力でも既存のノックバック等で実位置は変化し、最終自機位置は開始位置と異なる。
- 旧 fixture の中央追従カメラ、完全見下ろし、debugCamera=false、通常の旧 temporal profile。代表元画像のメタデータは TAA/jitter/SSAO/SSR=1、Bloom mode=2。Depth の temporal 抑制は入っていない。
- HUD を含む、debug UI を含まない、音声なし。アップスケール、補間、追加エフェクト、サウンドトラックはない。
- 実 GPU は NVIDIA GeForce RTX 4060 Laptop GPU。D3D12 debug layer=true、GPU-based validation=false。

旧ボスは実際に移動・射撃・ダッシュしている。最終 snapshot は bossShots=8、bossDashes=1、bossHp=900、bossDead=false、playerHp=120、primaryAttacks=0。これは自動入力と無敵の条件下の実ゲーム状態である。

## 最終派生ファイルと検証

既存 FFmpeg 8.1.2-full_build-www.gyan.dev を使用。フル動画と loop は libx264、CRF20、preset medium、yuv420p、音声なし、`+faststart`。MP4 box 検査で moov が mdat より前にある。ffprobe はフル 480 frames/60 fps/8 秒、loop 240 frames/60 fps/4 秒、両方 native 1280×720 を確認する。

loop は元 sequenceFrame 120–359 の連続 240 枚、poster は sequenceFrame120（simulationFrame121）の元 PNG から生成した。元 PNG は別に保存されている。初回 `media_a` の poster は RGB 入力経路で色が崩れたため、最終 `media_b` は libwebp に `-pix_fmt yuv420p -quality 86` を明示した。最終 poster と元 PNG をこの文書作成時にも並べて目視し、native 寸法、同じ HUD・ボス位置・線・主要色の対応を確認した。WebP は非可逆圧縮なので、元 PNG との pixel equality は主張しない。

`media_b/internal-tool-runs.json` に encode、ffprobe、全ファイルの `ffmpeg -v error -xerror -err_detect explode -f null NUL` decode の exitCode=0 が保存されている。decode の成功とブラウザでの再生確認を区別する。

ブラウザの `playback-full-state.json` は currentTime=duration=8、ended=true、endedCount=1、decodedFrames=480、droppedFrames=1、error=null。`playback-loop-state.json` は duration=4、playingCount=29、decodedFrames=5988、droppedFrames=44、error=null と複数回の loop 再生を記録する。loop は連続ゲーム状態の抜粋であり、開始・終了の姿勢が同じ seamless loop とは表示しない。実再生の reviewed screenshot は `media_b/playback-reviewed.png` にある。

`public-media-manifest.json` の最終 status は `ENCODE_DECODE_BROWSER_PLAYBACK_AND_VISUAL_REVIEW_VERIFIED`。`actualBrowserReview` に上記の full/loop state、元 sequenceFrame120/310 の画像確認、final poster 確認、実再生 screenshot、poster 修正理由、browserTestClosed=true が保存された。親担当が確認用の専用タブとサーバーを終了した。初回生成時の未実施 status から、この実証に基づいて更新した記録である。

| `media_b` ファイル | bytes | SHA256 |
|---|---:|---|
| neon-depth-gameplay.mp4 | 3,260,450 | `445065FCD9A557DCE1ABAA18A76D473EC713B1EFDEE645109430113A3400AB54` |
| neon-depth-loop.mp4 | 1,648,634 | `A8E8ADDCAEC63B66CFDF097872B2F911EEA425859C92155B185DAA40BD44AD11` |
| neon-depth-poster.webp | 39,886 | `CC379AF86CC7FAF49A32E6C962EE95731D20DEEDDDE7187058298A6CB69277BC` |

## 証拠を識別するハッシュ

次の paths はすべて `generated/neon-boss-depth/m0-recorder/legacy_20261005_a/` を基準とする。バイナリ・source/resource manifest のハッシュは収録 provenance、その他は文書作成時の実ファイル照合値である。

| 対象 | SHA256 |
|---|---|
| 収録 Development CG2.exe | `233C14167BB82AC639D97BC950B171B543436233499181CDAAFF4324203D752D` |
| source manifest | `93C6370D7593F5F3B8E003E09BDB48387BE88DBA0179363BA2B6B9D3DBB9FA50` |
| resource manifest | `AC562C9FB69E961D96F6C0CBEFDCCF5E43772773ED90D34D6799521DCEBAB104` |
| results.json | `239B510F5DBE525C4E159FCF42B0553AE712EDBC621ED1A33636F7860C098E59` |
| evidence/report.json | `B7038E69CB17ADDFE32EB3F563D7CA297CA0B842BB527E3624BC4707E0BFFAB3` |
| evidence/recording/recording-report.json | `DE79A55285D67A9271BED7F658A8B09739A1925E225663C42862B138F7777CB4` |
| evidence/recording/recording-manifest.json | `F1D9EAB4007C2120E14C9F5EEF006A9CA5DEE347192640390DFA8BBD2A6AFD7D` |
| evidence/recording/frames/frame_00120.png | `9EA412360F2CBE6F9163601AB12FA7476A555285E71FE2C441D084CE6FC7B7FB` |
| media_b/internal-source-manifest.json | `25A9C4110A6E99FFE9F591391546C67CBEDEFD52B65D52757E27C4BC31537EE1` |
| media_b/internal-tool-runs.json | `FA4659421E14A46A9B368562622B552A5D3E31314818AB64F1422917E9C00295` |
| media_b/public-media-manifest.json | `748DB2252721FE01E2D2BD776C25839BD7B0341B44B113AB474E8582DEB48CB7` |
| media_b/playback-full-state.json | `ADE033BF9EC5B52D8E3C9288E5F48CC288239B6883C20774294BCA1FDB479B63` |
| media_b/playback-loop-state.json | `380DFFE1963136EDB4BD408BBEBA5E8E921624925E21AAE31E61670E8AD5B8FE` |

M0 の実機録画・encode・browser 再生は親担当が実行した。本担当は保存済み証拠を読み、元ファイルの全ハッシュ再照合と poster の画像確認を行った。この文書作成のための CG2 起動、ビルド、GPU 実行、encode の再実行は行っていない。
