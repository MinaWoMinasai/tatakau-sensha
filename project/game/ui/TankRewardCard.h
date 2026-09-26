#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"
#include "game/run/TankBuildStyle.h"
#include "game/run/TankRunDirector.h"
#include "TankRewardCardDemo.h"
#include "TankRewardPreviewRenderer.h"
#include "game/player/TankCombatStyleBalance.h"

struct TankRewardCardModel {
    std::string id,title,description,footer,authoredVariant;
    tankbuild::Style style=tankbuild::Style::Shooter;
    int rarity=0;
    std::vector<tankrun::CardId> effects;
    tankrun::CardCounts ownedEffects{};
    bool styleChoice=false;
    // Authored visual characteristics are supplied explicitly by the scene.
    // They do not read/instantiate the player's class or authored JSON catalog.
    int barrels=1,drones=3;
    bool reflect=false,penetrate=false;
    int currentDrones=3,currentBarrels=1;
    bool currentReflect=false,currentPenetrate=false;
    float damageScale=1.0f,reloadScale=1.0f,bulletSpeedScale=1.0f;
    float currentDamageScale=1.0f,currentReloadScale=1.0f,currentBulletSpeedScale=1.0f;
    bool previewKnown=true;
    TankCombatStyleProfile profile{};
    TankRunGrowth growth{};
    std::array<float,20> effectPower{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    std::array<float,20> ownedEffectPower{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
};

class TankRewardCard {
public:
    void Initialize(SpriteCommon* spriteCommon);
    void InitializePreview(SrvManager* srvManager);
    void SetPreviewAppearance(const TankRewardPreviewAppearance& appearance);
    void PreparePreviewRender();
    uint64_t GetPreviewRenderCount()const{return preview_?preview_->GetRenderCount():0;}
    uint32_t GetPreviewVertexCount()const{return preview_?preview_->GetGeometryVertexCount():0;}
    // Safe to call each Update; an equal model does not rebuild any text.
    void SetModel(const TankRewardCardModel& model);
    void Update(const Vector2& center,const Vector2& size,float deltaTime,bool hovered,bool enabled=true);
    void PlayAcquire();
    void Draw();
    // Optional scene bloom pass. Draw() already includes the small edge glow
    // textures; an additional full-screen effect is not required for these cards.
    void DrawBloomSource();
    const TankRewardCardModel& GetModel()const{return model_;}
    const tankreward::DemoSnapshot& GetDemoSnapshot(bool after=true)const{return after?after_:before_;}
    float GetDemoTime()const{return clock_.Elapsed();}
    std::uint64_t GetDemoAdvanceCount()const{return clock_.AdvanceCount();}
    bool IsAcquireAnimating()const{return acquireTime_>=0.0f&&acquireTime_<0.62f;}
private:
    static constexpr std::size_t kSolidCapacity=420,kGlowCapacity=80,kLabelCount=8;
    void EnsureSprite(std::unique_ptr<Sprite>& sprite,const char* texture);
    void RefreshText();
    void BuildFrame();
    void BuildDemo();
    void DrawLane(const tankreward::DemoSnapshot& state,const Vector2& origin,const Vector2& size,bool after);
    void Rect(Vector2 center,Vector2 size,Vector4 color,float rotation=0.0f);
    void Line(Vector2 a,Vector2 b,float width,Vector4 color);
    void Glow(Vector2 center,Vector2 size,Vector4 color,float rotation=0.0f);
    void Tank(Vector2 center,float radius,float angle,Vector4 color,bool drone=false,int barrels=1);
    void Label(std::size_t index,Vector2 position,float maxWidth,float maxHeight,float alpha=1.0f);
    tankreward::DemoConfig DemoConfig(bool after)const;
    SpriteCommon* spriteCommon_=nullptr;
    TankRewardCardModel model_{};
    std::array<std::unique_ptr<Sprite>,kSolidCapacity> solids_;
    std::array<std::unique_ptr<Sprite>,kGlowCapacity> glows_;
    std::array<std::unique_ptr<TextLabel>,kLabelCount> labels_;
    std::unique_ptr<TankRewardPreviewRenderer> preview_;
    std::unique_ptr<Sprite> previewSprite_;
    std::size_t solidCount_=0,glowCount_=0;
    std::array<bool,kLabelCount> labelVisible_{};
    tankreward::DemoClock clock_;
    tankreward::DemoSnapshot before_{},after_{};
    Vector2 center_{},size_{300,410};
    Vector4 accent_{0.9f,0.96f,1.0f,1.0f};
    float hoverBlend_=0.0f,visualTime_=0.0f,acquireTime_=-1.0f,alpha_=1.0f;
    bool dirty_=true,hovered_=false,enabled_=true;
    bool previewDirty_=true;
};
