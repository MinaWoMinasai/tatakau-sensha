#include "DirectXCommon.h"
#include "ResourcePaths.h"

#include <cassert>
#include "Bloom.h"
#include "StartupTrace.h"
#include "Utf8Path.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace myEngine {

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp, bool titleFirst)
{
    StartupTrace::Scope startupScope("DirectXCommon.Initialize");
    assert(winApp);
    winApp_ = winApp;

    // FPS固定初期化
    InitializeFixFPS();

    {
        StartupTrace::Scope scope("DirectXCommon.Device");
        InitializeDevice();
    }
    InitializeCommand();
    CreateSwapChain();
    CreateDepthBuffer();
    CreateDescriptorHeap();
    CreateSwapChainRtv();
    InitializeDepthStencilView();
    InitializeFence();

    shaderCompiler_.Initialize();
    InitializeImGui();

    pipelines_.Initialize(this);
    if (titleFirst) {
        pipelines_.InitializeStep(); // スプライト用の3つのパイプラインだけ。
    } else {
        pipelines_.CreateAll();
    }
}

bool DirectXCommon::InitializePipelinesStep()
{
    return pipelines_.InitializeStep();
}

void DirectXCommon::PreDraw()
{
    // これから書き込むバックバッファのインデックスを取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    // TransitionBarrierの設定
    D3D12_RESOURCE_BARRIER barrier{};
    // 今回のバリアはTransition
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    // Noneにしておく
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    // バリアを張る対象のリソース。現在のバックバッファに対して行う
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    // 遷移前(現在)のResourceState
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    // 遷移後のResourceState
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // TransitionBarrierを張る
    list_->ResourceBarrier(1, &barrier);
    // 描画先のRTVとDSVを設定する
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    SetRenderTarget(rtvHandles_[backBufferIndex], dsvHandle);
    // 指定した色で画面全体をクリアする
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};

    list_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor, 0, nullptr);
    // 指定して深度で画面全体をクリアする
    list_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    SetViewport(WinApp::kRenderWidth, WinApp::kRenderHeight);
}

void DirectXCommon::PostDraw()
{
    // これから書き込むバックバッファのインデックスを取得
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
    // TransitionBarrierの設定
    D3D12_RESOURCE_BARRIER barrier{};
    // 今回のバリアはTransition
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    // Noneにしておく
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    // バリアを張る対象のリソース。現在のバックバッファに対して行う
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    // 遷移前(現在)のResourceState
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    // 遷移後のResourceState
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    // TranssitionBarrierを張る
    list_->ResourceBarrier(1, &barrier);

    CommandListExecuteAndReset();
}

void DirectXCommon::InitializeDevice()
{
    HRESULT hr;

    char debugLayerValue[8]{};
    const DWORD debugLayerValueLength =
        GetEnvironmentVariableA("MYENGINE_D3D12_DEBUG_LAYER", debugLayerValue, static_cast<DWORD>(std::size(debugLayerValue)));
    // 未設定と "1" は従来どおりON。明示的な "0" だけOFFにする。
    const bool debugLayerRequested = !(debugLayerValueLength == 1 && debugLayerValue[0] == '0');

    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
    if (debugLayerRequested && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        // デバッグレイヤーを有効化する
        debugController->EnableDebugLayer();
        d3d12DebugLayerEnabled_ = true;
        // GPU-Based Validation は描画命令数に比例して極端に重くなるため、必要時のみ有効化する。
        char gpuValidationValue[8]{};
        const DWORD valueLength =
            GetEnvironmentVariableA("MYENGINE_GPU_BASED_VALIDATION", gpuValidationValue, static_cast<DWORD>(std::size(gpuValidationValue)));
        gpuBasedValidationEnabled_ = valueLength > 0 && gpuValidationValue[0] == '1';
        debugController->SetEnableGPUBasedValidation(gpuBasedValidationEnabled_ ? TRUE : FALSE);
    }

    hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // 良い順にアダプタを組む
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) !=
                     DXGI_ERROR_NOT_FOUND;
         ++i) {
        // アダプターの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter_->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr)); // 取得できないのは一大事
        // ソフトウェアアダプタでなければ採用!
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            // 採用したアダプタの情報をログに出力。wstringのほうなので注意

            break;
        }
        useAdapter_ = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
    }
    // 適切なアダプタが見当たらなかったので起動できない
    assert(useAdapter_ != nullptr);

    // 機能レベルとログ出力用の文字列
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
    const char* featrueLevelStrings[] = {"12.2", "12.1", "12.0"};
    // 高い順に生成できるか試していく
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        // 採用したアダプターでデバイスを生成
        hr = D3D12CreateDevice(useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
        // 指定した機能レベルでデバイスを生成できたかを確認する
        if (SUCCEEDED(hr)) {
            // 生成できたのでループを抜ける

            break;
        }
    }
    // デバイスの生成が上手くいかなかったので起動できない
    assert(device_ != nullptr);

#ifdef _DEBUG

    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        // やばいエラー時にとまる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時にとまる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時にとまる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        // 抑制するメッセージのID
        D3D12_MESSAGE_ID denyIds[] = {// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
                                      // https://stackoverflow.com/qiestions/69805245/direct-12-application-is-crashing-in-windows-11
                                      D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE

        };

        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);
    }
#endif
}

void DirectXCommon::InitializeCommand()
{
    HRESULT hr;

    // コマンドキューを生成する
    hr = device_->CreateCommandQueue(&queueDesc_, IID_PPV_ARGS(&queue_));
    // コマンドキューの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドアロケータを生成する
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_));
    // コマンドアロケータの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));

    // コマンドリストを生成する
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&list_));
    // コマンドリストの生成が上手くいかなかったので起動できない
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain()
{
    swapChainDesc_.Width = WinApp::kRenderWidth;                  // 画面の幅。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc_.Height = WinApp::kRenderHeight;                // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
    swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;           // 色の形式
    swapChainDesc_.SampleDesc.Count = 1;                          // マルチサンプルしない
    swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
    swapChainDesc_.BufferCount = 2;                               // ダブルバッファ
    swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;    // モニタをうつしたら、中身を破棄

    // コマンドキュー、ウィンドウハンドル、設定を渡して生成する
    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(queue_.Get(), winApp_->GetHwnd(), &swapChainDesc_, nullptr, nullptr,
                                                      reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    dxgiFactory_->MakeWindowAssociation(winApp_->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
}

void DirectXCommon::CreateDepthBuffer()
{
    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kRenderWidth;                    // テクスチャの幅
    resourceDesc.Height = WinApp::kRenderHeight;                  // テクスチャの高さ
    resourceDesc.MipLevels = 1;                                   // mipmapの数
    resourceDesc.DepthOrArraySize = 1;                            // 奥行き or 配列Textureの配列数
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;          // TextureのFormat
    resourceDesc.SampleDesc.Count = 1;                            // サンプリングカウント。1固定。
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

    // 利用するHeapの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る
    // 深度値のクリア設定
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;              // 1.0f(最大値)でクリア
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceとあわせる

    // Resourceの生成
    HRESULT hr = device_->CreateCommittedResource(&heapProperties,                       // Heapの設定
                                                  D3D12_HEAP_FLAG_NONE,                  // Heapの特殊な設定。特になし。
                                                  &resourceDesc,                         // Resourceの設定
                                                  D3D12_RESOURCE_STATE_DEPTH_WRITE,      // 初回のResourceState。Textureは基本読むだけ
                                                  &depthClearValue,                      // Clear最適地。
                                                  IID_PPV_ARGS(&depthStencilResource_)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDescriptorHeap()
{
    descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleは false
    rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    // DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleは false
    dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kMaxDsvCount, false);
}

void DirectXCommon::CreateSwapChainRtv()
{
    // SwapChainからResourceを引っ張ってくる
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
    // 上手く取得できなければ起動できない
    assert(SUCCEEDED(hr));
    hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
    assert(SUCCEEDED(hr));

    // RTVの設定
    rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;      //
    rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; //
    // ディスクリプタの先頭を取得する
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    //RTVを2つつくるのでディスクリプタを2つ用意
    // まず1つ目をつくる。1つ目は最初の所に作る。作る場所を指定してあげる必要がある
    rtvHandles_[0] = rtvStartHandle;
    device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc_, rtvHandles_[0]);
    // 2つ目のディスクリプタハンドルを得る
    rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    // 2つ目を作る
    device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc_, rtvHandles_[1]);

    rtvHandles_[0] = rtvStartHandle;
    rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void DirectXCommon::InitializeDepthStencilView()
{
    // DSVの設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;        // Format。基本的にはResourceに合わせる
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2dTexture
    // DSVHeapの先頭にDSVをつくる
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    // DSVヒープの先頭ハンドルを取得して保持
    dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 保持したハンドルに対して作成
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);
    dsvHeapIndex_ = 1;
}

void DirectXCommon::InitializeFence()
{
    // 初期値0でFenceを作る
    HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    // FenceのSignalを持つためのイベントを作成する
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeViewport()
{
    //クライアント領域のサイズと一緒にして画面全体に表示
    viewportRect_.Width = FLOAT(WinApp::kRenderWidth);
    viewportRect_.Height = FLOAT(WinApp::kRenderHeight);
    viewportRect_.TopLeftX = 0;
    viewportRect_.TopLeftY = 0;
    viewportRect_.MinDepth = 0.0f;
    viewportRect_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect()
{
    //基本的にビューポートと同じ矩形が構成されるようにする
    scissorRect_.left = 0;
    scissorRect_.right = LONG(WinApp::kRenderWidth);
    scissorRect_.top = 0;
    scissorRect_.bottom = LONG(WinApp::kRenderHeight);
}

void DirectXCommon::InitializeImGui()
{
    // 互換用の空の窓口。実際の初期化はGame::InitializeImGuiが担当する。
}

void DirectXCommon::CommandListExecuteAndReset()
{
    using Clock = std::chrono::steady_clock;
    auto elapsedMs = [](Clock::time_point start, Clock::time_point end) {
        return std::chrono::duration<float, std::milli>(end - start).count();
    };
    frameSubmitProfile_ = {};
    const auto totalStart = Clock::now();

    // コマンドリストの内容を確定させるすべてのコマンドを積んでからCloseすること
    const auto closeStart = Clock::now();
    HRESULT hr = list_->Close();
    assert(SUCCEEDED(hr));
    const auto closeEnd = Clock::now();
    frameSubmitProfile_.closeMs = elapsedMs(closeStart, closeEnd);

    // GPUにコマンドリストの実行を行わせる
    ID3D12CommandList* commandLists[] = {list_.Get()};
    const auto executeStart = Clock::now();
    queue_->ExecuteCommandLists(1, commandLists);
    const auto executeEnd = Clock::now();
    frameSubmitProfile_.executeMs = elapsedMs(executeStart, executeEnd);
    //GPUとOSに画面の交換を行うよう通知する
    const auto presentStart = Clock::now();
    swapChain_->Present(0, 0);
    const auto presentEnd = Clock::now();
    frameSubmitProfile_.presentMs = elapsedMs(presentStart, presentEnd);
    // fenceの値を更新
    fenceValue_++;
    // GPUがここまでたどり着いたときに、Fenceの値を指定して値に代入するようにSignalを送る
    queue_->Signal(fence_.Get(), fenceValue_);
    // Fenceの値が指定したSignal値にたどり着いているか確認する
    // GetCompletedValueの初期値はFence作成時に渡した初期値
    if (fence_->GetCompletedValue() < fenceValue_) {
        // 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        // イベント待つ
        const auto fenceStart = Clock::now();
        WaitForSingleObject(fenceEvent_, INFINITE);
        frameSubmitProfile_.fenceWaitMs = elapsedMs(fenceStart, Clock::now());
    }
    releaseAfterExecution_.clear();

    // FPS固定
    const auto fpsLimitStart = Clock::now();
    UpdateFixFPS();
    frameSubmitProfile_.fpsLimitMs = elapsedMs(fpsLimitStart, Clock::now());

    // 次のフレーム用のコマンドリストを準備
    const auto resetStart = Clock::now();
    hr = allocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = list_->Reset(allocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
    const auto totalEnd = Clock::now();
    frameSubmitProfile_.resetMs = elapsedMs(resetStart, totalEnd);
    frameSubmitProfile_.totalMs = elapsedMs(totalStart, totalEnd);
}

void DirectXCommon::ExecuteCommandListAndWait()
{
    // Close
    HRESULT hr = list_->Close();
    assert(SUCCEEDED(hr));

    // 実行
    ID3D12CommandList* lists[] = {list_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    // Fence
    fenceValue_++;
    hr = queue_->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));
    if (fence_->GetCompletedValue() < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        assert(SUCCEEDED(hr));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
    releaseAfterExecution_.clear();

    // Reset
    hr = allocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = list_->Reset(allocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile)
{
    return shaderCompiler_.Compile(filePath, profile);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
                                                                                 bool shaderVisible)
{
    // ディスクリプタヒープの生成
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;                 // レンダーターゲットビュー用
    descriptorHeapDesc.NumDescriptors = numDescriptors; // ダブルバッファ用に2つ。多くても別に構わない
    descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // シェーダーからアクセスできるようにする
    HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap)); //
    // ディスクリプタヒープが作れなかったので起動できない
    assert(SUCCEEDED(hr));

    return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format,
                                                                            D3D12_RESOURCE_FLAGS flags, const D3D12_CLEAR_VALUE* clearValue,
                                                                            D3D12_RESOURCE_STATES initialState)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;

    ComPtr<ID3D12Resource> resource;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr =
        device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initialState, clearValue, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

void DirectXCommon::SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    currentRtvHandle_ = rtvHandle;
    currentRtvHandles_ = {rtvHandle, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = dsvHandle;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
}

void DirectXCommon::SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                                     D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] = {
        colorRtvHandle,
        normalRtvHandle,
    };
    currentRtvHandle_ = colorRtvHandle;
    currentRtvHandles_ = {colorRtvHandle, normalRtvHandle, {}};
    currentRtvCount_ = 2;
    currentDsvHandle_ = dsvHandle;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, false, &dsvHandle);
}

void DirectXCommon::SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE colorRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE normalRtvHandle,
                                     D3D12_CPU_DESCRIPTOR_HANDLE materialRtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] = {
        colorRtvHandle,
        normalRtvHandle,
        materialRtvHandle,
    };
    currentRtvHandle_ = colorRtvHandle;
    currentRtvHandles_ = {colorRtvHandle, normalRtvHandle, materialRtvHandle};
    currentRtvCount_ = 3;
    currentDsvHandle_ = dsvHandle;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, false, &dsvHandle);
}

void DirectXCommon::SetRenderTargetNoDepth(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    currentRtvHandle_ = rtvHandle;
    currentRtvHandles_ = {rtvHandle, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = {};
    currentHasDsv_ = false;
    list_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
}

void DirectXCommon::RestoreRenderTargetState(const RenderTargetState& state)
{
    assert(state.count >= 1 && state.count <= state.rtvs.size());
    currentRtvHandles_ = state.rtvs;
    currentRtvCount_ = state.count;
    currentRtvHandle_ = state.rtvs[0];
    currentDsvHandle_ = state.dsv;
    currentHasDsv_ = state.hasDepth;
    viewportRect_ = state.viewport;
    scissorRect_ = state.scissor;
    list_->OMSetRenderTargets(state.count, state.rtvs.data(), false, state.hasDepth ? &state.dsv : nullptr);
    list_->RSSetViewports(1, &viewportRect_);
    list_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
    float clearColor[4] = {0, 0, 0, 1};
    ClearRenderTarget(rtvHandle, clearColor);
}

void DirectXCommon::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const float clearColor[4])
{
    list_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void DirectXCommon::ClearDepthBuffer()
{
    list_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void DirectXCommon::SetBackBuffer()
{
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHandles_[backBufferIndex];
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    currentRtvHandle_ = rtv;
    currentRtvHandles_ = {rtv, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = dsv;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(1, &rtv, false, &dsv);
}

void DirectXCommon::SetPresentTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv)
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    currentRtvHandle_ = rtv;
    currentRtvHandles_ = {rtv, {}, {}};
    currentRtvCount_ = 1;
    currentDsvHandle_ = dsv;
    currentHasDsv_ = true;
    list_->OMSetRenderTargets(1, &rtv, false, &dsv);
}

void DirectXCommon::SetViewport(uint32_t width, uint32_t height)
{
    viewportRect_.TopLeftX = 0.0f;
    viewportRect_.TopLeftY = 0.0f;
    viewportRect_.Width = FLOAT(width);
    viewportRect_.Height = FLOAT(height);
    viewportRect_.MinDepth = 0.0f;
    viewportRect_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = LONG(width);
    scissorRect_.bottom = LONG(height);

    list_->RSSetViewports(1, &viewportRect_);
    list_->RSSetScissorRects(1, &scissorRect_);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetNewDsvHandle()
{
    assert(dsvHeapIndex_ < kMaxDsvCount);

    // 先頭のハンドルを取得
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 現在のインデックス分だけずらす
    handle.ptr += (descriptorSizeDSV * dsvHeapIndex_);

    // 次回のためにインデックスを進める
    dsvHeapIndex_++;

    return handle;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    // 頂点リソース用のヒープの設定
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeap5を使う
    // 頂点リソースの設定
    D3D12_RESOURCE_DESC vertexResourceDesc{};
    //バッファリソース。テクスチャの場合はまた別の設定をする
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; //リソースのサイズ
    // バッファの場合はこれらは1にする決まり
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // 実際に頂点リソースを作る
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexResource));
    assert(SUCCEEDED(hr));

    return vertexResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateUAVBufferResource(size_t sizeInBytes, D3D12_RESOURCE_FLAGS flags)
{
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = flags;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
                                                  nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetDescriptorCPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                                  uint32_t descriptorSize, uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetDescriptorGPUHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
                                                                  uint32_t descriptorSize, uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}

void DirectXCommon::InitializeFixFPS()
{
    framePacer_.Initialize();
}

void DirectXCommon::ResetFixFPS()
{
    framePacer_.Reset();
}

void DirectXCommon::UpdateFixFPS()
{
    framePacer_.Wait();
}

void DirectXCommon::Release()
{
    pipelines_.Release();
    // シェーダーとルートシグネチャのBlobはComPtrが持つので、ここではDXCとフェンスのイベントだけを手放す。
    shaderCompiler_.Release();
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
}

} // namespace myEngine
