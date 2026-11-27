# Third-party software and assets

このプロジェクトで使っている第三者製のコードと素材の一覧です。ライセンスの解釈は各原文が優先します。作者の All Rights Reserved 表示は、第三者の著作物には適用しません。

各ライブラリのソースヘッダー、LICENSE、素材の表示はそのまま残しています。リポジトリに本文が無かったライセンスは `licenses/third-party/` に入れています。

## ライブラリとフォント

| ライブラリ / 置き場所 | 版 | 権利者・表示 | ライセンスと原文 |
| --- | --- | --- | --- |
| Dear ImGui / `project/externals/imgui/` | ― | Copyright (c) 2014-2026 Omar Cornut | MIT。[原文](project/externals/imgui/LICENSE.txt)。コピーには著作権・許諾表示を含める |
| ImGui 内 stb / `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h` | ― | Copyright (c) 2017 Sean Barrett | 各ヘッダー末尾の MIT / Public Domain の選択条項。[原文の抜粋](licenses/third-party/stb-LICENSE.txt)。ヘッダーの表示と変更履歴はそのまま |
| DirectXTex / `project/externals/DirectXTex/` | `DIRECTX_TEX_VERSION` 198 | Copyright (c) Microsoft Corporation | MIT。[公式 LICENSE のコピー](licenses/third-party/DirectXTex-LICENSE.txt)。ヘッダーの表示はそのまま |
| DirectX 12 helper `d3dx12.h` / 同上 | ― | Copyright (c) Microsoft Corporation | MIT（ヘッダーに記載）。ヘッダーの表示はそのまま |
| JSON for Modern C++ / `project/externals/nlohmann/` | 3.12.0 | Copyright (c) 2013-2026 Niels Lohmann | MIT。[公式 LICENSE のコピー](licenses/third-party/nlohmann-json-LICENSE.MIT) |
| Assimp / `project/externals/assimp/` | v5.4.3 | assimp team（LICENSE は 2006-2021。ヘッダーによって別の年の表示もある） | BSD 3-Clause。[LICENSE](project/externals/assimp/LICENSE.txt) を無改変で同梱。ソースの表示、バイナリ配布時の文書への表示、宣伝への名称使用の制限に従う。ファイル内の Poly2Tri の表示もそのまま。VS 2022 でビルドした `lib/assimp-vc143-mt.lib`・`runtime/assimp-vc143-mt.dll`（glTF importer のみ、`ASSIMP_BUILD_ZLIB=ON`）を同梱 |
| RapidJSON / ローカルでビルドする Assimp の依存 | ― | THL A29 Limited / Milo Yip ほか | MIT と、原文に書かれた第三者条件。[Assimp のソースにある本文](licenses/third-party/RapidJSON-LICENSE.txt) |
| zlib / ローカルでビルドする Assimp の依存 | ― | Jean-loup Gailly / Mark Adler | zlib license。[Assimp のソースにある本文](licenses/third-party/zlib-LICENSE.txt)。ビルドスクリプトで `ASSIMP_BUILD_ZLIB=ON` を指定 |
| flecs / `project/externals/flecs/` | v4.1.6 | Copyright (c) 2025 Sander Mertens。Portions Copyright (c) Meta Platforms, Inc. and affiliates | MIT。`distr/flecs.c`・`distr/flecs.h` を無改変で収録。[LICENSE](project/externals/flecs/LICENSE) と [コピー](licenses/third-party/flecs-LICENSE.txt)。版とハッシュは [README](project/externals/flecs/README.md)。`flecs.c` 内の wyhash（Wang Yi ほか、The Unlicense と記載）と stm32tpl（Copyright (c) 2011-2020 Anton B. Gusev）由来部分の表示もそのまま |
| ImGuizmo / `project/externals/imguizmo/` | 1.10 | Copyright (c) 2016 Cedric Guillemet | MIT。`src/ImGuizmo.h`・`src/ImGuizmo.cpp` を無改変で収録。[LICENSE](project/externals/imguizmo/LICENSE) と [コピー](licenses/third-party/ImGuizmo-LICENSE.txt)。版とハッシュは [README](project/externals/imguizmo/README.md)。エディタ用で、Release ではコンパイルしない |
| Zen Maru Gothic Bold / `project/resources/fonts/` | ― | Copyright 2021 The Zen Maru Gothic Project Authors | SIL Open Font License 1.1。フォントと [OFL 原文](project/resources/fonts/ZenMaruGothic-OFL.txt) を同梱。配布時は著作権・ライセンス表示も一緒に配る。名称などの制限は原文に従う |

## nlohmann/json 内の追加の表示

nlohmann/json の中には、ライブラリ本体とは別の権利者の表示があります。ライブラリ名だけでこれらを置き換えないでください。

- `detail/conversions/to_chars.hpp`：Copyright (c) 2009 Florian Loitsch。MIT。
- `detail/output/serializer.hpp`：Copyright (c) 2008-2009 Bjoern Hoehrmann。UTF-8 decoder の由来と変更についてのコメントがある。
- `thirdparty/hedley/hedley.hpp`：Evan Nemerson、2016-2021。上位ヘッダーの MIT 表示に加えて、元の Hedley の CC0-1.0 表示がある。[Creative Commons の公式 CC0 本文](licenses/third-party/Hedley-CC0-1.0.txt) を同梱。
- `detail/meta/cpp_future.hpp`：2018 The Abseil Authors。C++11 向けの分岐に Apache-2.0 由来のコードがある。[Apache 原文](licenses/third-party/Abseil-LICENSE.txt) を同梱。このゲームは C++20 でビルドするのでこの分岐は通らないが、ソースには含まれている。

## Windows と開発用の依存

Windows SDK の DirectX、DirectInput、XInput、XAudio2、DirectWrite、Media Foundation などを使っています。Windows の Meiryo はシステムフォントとして参照するだけで、フォントファイルは収録していません。

`dxcompiler.dll` と `dxil.dll` はビルド時に Windows SDK からコピーします（[DirectXShaderCompiler の原文](https://github.com/microsoft/DirectXShaderCompiler/blob/main/LICENSE.TXT)）。

音声生成などの補助ツールは Python / NumPy を、Blender 関連のツールは Blender を使います。これらの実行環境は同梱していません。

Assimp の `include/assimp/fast_atof.h` には、Nikolaus Gebhardt、Irrlicht / irrXML 由来の表示があります。表示はそのまま残しています。

## VRoid Project: AvatarSample_B

`project/resources/models/neon_hologram/AvatarSample_B.glb` は VRoid Project のサンプルモデルで、[公式の AvatarSample A〜Z 利用条件](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z)に従って使っています。CC0 ではなく、このリポジトリのコードのライセンスの対象でもありません。

提供元の VRM を、2026-10-01 にバイト列を変えずに GLB の名前へコピーしたものです。SHA-256、用途、利用条件は[モデルの README](project/resources/models/neon_hologram/README.md) に書いています。Developer Preview 専用で、Release の提出パッケージからはこの GLB を除外します。

## 音声

`project/resources/audio/tank_expedition/` の音声は、このプロジェクト向けに波形合成したもので、外部の録音やサンプルは使っていません（[説明](project/resources/audio/tank_expedition/README.md)、`generate_audio.py`、`measurements.json`）。

## 配布時の扱い

ローカルの配布ツールは、`COPYRIGHT.md`、この文書、Assimp / ImGui の LICENSE、`licenses/third-party/` の本文を同梱します。フォントの OFL は `resources/fonts/` に置いたままです。この文書の相対リンクはソースリポジトリ用なので、配布フォルダーでは同梱の `licenses/` と `licenses/third-party/` を見てください。

試聴専用の `preview.wav` はソースリポジトリには置き、実行用の配布からは除外します。

## 配布する前に

| 対象 | すること |
| --- | --- |
| `dxcompiler.dll`・`dxil.dll` | 配布する SDK / DXC の版ごとに、再配布の条件と必要な告知を原文で確かめる |
| flecs 内の stm32tpl 由来部分 | 条件の本文を確かめる |
| Assimp の `fast_atof.h` が参照する `irrlicht.h` / `irrXML.h` | 使っている版の原文で条件を確かめる |
| DirectXTex | 上流のコミットを記録する |
| `project/resources/bulletShoot.mp3` | 共通の起動処理で読み込む。入手元と素材単体の再配布条件を確かめるか、差し替える |
| その他の既存の画像・モデル・音声 | 制作記録・入手元・利用許諾と照らし合わせる |
