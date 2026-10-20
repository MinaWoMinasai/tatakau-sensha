# flecs

- 取得元: https://github.com/SanderMertens/flecs
- 版: v4.1.6（タグのコミット fb55f3c25660425cfe1bc4cf5e6bff8b3f18a9b8）
- ファイル: リポジトリの `distr/flecs.c` と `distr/flecs.h`、`LICENSE`（MIT）。中身は変更していない。
- SHA-256:
  - flecs.c `6005392eb13c0f3c7abdecb2f85271e50934f63919afebf0bbe85f6dfc7320d6`
  - flecs.h `526036a5a41678e2a43a3cb835e9eaa70fd1993868b1978950c0d275752f69b1`
  - LICENSE `5f89f3edd5d1e458cd48509818e23b57d85869f7d57d0ebe31bcfd3a73e5dd60`
- 使う機能（カスタムビルド）の定義は `project/MyEngine.Common.props` にまとめている。flecs.h の中身が変わるため、エンジン・ゲーム・flecs.c のすべてで同じ定義にする。
- 更新するときは、上の3ファイルを新しいタグのものに置き換え、この README の版とハッシュを書き換える。
