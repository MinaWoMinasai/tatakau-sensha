# たたかうせんしゃ：Phase 1.5 グラフィックス改修

2026-10-14。直線の連続壁、外側の構造物、内側のZ方向の段積み、自機に追従する床の局所光を実装した。通常カメラ・XY衝突・弾道・AI・HUDの基準は維持した。参考作品と同等の最終品質に達したとは扱わない。機体の本格的な3D化、新規ライブラリ追加、Gitのcommit／push／mergeは行っていない。

## 調査結果と変更の理由

[Phase 1の報告](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/PHASE1_GRAPHICS_REPORT.md)と関連実装を確認した。XYゲーム平面はZ=0、通常カメラは負のZ側から+Zを向く。通常の投影はFOV=.45、カメラ距離約55。ground.objは初期化されるが、本編で描く床はPhase 1の専用XYメッシュだった。

Phase 1のベベル幅.14と、セルごとに独立した発光枠が丸いタイル感を作っていた。暗い床には自機からの照明がなく、正面カメラでは側面も小さい。既存PBRの法線ディテールを大きな床へ試すと広い縞が出た。SSRをOFFにしても縞は残り、法線ディテール0で消えることを確認したため標準値を0にした。Bloomの強度は今回の改善手段として増やしていない。

## 実装した描画

- **連続壁**：同種の有効なマップセルを矩形へまとめ、XYの隙間をなくす。暗いPBR本体と細い直線格子を別経路で描く。ベベルを廃止した共通直方体は24頂点・12三角形。普通／ダメージ壁の種類を混ぜない。
- **内側の段積み**：標準3段、全高4.8。各段の共通メッシュにはごく小さいZ方向の継ぎ目を設け、側面の水平線・縦線と面の陰影で積層を見せる。孤立した1セルは1段の低いブロックとして区別する。XYの占有形状は同じ。
- **外側の構造物**：有効ブロックの実AABB範囲から間隔2、幅2、高さ8の外周4本を生成する。赤い格子と暗い本体で内側と区別する。これは描画専用の装飾で、衝突ブロックの追加やマップCSVの変更は行わない。
- **更新と再利用**：有効状態・種類・配置・外周設定のハッシュが変わった場合に壁の矩形を更新する。破壊・無効化で露出した端面も更新する。メッシュは段数別に共有し、Object3dもプールして再利用する。既存のレベル障害物は個別の描画姿勢を保持する。
- **床**：Phase 1の不透明PBR床を継続利用する。マップ、実障害物、外周装飾、カメラ可視範囲と余白を覆う。床の粗さは.52、金属度.20。自機XYへ追従する1個のPBR点光源を床の描画物だけに設定する。色、強さ24、半径11、高さ3を調整可能にした。高さはXY平面からカメラ側への距離で、床からの距離はfloorDepthを加えた値になる。死亡時は局所光の強さを0にする。
- **発光と深度**：不透明本体はemissive=0。格子線は角丸キャップを使わず、弱いハローと細いコアにする。同じ端点の線は重複を除く。床グリッドは床よりカメラ側へ.025離して重ねる。

衝突用Block／AABB／OBB／mergedBlocksは描画用SurfaceWallと別で、Stageの衝突・弾反射処理を変更していない。外周装飾へ新しい衝突は発生しない。標準の連続結合対象は軸平行のマップセルで、追加のレベル障害物同士は結合しない。

## 描画パスとカメラ

既存Object3dのCook–Torrance PBR、Scene HDR色・法線・材質MRT、DSV、skybox.ddsを流用した。シェーダーや外部モデルを新設していない。

1. Scene MRT／深度へ不透明床・壁本体を描く。
2. 既存グリッド・壁格子・機体・弾・透明VFXを、それぞれの既存深度／発光経路へ送る。壁本体は共有Bloomキャプチャで重複して描かない。
3. 既存SSAO／SSR／HDR／Bloom合成を通す。
4. 既存HUD・テキスト・開発UIを描く。

通常のカメラ追従・投影は維持した。Developmentだけに5〜28度、標準18度の比較視点を追加した。毎フレーム元の追従姿勢へ戻してから比較角度を適用し、カメラ位置の累積ずれを防ぐ。照準は既存の表示VPとXY平面との交点計算へ切り替え、カリングは比較前の床中心を使う。NeonDepthの特殊カメラとデバッグカメラを優先する。比較角度は保存せず、Releaseへはコンパイルしない。

## 比較結果

同じseed=1337、crossfire、敵8体、同じ移動・射撃入力の180フレームを比較した。旧表示／Phase 1／Phase 1.5のカメラ行列とゲーム状態スナップショットが一致した。描画リソース数のみ比較から除外した。

![左：旧表示、中央：Phase 1、右：Phase 1.5](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/comparison.png)

個々の白く膨らんだ枠をなくし、1本の連続した壁面へ変わった。左に赤い外周構造物、内側に暗い青の連続壁を確認できる。正面でも壁の端に側面の段線が見えるが、積層の読み取りは傾斜視点の方が明確。

![通常視点と18度の比較視点](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/camera_comparison.png)

![両方Bloom OFF：局所光OFF／ON](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/floor_light_comparison.png)

BloomをOFFにしても床の光が残る。自機から30〜100pxの範囲で局所光ONによる平均RGB増分は約(25,82,72)、350pxより遠方では(0,0,0)。単なる画面全体の明度変更ではない。色を赤へ変えた場合、半径を11から5へ変えた場合も床の反応が変わった。

![局所光の色・半径比較](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/floor_parameters.png)

[3画面の比較動画](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/motion_comparison.mp4)は左が旧表示、中央がPhase 1、右がPhase 1.5。[Phase 1.5の単独動画](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/phase15_play.mp4)も用意した。各版は実際に更新した連続180フレームを60fpsで符号化した3秒の動画で、静止画の引き延ばしや補間は使っていない。各フレームの更新時計・heldDrawCount=0・D3D12警告／エラー0を確認した。移動中も床の光が追従し、壁面が連続して表示される。静止比較は再現用の停止キャプチャを使用し、録画では停止していない。

## 調整・復帰方法

Developmentの**F3 → 床・立体ブロック**で調整する。変更後は「適用」、F3で閉じて試遊、保存する場合は「保存」を使う。

- 「壁の形状」でPhase 1のベベルタイルとPhase 1.5の連続壁を切り替える。形状比較なので、他の材質値は現在の値を保持する。
- 「従来表示へ戻す」→適用で、不透明床・新ブロックを使わない旧経路へ戻せる。
- 外周の高さ／幅／間隔、内側の積層数、発光線幅、ブロックの高さ／本体色／発光強度を調整できる。
- 床の局所光の色／強さ／半径／高さ、床の色／金属度／粗さを調整できる。法線ディテールは実験項目で標準0。
- 「開発用／軽く傾けた視点」は比較専用。OFFで通常視点へ戻る。
- F12 → 見た目 → Phase 1.5では、Phase 1の材質・高さまで戻す比較プリセットと、Bloom／SSAO／SSR／G-buffer診断を使用できる。

設定形式はversion=3。新項目のないversion=2では従来のPhase 1形状、外周OFF、局所光OFFで読み込み、既存のenabledスイッチを尊重する。F3の積層数3→4の変更・保存・再読込を実操作し、保存ファイルに反映されたことを確認した。旧配布設定の再読込で関係のないネオン値まで起動時と違う値に戻る不整合も修正し、起動時と同じexpeditionAuthored移行規則に揃えた。修正後の再読込→保存で、積層数以外の見た目データが初期の実行設定と完全一致した。検証用変更は戻し、現在の設定は標準3段。

## ビルド・動作検証

Development x64／Release x64とも最終ビルド成功、警告0・エラー0。既存環境のPDB形式に関するC1902／LNK1318を避けるため、検証用targetsで/Z7を使用し、Releaseのリンク用デバッグ情報をOFFにした。通常のプロジェクト設定は変更していない。ビルドログとtargetsはローカルの比較フォルダーへ保存した。

| 項目 | 実際に行った検証 |
| --- | --- |
| 通常戦闘・表示切替 | 各180フレーム。旧版／Phase 1／Phase 1.5の状態・通常カメラ一致 |
| 傾斜比較 | 180フレーム。通常版とゲーム状態一致。693点の表示→照準往復テスト（0／18／28度、クライアント1280／1600／1920幅） |
| 実マウス・UI | 1600×900のゲーム画面でF3形状切替、適用、保存、再読込。18度視点で射撃・命中後のHP表示、通常視点への復帰 |
| 壁と移動 | 壁へ向かう移動・射撃180フレーム、従来の衝突コード維持。描画結合の占有・破壊後の穴を800ケースで検査 |
| メッシュ | 鋭い箱＋段数1〜6の7メッシュで閉じた面、法線、巻き順、XY外形を検査。Phase 1の既存ベベルテストも成功 |
| ボス・特殊カメラ | rival_boss 360フレーム。NeonDepth 1800フレームを比較視点ON／OFFで実行し、90／1800時点の特殊カメラ行列とゲーム状態一致 |
| 進行 | expedition_transition 1200フレーム。既存の部屋完了→サービス購入→次の実戦闘まで成功 |
| 大量表示 | 敵128体＋初期弾480発、360フレーム成功 |
| 描画整合性 | 各取得フレームでD3D12警告／エラー0。GPU-based validation ONの通常戦闘も180フレーム成功 |
| ポスト診断 | Bloom OFF／ON、SSAO・SSR OFF、法線・深度・粗さ・SSAO・SSR・SSR maskを取得・確認 |
| Release実行 | 本編プロジェクトで起動→初期戦闘→マウス射撃・命中後のHP表示、床・外周壁・HUDを確認。長時間の全ルート試遊はしていない |

録画・診断・設定・性能集計は[検証データ](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase15_surfaces/verification_summary.json)へ保存した。単体テストは[壁・照準テスト](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/tools/stage_surface_wall_tests.cpp)。比較資材とローカルテストは既存の.gitignore規則に従ってコミット対象外。

## 性能

RTX 4060 Laptop GPU、1280×720、同じcrossfire戦闘、ウォームアップ30フレーム後の120フレーム。CPUは120サンプル、GPUは有効なtimestampの115／118／117サンプルの中央値。PNG保存の時間は描画時間へ含めない。これは単一場面のDevelopment測定で、全環境の性能保証ではない。

| 指標 | 旧表示 | Phase 1 | Phase 1.5 |
| --- | ---: | ---: | ---: |
| GPUフレーム | 4.829 ms | 3.648 ms | 3.921 ms |
| CPU描画記録全体 | 3.946 ms | 4.138 ms | 2.711 ms |
| 床・壁のCPU記録 | — | 1.306 ms | .260 ms |
| 床・壁のGPU | — | 1.162 ms | 1.408 ms |
| 新しい壁本体のDraw数 | — | 70 | 9 |
| 床＋壁本体の三角形数 | — | 3082 | 362 |

壁の結合でCPU記録と本体Draw数は減った。一方、Phase 1に比べて局所光を含む床・壁GPU時間は約.246ms、GPU全体は約.273ms増えた。描画回数は新しい床・壁本体のカウンターであり、全エンジンのDrawCall総数ではない。共通メッシュと描画物プールにより使い回すが、旧比較用のセル別描画物も残しているためメモリが減ったとは主張しない。継続的なVRAM／長時間ヒープ計測は未実施。

## 残る品質差・未検証

1. **床への落ち影がない**。Stageの床・壁をシャドウマップへ描くパスは今回追加していない。局所光にも壁の遮蔽がないので、光が壁越しに回る。次には安定した壁の接地影と、局所光の遮蔽が必要。
2. **SSRは弱い**。現在の正面カメラと粗い床ではSSR診断がほぼ黒で、床の鮮明な鏡面反射は得られていない。法線・粗さ・深度は参加しており、SSAOでは端の接触陰影を確認できるが控えめ。反射の主要な改善には、発光物の反射対象への参加、SSRのレイ／ヒット条件・品質の調査、必要なら平面反射などが次の課題。今回の局所光を反射そのものとは扱わない。
3. **通常視点では側面が限られる**。厚みと直線格子は改善したが、参考映像のようにすべての側面が強く見えるわけではない。比較角度は開発用のまま。正式採用には全ルート、全戦闘役割、全特殊カメラで視認性と操作性を検証する必要がある。
4. **光源は自機1個、床のみ**。敵・弾・爆発に対応した複数光源、壁面への局所照明、床の細かな材質テクスチャは次の拡張。機体は既存の2Dネオン表示のまま。
5. **結合は矩形単位**。L字形などは複数矩形になる。巨大な任意形状の単一メッシュ化、追加レベル障害物間の結合、すべての複雑な破壊パターンの実プレイは未検証。破壊後の占有整合は単体テストで確認したが、全破壊パターンの実画面確認とは区別する。
6. **外周は装飾**。プレイエリアの元の壁とは別で衝突しない。全ての編集済み／将来の開放マップで外周装飾の位置が適切かは未検証。長時間試遊、全解像度／全GPU／全デバッグシーンを網羅した検証も未実施。

## 変更ファイル

| ファイル | 主な変更 |
| --- | --- |
| [Object3d.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/3d/Object3d.h) | 描画物単位の点光源設定 |
| [StageSurfaceSettings.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/settings/StageSurfaceSettings.h) | 壁・外周・積層・局所光設定、検証、Phase 1プリセット |
| [StageSurfaceGeometry.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/StageSurfaceGeometry.h) | 共通の鋭い箱・積層メッシュ、占有セルの矩形結合 |
| [Stage.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/player/actor/Stage.h)、[Stage.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/player/actor/Stage.cpp) | 描画壁キャッシュ・プール・外周、床の点光源 |
| [GameplayRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/session/GameplayRenderer.cpp) | 床光源の位置・死亡時OFF、実三角形数の計測 |
| [StageNeonRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/session/StageNeonRenderer.cpp) | 直線壁の細い格子、面の可視判定、重複線の除去 |
| [ResourceNeonRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/session/ResourceNeonRenderer.cpp) | 比較カメラの床中心でカリング |
| [CombatFramePipeline.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/session/CombatFramePipeline.cpp)、[NeonPresentationState.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/session/state/NeonPresentationState.h) | 開発用傾斜カメラと既存XY照準の選択 |
| [GameplaySettings.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/editor/session/GameplaySettings.cpp) | version 3保存・旧設定互換 |
| [ExpeditionAuthoring.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/editor/session/ExpeditionAuthoring.cpp)、[GameplayEditor.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/editor/session/GameplayEditor.cpp) | F3／F12の比較・調整UI、再読込移行の修正 |
| [RunVisualSettings.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/settings/RunVisualSettings.cpp) | 開発用の再現比較パラメータ |
| [gameVisuals.json](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/resources/configs/gameVisuals.json) | 新表示の標準候補値 |
| この報告書 | 比較、検証、性能と残る課題 |
