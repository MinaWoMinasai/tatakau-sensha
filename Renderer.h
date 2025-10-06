#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <cassert>
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <filesystem>
#include <chrono>
#include <dxgidebug.h>
#include <dxgi1_6.h>

class Renderer {
public:
    void Initialize(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* commandList,
        ID3D12CommandQueue* commandQueue,
        ID3D12Fence* fence,
        HANDLE fenceEvent,
        ID3D12CommandAllocator* commandAllocator,
        UINT rtvIndexCount);

    void BeginFrame(UINT backBufferIndex);
    void DrawModel(
        const D3D12_VERTEX_BUFFER_VIEW& vbv,
        const D3D12_INDEX_BUFFER_VIEW* ibv,
        D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
        D3D12_GPU_VIRTUAL_ADDRESS wvpCBV,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrv,
        D3D12_GPU_VIRTUAL_ADDRESS lightCBV,
        UINT vertexCount, UINT count = 1, UINT indexCount = 0);
    void DrawLine(
        const D3D12_VERTEX_BUFFER_VIEW& vbv,
        const D3D12_INDEX_BUFFER_VIEW* ibv,
        D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
        D3D12_GPU_VIRTUAL_ADDRESS wvpCBV,
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrv,
        D3D12_GPU_VIRTUAL_ADDRESS lightCBV,
        UINT vertexCount, UINT indexCount = 0);

    void DrawImGui();
    void EndFrame();

    void SetSwapChainResources(const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& resources) {
        swapChainResources = resources;
    }
    void SetRTVHandles(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& handles){
        rtvHandles = handles;
    }
    void SetDSVHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) { dsvHandle = handle; }
    void SetRootSignature(ID3D12RootSignature* signature){ rootSignature = signature; }
    void SetSRVHeap(ID3D12DescriptorHeap* heap) {srvHeap = heap;}
    void SetGraphicsPipelineState(const Microsoft::WRL::ComPtr<ID3D12PipelineState>& pso) { graphicsPipelineState = pso; }
    void SetViewportAndScissor(float width, float height);
    void SetViewport(D3D12_VIEWPORT view) { viewport = view; }
    void SetScissorRect(D3D12_RECT rect) { scissorRect = rect; }
    void SetFenceValue(UINT64 value) { fenceValue = value; }
    void SetSwapChain(const Microsoft::WRL::ComPtr<IDXGISwapChain4>& chain) { swapChain = chain; }

private:
    void Transition(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

private:
    ID3D12Device* device = nullptr;
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12CommandQueue* commandQueue = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE fenceEvent = nullptr;
    ID3D12CommandAllocator* commandAllocator = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> swapChainResources;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissorRect;
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12DescriptorHeap* srvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;

    UINT backBufferIndex = 0;
    UINT64 fenceValue = 0;
};
