#include "RuntimeProfiler.h"
#include "DeveloperTools.h"
#include "StartupTrace.h"
#include "DirectXCommon.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>

namespace cg2 {

namespace {
std::string Environment(const char* name) {
    char value[2048]{};
    const DWORD count=GetEnvironmentVariableA(name,value,sizeof(value));
    return count>0 && count<sizeof(value) ? std::string(value,count) : std::string{};
}
int EnvironmentInt(const char* name,int fallback,int maximum) {
    const auto value=Environment(name);
    if(value.empty()) return fallback;
    try { return (std::clamp)(std::stoi(value),0,maximum); } catch(...) { return fallback; }
}
const char* DisplayName(const std::string& name) {
    static constexpr std::pair<const char*,const char*> names[]={
        {"GPU frame","GPUフレーム全体"},{"Scene 3D (includes effects)","3D全体 (個別発光を含む)"},
        {"Global Bloom / Post","全画面ブルーム / ポスト"},{"After Post / Text Glow","ポスト後 / 文字の発光"},
        {"2D / Game UI","2D / ゲームUI"},{"Diagnostics UI","性能表示 / 開発UI"},
        {"Player / Drones Update","プレイヤー / ドローン更新"},{"Enemy AI Update","敵AI更新"},
        {"Bullets / Trails / Wall Collision","弾 / 軌跡更新・壁判定"},{"Actor / Bullet Collision","機体 / 弾の衝突判定"},
        {"Particles Update","パーティクル更新"},{"Scene Update (total)","シーン更新全体"},
        {"Engine Update","エンジン共通更新"},{"Draw Record (total)","描画命令作成 全体"},
        {"Global Bloom record","全画面ブルーム 命令作成"},{"2D / UI record","2D / UI 命令作成"},
        {"Grid Post","グリッド発光"},{"Actor Neon Fill","機体ネオン本体"},
        {"Base Objects","本体 / 通常オブジェクト"},{"Stage Block Neon","ステージ枠のネオン"},
        {"Trail Post","弾・軌跡と個別発光"},{"Trail Draw","軌跡描画"},
        {"Shared Glow","オブジェクト共通発光"},{"Particle Glow","パーティクル発光"},
        {"Particles","パーティクル描画"},{"Bullets: player","自機の弾数"},{"Bullets: enemy","敵の弾数"},
        {"Enemies","敵数"},{"Particle count","パーティクル数"},{"Trails (drawable)","描画対象の軌跡数"},{"Trail vertices","軌跡の頂点数"},
        {"Trail draw calls","軌跡の描画呼び出し"},{"Trail upload bytes","軌跡転送量 (bytes)"},
        {"Trail truncated vertices","上限超過で省略した頂点"},
    };
    for(const auto& item:names) if(name==item.first) return item.second;
    return name.c_str();
}
}

RuntimeProfiler& RuntimeProfiler::Get() { static RuntimeProfiler instance; return instance; }

void RuntimeProfiler::Initialize(DirectXCommon* dx) {
    Shutdown();
    captureCompleted_=false;captureWritten_=0;displayMode_=0;page_=0;
    cpu_.clear();gpu_.clear();counters_.clear();
    fps_=0;windowFrames_=0;windowGpuFrames_=0;shownGpuValid_=false;windowMs_=0;maxFrameMs_=0;shownMaxMs_=0;
    frameSum_=presentSum_=fenceSum_=limitSum_=cpuSum_=0;frameHistory_.fill(0);historyCursor_=0;
    dx_=dx;
    allowed_=cg2::kDeveloperTools&&Environment("CG2_PERF_DISABLED")!="1";
    StartupTrace::Count("ui.runtime_profiler_allowed", allowed_ ? 1 : 0);
    displayMode_=allowed_ ? EnvironmentInt("CG2_PERF_OVERLAY",0,2) : 0;
    captureFrames_=allowed_ ? EnvironmentInt("CG2_PERF_CAPTURE_FRAMES",0,36000) : 0;
    captureWarmup_=EnvironmentInt("CG2_PERF_CAPTURE_WARMUP",60,36000);
    cpu_.reserve(48);gpu_.reserve(kMaxGpuScopes);counters_.reserve(16);
    if(!allowed_) return;
    D3D12_QUERY_HEAP_DESC query{};
    query.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;query.Count=kMaxGpuScopes*2;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=sizeof(UINT64)*query.Count;
    buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;
    buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(FAILED(dx_->GetQueue()->GetTimestampFrequency(&frequency_)) || frequency_==0 ||
       FAILED(dx_->GetDevice()->CreateQueryHeap(&query,IID_PPV_ARGS(&queryHeap_))) ||
       FAILED(dx_->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,
           D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback_)))) {
        queryHeap_.Reset();readback_.Reset();frequency_=0;
    }
    if(captureFrames_>0) {
        auto path=Environment("CG2_PERF_CAPTURE_PATH");
        if(path.empty()) path="generated/performance/session_"+std::to_string(GetCurrentProcessId())+".csv";
        std::error_code error;
        const auto parent=std::filesystem::path(path).parent_path();
        if(!parent.empty()) std::filesystem::create_directories(parent,error);
        capture_.open(path);
        if(capture_) capture_<<"frame,category,name,value\n";
        else captureFrames_=0;
    }
}

void RuntimeProfiler::Shutdown() {
    capture_.close();recording_=false;queryHeap_.Reset();readback_.Reset();dx_=nullptr;frequency_=0;
}

void RuntimeProfiler::HandleShortcut(bool shift) {
    if(!allowed_) return;
    if(shift) { page_=(page_+1)%4;displayMode_=2; }
    else displayMode_=(displayMode_+1)%3;
}

void RuntimeProfiler::BeginFrame() {
    const bool enabled=allowed_ && (displayMode_!=0 || (captureFrames_>0 && !captureCompleted_));
    if(enabled && !recording_) {
        windowFrames_=0;windowGpuFrames_=0;shownGpuValid_=false;windowMs_=0;frameSum_=presentSum_=fenceSum_=limitSum_=cpuSum_=0;maxFrameMs_=0;
        cpu_.clear();gpu_.clear();counters_.clear();frameHistory_.fill(0);historyCursor_=0;
    }
    recording_=enabled;
    gpuCount_=0;resolved_=false;gpuValid_=false;
    if(!recording_) return;
    beginFence_=dx_->GetFenceValue();
    for(auto* group:{&cpu_,&gpu_,&counters_}) for(auto& metric:*group) {metric.latest=0;metric.seen=false;}
}

void RuntimeProfiler::Add(std::vector<Metric>& metrics,const char* name,double value) {
    if(!std::isfinite(value)||value<0) return;
    for(auto& metric:metrics) if(metric.name==name) {metric.latest+=value;metric.seen=true;return;}
    metrics.push_back({name,0,value,0,true});
}
void RuntimeProfiler::AddCpu(const char* name,double ms) {if(recording_) Add(cpu_,name,ms);}
void RuntimeProfiler::SetCounter(const char* name,double value) {if(recording_) Add(counters_,name,value);}

int RuntimeProfiler::BeginGpu(const char* name) {
    if(!recording_||!queryHeap_||gpuCount_>=kMaxGpuScopes||resolved_) return -1;
    const unsigned index=gpuCount_++;
    gpuSamples_[index]={name,false};
    dx_->GetList()->EndQuery(queryHeap_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,index*2);
    return static_cast<int>(index);
}
void RuntimeProfiler::EndGpu(int token) {
    if(token<0||static_cast<unsigned>(token)>=gpuCount_||resolved_||gpuSamples_[token].ended) return;
    dx_->GetList()->EndQuery(queryHeap_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,static_cast<UINT>(token)*2+1);
    gpuSamples_[token].ended=true;
}
void RuntimeProfiler::ResolveGpu() {
    if(!recording_||!queryHeap_||gpuCount_==0) return;
    // Resolve only pairs with both timestamps written, including on an early-return path.
    for(unsigned index=0;index<gpuCount_;++index) if(gpuSamples_[index].ended) {
        dx_->GetList()->ResolveQueryData(queryHeap_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,index*2,2,
            readback_.Get(),sizeof(UINT64)*index*2);
    }
    resolveFence_=dx_->GetFenceValue()+1;resolved_=true;
}

void RuntimeProfiler::FinishFrame(double frameMs,double presentMs,double fenceMs,double limitMs) {
    if(!recording_) return;
    // Resource loading can submit midway through Update. Exclude such frames:
    // their timestamp span includes CPU idle gaps between command submissions.
    if(resolved_ && resolveFence_==beginFence_+1 && dx_->GetFence()->GetCompletedValue()>=resolveFence_) {
        UINT64* ticks=nullptr;
        D3D12_RANGE range{0,sizeof(UINT64)*gpuCount_*2};
        if(SUCCEEDED(readback_->Map(0,&range,reinterpret_cast<void**>(&ticks)))) {
            for(unsigned index=0;index<gpuCount_;++index) if(gpuSamples_[index].ended && ticks[index*2+1]>=ticks[index*2]) {
                Add(gpu_,gpuSamples_[index].name,static_cast<double>(ticks[index*2+1]-ticks[index*2])*1000.0/static_cast<double>(frequency_));
            }
            D3D12_RANGE written{0,0};readback_->Unmap(0,&written);gpuValid_=true;++windowGpuFrames_;
        }
    }
    frameMs_=frameMs;presentMs_=presentMs;fenceMs_=fenceMs;limitMs_=limitMs;
    cpuMs_=(std::max)(0.0,frameMs-presentMs-fenceMs-limitMs);
    frameHistory_[historyCursor_]=static_cast<float>(frameMs);historyCursor_=(historyCursor_+1)%static_cast<int>(frameHistory_.size());
    for(auto* group:{&cpu_,&gpu_,&counters_}) for(auto& metric:*group) metric.sum+=metric.latest;
    ++windowFrames_;windowMs_+=frameMs;maxFrameMs_=(std::max)(maxFrameMs_,frameMs);
    frameSum_+=frameMs;presentSum_+=presentMs;fenceSum_+=fenceMs;limitSum_+=limitMs;cpuSum_+=cpuMs_;
    if(captureFrames_>0&&!captureCompleted_) {
        if(captureWarmup_>0) --captureWarmup_;
        else WriteCaptureRow();
    }
    if(windowMs_>=250.0) FlushWindow();
}

void RuntimeProfiler::FlushWindow() {
    if(!windowFrames_) return;
    const double count=static_cast<double>(windowFrames_);
    fps_=windowMs_>0 ? count*1000.0/windowMs_ : 0;
    shownMaxMs_=maxFrameMs_;
    // Summary metrics are averaged with the same window as the detailed rows.
    for(auto* group:{&cpu_,&counters_}) for(auto& metric:*group) {metric.average=metric.sum/count;metric.sum=0;}
    shownGpuValid_=windowGpuFrames_>0;
    for(auto& metric:gpu_) {metric.average=windowGpuFrames_?metric.sum/static_cast<double>(windowGpuFrames_):0;metric.sum=0;}
    auto summary=[&](const char* name,double value) {
        for(auto& metric:cpu_) if(metric.name==name) {metric.average=value/count;return;}
        cpu_.push_back({name,0,0,value/count,false});
    };
    summary("CPU work estimate",cpuSum_);summary("Present wait",presentSum_);
    summary("GPU fence wait",fenceSum_);summary("60 FPS wait",limitSum_);summary("Frame elapsed",frameSum_);
    windowFrames_=0;windowGpuFrames_=0;windowMs_=0;frameSum_=presentSum_=fenceSum_=limitSum_=cpuSum_=0;maxFrameMs_=0;
}

void RuntimeProfiler::WriteCaptureRow() {
    if(!capture_) return;
    const int frame=++captureWritten_;
    auto row=[&](const char* category,const char* name,double value){capture_<<frame<<','<<category<<','<<name<<','<<std::setprecision(8)<<value<<'\n';};
    row("frame","elapsed_ms",frameMs_);row("frame","cpu_work_estimate_ms",cpuMs_);
    row("frame","present_ms",presentMs_);row("frame","gpu_fence_wait_ms",fenceMs_);row("frame","limiter_wait_ms",limitMs_);
    row("frame","gpu_valid",gpuValid_?1:0);
    for(const auto& metric:cpu_) if(metric.seen) row("cpu",metric.name.c_str(),metric.latest);
    if(gpuValid_) for(const auto& metric:gpu_) if(metric.seen) row("gpu",metric.name.c_str(),metric.latest);
    for(const auto& metric:counters_) if(metric.seen) row("count",metric.name.c_str(),metric.latest);
    if(captureWritten_>=captureFrames_) {captureCompleted_=true;capture_.close();}
}

void RuntimeProfiler::DrawOverlay(bool limitEnabled,const char* scene) {
    if(!allowed_||displayMode_==0) return;
    const bool detailed=displayMode_==2;
    ImGui::SetNextWindowPos(ImVec2(14,145),ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(detailed?700.0f:510.0f,detailed?548.0f:190.0f),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking;
    if(ImGui::Begin("Runtime performance",nullptr,flags)) {
        ImGui::Text("性能モニター   F1: 簡易 / 詳細 / OFF   Shift+F1: 項目");
        ImGui::Text("%.1f FPS   %.2f ms/frame   60FPS制御: %s",fps_,fps_>0?1000.0/fps_:0,limitEnabled?"ON":"OFF (診断用)");
        auto average=[&](const std::vector<Metric>& metrics,const char* name){for(const auto& metric:metrics)if(metric.name==name)return metric.average;return 0.0;};
        ImGui::Text("CPU処理(推定) %.2f ms   GPU描画 %s",average(cpu_,"CPU work estimate"),shownGpuValid_?"":"N/A");
        if(shownGpuValid_) {ImGui::SameLine();ImGui::Text("%.2f ms",average(gpu_,"GPU frame"));}
        ImGui::Text("Present %.2f / GPU待ち %.2f / 60FPS待ち %.2f ms",average(cpu_,"Present wait"),average(cpu_,"GPU fence wait"),average(cpu_,"60 FPS wait"));
        ImGui::PlotLines("##frame",frameHistory_.data(),static_cast<int>(frameHistory_.size()),historyCursor_,"16.67ms = 60FPS",0,40,ImVec2(-1,36));
        if(detailed) {
            ImGui::Separator();
            ImGui::Text("%s | %s | 直近250ms平均 / 最大 %.2fms",scene,page_==0?"CPU更新":page_==1?"GPU全体":page_==2?"描画パス":"描画量",shownMaxMs_);
            if(page_==0) ImGui::TextWrapped("CPUは更新と描画命令作成の時間。親項目は子項目を含むため、全行を合計しません。");
            if(page_==1) ImGui::TextWrapped("GPUの実測時間。Scene 3Dは個別発光を含み、Global Bloomは全画面ポスト処理です。");
            if(page_==2) ImGui::TextWrapped("各パスのCPU描画命令作成とGPU実行時間。Scene 3Dの内訳です。");
            if(page_==3) ImGui::TextWrapped("発光の強さや軌跡の寿命は維持。軌跡は複数本をまとめて描画します。");
            const auto& metrics=page_==0?cpu_:page_==3?counters_:gpu_;
            auto gpuParent=[](const std::string& name) {
                return name=="GPU frame"||name=="Scene 3D (includes effects)"||name=="Global Bloom / Post"||
                    name=="After Post / Text Glow"||name=="2D / Game UI"||name=="Diagnostics UI";
            };
            auto drawChild=[&](const std::string& name) {
                for(const auto& metric:gpu_) if(metric.name==name && !gpuParent(name)) return true;
                return false;
            };
            if(ImGui::BeginTable("metrics",page_==2?3:2,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("処理 / 項目",ImGuiTableColumnFlags_WidthStretch);
                if(page_==2) ImGui::TableSetupColumn("CPU ms",ImGuiTableColumnFlags_WidthFixed,100);
                ImGui::TableSetupColumn(page_==3?"値":page_==2?"GPU ms":"平均 ms",ImGuiTableColumnFlags_WidthFixed,110);
                ImGui::TableHeadersRow();
                for(const auto& metric:metrics) {
                    if(page_==0 && (metric.name=="CPU work estimate"||metric.name=="Present wait"||metric.name=="GPU fence wait"||metric.name=="60 FPS wait"||metric.name=="Frame elapsed")) continue;
                    if(page_==0 && drawChild(metric.name)) continue;
                    if(page_==1 && !gpuParent(metric.name)) continue;
                    if(page_==2 && gpuParent(metric.name)) continue;
                    ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(DisplayName(metric.name));
                    if(page_==2) {ImGui::TableNextColumn();ImGui::Text("%.3f",average(cpu_,metric.name.c_str()));}
                    ImGui::TableNextColumn();ImGui::Text(page_==3?"%.0f":"%.3f",metric.average);
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}

} // namespace cg2
