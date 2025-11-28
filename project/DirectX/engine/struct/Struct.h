#pragma once
#include <format>
#include <dxgi1_6.h>
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

struct alignas(16) Material {
	Vector4 color;
	int32_t enableLighting;
	int32_t lightingMode;
	float padding[2];
	Matrix4x4 uvTransform;
	float shininess;
	float pad2[3];
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
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

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
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

struct Block {
	Transform transform;
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix* wvpData = nullptr;
};

struct Particle {
	Transform transform;
	Vector3 velocity;
	Vector3 acceleration;
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

struct alignas(16) Camera {
	Vector3 worldPosition;
	float pad; // 16バイトアラインメント対策
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