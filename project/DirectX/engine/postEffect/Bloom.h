#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "RenderTexture.h"
#include "Struct.h"
#include "PostEffect.h"
#include "BloomConstantBuffer.h" 
#include "RtvManager.h"
#include "SceneManager.h"

class Bloom {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager);
    void Update();   // ImGuiと定数バッファ更新
    void PreDraw();  // 1. SceneRTをセット
    void PostDraw(); // 2. 抽出・ぼかし・合成を実行

private:
    // 便利関数：リソースバリアの切り替え
    void Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;

    // 各種レンダーターゲット（クラス化したRenderTexture等があると仮定）
    std::unique_ptr<RenderTexture> sceneRT_;
    std::unique_ptr<RenderTexture> bloomRT_Half_;
    std::unique_ptr<RenderTexture> bloomRT_A_;
    std::unique_ptr<RenderTexture> bloomRT_B_;

    // ポストエフェクト実行クラス
    std::unique_ptr<PostEffect> postEffect_;

    // パラメータと定数バッファ
    BloomParam bloomParam_;
    std::unique_ptr<BloomConstantBuffer> bloomCB_;
    float timer_ = 0.0f;
};
