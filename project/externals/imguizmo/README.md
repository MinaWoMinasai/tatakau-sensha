# ImGuizmo

- 取得元: https://github.com/CedricGuillemet/ImGuizmo
- 版: 1.10（タグのコミット b796ac3b861afc6e91ca74e4611effd9c9527367）
- ファイル: リポジトリの `src/ImGuizmo.h` と `src/ImGuizmo.cpp`、`LICENSE`（MIT）。中身は変更していない。シーケンサーやグラフエディタなど、ほかのファイルは入れていない。
- SHA-256（取得したときのファイル。チェックアウト時の改行の変換で変わることがある）:
  - ImGuizmo.cpp `ad7ffeeacc9753e804d3b77f427c6629983ed19dcc9e15f77799568199551ba0`
  - ImGuizmo.h `11378812f1e73ef0fa9a62ff276331a84fe2ffad6fd24d264900d1f1a100fd29`
  - LICENSE `b957968be18bbc4fde7705cfcbda10bea1a71614e62361ace0473bb97dd0b700`
- エディタのギズモにだけ使うので、Release ではコンパイルしない。`ImGuizmo.cpp` と `ImGuizmo.h` は `imgui.h`・`imconfig.h` を名前だけでインクルードするため、使う側では `externals/imgui` をインクルードの検索先に入れる。
- 更新するときは、上の3ファイルを新しいタグのものに置き換え、この README の版とハッシュを書き換える。
