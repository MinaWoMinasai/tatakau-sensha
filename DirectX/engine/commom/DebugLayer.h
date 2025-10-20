#pragma once
#include <wrl.h>
#include <d3d12.h>

class DebugLayer {
public:
    static void EnableDebugLayer(ID3D12Device* device);
};

