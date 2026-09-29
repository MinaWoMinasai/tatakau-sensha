# 第4段階：参考動画のレティクルとCG2の描画設計

調査日：2026-09-12。対象はユーザー提供の `ローカル参考資料（非同梱）` と、既存のスプラシューター参考実装。トライストリンガーにも使える表示入力を用意するが、レティクル描画側に武器の弾道・ダメージ・チャージ時間を実装しない。

## 提供動画から直接確認した形

ffprobeで確認した動画は **1038×860、60fps、6.283333秒**。前段階の30fps参考動画とは別のファイルである。FFmpegで10fpsの観察用PNGと4fpsの一覧を出力した。抽出画像は [reticle_reference](../generated/ink_phase4/reticle_reference)、一覧は [contact.png](../generated/ink_phase4/reticle_reference/contact.png) にある。10fpsの観察用画像の番号を、元動画の60Hzフレーム数として扱わない。

中央には薄い灰白色の細い円と小さな点がある。外枠は円ではなく **四隅の短い白い斜線4本**。ジャンプすると主に左右の間隔が広がり、縦の間隔はおおむね一定である。着地後は横幅が狭い状態へ戻る。中央の円と点は、外側の広がりに合わせて拡大していない。

| 観察画像 | 中央の概略位置 | 外側の斜線中心の概略位置 | 観察 |
| --- | --- | --- | --- |
| [frame_001.png](../generated/ink_phase4/reticle_reference/frame_001.png) | x511、y273 | 左右x444／579、上下y238／307 | 地上。横半幅約67、縦半幅約34 |
| [frame_011.png](../generated/ink_phase4/reticle_reference/frame_011.png) | x511、y199 | 左右x395／628、上下y164／233 | 空中。横半幅約116、縦半幅約34 |
| [frame_014.png](../generated/ink_phase4/reticle_reference/frame_014.png) | x511、y241付近 | 地上より広い左右間隔 | 空中から着地へ向かう途中 |
| [frame_020.png](../generated/ink_phase4/reticle_reference/frame_020.png) | x511、y277付近 | 地上に近い左右間隔 | 横幅が戻った状態 |

座標は画像を見た概略の読み取りで、ピクセル検出器による厳密な計測ではない。中央の円は半径約19pxで、斜線は長さ約22px・太さ約6pxに見える。ただし、動画は一般的な16:9の全画面ではない。元の画面解像度、切り抜きと拡大の有無、FOV、入力履歴は不明なので、そのまま1280×720のゲームHUDへコピーする根拠にはしない。画像内では中央の円自体も上下へ移動しているが、その原因をカメラ・照準補正・編集のいずれかと断定しない。

## 公開説明との照合

[かなもじの弾ブレの説明](https://note.com/kanamoji_1027/n/nfd4a961652a6) は、最大拡散角がレティクルの横幅に反映され、拡散0では正方形に近くなると説明している。今回の動画で確認できる「上下左右が同時に広がる十字ではなく、左右の広がりが変わる四隅」という形と整合する。この説明は2024年の記事であり、例示された5°／12°を現在のスプラシューターの値へ再採用しない。現在のCG2が使う4.86°／11.66°と回復時間は既存の調査・Simulationを参照する。

同じ記事が説明するbiasは弾の分布の集中度であり、外枠の最大拡散角とは異なる。第3段階で実装した分布式は [射撃挙動の調査](ink_shooter_phase3_shooting_research.md) を参照する。今回の外枠はSimulationが出力する最大角に従い、biasや見た目の反動だけで不必要に拡大しない。

[任天堂の更新履歴](https://www.nintendo.com/en-gb/Support/Nintendo-Switch/Game-Updates/Splatoon-3-Update-History-2358763.html) には、シューター等で上下を向いた際に左右ブレが狭くなる不具合の修正がある。照準の上下角を理由にHUDの角度幅を縮める設計にはしない。ただし、この記述だけから原作HUDのピクセル寸法や投影式が分かるわけではない。

## FOVと拡散角を使ったCG2の投影

CG2では、画面の高さH、縦FOVのラジアン値F、拡散の最大半角sから次を計算する。

```text
focalPixels = (H / 2) / tan(F / 2)
spreadHalfPixels = focalPixels * tan(s * pi / 180)
```

これは透視投影の幾何として導いたCG2の表示設計であり、原作が同じピクセル式を使うという主張ではない。高さ720・FOV1.02では、地上4.86°は約54.7229px、ジャンプ11.66°は約132.8134pxになる。FOVを広くすれば表示幅は狭まり、同じFOVで描画解像度を2倍にすればピクセル寸法も2倍になる。生の度数に固定係数を掛ける旧表示より、カメラ設定変更との関係が明確になる。

今回の動画の概略幅比は約1.7、上記投影による幅比は約2.4である。**動画の寸法に完全一致したという報告はしない。** CG2では角度の投影に加え、拡散0でも四隅が中央の円を潰さない最小寸法を置く。シューターは縦の帯を一定に保つ。装飾の最小幅・太さ・円の寸法はCG2の調整値である。

## トライストリンガーへの拡張境界

[任天堂のブキ紹介](https://splatoon.nintendo.com/en/news/beginner-basics-for-splatoon-3-choosing-the-right-weapons/) は、トライストリンガーについてチャージで射程が伸びることと、チャージした弾が着弾後に爆発することを説明している。3点の配置、地上の横配置から空中の縦配置への変更、2段階のチャージは今後の武器側が決定し、描画側はその値を受け取る。

参考として [InkipediaのStringer項目](https://splatoonwiki.org/wiki/Stringers) は、横／縦の配置と2段階チャージを説明し、Nintendo Splatoon Baseの紹介文も掲載している。ただし、今回Nintendo Splatoon Baseの該当ページを直接取得できておらず、引用転載を原典の直接確認として扱わない。トライストリンガーの外枠の正確な意匠は今回のシューター動画では確認できないため、追加した3点と2本のチャージ弧はCG2側の共通表示案とする。

銃口の左右オフセットや曲射による3本の予想着弾位置は、弾道・カメラの計算側で扱う。描画側で `0.2 / 距離` のような固定式を隠して追加しない。今回の三点表示は入力した主軸拡散角に沿う対称配置であり、地形衝突後の3点を予測する機能ではない。全チャージ付近で点が重なる場合があることも、実際の3本が必ず全弾命中する保証とは分ける。

## 実装API

[InkReticleRenderer.h](../project/game/ink/InkReticleRenderer.h) と [.cpp](../project/game/ink/InkReticleRenderer.cpp) を追加した。入力名は次の通り。

```cpp
struct InkReticleState {
    Vector2 center{-1, -1};
    float spreadDegrees = 0;
    float verticalSpreadDegrees = 0;
    float charge = 0;
    float firstChargeRatio = 0.416667f;
    bool stringer = false;
    bool vertical = false;
    bool submerged = false;
    bool outOfInk = false;
    bool target = false;
    float displayScale = 1;
    int projectileCount = 3;
};

void InkReticleRenderer::Initialize(DirectXCommon*);
void InkReticleRenderer::Draw(const InkReticleState&, float viewportWidth,
    float viewportHeight, float verticalFovRadians);
```

- `center` はビューポート内のピクセル座標。負の成分はその軸の画面中央を選ぶ。カメラの照準点を動かす場合、親側が座標を供給する。
- `spreadDegrees` は主軸の最大半角、`verticalSpreadDegrees` は副軸の最大半角。シューターは副軸0。ストリンガーで `vertical=true` の場合だけ主軸を縦へ回すため、呼び出し側でさらにXYを交換しない。
- `charge` は全チャージまでの0～1。`firstChargeRatio` は第1段階の閾値を全チャージに対する比率で渡す。既定値を原作の全状況に共通する時間として扱わない。地上／空中で閾値が変わる場合は武器側の値を渡す。
- `submerged` は表示を薄くし、`outOfInk` と `target` は色による補助表示を選ぶ。命中・遮蔽・インク不足の判定は呼び出し側が行う。色や薄さはCG2のUI調整であり、今回の動画で全状態を検証したわけではない。
- `displayScale` は円や線の太さ・最小寸法にだけ作用し、角度投影には掛けない。描画解像度のH/720による拡大は装飾にも適用する。
- `projectileCount` はストリンガーの1〜3本の配置に使う。中央の照準円は本数に関わらず表示する。

純粋な計算を [InkReticleMath.h](../project/game/ink/InkReticleMath.h) へ分離した。`ink::reticle::ProjectSpreadPixels()` と `SplitCharge()` はDirectXへの依存がなく、投影と2段階への分割を検査できる。

## 描画パスと負荷

描画は中央を囲む小さな四角形1枚、頂点6個、DrawInstancedを1回。SDFで円・点・斜線・チャージ弧を評価し、微分を使って輪郭のカバレッジを調整する。テクスチャ、SRV、頂点バッファ、フレームごとのリソース確保は不要。24個の32bitルート定数、計96バイトを渡す。大きな画面全体を毎回シェーディングする設計にはしていない。

通常のHUDスプライトの後、スクリーンショットのCopyCaptureより前に呼ぶ。ターゲットはエンジンの `kBackBufferRenderTargetFormat`、つまり単一のsRGBスワップチェーンRTV。シーンのHDR・法線・材質MRTへは書かない。既存のバックバッファ描画でD24の深度ビューが結合されるためPSOのDSVFormatはそれに合わせるが、深度テストと深度書き込みは無効にする。

ピクセルシェーダー内部で各線を重ねた結果をpremultiplied alphaで出し、ONE／INV_SRC_ALPHAで通常の透過合成を行う。加算発光ではなく、ポストエフェクトの後なのでBloomにも入らない。明るい背景でも線が消えにくいように薄い暗色の縁を付ける。

この描画は自身のルートシグネチャとPSOを結合する。後から通常スプライトを追加する場合はSpriteCommon::PreDrawを呼び直す。シーンの3D用黄色い着弾マーカーは別の機能であり、中央の照準・外側の拡散表示と意味を混同しない。

## 確認状況

- HLSLのVS／PSはDXC、Shader Model 6.0、`-O3 -WX` でコンパイルPASS。
- C++はMSVC C++20、`/W4 /WX /Zs` で構文検査PASS。
- 数学ヘルパーはMSVC C++17、`/W4 /WX /O2` で実行PASS。地上／ジャンプの投影値、解像度を2倍にした寸法、FOVを広げた縮小、拡散角に対する単調性、0／NaNの入力、0・第1段階・全チャージでの弧の割合を確認した。
- ゲームへの組み込み、実際のシューター／ストリンガー切り替え、ジャンプの外枠拡大、2段階リング、空中の縦配置はネイティブの自動操作と画面で確認した。日本語エディターから複製を2本へ変更・保存・再読込・適用し、点の間隔とHUDが変わることも確認した。GPU診断と画像は [全体の実装報告](ink_phase4_implementation.md) に集約する。解像度・FOV変更の数式は上記テストで検証し、全ディスプレイ設定の実機保証とはしない。
