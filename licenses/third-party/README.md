# 補完したライセンス本文の出典

2026-09-29 に不足分を補完しました。既存の vendor ソース、著作権表示、LICENSE は変更していません。これらは対象の第三者製品の条件であり、ゲーム全体へのライセンス付与ではありません。

| ファイル | 取得元 / 確認根拠 |
| --- | --- |
| `DirectXTex-LICENSE.txt` | [microsoft/DirectXTex の公式 LICENSE](https://raw.githubusercontent.com/microsoft/DirectXTex/main/LICENSE)。vendor 内の Microsoft / MIT 表示と照合 |
| `nlohmann-json-LICENSE.MIT` | [nlohmann/json の公式 LICENSE.MIT](https://raw.githubusercontent.com/nlohmann/json/develop/LICENSE.MIT)。vendor の 2013-2026 表示と照合 |
| `Abseil-LICENSE.txt` | [ソース内で参照される Abseil コミット](https://raw.githubusercontent.com/abseil/abseil-cpp/10cb35e459f5ecca5b2ff107635da0bfa41011b4/LICENSE) |
| `Hedley-CC0-1.0.txt` | [Creative Commons 公式 CC0 1.0 原文](https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt)。`nlohmann/thirdparty/hedley/hedley.hpp` の CC0-1.0 表示に対応し、無改変コピーを同梱 |
| `stb-LICENSE.txt` | `project/externals/imgui/imstb_*.h` 3ファイル末尾の同一ライセンス本文を、そのまま抜粋 |
| `RapidJSON-LICENSE.txt` | ローカル Assimp ソース `project/.deps/assimp/source/contrib/rapidjson/license.txt` から無改変コピー |
| `flecs-LICENSE.txt` | 2026-10-20 追加。[flecs v4.1.6 の LICENSE](https://raw.githubusercontent.com/SanderMertens/flecs/fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8/LICENSE) を無改変コピー（`project/externals/flecs/LICENSE` と同一） |
| `zlib-LICENSE.txt` | ローカル Assimp ソース `project/.deps/assimp/source/contrib/zlib/LICENSE` から無改変コピー |

ライブラリ本体を更新するときは、この補完本文と既存表示も使用版に照らして確認してください。本文のない依存・出典未確認の素材については [一覧と保留事項](../../THIRD_PARTY_NOTICES.md) を参照してください。
