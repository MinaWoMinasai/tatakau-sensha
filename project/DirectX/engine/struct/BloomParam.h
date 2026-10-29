#pragma once

namespace myEngine {

/// @brief ブルームや画面効果の強度・範囲などのパラメーターを表す。
struct BloomParam {
    float threshold;
    float intensity;
    float vignetteIntensity;
    float vignetteScale;
    float timer;            // 経過時間
    float distortionAmount; // うねうねの強さ
    float chromAbAmount;    // 色収差（にじみ）の強さ
    float isGrayscale;
    float isInverted;
    float noiseIntensity;         // ノイズの強さ
    float scanlineIntensity;      // 走査線の強さ
    float scanlineFrequency;      // 走査線の密度
    float curvature;              // 画面の膨らみ具合 (0.02 くらいがおすすめ)
    float borderSharp;            // 枠の角の鋭さ (20.0 くらい)
    float glitchAmount;           // 追加：グリッチの強さ（0.0 ~ 0.1くらい）
    float gaussianIntensity;      // Gaussian Blurの全画面ブレンド量
    float dissolveThreshold;      // ディゾルブの進行度 (0~1)
    float outlineWidth;           // アウトラインの太さ
    float outlineThreshold;       // エッジ検出のしきい値
    float boxBlurIntensity;       // 全体ポストのボックスぼかし合成量
    Vector3 outlineColor;         // アウトラインの色
    float outlineBloomIntensity;  // アウトラインだけのブルーム強度
    float outlineBloomWidth;      // アウトラインブルームの広がり
    float boxBlurRadius;          // 全体ポストのボックスぼかし半径(px)
    float fullScreenBoxBlurBlend; // 5x5 Box Filterの全画面ブレンド量
    float depthOutlineEnabled;    // DepthBufferを利用したアウトラインの有効化
    float depthNearClip;          // View空間Z復元用のnear clip
    float depthFarClip;           // View空間Z復元用のfar clip
    float depthOutlineScale;      // View空間Z差分の強調倍率
    float depthBufferPadding;     // HLSL cbufferのfloat2境界に合わせる
    Vector2 shockwaveCenter;      // 画面UV上の衝撃波中心
    float shockwaveRadius;
    float shockwaveWidth;
    float shockwaveStrength;
    float shockwavePadding[3];
    Vector2 radialBlurCenter;  // 放射状ブラーの中心（画面UV）
    float radialBlurWidth;     // 中心から外側へ進めるサンプリング幅
    float radialBlurIntensity; // 元画像との合成量
    Vector3 dissolveEdgeColor; // ディゾルブ境界の発光色
    float dissolveEdgeWidth;   // 閾値から境界色を付ける幅
    float dissolveNoiseScale;  // 手続きノイズマスクの細かさ
    float dissolveNoiseSpeed;  // ノイズマスクの時間変化速度
    float postEffectPadding[2];
    float randomIntensity;        // 入力画像へ乗算する乱数の強さ
    float randomScale;            // 乱数セルの細かさ
    float randomTimeScale;        // timeをSeedへ加える速度
    float randomGrayscalePreview; // 乱数を白黒で直接表示
    float exposure;               // HDRシーンをLDRへ落とす前の露出
    float toneMappingMode;        // 0: Reinhard、1: ACES
    float hdrWhitePoint;          // Reinhard系ToneMappingの白基準
    float hdrPadding;
    float
        renderDebugMode; // 0: Final, 1: Scene, 2: Bloom, 3: LinearDepth, 4: DepthEdge, 5: DepthNormal, 6: NormalBuffer, 7: SSAO, 8: SSR, 9: SSRMask, 10-13: Material, 14: Motion, 15: MotionDiagnostic, 16: TemporalSource
    float linearDepthDebugRange; // LinearDepth表示の白基準距離
    float depthNormalScale;      // Depthから推定する画面空間法線の強調倍率
    float depthFogEnabled;       // DepthFogの有効化
    Vector3 depthFogColor;       // DepthFogの色
    float depthFogStart;         // Fog開始距離
    float depthFogEnd;           // Fog最大距離
    float depthFogDensity;       // 指数Fogの濃さ
    float depthFogMaxOpacity;    // Fogの最大合成率
    float renderDebugPadding;
    float ssaoEnabled;                            // Screen Space Ambient Occlusionの有効化
    float ssaoRadius;                             // SSAOサンプル半径(px)
    float ssaoIntensity;                          // SSAOの暗さ
    float ssaoBias;                               // 自己遮蔽を抑える深度バイアス
    float ssaoPower;                              // AOカーブ
    float ssaoSampleCount;                        // 使用サンプル数
    float ssaoNormalInfluence;                    // 法線差による重み
    float ssaoDistanceFalloff;                    // 深度差の減衰距離
    Matrix4x4 ssrViewMatrix;                      // ワールド法線をView空間へ変換するための現在カメラView
    Vector2 ssrProjectionScale;                   // View空間座標復元用: tan(fovY/2)*aspect, tan(fovY/2)
    float ssrEnabled;                             // Screen Space Reflectionの有効化
    float ssrIntensity;                           // 反射合成の強さ
    float ssrMaxDistance;                         // View空間での最大レイ距離
    float ssrThickness;                           // 深度ヒット許容幅
    float ssrStepCount;                           // レイマーチサンプル数
    float ssrStride;                              // レイの進み幅倍率
    float ssrFresnelPower;                        // 視線角による反射強度カーブ
    float ssrEdgeFade;                            // 画面端フェード幅
    float ssrDepthFade;                           // 遠距離フェード開始の目安
    float ssrNormalFade;                          // 正面向き面の反射抑制
    float ssrMaskPower;                           // マテリアル反射マスクのカーブ
    float ssrBlurRadius;                          // 反射色の簡易ぼかし半径(px)
    float ssrDenoiseEnabled;                      // SSR専用デノイズの有効化
    float ssrDenoiseRadius;                       // SSRデノイズ半径(px)
    float ssrDenoiseDepthSigma;                   // 深度差によるにじみ抑制
    float ssrDenoiseNormalSigma;                  // 法線差によるにじみ抑制
    float ssaoDenoiseEnabled;                     // SSAO専用デノイズの有効化
    float ssaoDenoiseRadius;                      // SSAOデノイズ半径(px)
    float ssaoDenoiseDepthSigma;                  // SSAOの深度差によるにじみ抑制
    float ssaoDenoiseNormalSigma;                 // SSAOの法線差によるにじみ抑制
    float motionMatrixPadding[2];                 // HLSL cbufferで後続Matrix4x4を16byte境界へ揃える
    Matrix4x4 motionCurrentViewProjection;        // 現在フレームのViewProjection
    Matrix4x4 motionPreviousViewProjection;       // 前フレームのViewProjection
    Matrix4x4 motionInverseCurrentViewProjection; // Depth復元用の現在ViewProjection逆行列
    float motionVectorEnabled;                    // Motion Vector Bufferの有効化
    float motionVectorScale;                      // 保存するモーション量の倍率
    float motionVectorDebugScale;                 // Debug表示時の強調倍率
    float motionVectorPadding;
    float temporalEnabled;          // Temporal履歴合成の有効化
    float temporalHistoryValid;     // 前フレーム履歴が使用可能か
    float temporalBlendFactor;      // 履歴色の合成率
    float temporalMotionRejection;  // 大きな動きで履歴を捨てる強さ
    Vector2 temporalJitter;         // 現在フレームのTAAサブピクセルジッター(NDC)
    Vector2 temporalPreviousJitter; // 前フレームのTAAサブピクセルジッター(NDC)
    float temporalJitterEnabled;    // カメラジッターの有効化
    float temporalJitterScale;      // Haltonジッターの倍率
    float temporalJitterPadding[2];
    float waterDiagnosticsEnabled; // Graphics Labの水面原因分離を有効化
    float waterTaaEnabled;         // 水面へ低履歴TAAを適用
    float waterBloomEnabled;       // 水面をBloom抽出へ含める
    float waterHistoryWeight;      // 水面TAAの最大history weight
    float waterDebugMode;          // 0: Final, 1: Normal, 2: Fresnel, 3: Sun, 4: Foam, 5: Reactive
    float waterPostPadding[3];
    // Legacyは強度の3乗を保つ。ピラミッドの係数は合成時にだけ掛ける。
    uint32_t bloomMode = 2; // 0: オフ、1: Legacy、2: Quality、3: Light
    float bloomSoftKnee = 0.5f;
    float bloomScatter = 0.3f;
    float bloomRadius = 0.75f;
    float bloomGain = 0.15f;
    uint32_t bloomOutputToHdr = 1;
    float bloomPresentationPadding[2]{};
    // アリーナ表示（合成のみ）。星雲は深度から復元したワールドXYに固定し、残光はブラウン管の焼き付きを模す。
    float arenaNebulaIntensity = 0.0f; // 0で無効
    float arenaNebulaScale = 0.035f;   // ワールド単位あたりの周波数
    float arenaPhosphorDecay = 0.0f;   // 前フレームの残光の残り率（0で無効）
    float arenaGrainIntensity = 0.0f;  // フィルム粒状ノイズ
    Vector3 arenaNebulaColorA{0.30f, 0.05f, 0.42f};
    float arenaNebulaDrift = 0.004f; // 秒あたりの流れ
    Vector3 arenaNebulaColorB{0.06f, 0.02f, 0.30f};
    float arenaMaskSoftness = 0.012f; // 角丸ビネット枠の柔らかさ
    float arenaBezelMargin = 0.0f;    // 角丸画面の外側の黒枠（UV比）
    float arenaBezelRadius = 0.0f;    // 角丸半径（UV比）
    float arenaEnabled = 0.0f;
    float arenaCurvature = 0.0f; // 最終提示パスの樽型歪み（UIを含む画面全体）
    float arenaChromAb = 0.0f;   // 最終提示パスの色収差
    float arenaVignette = 0.0f;
    float arenaScanlineIntensity = 0.0f;
    float arenaScanlineFrequency = 0.0f; // ラジアン/UV
    // 残光を出さない円（UV）。半径は画面の高さに対する比で、0で無効。aspectは横/縦。
    float arenaPersistenceMaskX = 0.5f;
    float arenaPersistenceMaskY = 0.5f;
    float arenaPersistenceMaskRadius = 0.0f;
    float arenaPersistenceMaskAspect = 1.0f;
};

static_assert(sizeof(BloomParam) % 16 == 0, "BloomParam must stay a whole number of HLSL cbuffer rows.");

} // namespace myEngine
