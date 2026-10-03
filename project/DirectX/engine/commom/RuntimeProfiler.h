#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

namespace cg2 {

class DirectXCommon;

// Optional diagnostics. GPU samples use the existing frame fence, never an extra wait.
/// @brief CPU区間とGPU区間の所要時間を収集し、実行時の性能表示へ提供する。
class RuntimeProfiler {
public:
    /// @brief 保持している値またはリソースを返す。
    static RuntimeProfiler& Get();
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dx);
    /// @brief 利用を終了し、保持している資源と計測状態を解放する。
    void Shutdown();
    /// @brief Allowedであるか判定する。
    bool IsAllowed() const
    {
        return allowed_;
    }
    /// @brief 表示中であるか判定する。
    bool IsVisible() const
    {
        return displayMode_ != 0;
    }
    /// @brief 記録中であるか判定する。
    bool IsRecording() const
    {
        return recording_;
    }
    /// @brief 開発機能を切り替えるショートカット入力を処理する。
    void HandleShortcut(bool shift);
    /// @brief フレームを開始する。
    void BeginFrame();
    /// @brief Cpuを追加する。
    void AddCpu(const char* name, double ms);
    /// @brief カウンターを設定する。
    void SetCounter(const char* name, double value);
    /// @brief GPUを開始する。
    int BeginGpu(const char* name);
    /// @brief GPUを終了する。
    void EndGpu(int token);
    /// @brief GPUを解決する。
    void ResolveGpu();
    /// @brief 1フレーム分の計測を終了し、表示用の結果を整える。
    void FinishFrame(double frameMs, double presentMs, double fenceMs, double limitMs);
    /// @brief 重ね表示を描画する。
    void DrawOverlay(bool limitEnabled, const char* scene);
    /// @brief 計測完了であるか判定する。
    bool IsCaptureComplete() const
    {
        return captureCompleted_;
    }
    bool IsCaptureActive() const { return captureFrames_ > 0 && !captureCompleted_; }
    // Developer UIから既存timestamp計測を再開する。無効環境では副作用なくfalse。
    bool StartCapture(const std::string& path, int frames = 300, int warmupFrames = 60);

    /// @brief スコープの開始・終了をCPU計測へ記録するRAIIオブジェクトを表す。
    class CpuScope {
    public:
        /// @brief インスタンスの初期値と利用先を設定する。
        explicit CpuScope(const char* name) : name_(name), enabled_(Get().IsRecording())
        {
            if (enabled_)
                start_ = Clock::now();
        }
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        ~CpuScope()
        {
            if (enabled_)
                Get().AddCpu(name_, std::chrono::duration<double, std::milli>(Clock::now() - start_).count());
        }
        CpuScope(const CpuScope&) = delete;
        CpuScope& operator=(const CpuScope&) = delete;

    private:
        using Clock = std::chrono::steady_clock;
        const char* name_;
        bool enabled_;
        Clock::time_point start_{};
    };
    /// @brief GPU計測の開始・終了を対応させるRAIIオブジェクトを表す。
    class GpuScope {
    public:
        /// @brief インスタンスの初期値と利用先を設定する。
        explicit GpuScope(const char* name) : token_(Get().BeginGpu(name)) {}
        /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
        ~GpuScope()
        {
            Get().EndGpu(token_);
        }
        GpuScope(const GpuScope&) = delete;
        GpuScope& operator=(const GpuScope&) = delete;

    private:
        int token_;
    };

private:
    /// @brief 1つの性能項目の時間・件数などの集計値を保持する。
    struct Metric {
        std::string name;
        double sum = 0, latest = 0, average = 0;
        bool seen = false;
    };
    /// @brief GPUのタイムスタンプ計測に必要な問い合わせ情報を保持する。
    struct GpuSample {
        const char* name = nullptr;
        bool ended = false;
    };
    /// @brief 指定した値や要素を加える。
    static void Add(std::vector<Metric>& metrics, const char* name, double value);
    /// @brief ウィンドウを予約分を処理する。
    void FlushWindow();
    /// @brief 計測Rowを書き込む。
    void WriteCaptureRow();
    static constexpr unsigned kMaxGpuScopes = 64;
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    std::array<GpuSample, kMaxGpuScopes> gpuSamples_{};
    std::vector<Metric> cpu_, gpu_, counters_;
    UINT64 frequency_ = 0, resolveFence_ = 0, beginFence_ = 0;
    unsigned gpuCount_ = 0;
    bool allowed_ = true, recording_ = false, resolved_ = false, gpuValid_ = false;
    int displayMode_ = 0, page_ = 0;
    unsigned windowFrames_ = 0, windowGpuFrames_ = 0;
    bool shownGpuValid_ = false;
    double windowMs_ = 0, maxFrameMs_ = 0, shownMaxMs_ = 0;
    double fps_ = 0, frameMs_ = 0, presentMs_ = 0, fenceMs_ = 0, limitMs_ = 0, cpuMs_ = 0;
    double frameSum_ = 0, presentSum_ = 0, fenceSum_ = 0, limitSum_ = 0, cpuSum_ = 0;
    std::array<float, 120> frameHistory_{};
    int historyCursor_ = 0;
    int captureFrames_ = 0, captureWarmup_ = 60, captureWritten_ = 0;
    bool captureCompleted_ = false;
    std::ofstream capture_;
};

} // namespace cg2
