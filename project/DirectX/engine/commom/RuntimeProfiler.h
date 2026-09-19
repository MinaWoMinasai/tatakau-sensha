#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;

// Optional diagnostics. GPU samples use the existing frame fence, never an extra wait.
class RuntimeProfiler {
public:
    static RuntimeProfiler& Get();
    void Initialize(DirectXCommon* dx);
    void Shutdown();
    bool IsAllowed() const { return allowed_; }
    bool IsVisible() const { return displayMode_ != 0; }
    bool IsRecording() const { return recording_; }
    void HandleShortcut(bool shift, bool inkScene);
    void BeginFrame();
    void AddCpu(const char* name, double ms);
    void SetCounter(const char* name, double value);
    int BeginGpu(const char* name);
    void EndGpu(int token);
    void ResolveGpu();
    void FinishFrame(double frameMs, double presentMs, double fenceMs, double limitMs);
    void DrawOverlay(bool limitEnabled, const char* scene);
    bool IsCaptureComplete() const { return captureCompleted_; }

    class CpuScope {
    public:
        explicit CpuScope(const char* name) : name_(name), enabled_(Get().IsRecording()) {
            if (enabled_) start_ = Clock::now();
        }
        ~CpuScope() { if (enabled_) Get().AddCpu(name_, std::chrono::duration<double, std::milli>(Clock::now()-start_).count()); }
        CpuScope(const CpuScope&) = delete;
        CpuScope& operator=(const CpuScope&) = delete;
    private:
        using Clock=std::chrono::steady_clock;
        const char* name_;
        bool enabled_;
        Clock::time_point start_{};
    };
    class GpuScope {
    public:
        explicit GpuScope(const char* name) : token_(Get().BeginGpu(name)) {}
        ~GpuScope() { Get().EndGpu(token_); }
        GpuScope(const GpuScope&) = delete;
        GpuScope& operator=(const GpuScope&) = delete;
    private:
        int token_;
    };
private:
    struct Metric { std::string name; double sum=0, latest=0, average=0; bool seen=false; };
    struct GpuSample { const char* name=nullptr; bool ended=false; };
    static void Add(std::vector<Metric>& metrics, const char* name, double value);
    void FlushWindow();
    void WriteCaptureRow();
    static constexpr unsigned kMaxGpuScopes=64;
    DirectXCommon* dx_=nullptr;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    std::array<GpuSample,kMaxGpuScopes> gpuSamples_{};
    std::vector<Metric> cpu_,gpu_,counters_;
    UINT64 frequency_=0, resolveFence_=0, beginFence_=0;
    unsigned gpuCount_=0;
    bool allowed_=true, recording_=false, resolved_=false, gpuValid_=false;
    int displayMode_=0, page_=0;
    unsigned windowFrames_=0, windowGpuFrames_=0;
    bool shownGpuValid_=false;
    double windowMs_=0, maxFrameMs_=0, shownMaxMs_=0;
    double fps_=0, frameMs_=0, presentMs_=0, fenceMs_=0, limitMs_=0, cpuMs_=0;
    double frameSum_=0,presentSum_=0,fenceSum_=0,limitSum_=0,cpuSum_=0;
    std::array<float,120> frameHistory_{};
    int historyCursor_=0;
    int captureFrames_=0, captureWarmup_=60, captureWritten_=0;
    bool captureCompleted_=false;
    std::ofstream capture_;
};
