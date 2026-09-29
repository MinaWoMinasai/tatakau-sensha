# 作者・作品情報

| 項目 | 表示内容 |
| --- | --- |
| ProductName | たたかうせんしゃ |
| Author / Company | MinaWoMinasai |
| LegalCopyright | Copyright © 2025-2026 MinaWoMinasai. All Rights Reserved. |
| Official Repository | [MinaWoMinasai/tatakau-sensha](https://github.com/MinaWoMinasai/tatakau-sensha) |

第三者製コード・素材の作者表示は [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) を参照してください。

## 現在の実装と追加案

- `WinApp.cpp` のウィンドウタイトルと `TitleScene.cpp` のタイトル表示は既に「たたかうせんしゃ」です。
- Windows の `VERSIONINFO` 用 `.rc` / `ResourceCompile` と、専用の Credits / About 画面は見つかりませんでした。今回それらのシステムは追加していません。
- `TitleScene` には既存の `TextLabel` があるため、将来は画面下部に作者・著作権・公式 URL を表示できます。文字の可読性、既存ヒントとの重なり、初回文字生成時間を実機で確認して追加する案です。
- Windows Version Info は、小さな `.rc` を追加して `ProductName` / `CompanyName` / `LegalCopyright` / `Comments` に上記を設定する方法が考えられます。`Comments` に公式 URL を含め、バージョン番号は実際のリリース方針を決めてから設定します。

表示だけで複製を防止したり、電子署名と同じ真正性を保証したりするものではありません。公式リポジトリと作者表示を対応付けるための案です。ゲームの難読化・DRM は追加していません。
