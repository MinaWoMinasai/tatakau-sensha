#pragma once
#include <memory>
#include "SceneManager.h"
#include "RenderTexture.h"
#include "PostEffect.h"
#include "ShadowMap.h"
#include "GameScene.h"
#include "TitleScene.h"

class Game {
public:
    bool Initialize();
    void Run();
    void Finalize();

private:
    void InitializeEngine();
    void InitializeImGui();
    void LoadResources();
    void MainLoop();

private:
    std::unique_ptr<DirectXCommon> dxCommon_;
    std::unique_ptr<SrvManager> srvManager_;
    std::unique_ptr<RtvManager> rtvManager_;
    std::unique_ptr<RenderTexture> sceneRenderTexture_;
    std::unique_ptr<BloomConstantBuffer> bloomCB_;
    std::unique_ptr<PostEffect> postEffect_;
    std::unique_ptr<RenderTexture> bloomRT_A_;
    std::unique_ptr<RenderTexture> bloomRT_B_;
    std::unique_ptr<RenderTexture> bloomRT_Half_;
    BloomParam bloomParam_{};
    std::unique_ptr<ShadowMap> shadowMap_;



    void TransitionResource(DirectXCommon* dxCommon, ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

    float timer = 0.0f;

};