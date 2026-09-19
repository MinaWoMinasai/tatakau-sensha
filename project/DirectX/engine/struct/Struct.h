#pragma once
#include <cstddef>
#include <cstdint>
#include <format>
#include <dxgi1_6.h>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

struct Vector2 {
	float x;
	float y;
};

struct Vector3 {
	float x;
	float y;
	float z;

	inline Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
	inline Vector3& operator-=(const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	inline Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
	inline Vector3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }

};

struct Vector4 {
	float x, y, z, w;

	inline Vector4& operator+=(const Vector4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
	inline Vector4& operator-=(const Vector4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
	inline Vector4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
	inline Vector4& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct Actor {
	Transform transform;
	float speed;
};

// 3x3の行列
struct Matrix3x3 {
	float m[3][3];
};

// 4x4の行列
struct Matrix4x4 {
	float m[4][4];
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
	Vector4 tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
};

struct Sphere {
	Vector3 center; // 中心点
	float radius;   // 半径
};

struct Line {
	Vector3 origin; // 始点
	Vector3 diff;   // 終点への差分ベクトル
};

struct Ray {
	Vector3 origin; // 始点
	Vector3 diff;   // 終点への差分ベクトル
};

struct Segment {
	Vector3 origin; // 始点
	Vector3 diff;   // 終点への差分ベクトル
};

struct Capsule {
	Segment segment;
	float radius;
};

struct Plane {
	Vector3 normal; // 法線
	float distance; // 距離
};

struct Triangle {
	Vector3 vertex[3]; // 頂点
};

struct AABB {
	Vector3 min; // 最小値
	Vector3 max; // 最大値
};

struct OBB {
	Vector3 center;
	Vector3 halfExtents;
	Vector3 orientation[3]; // 正規化済み
};

// GPU constant-buffer layout. Keep this field order exactly synchronized with
// Material in resources/shaders/Object3d.PS.hlsl.
struct alignas(16) Material {
	Vector4 color;
	int32_t enableLighting;
	int32_t lightingMode;
	float environmentCoefficient; // 追加：環境マッピング係数 (0.0~1.0)
	float padding; // 16バイトアライメントのための調整
	Matrix4x4 uvTransform;
	float shininess;
	float waterDiagnosticsEnabled;
	float waterSunPathEnabled;
	float waterAtmosphereEnabled;
	float waterFarFlattenEnabled;
	float waterProceduralCloudReflectionEnabled;
	float waterDebugMode;
	float waterAtmosphereStrength;
	float waterFarFlattenStrength;
	float metallic;
	float roughness;
	float ambientOcclusion;
	Vector3 emissiveColor;
	float emissiveIntensity;
	float iblDiffuseIntensity;
	float iblSpecularIntensity;
	float iblMaxMipLevel;
	float pbrEnvironmentMode;
	float shadowReceiveStrength;
	float normalDetailStrength;
	float normalDetailScale;
	float normalMapStrength;
	float metallicMapStrength;
	float roughnessMapStrength;
	float occlusionMapStrength;
	float metallicMapChannel;
	float roughnessMapChannel;
	float occlusionMapChannel;
	float shadowDepthBias;
	float shadowSlopeBias;
	float shadowPcfRadius;
	float materialDebugMode;
	float materialDebugPadding[2];
	float characterLightWrap;
	float characterShadowSoftness;
	float characterShadowStrength;
	float characterRimStrength;
	float characterRimPower;
	float characterSpecularStrength;
	float characterSpecularPower;
	float characterPadding;
	float crystalEnabled;
	float crystalFresnelPower;
	float iridescenceFactor;
	float iridescenceIor;
	float iridescenceThicknessMinimumNm; // Thin-film thickness in nanometers.
	float iridescenceThicknessMaximumNm; // Thin-film thickness in nanometers.
	float crystalEdgeEmission;
	float crystalCoreEmission;
	Vector3 crystalCoreColor;
	float crystalCorePadding;
	Vector3 crystalEdgeColor;
	float crystalEdgePadding;
	float enableCaustics;
	float causticsScale;
	float causticsIntensity;
	float causticsPadding;
	Vector3 causticsColor;
	float causticsColorPadding;
	float causticsAnimationEnabled;
	float causticsPlaybackTime;
	float causticsLoopDuration;
	float causticsFrameCount;
	float causticsAtlasColumns;
	float causticsAtlasRows;
	float causticsAnimationPadding[2];
};

static_assert(sizeof(Material) % 16 == 0, "Material constant buffer must be 16-byte aligned.");
static_assert(sizeof(Material) == 400, "Update the HLSL Material layout when changing Material.");
static_assert(offsetof(Material, uvTransform) == 32);
static_assert(offsetof(Material, emissiveColor) == 144);
static_assert(offsetof(Material, characterLightWrap) == 240);
static_assert(offsetof(Material, crystalEnabled) == 272);
static_assert(offsetof(Material, iridescenceThicknessMinimumNm) == 288);
static_assert(offsetof(Material, crystalCoreColor) == 304);
static_assert(offsetof(Material, crystalEdgeColor) == 320);
static_assert(offsetof(Material, enableCaustics) == 336);
static_assert(offsetof(Material, causticsColor) == 352);
static_assert(offsetof(Material, causticsAnimationEnabled) == 368);
static_assert(offsetof(Material, causticsAtlasColumns) == 384);

struct CrystalMaterialSettings {
	bool enabled = true;
	float fresnelPower = 4.6f;
	float iridescenceFactor = 0.72f;
	float iridescenceIor = 1.52f;
	float thicknessMinimumNm = 360.0f;
	float thicknessMaximumNm = 720.0f;
	float edgeEmission = 3.0f;
	float coreEmission = 0.08f;
	Vector3 coreColor = { 0.08f, 0.48f, 0.82f };
	Vector3 edgeColor = { 0.92f, 0.20f, 0.72f };
};

struct OceanWakeData {
	Vector4 wakePoints[16];
	Vector4 wakeDirections[16];
	Vector4 parameters;
};

inline Material MakeDefaultMaterial()
{
	Material material{};
	material.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.enableLighting = false;
	material.lightingMode = 0;
	material.environmentCoefficient = 0.0f;
	material.padding = 0.0f;
	material.uvTransform = {};
	material.uvTransform.m[0][0] = 1.0f;
	material.uvTransform.m[1][1] = 1.0f;
	material.uvTransform.m[2][2] = 1.0f;
	material.uvTransform.m[3][3] = 1.0f;
	material.shininess = 32.0f;
	material.waterDiagnosticsEnabled = 0.0f;
	material.waterSunPathEnabled = 1.0f;
	material.waterAtmosphereEnabled = 1.0f;
	material.waterFarFlattenEnabled = 1.0f;
	material.waterProceduralCloudReflectionEnabled = 1.0f;
	material.waterDebugMode = 0.0f;
	material.waterAtmosphereStrength = 1.0f;
	material.waterFarFlattenStrength = 1.0f;
	material.metallic = 0.0f;
	material.roughness = 0.5f;
	material.ambientOcclusion = 1.0f;
	material.emissiveColor = { 0.0f, 0.0f, 0.0f };
	material.emissiveIntensity = 0.0f;
	material.iblDiffuseIntensity = 1.0f;
	material.iblSpecularIntensity = 1.0f;
	material.iblMaxMipLevel = 7.0f;
	material.pbrEnvironmentMode = 0.0f;
	material.shadowReceiveStrength = 1.0f;
	material.normalDetailStrength = 0.0f;
	material.normalDetailScale = 24.0f;
	material.normalMapStrength = 0.0f;
	material.metallicMapStrength = 0.0f;
	material.roughnessMapStrength = 0.0f;
	material.occlusionMapStrength = 0.0f;
	material.metallicMapChannel = 2.0f;
	material.roughnessMapChannel = 1.0f;
	material.occlusionMapChannel = 0.0f;
	material.shadowDepthBias = 0.00035f;
	material.shadowSlopeBias = 0.0018f;
	material.shadowPcfRadius = 1.0f;
	material.materialDebugMode = 0.0f;
	material.materialDebugPadding[0] = 0.0f;
	material.materialDebugPadding[1] = 0.0f;
	material.characterLightWrap = 0.28f;
	material.characterShadowSoftness = 0.16f;
	material.characterShadowStrength = 0.54f;
	material.characterRimStrength = 0.12f;
	material.characterRimPower = 3.2f;
	material.characterSpecularStrength = 0.075f;
	material.characterSpecularPower = 42.0f;
	material.characterPadding = 0.0f;
	material.crystalEnabled = 0.0f;
	material.crystalFresnelPower = 4.6f;
	material.iridescenceFactor = 0.72f;
	material.iridescenceIor = 1.52f;
	material.iridescenceThicknessMinimumNm = 360.0f;
	material.iridescenceThicknessMaximumNm = 720.0f;
	material.crystalEdgeEmission = 3.0f;
	material.crystalCoreEmission = 0.08f;
	material.crystalCoreColor = { 0.08f, 0.48f, 0.82f };
	material.crystalCorePadding = 0.0f;
	material.crystalEdgeColor = { 0.92f, 0.20f, 0.72f };
	material.crystalEdgePadding = 0.0f;
	material.enableCaustics = 0.0f;
	material.causticsScale = 0.035f;
	material.causticsIntensity = 0.25f;
	material.causticsPadding = 0.0f;
	material.causticsColor = { 0.75f, 0.92f, 1.0f };
	material.causticsColorPadding = 0.0f;
	material.causticsAnimationEnabled = 0.0f;
	material.causticsPlaybackTime = 0.0f;
	material.causticsLoopDuration = 4.0f;
	material.causticsFrameCount = 1.0f;
	material.causticsAtlasColumns = 1.0f;
	material.causticsAtlasRows = 1.0f;
	material.causticsAnimationPadding[0] = 0.0f;
	material.causticsAnimationPadding[1] = 0.0f;
	return material;
}

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
};

struct TransformationMatrixWithShadow {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
	Matrix4x4 LightWVP;
};


struct ModelParticleTransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
	Vector4 color;
};

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

struct DirectionalLight {
	Vector4 color; // ライトの色
	Vector3 direction; // ライトの方向
	float intensity; // ライトの光度
};

enum class MaterialSemantic : uint32_t {
	Unknown = 0,
	GenericPbr,
	CharacterSkin,
	CharacterHair,
	CharacterCloth,
	CharacterEye,
	Metal,
	Glass,
	Emissive,
};

inline const char* MaterialSemanticName(MaterialSemantic semantic)
{
	switch (semantic) {
	case MaterialSemantic::GenericPbr:
		return "Generic PBR";
	case MaterialSemantic::CharacterSkin:
		return "Character skin";
	case MaterialSemantic::CharacterHair:
		return "Character hair";
	case MaterialSemantic::CharacterCloth:
		return "Character cloth";
	case MaterialSemantic::CharacterEye:
		return "Character eye";
	case MaterialSemantic::Metal:
		return "Metal";
	case MaterialSemantic::Glass:
		return "Glass";
	case MaterialSemantic::Emissive:
		return "Emissive";
	default:
		return "Unknown";
	}
}

struct MaterialData {
	std::string materialName;
	MaterialSemantic semantic = MaterialSemantic::GenericPbr;
	bool semanticInferred = false;
	std::string textureFilePath;
	uint32_t textureIndex = 0;
	Vector4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool hasBaseColorFactor = false;
	float metallicFactor = 0.0f;
	float roughnessFactor = 0.5f;
	float ambientOcclusionFactor = 1.0f;
	bool hasPbrFactors = false;
	Vector3 emissiveColor = { 0.0f, 0.0f, 0.0f };
	float emissiveIntensity = 0.0f;
	bool hasEmissive = false;
	std::string normalTextureFilePath;
	uint32_t normalTextureIndex = 0;
	bool hasNormalTexture = false;
	std::string metallicRoughnessTextureFilePath;
	uint32_t metallicRoughnessTextureIndex = 0;
	bool hasMetallicRoughnessTexture = false;
	std::string metallicTextureFilePath;
	std::string roughnessTextureFilePath;
	bool hasMetallicTexture = false;
	bool hasRoughnessTexture = false;
	float metallicMapChannel = 2.0f;
	float roughnessMapChannel = 1.0f;
	std::string occlusionTextureFilePath;
	uint32_t occlusionTextureIndex = 0;
	bool hasOcclusionTexture = false;
	float occlusionMapChannel = 0.0f;
};

struct ModelSubmesh {
	uint32_t startIndex = 0;
	uint32_t indexCount = 0;
	uint32_t materialIndex = 0;
	std::string materialName;
};

struct ModelData {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
	std::vector<MaterialData> materials;
	std::vector<ModelSubmesh> submeshes;
};

struct Quaternion {
	float x;
	float y;
	float z;
	float w;
};

struct ChunkHeader
{
	char id[4];// チャンク毎のID
	int32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader
{
	ChunkHeader chunk; // "RIFF"
	char type[4]; // "WAVE"
};

// FMTチャンク
struct FormatChunk
{
	ChunkHeader chunk; // "fmt"
	WAVEFORMATEX fmt; // 波形フォーマット
};

struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 angularVelocity;
	Vector3 kVelocity;
	Vector4 color;
	float lifeTime;
	float currentTime;
};

struct Emitter {
	Transform transform; // エミッタの位置
	uint32_t count; // 発生数
	float frequency; // 発生頻度
	float frequencyTime; // 頻度用時刻
};

struct AccelerationField {
	Vector3 acceleration; // 加速度
	AABB area; // 効果範囲
};

struct CameraData
{
	Vector3 worldPosition;
	float padding; // 16byte アラインメント用（重要）
};

enum class FireworkState {
	Rise,
	Explode
};

struct FireworkShell {
	Vector3 pos;
	Vector3 velocity;
	FireworkState state;

	float timer;
	float explodeTime;
	bool isRemove;

	Vector4 color;
};

struct TornadoParticle {
	Vector3 pos;
	float angle;
	float height;
	float baseRadius;
	float rotateSpeed;
	float upSpeed;
	float maxHeight; // ← 無限ループ用
	Vector4 color;
};

enum AxisXYZ {
	X,
	Y,
	Z,
};

enum BlendMode {
	kNone,
	kNormal,
	kAdd,
	kShadow,

	kAdd_Bloom_Extract,
	kAdd_Bloom_BlurH,
	kAdd_Bloom_BlurV,
	kAdd_Bloom_Composite,
	kAdd_Bloom_Downsample,
	kAdd_ObjectPost_Composite,
	kAdd_ObjectPost_OutlineAdd,
	kAdd_ObjectPost_BloomAdd,
	kRandom,
	kAdd_SSAO_Resolve,
	kAdd_SSAO_Denoise,
	kAdd_SSR_Resolve,
	kAdd_SSR_Denoise,
	kAdd_MotionVector_Resolve,
	kAdd_Temporal_Resolve,
};

enum Phase {
	kFadeIn,
	kMain,
	kFadeOut,
};

struct AttackParam {
	float bulletSpeed = 0.0f;
	int bulletCount = 1;
	float spreadAngleDeg = 0.0f;
	bool randomSpread = false;

	bool reflect = false;
	bool penetrate = false;

	float cooldown = 0.0f;

	uint32_t damage = 0;
	float bulletHp = 0.0f;
	float bulletPenetration = 0.0f;
	// Neutral Shooter projectiles must not claim a rival's shared resource.
	bool canClaimRunResource = true;
};

enum BulletOwner {
	kPlayer,
	kEnemy,
	kExpEnemyHostile
};

struct CollisionResult {
	bool hit = false;     // 衝突しているか
	Vector3 normal;       // 押し戻し方向（正規化済み）
	float depth = 0.0f;   // 侵入量（押し戻す距離）
}; 

struct BloomParam
{
	float threshold;
	float intensity;
	float vignetteIntensity;
	float vignetteScale;
	float timer; // 経過時間
	float distortionAmount; // うねうねの強さ
	float chromAbAmount; // 色収差（にじみ）の強さ
	float isGrayscale;
	float isInverted;
	float noiseIntensity; // ノイズの強さ
	float scanlineIntensity; // 走査線の強さ
	float scanlineFrequency; // 走査線の密度
	float curvature; // 画面の膨らみ具合 (0.02 くらいがおすすめ)
	float borderSharp; // 枠の角の鋭さ (20.0 くらい)
	float glitchAmount; // 追加：グリッチの強さ（0.0 ~ 0.1くらい）
	float gaussianIntensity; // Gaussian Blurの全画面ブレンド量
	float dissolveThreshold; // ディゾルブの進行度 (0~1)
	float outlineWidth; // アウトラインの太さ
	float outlineThreshold; // エッジ検出のしきい値
	float boxBlurIntensity; // 全体ポストのボックスぼかし合成量
	Vector3 outlineColor; // アウトラインの色
	float outlineBloomIntensity; // アウトラインだけのブルーム強度
	float outlineBloomWidth; // アウトラインブルームの広がり
	float boxBlurRadius; // 全体ポストのボックスぼかし半径(px)
	float fullScreenBoxBlurBlend; // 5x5 Box Filterの全画面ブレンド量
	float depthOutlineEnabled; // DepthBufferを利用したアウトラインの有効化
	float depthNearClip; // View空間Z復元用のnear clip
	float depthFarClip; // View空間Z復元用のfar clip
	float depthOutlineScale; // View空間Z差分の強調倍率
	float depthBufferPadding; // HLSL cbufferのfloat2境界に合わせる
	Vector2 shockwaveCenter; // 画面UV上の衝撃波中心
	float shockwaveRadius;
	float shockwaveWidth;
	float shockwaveStrength;
	float shockwavePadding[3];
	Vector2 radialBlurCenter; // 放射状ブラーの中心（画面UV）
	float radialBlurWidth; // 中心から外側へ進めるサンプリング幅
	float radialBlurIntensity; // 元画像との合成量
	Vector3 dissolveEdgeColor; // ディゾルブ境界の発光色
	float dissolveEdgeWidth; // 閾値から境界色を付ける幅
	float dissolveNoiseScale; // 手続きノイズマスクの細かさ
	float dissolveNoiseSpeed; // ノイズマスクの時間変化速度
	float postEffectPadding[2];
	float randomIntensity; // 入力画像へ乗算する乱数の強さ
	float randomScale; // 乱数セルの細かさ
	float randomTimeScale; // timeをSeedへ加える速度
	float randomGrayscalePreview; // 乱数を白黒で直接表示
	float exposure; // HDRシーンをLDRへ落とす前の露出
	float toneMappingMode; // 0: Reinhard, 1: ACES
	float hdrWhitePoint; // Reinhard系ToneMappingの白基準
	float hdrPadding;
	float renderDebugMode; // 0: Final, 1: Scene, 2: Bloom, 3: LinearDepth, 4: DepthEdge, 5: DepthNormal, 6: NormalBuffer, 7: SSAO, 8: SSR, 9: SSRMask, 10-13: Material, 14: Motion, 15: MotionDiagnostic, 16: TemporalSource
	float linearDepthDebugRange; // LinearDepth表示の白基準距離
	float depthNormalScale; // Depthから推定する画面空間法線の強調倍率
	float depthFogEnabled; // DepthFogの有効化
	Vector3 depthFogColor; // DepthFogの色
	float depthFogStart; // Fog開始距離
	float depthFogEnd; // Fog最大距離
	float depthFogDensity; // 指数Fogの濃さ
	float depthFogMaxOpacity; // Fogの最大合成率
	float renderDebugPadding;
	float ssaoEnabled; // Screen Space Ambient Occlusionの有効化
	float ssaoRadius; // SSAOサンプル半径(px)
	float ssaoIntensity; // SSAOの暗さ
	float ssaoBias; // 自己遮蔽を抑える深度バイアス
	float ssaoPower; // AOカーブ
	float ssaoSampleCount; // 使用サンプル数
	float ssaoNormalInfluence; // 法線差による重み
	float ssaoDistanceFalloff; // 深度差の減衰距離
	Matrix4x4 ssrViewMatrix; // ワールド法線をView空間へ変換するための現在カメラView
	Vector2 ssrProjectionScale; // View空間座標復元用: tan(fovY/2)*aspect, tan(fovY/2)
	float ssrEnabled; // Screen Space Reflectionの有効化
	float ssrIntensity; // 反射合成の強さ
	float ssrMaxDistance; // View空間での最大レイ距離
	float ssrThickness; // 深度ヒット許容幅
	float ssrStepCount; // レイマーチサンプル数
	float ssrStride; // レイの進み幅倍率
	float ssrFresnelPower; // 視線角による反射強度カーブ
	float ssrEdgeFade; // 画面端フェード幅
	float ssrDepthFade; // 遠距離フェード開始の目安
	float ssrNormalFade; // 正面向き面の反射抑制
	float ssrMaskPower; // マテリアル反射マスクのカーブ
	float ssrBlurRadius; // 反射色の簡易ぼかし半径(px)
	float ssrDenoiseEnabled; // SSR専用デノイズの有効化
	float ssrDenoiseRadius; // SSRデノイズ半径(px)
	float ssrDenoiseDepthSigma; // 深度差によるにじみ抑制
	float ssrDenoiseNormalSigma; // 法線差によるにじみ抑制
	float ssaoDenoiseEnabled; // SSAO専用デノイズの有効化
	float ssaoDenoiseRadius; // SSAOデノイズ半径(px)
	float ssaoDenoiseDepthSigma; // SSAOの深度差によるにじみ抑制
	float ssaoDenoiseNormalSigma; // SSAOの法線差によるにじみ抑制
	float motionMatrixPadding[2]; // HLSL cbufferで後続Matrix4x4を16byte境界へ揃える
	Matrix4x4 motionCurrentViewProjection; // 現在フレームのViewProjection
	Matrix4x4 motionPreviousViewProjection; // 前フレームのViewProjection
	Matrix4x4 motionInverseCurrentViewProjection; // Depth復元用の現在ViewProjection逆行列
	float motionVectorEnabled; // Motion Vector Bufferの有効化
	float motionVectorScale; // 保存するモーション量の倍率
	float motionVectorDebugScale; // Debug表示時の強調倍率
	float motionVectorPadding;
	float temporalEnabled; // Temporal履歴合成の有効化
	float temporalHistoryValid; // 前フレーム履歴が使用可能か
	float temporalBlendFactor; // 履歴色の合成率
	float temporalMotionRejection; // 大きな動きで履歴を捨てる強さ
	Vector2 temporalJitter; // 現在フレームのTAAサブピクセルジッター(NDC)
	Vector2 temporalPreviousJitter; // 前フレームのTAAサブピクセルジッター(NDC)
	float temporalJitterEnabled; // カメラジッターの有効化
	float temporalJitterScale; // Haltonジッターの倍率
	float temporalJitterPadding[2];
	float waterDiagnosticsEnabled; // Graphics Labの水面原因分離を有効化
	float waterTaaEnabled; // 水面へ低履歴TAAを適用
	float waterBloomEnabled; // 水面をBloom抽出へ含める
	float waterHistoryWeight; // 水面TAAの最大history weight
	float waterDebugMode; // 0: Final, 1: Normal, 2: Fresnel, 3: Sun, 4: Foam, 5: Reactive
	float waterPostPadding[3];
};

struct PointLightData {
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	float padding[2];
};

struct ShadowTransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 LightWVP;
};
struct ShadowData {
	Matrix4x4 lightViewProjection;
};

struct TrailVertex {
	Vector3 pos;   // POSITION
	Vector4 color; // COLOR (ここを毎フレーム変えてフェードアウトさせる)
	Vector2 uv;    // TEXCOORD
};
