#include "Renderer.h"

void Renderer::Initialize(
    ID3D12Device* dev,
    ID3D12GraphicsCommandList* cl,
    ID3D12CommandQueue* cq,
    ID3D12Fence* f,
    HANDLE fe,
    ID3D12CommandAllocator* ca,
    UINT rtvCount)
{
    device = dev;
    commandList = cl;
    commandQueue = cq;
    fence = f;
    fenceEvent = fe;
    commandAllocator = ca;

}

void Renderer::BeginFrame(UINT bufferIndex) {
    backBufferIndex = bufferIndex;
    Transition(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}

void Renderer::DrawModel(
    const D3D12_VERTEX_BUFFER_VIEW& vbv,
    const D3D12_INDEX_BUFFER_VIEW* ibv,
    D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
    D3D12_GPU_VIRTUAL_ADDRESS wvpCBV,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrv,
    D3D12_GPU_VIRTUAL_ADDRESS cameraCBV,
    D3D12_GPU_VIRTUAL_ADDRESS lightCBV,
    UINT vertexCount, UINT count, UINT indexCount)
{
    ID3D12DescriptorHeap* heaps[] = { srvHeap };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->SetPipelineState(graphicsPipelineState.Get()); // PSOを設定
    commandList->IASetVertexBuffers(0, 1, &vbv);
    if (ibv) commandList->IASetIndexBuffer(ibv);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootConstantBufferView(0, materialCBV);
    commandList->SetGraphicsRootConstantBufferView(1, wvpCBV);
    commandList->SetGraphicsRootDescriptorTable(2, textureSrv);
    commandList->SetGraphicsRootConstantBufferView(3, lightCBV);
    commandList->SetGraphicsRootConstantBufferView(4, cameraCBV);

    if (ibv) {
        commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    } else {
        commandList->DrawInstanced(vertexCount, count, 0, 0);
    }
}

void Renderer::DrawLine(const D3D12_VERTEX_BUFFER_VIEW& vbv, const D3D12_INDEX_BUFFER_VIEW* ibv, D3D12_GPU_VIRTUAL_ADDRESS materialCBV, D3D12_GPU_VIRTUAL_ADDRESS wvpCBV, D3D12_GPU_DESCRIPTOR_HANDLE textureSrv, D3D12_GPU_VIRTUAL_ADDRESS lightCBV, UINT vertexCount, UINT indexCount)
{
    ID3D12DescriptorHeap* heaps[] = { srvHeap };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->SetPipelineState(graphicsPipelineState.Get()); // PSOを設定
    commandList->IASetVertexBuffers(0, 1, &vbv);
    if (ibv) commandList->IASetIndexBuffer(ibv);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    commandList->SetGraphicsRootConstantBufferView(0, materialCBV);
    commandList->SetGraphicsRootConstantBufferView(1, wvpCBV);
    commandList->SetGraphicsRootDescriptorTable(2, textureSrv);
    commandList->SetGraphicsRootConstantBufferView(3, lightCBV);

    if (ibv) {
        commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
    } else {
        commandList->DrawInstanced(vertexCount, 1, 0, 0);
    }
}

void Renderer::DrawImGui() {
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void Renderer::EndFrame() {
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    commandList->Close();
    ID3D12CommandList* lists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, lists);
    swapChain->Present(1, 0);
    commandQueue->Signal(fence, ++fenceValue);

    if (fence->GetCompletedValue() < fenceValue) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    commandAllocator->Reset();
    commandList->Reset(commandAllocator, nullptr);
}

void Renderer::SetViewportAndScissor(float width, float height)
{
    viewport = { 0.0f, 0.0f, width, height, 0.0f, 1.0f };
    scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void Renderer::Transition(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);
}
