#pragma once
#include <vector>
#include <string>
#include <map>
#include <random>
#include <memory>
#include <initializer_list>
#include <cstddef>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ModelManager.h"
#include "Camera.h"
#include "NeonParticlePresentation.h"
#include <nlohmann/json.hpp>

namespace cg2 {

class DebugCamera;

// エミッター形状
enum class EmitterShape : uint32_t {
    Point = 0,
    Sphere = 1,
    Box = 2
};
// イージングタイプ
enum class EasingType : uint32_t {
    Linear = 0,
    EaseIn = 1,
    EaseOut = 2
};

// エフェクト設定構造体
/// @brief パーティクルの発生数・姿勢・速度・色・寿命を指定する。
struct ParticleEmitterConfig {
    Vector3 position = {0, 0, 0};
    float speedMin = 0.0f;
    float speedMax = 1.0f;
    float lifeTimeMin = 2.0f;
    float lifeTimeMax = 3.0f;
    Vector3 gravity = {0.0f, -0.08f, 0.0f};
    float startScale = 1.0f;
    float endScale = 0.0f;
    Vector3 startScaleMin = {1.0f, 1.0f, 1.0f};
    Vector3 startScaleMax = {1.0f, 1.0f, 1.0f};
    Vector3 endScaleMin = {0.0f, 0.0f, 0.0f};
    Vector3 endScaleMax = {0.0f, 0.0f, 0.0f};
    Vector4 startColor = {1, 1, 1, 1};
    Vector4 endColor = {1, 1, 1, 0};
    std::string modelPath = "plane.obj";
    EmitterShape emitterShape = EmitterShape::Point;
    Vector3 shapeSize = {1.0f, 1.0f, 1.0f};
    EasingType easingType = EasingType::Linear;
    bool isBillboard = false;
    bool alignToVelocity = false;
    // Opt in to the emissive contour and its single, CPU/GPU-matched lifetime fade.
    bool neonRadiance = false;
    NeonParticleShape neonShape = NeonParticleShape::Triangle;

    /// @brief 保存する設定をJSON形式へ変換して返す。
    nlohmann::json ToJson() const;
    /// @brief JSONの値を対応する設定項目へ読み取る。省略時の扱いは各項目の既定値に従う。
    void FromJson(const nlohmann::json& j);
};

/// @brief パーティクルの発生・更新とCPU/GPUの描画資源を管理する。
class ParticleManager {
public:
    enum class NeonTriangleEffectMode : uint32_t {
        Outline = 0,
        LegacyModel = 1,
        Hybrid = 2
    };
    /// @brief 画面効果へ渡すパルスの強度・時間などを表す。
    struct ScreenPulseEvent {
        Vector3 position{};
        float strength = 1.0f;
    };
    /// @brief ネオン三角形の発生位置・色・動作を表す。
    struct NeonTriangleEvent {
        Vector3 position{};
        Vector3 velocity{};
        float radius = 0.35f;
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
        float lineWidth = 0.055f;
        float lifeTime = 0.35f;
        float tiltRad = 0.0f;
        uint32_t trailCopies = 0;
        bool isBillboard = false;
        NeonParticleShape shape = NeonParticleShape::Triangle;
        float endRadius = -1.0f;
        Vector4 color{1.0f, 0.4f, 1.0f, 1.0f};
    };
    // GPUに送るパーティクル1粒のデータ
    /// @brief GPU計算で保持する1粒子の状態を表す。HLSL側の構造と合わせる。
    struct ParticleGPU {
        Vector3 position;
        float currentTime;
        Vector3 velocity;
        float lifeTime;
        Vector3 acceleration;
        float startScale;
        Vector4 startColor;
        Vector4 endColor;
        float endScale;
        uint32_t isActive;
        uint32_t easingType;
        uint32_t isBillboard;
        Vector3 rotate;
        float padding1;
        Vector3 angularVelocity;
        float padding2;
    };
    static_assert(sizeof(ParticleGPU) == 128, "ParticleGPU must match ParticleCompute.hlsli");
    static_assert(offsetof(ParticleGPU, padding1) == 108, "neonRadiance uses the existing padding1 slot");
    static_assert(offsetof(ParticleGPU, padding2) == 124, "neonShape uses the existing padding2 slot");

    /// @brief 1粒子の姿勢・速度・色・寿命などの状態を表す。
    struct Particle {
        Transform transform;
        Vector3 startScaleVector;
        Vector3 endScaleVector;
        Vector3 velocity;
        Vector3 acceleration;
        Vector3 angularVelocity;
        float lifeTime;
        float currentTime;
        float startScale;
        float endScale;
        Vector4 startColor;
        Vector4 endColor;
        EasingType easingType;
        bool isBillboard;
        bool alignToVelocity;
        bool neonRadiance = false;
        NeonParticleShape neonShape = NeonParticleShape::Triangle;
    };

    // シェーダー用定数バッファ構造体
    /// @brief モデルを使う粒子の姿勢と頂点変換情報をGPUへ渡す。
    struct ModelParticleTransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
        Vector4 color;
    };
    static_assert(sizeof(ModelParticleTransformationMatrix) == 208, "ModelParticle RenderData stride must remain unchanged");

    /// @brief cg2::ParticleManagerで使う設定値をまとめる。生成・更新される実行状態とは分けて扱う。
    struct GlobalConfig {
        float deltaTime;
        uint32_t maxParticles;
        float time;
        uint32_t itemCount;
    };
    /// @brief cg2::ParticleManagerで使う設定値をまとめる。生成・更新される実行状態とは分けて扱う。
    struct SceneConfig {
        Matrix4x4 viewProjection;
        Vector3 cameraPosition;
        float scenePadding;
    };

    // ParticleEmit.CS.hlsl と同じ128-byteレイアウト。
    /// @brief GPUの粒子発生処理へ渡す1回分の要求を表す。
    struct GpuEmitterRequest {
        Vector3 position;
        float radius;
        Vector3 boxSize;
        uint32_t shape;
        float speedMin;
        float speedMax;
        float lifeTimeMin;
        float lifeTimeMax;
        Vector3 acceleration;
        float startScaleMin;
        float startScaleMax;
        float endScaleMin;
        float endScaleMax;
        uint32_t easingType;
        Vector4 startColor;
        Vector4 endColor;
        uint32_t isBillboard;
        float seed;
        float padding[2];
    };
    static_assert(sizeof(GpuEmitterRequest) == 128, "GpuEmitterRequest must match ParticleEmit.CS.hlsl");

    /// @brief 共有インスタンスを返す。呼び出し側は取得したポインターをdeleteしない。
    static ParticleManager* GetInstance();
    static const uint32_t kMaxInstance = 100000; // 実行環境に合わせて調整

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime, Camera* camera, DebugCamera* debugCamera = nullptr);
    /// @brief 設定したGPU計算のスレッドグループを実行する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Dispatch(float deltaTime, Camera* camera);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();

    /// @brief 演出を登録する。
    void RegisterEffect(const std::string& effectName, const ParticleEmitterConfig& config);
    /// @brief 演出を登録する。
    void RegisterEffect(const std::string& effectName, const std::string& jsonPath);
    /// @brief 演出を保存する。
    void SaveEffect(const std::string& effectName, const std::string& jsonPath) const;
    /// @brief 設定に従って演出を発生させる。
    void Emit(const std::string& effectName, const Vector3& position, uint32_t count);
    /// @brief 命中演出を発生させる。
    void EmitHitEffect(const Vector3& position);
    /// @brief ネオン死亡演出を発生させる。
    void EmitNeonDeathEffect(const Vector3& position, const Vector4& primaryColor, const Vector4& secondaryColor, float strength = 1.0f);
    /// @brief ネオン衝撃演出を発生させる。
    void EmitNeonImpactEffect(const Vector3& position, const Vector3& impactNormal, const Vector4& color, uint32_t count = 10);
    /// @brief ネオン移動演出を発生させる。
    void EmitNeonMovementEffect(const Vector3& position, const Vector3& movementDirection);
    /// @brief 画面パルスイベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<ScreenPulseEvent> ConsumeScreenPulseEvents();
    /// @brief ネオン三角形イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<NeonTriangleEvent> ConsumeNeonTriangleEvents();
    /// @brief 設定に従って演出を発生させる。
    void Emit(const cg2::Particle& particle);
    /// @brief ImGUI編集画面を描画する。
    void DrawImGuiEditor();
    /// @brief 有効件数を返す。
    uint32_t GetActiveCount() const;
    /// @brief 描画可能粒子が存在するか判定する。
    bool HasDrawableParticles() const;
    /// @brief 使用GPU更新を設定する。
    void SetUseGpuUpdate(bool useGpuUpdate);
    /// @brief ネオン三角形演出方式を設定する。
    void SetNeonTriangleEffectMode(NeonTriangleEffectMode mode)
    {
        neonTriangleEffectMode_ = mode;
    }
    /// @brief ネオン三角形演出方式を返す。
    NeonTriangleEffectMode GetNeonTriangleEffectMode() const
    {
        return neonTriangleEffectMode_;
    }
    /// @brief 使用GPU更新であるか判定する。
    bool IsUseGpuUpdate() const
    {
        return useGpuUpdate_;
    }
    // Keep particle count / silhouette changes from consuming gameplay RNG.
    void SetPresentationSeed(uint32_t seed) { presentationRandom_.seed(seed); }

private:
    float Rand(float min, float max);
    Vector3 Rand(const Vector3& min, const Vector3& max);
    Vector3 RandomUnitVector();
    std::mt19937 presentationRandom_{std::random_device{}()};
    /// @brief 発生中の粒子に必要な状態と設定の参照を保持する。
    struct ActiveParticle {
        Particle particle;
    };

    /// @brief インスタンスの初期値と利用先を設定する。
    ParticleManager() = default;
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~ParticleManager() = default;
    /// @brief Easingを現在の状態へ適用する。
    float ApplyEasing(float t, EasingType type) const;
    /// @brief 係数を0〜1に制限し、2つのRGBA色を線形補間して返す。
    Vector4 LerpColor(const Vector4& a, const Vector4& b, float t) const;
    /// @brief ベクトル3を線形補間する。
    Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) const;
    /// @brief ビルボード行列を作成して返す。
    Matrix4x4 MakeBillboardMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate, Camera* camera,
                                  DebugCamera* debugCamera) const;
    /// @brief 既定値演出を生成する。
    void CreateDefaultEffects();
    /// @brief GPUをGPU計算を実行する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void DispatchGpu(float deltaTime, const Matrix4x4& viewProjection, const Vector3& cameraPosition);
    /// @brief GPU発生源をGPU計算を実行する。
    void DispatchGpuEmitters();
    /// @brief GPU粒子を初期化する。
    void InitializeGpuParticles();
    /// @brief GPU発生源を後で処理するために予約する。
    void QueueGpuEmitter(const ParticleEmitterConfig& config, const Vector3& position, uint32_t count);
    /// @brief UavBarrierを追加する。
    void AddUavBarrier(std::initializer_list<ID3D12Resource*> resources);
    /// @brief GPUリソースを次の利用に必要な状態へ遷移させる。
    void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    /// @brief 描画引数を初期状態へ戻す。
    void ResetDrawArgs();
    /// @brief 一括処理を発生させる。
    void EmitBatch(const std::vector<Particle>& particles);
    /// @brief 粒子を作成して返す。
    Particle MakeParticle(const ParticleEmitterConfig& config);
    /// @brief ネオン死亡破裂Nowを発生させる。
    void EmitNeonDeathBurstNow(const Vector3& position, const Vector4& primaryColor, const Vector4& secondaryColor, float strength);
    /// @brief 予約中死亡Burstsを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePendingDeathBursts(float deltaTime);

    /// @brief 粒子の消滅時に発生させる演出を後段へ予約する。
    struct PendingDeathBurst {
        Vector3 position{};
        Vector4 primaryColor{};
        Vector4 secondaryColor{};
        float strength = 1.0f;
        float timer = 0.28f;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::map<std::string, ParticleEmitterConfig> effectLibrary_;
    std::vector<PendingDeathBurst> pendingDeathBursts_;
    std::vector<ScreenPulseEvent> screenPulseEvents_;
    std::vector<NeonTriangleEvent> neonTriangleEvents_;
    NeonTriangleEffectMode neonTriangleEffectMode_ = NeonTriangleEffectMode::Outline;
    Model* model_ = nullptr;
    // The procedural contour owns a padded quad; shared OBJ assets stay unchanged.
    std::unique_ptr<ModelCommon> contourModelCommon_;
    std::unique_ptr<Model> contourModel_;
    std::vector<ActiveParticle> activeParticles_;
    uint32_t instanceCount_ = 0;

    // リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ModelParticleTransformationMatrix* instancingData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuInstancingResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    Material* materialData_ = nullptr;
    DirectionalLight* directionalLightData_ = nullptr;
    CameraData* cameraData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> drawArgsResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> aliveIndicesResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> computeConfigResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> computeSceneResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> initializeConfigResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterConfigResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> batchConfigResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterRequestResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> emitStagingResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> resetResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> drawArgsInitResource_;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;

    uint32_t srvIndexInstancing_ = 0;
    uint32_t srvIndexGpuInstancing_ = 0;
    uint32_t uavIndexParticles_ = 0;
    uint32_t uavIndexFreeListIndex_ = 0;
    uint32_t uavIndexFreeList_ = 0;
    uint32_t uavIndexRenderData_ = 0;
    uint32_t uavIndexAliveIndices_ = 0;
    uint32_t uavIndexDrawArgs_ = 0;
    uint32_t srvIndexEmitterRequests_ = 0;
    uint32_t srvIndexPrebuiltParticles_ = 0;

    std::vector<GpuEmitterRequest> pendingGpuEmitters_;
    std::vector<ParticleGPU> pendingGpuParticles_;
    float gpuTime_ = 0.0f;
    uint32_t emitterSeed_ = 1;
    static constexpr uint32_t kMaxEmitRequests = 16384;
    std::string editorEffectName_ = "HitSpark";
    ParticleEmitterConfig editorConfig_;
    bool useGpuUpdate_ = true;
    bool gpuDrawReady_ = false;
};

} // namespace cg2
