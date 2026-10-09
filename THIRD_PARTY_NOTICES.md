# Third-party software and assets

この文書は、同梱・使用する第三者製コードと素材の案内です。各原文ライセンスが優先します。作者の All Rights Reserved 表示は第三者の著作物には適用しません。

調査日：2026-09-29。既存のソースヘッダー、LICENSE、素材の表示を保持し、不足していたライセンス本文を `licenses/third-party/` に補完しました。**素材の出典と一部の依存物の確認は未完了です。全収録物の公開・再配布を保証する一覧ではありません。**

## ライブラリとフォント

| 対象 / 同梱場所 | 権利者・表示 | ライセンス / 対応 |
| --- | --- | --- |
| Dear ImGui / `project/externals/imgui/` | Copyright (c) 2014-2026 Omar Cornut | MIT。[既存原文](project/externals/imgui/LICENSE.txt) を保持。著作権・許諾表示をコピーに含める |
| ImGui 内 stb / `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h` | Copyright (c) 2017 Sean Barrett | 各ヘッダー末尾に MIT / Public Domain の選択条項。共通の [原文抜粋](licenses/third-party/stb-LICENSE.txt) を追加。既存表示と変更履歴も保持 |
| DirectXTex / `project/externals/DirectXTex/` | Copyright (c) Microsoft Corporation | MIT。ヘッダー内の表示を保持し、[公式 LICENSE のコピー](licenses/third-party/DirectXTex-LICENSE.txt) を追加。ソースの `DIRECTX_TEX_VERSION` は 198。上流コミットは未特定 |
| DirectX 12 helper `d3dx12.h` / 同上 | Copyright (c) Microsoft Corporation | ヘッダーで MIT と明記。既存ヘッダー表示を保持 |
| JSON for Modern C++ / `project/externals/nlohmann/` | Copyright (c) 2013-2026 Niels Lohmann | ヘッダーは 3.12.0 / MIT。[公式 LICENSE のコピー](licenses/third-party/nlohmann-json-LICENSE.MIT) を追加 |
| Assimp / `project/externals/assimp/` | assimp team。既存 LICENSE は 2006-2021、各ヘッダーには別年の表示もある | BSD 3-Clause。[既存 LICENSE](project/externals/assimp/LICENSE.txt) を改変せず保持。ソース表示・バイナリ配布時の文書への表示・推奨への名称使用制限を確認。既存ファイル内の Poly2Tri 表示も保持 |
| RapidJSON / ローカル生成 Assimp の依存 | THL A29 Limited / Milo Yip ほか | MIT および原文に記された第三者条件。[同じローカル Assimp ソースにある本文](licenses/third-party/RapidJSON-LICENSE.txt) を追加。本文中の他コンポーネントの記載だけで、それら全てが本ゲームへリンクされると断定しない |
| zlib / ローカル生成 Assimp の依存 | Jean-loup Gailly / Mark Adler | zlib license。[同じローカル Assimp ソースにある本文](licenses/third-party/zlib-LICENSE.txt) を追加。ビルドスクリプトで `ASSIMP_BUILD_ZLIB=ON` |
| Zen Maru Gothic Bold / `project/resources/fonts/` | Copyright 2021 The Zen Maru Gothic Project Authors | SIL Open Font License 1.1。[既存 OFL 原文](project/resources/fonts/ZenMaruGothic-OFL.txt) とフォントを保持。フォントの配布時は著作権・ライセンス表示を同梱。名称等の制限は原文に従う |

## nlohmann/json 内の追加の表示

ライブラリ名だけで内部の権利者表示を置き換えないでください。ソース内で以下を確認しました。

- `detail/conversions/to_chars.hpp`：Copyright (c) 2009 Florian Loitsch、MIT の記載。
- `detail/output/serializer.hpp`：Copyright (c) 2008-2009 Bjoern Hoehrmann。UTF-8 decoder の由来・変更についてコメントあり。
- `thirdparty/hedley/hedley.hpp`：Evan Nemerson、2016-2021 の表示。上位ヘッダーの MIT 表示に加え、元 Hedley の CC0-1.0 表示あり。[Creative Commons の公式 CC0 本文](licenses/third-party/Hedley-CC0-1.0.txt) を補完し、既存ヘッダーも保持。
- `detail/meta/cpp_future.hpp`：2018 The Abseil Authors。C++11 向け分岐に Apache-2.0 由来のコードとの明記があるため、参照先コミットの [Apache 原文](licenses/third-party/Abseil-LICENSE.txt) も追加。現在のゲームビルドは C++20 ですが、ソースとして収録されている表示も保持。

## Windows と開発用依存

Windows SDK の DirectX、DirectInput、XInput、XAudio2、DirectWrite、Media Foundation 等を使用します。Windows の Meiryo はシステムフォントとして参照し、フォントファイル自体は収録していません。

`dxcompiler.dll` と `dxil.dll` はビルド時に Windows SDK からコピーされます。実際に配布する SDK / DXC のバージョンに対応する再配布条件と必要な告知は、配布前に確認してください。参照先：[DirectXShaderCompiler の原文](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT)。このリンクだけで配布条件の確認済みとは扱いません。

音声生成など一部の補助ツールは Python / NumPy、Blender 関連ツールは Blender の環境を利用します。これらの実行環境をこのリポジトリに同梱しているわけではありません。

Assimp の `include/assimp/fast_atof.h` には Nikolaus Gebhardt、Irrlicht / irrXML 由来の表示がありますが、参照される `irrlicht.h` / `irrXML.h` の条件本文は vendor 内に見つかりませんでした。Assimp の全依存を既存の BSD 表示だけで一括して扱わず、使用版の原文確認を残しています。

## VRoid Project: AvatarSample_B

`project/resources/models/neon_hologram/AvatarSample_B.glb`はVRoid Projectのサンプルモデルです。[公式のAvatarSample A〜Z利用条件](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z)に従い扱います。CC0ではなく、Repositoryのコードライセンスの対象でもありません。作者提供のVRMを2026-10-01にバイト列を変えずGLB名へコピーしました。取得経路、SHA-256、用途、利用条件の記録は[モデルREADME](project/resources/models/neon_hologram/README.md)を参照してください。現在はDeveloper Preview専用で、Release提出パッケージからこのGLBだけを除外します。

## 素材の出典・公開条件を確認する必要があるもの

| パス | 調査結果 / 確認事項 |
| --- | --- |
| `project/resources/bulletShoot.mp3` | 共通起動処理で使用。入手元・素材再配布条件の確認または差し替えが必要 |
| その他の既存画像・モデル・音声 | ファイル名や同梱だけでは自作と断定できない。作者の制作記録・入手元・利用許諾との照合が必要 |

`project/resources/audio/tank_expedition/` は [同梱説明](project/resources/audio/tank_expedition/README.md)、`generate_audio.py`、`measurements.json` に、このプロジェクト向けの波形合成で外部録音・サンプルを使わない旨の記録があります。これは当該音声の根拠であり、`resources/` 全体の出典を保証するものではありません。

## 配布時の扱い

ローカルの配布ツールは `COPYRIGHT.md`、本書、既存 Assimp / ImGui の LICENSE、`licenses/third-party/` の補完本文を同梱します。フォントの OFL は `resources/fonts/` 内に保持します。ソース用の相対リンクは公式リポジトリで参照し、配布フォルダーでは同梱の `licenses/` と `licenses/third-party/` の本文を確認してください。

ライセンス本文の同梱は、出典未確認の素材の確認完了を意味しません。未確認項目は上記「素材の出典・公開条件を確認する必要があるもの」を参照してください。

旧専用シーンと専用素材の整理記録はローカル資料として保管しています。共通起動時にロードする`bulletShoot.mp3`は保持し、出典・素材単体の再配布条件は未確認です。試聴専用`preview.wav`はソースリポジトリに保持し、実行用配布から除外します。
