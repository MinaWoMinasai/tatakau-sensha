# Ink Shooter Lab 第5段階：イカの慣性と独自合成SE

2026-09-12。第4段階のレティクル・ブキ編集・ストリンガーを引き継ぎ、自インクから未塗装へ出るときの急減速を修正した。SEはユーザーの選択に合わせ、原作の音の特徴を参考にして新たに合成した。

## 操作

```powershell
.\project\tools\run_ink_shooter.ps1 -Settings
# 23秒の動作・SE検証
.\project\tools\run_ink_shooter.ps1 -Feel
# 8組のシミュレーション・データ・表示計算テスト
.\project\tools\test_ink_simulation.ps1
# 音声ファイル・ボイス寿命・旧MP3互換の独立テスト
.\project\tools\test_ink_audio_runtime.ps1
```

基本操作は従来どおり。WASDで移動、Shiftでイカ、Spaceでジャンプ、1/2/Qでブキ切替。F1の「イカの慣性」と「SE・音量」を追加した。通常のF10画像保存先は `project/generated/ink_phase5/manual.png`。

## 自インクを出たときの慣性

以前は、遊泳から露出イカへ移る瞬間と乾いた床の接地処理で速度を一気に制限していた。現在は境界でその時点の水平速度を引き継ぎ、乾いた床での目標速度へ滑らかに近づける。余韻がある間も状態は露出イカで、高速補給や潜伏判定は与えない。

既定の余韻は最大0.60秒、減速9.0距離/秒²、入力を離す・逆方向の制動は2.2倍。敵インク・人型化・壁泳ぎ・乾いた床への着地・リセットの制限を維持する。壁で消えた速度を復活させず、小さな塗りの隙間を何度渡っても速度が増殖しない。

実機CSVでは自色から出た後、5.76→5.31→4.86→3.44→2.01→0.9と減速した。約0.7距離の隙間は、速度を残して再入水できた。F1の3項目は今回の試し撃ち中の調整で、ブキファイルへは保存しない。

減速式と3つの値は **CG2の近似**。任天堂の公式説明と公開解析パラメータを調べたが、原作のこの境界での摩擦式・厳密なフレーム数を確定できなかった。調査・状態ごとの処理・7グループの検証は [移動の記録](ink_phase5_movement.md)。

## 独自合成SE

録画の音声は波形・スペクトルと動画中の発射タイミングの比較に用いた。ゲームに置く素材は数学的な波形と新規ノイズから生成しており、元録画の波形を混ぜていない。

| 音 | 合成とゲーム内の対応 |
|---|---|
| シューター発射 | 短い圧力音、液体の粒、鋭い立ち上がり。成功した1発につき1回 |
| ストリンガー短押し／1段階／最大 | 弦を弾く共鳴、下降する音程、空気・水の粒を重ねる。3矢の斉射で1回 |
| チャージ持続 | 周期的にループする合成音。実際のチャージ進捗で音程を上げ、開始・終了をフェード |
| 1段階／最大到達 | 到達時に1回だけ短い合成チャープ。満チャージ保持で連打しない |
| 冷却矢の着弾 | 矢が地形に残ったイベントに同期する短い硬質音 |
| 矢の爆発 | 実際の遅延爆発イベントに湿った破裂音。近い同時着弾・爆発はまとめて過度な重なりを抑える |

空中の遅いチャージも同じゲーム内の進捗を使う。Shift・ブキ変更・設定画面・ウィンドウのフォーカス喪失でチャージを中断する。音は射撃・塗りの乱数を消費せず、ゲームの性能に影響しない。インク不足の不発では発射音を鳴らさない。

F1から全体／発射／チャージ／着弾・爆発の音量、無効化、各単発音の試聴ができる。0にしたカテゴリの再生中の音も停止する。「音量を保存」で次回起動へ残す。録画に近い音色を目指すための初版であり、原作と同じ音や内部の音源制作手法を再現したという意味ではない。

この環境では耳で原作と比較する評価は行えていない。PCM形式・ピーク・ループ・再現性と、実XAudio2での再生開始・停止を検証した。[6.8秒のストリンガー試聴](../generated/ink_phase5/synth_audio/preview_stringer_sequence.wav)で短押し・1段階・最大の順に確認できる。これは合成音の構成をまとめた音源であり、ゲーム録音ではない。

素材は `project/resources/audio/ink/`。生成方法とseed・ハッシュを `manifest.json` に記録した。`project/tools/generate_ink_audio.py --output-dir <出力先>` でNumPyを使って再生成できる。音作りの公式資料・参考録画の観察・合成の説明は [音の調査](ink_phase5_audio_research.md)。

## 音声処理

既存のXAudio2デバイスを使い、シーン初回Updateで9音をロードする。GameがシーンInitializeより後に音を初期化する既存順序を考慮した。ボイスを事前確保して再利用し、シーン終了時は自身の音だけを解放する。

WAVはPCM16の限定読込で形式・サイズ・範囲を検査し、失敗時は既存バンクを保持する。古いボイスハンドルが再利用後の新しい音を止めないよう世代を管理する。音声デバイスや素材が利用できない場合は状態を表示し、無音でゲームを続けられる。

既存のMP3読込・PlayAudioSE互換も検証した。音源1つあたり16ボイス、全体128ボイス、限定WAV読込は16MiB以下。Ink用は計24ボイスを事前確保する。構造・寿命・負荷の範囲と独立テストは [音声ランタイム](ink_phase5_audio_runtime.md)。

## 検証結果

- シミュレーション等8組がPASS。慣性は境界・隙間・逆方向・敵色・壁・空中・リセット・60/120/240Hzを含む。音イベントは成功した発射、段階到達、取消、遅延爆発、重複読取防止、キュー上限を確認。
- 音声ランタイムの独立テストがPASS。PCM破損入力、旧MP3互換、ボイス上限、失敗置換時の保持、ループ・ピッチ・ハンドル世代・停止・解放を確認。
- Developmentの23秒実機検証で全9音をロードし、全種類の再生開始を確認。再生失敗0、終了時のチャージループ0、D3D12エラー0・警告0。

- Releaseもビルド成功。23秒の実機検証で全9音の読込・全種類の再生開始を確認し、再生失敗0、終了時のチャージループ0、D3D12エラー0・警告0。両構成ともD3D12デバッグレイヤー有効、GPU Based Validation無効。
- 日本語画面で最大チャージ音の試聴ボタン、全体音量0への変更と保存、通常音量0.55への復元を操作し、保存したJSONを検査した。慣性3項目の表示も確認。最新版は通常音量の設定画面で起動している。
- 合成WAVは同seed・同環境で再生成し全バイト一致。48kHzモノラルPCM16、クリップ0、単発音の両端0。持続音のループ境界に内部波形を超える異常な段差なし。試聴ファイルのピークは約0.286。

| 証跡 | 保存場所 |
|---|---|
| 8組のテスト | [ink_phase5_tests.log](../generated/ink_phase5_tests.log) |
| ビルド | [Development](../generated/ink_phase5_development_build.log) / [Release](../generated/ink_phase5_release_build.log) |
| 慣性と入力CSV | [Development](../generated/ink_phase5/development_ink_replay.csv) / [Release](../generated/ink_phase5/release_ink_replay.csv) |
| 音声診断 | [Development](../generated/ink_phase5/development_ink_audio_validation.txt) / [Release](../generated/ink_phase5/release_ink_audio_validation.txt) |
| GPU診断 | [Development](../generated/ink_phase5/development_ink_gpu_validation.txt) / [Release](../generated/ink_phase5/release_ink_gpu_validation.txt) |
| 日本語設定 | [音](../project/generated/ink_phase5/07_japanese_audio_editor.jpg) / [慣性](../project/generated/ink_phase5/08_japanese_carry_editor.jpg) |
| 合成素材の記録 | [manifest.json](../project/resources/audio/ink/manifest.json) |

`generated` 以下はローカルの検証成果物で、ソース管理の追跡対象とは限らない。
