#include "TankRewardCard.h"
#include "StartupTrace.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
constexpr float kPi=3.14159265359f;
constexpr const char* kWhite="resources/white512x512.png";
constexpr const char* kGlow="resources/ui/salvage_orb.png";
Vector4 Tint(Vector4 c,float alpha) {c.w*=alpha;return c;}
Vector4 RarityColor(int rarity,float phase=0.0f) {
    switch(rarity) {
    case 1:return {0.70f,1.0f,0.26f,1};
    case 2:return {0.28f,0.85f,1.0f,1};
    case 3:return {0.80f,0.42f,1.0f,1};
    case 4:return {0.68f+0.32f*std::sin(phase),0.68f+0.32f*std::sin(phase+2.0944f),0.68f+0.32f*std::sin(phase+4.1888f),1};
    default:return {0.90f,0.95f,1.0f,1};
    }
}
bool SameModel(const TankRewardCardModel& a,const TankRewardCardModel& b) {
    const auto& p=a.profile;const auto& q=b.profile;
    const bool sameProfile=p.maxHp==q.maxHp&&p.moveSpeed==q.moveSpeed&&p.maxStamina==q.maxStamina&&
        p.staminaRecovery==q.staminaRecovery&&p.bodyDamage==q.bodyDamage&&p.attackDamage==q.attackDamage&&
        p.attackIntervalSeconds==q.attackIntervalSeconds&&p.bulletSpeed==q.bulletSpeed&&p.meleeRange==q.meleeRange&&
        p.meleeKnockback==q.meleeKnockback&&p.droneCount==q.droneCount&&p.droneFollowSpeed==q.droneFollowSpeed&&
        p.droneCatchupSpeed==q.droneCatchupSpeed&&p.droneResponse==q.droneResponse&&p.droneFormationRadius==q.droneFormationRadius;
    return a.id==b.id&&a.title==b.title&&a.description==b.description&&a.footer==b.footer&&
        a.authoredVariant==b.authoredVariant&&a.style==b.style&&a.rarity==b.rarity&&
        a.effects==b.effects&&a.ownedEffects==b.ownedEffects&&a.styleChoice==b.styleChoice&&
        a.barrels==b.barrels&&a.drones==b.drones&&a.reflect==b.reflect&&a.penetrate==b.penetrate&&
        a.damageScale==b.damageScale&&a.reloadScale==b.reloadScale&&a.bulletSpeedScale==b.bulletSpeedScale&&
        a.currentDamageScale==b.currentDamageScale&&a.currentReloadScale==b.currentReloadScale&&
        a.currentBulletSpeedScale==b.currentBulletSpeedScale&&a.previewKnown==b.previewKnown&&
        a.currentDrones==b.currentDrones&&a.currentBarrels==b.currentBarrels&&
        a.currentReflect==b.currentReflect&&a.currentPenetrate==b.currentPenetrate&&sameProfile&&
        a.fanAngle==b.fanAngle&&a.currentFanAngle==b.currentFanAngle&&a.alternate==b.alternate&&a.currentAlternate==b.currentAlternate&&
        a.effectPower==b.effectPower&&a.ownedEffectPower==b.ownedEffectPower&&
        a.growth.hp==b.growth.hp&&a.growth.damage==b.growth.damage&&a.growth.bulletSpeed==b.growth.bulletSpeed&&
        a.growth.reload==b.growth.reload&&a.growth.move==b.growth.move;
}
bool Offers(const TankRewardCardModel& m,tankrun::CardId effect) {
    return std::find(m.effects.begin(),m.effects.end(),effect)!=m.effects.end();
}
std::string Wrap(const std::string& text,int columns,int maxLines) {
    std::string result;int count=0,line=1;
    for(std::size_t i=0;i<text.size();) {
        const auto byte=static_cast<unsigned char>(text[i]);
        const std::size_t length=byte<0x80?1:(byte&0xe0)==0xc0?2:(byte&0xf0)==0xe0?3:4;
        const int width=byte<0x80?1:2;
        if(text[i]=='\n') {if(line>=maxLines)break;result+='\n';++line;count=0;++i;continue;}
        if((byte>='0'&&byte<='9')&&(i==0||text[i-1]<'0'||text[i-1]>'9')) {
            std::size_t end=i;
            while(end<text.size()&&((text[end]>='0'&&text[end]<='9')||text[end]=='.'||text[end]=='%')) ++end;
            if(count>0&&count+static_cast<int>(end-i)>columns) {
                if(line>=maxLines){result+="…";break;}
                result+='\n';++line;count=0;
            }
        }
        if(count+width>columns) {
            if(line>=maxLines){result+="…";break;}
            result+='\n';++line;count=0;
        }
        result.append(text,i,(std::min)(length,text.size()-i));i+=length;count+=width;
    }
    return result.empty()?" ":result;
}
tankreward::DemoKind Kind(const TankRewardCardModel& m) {
    using C=tankrun::CardId;using D=tankreward::DemoKind;
    if(!m.previewKnown)return D::Info;
    if(m.styleChoice)return m.style==tankbuild::Style::Drone?D::Drone:m.style==tankbuild::Style::Melee?D::Melee:D::Shooter;
    if(Offers(m,C::RailCannon))return D::RailCannon;
    if(Offers(m,C::DroneLaserLink))return D::DroneLaserLink;
    if(Offers(m,C::SlashWave))return D::SlashWave;
    if(Offers(m,C::ParryBlade))return D::ParryBlade;
    if(Offers(m,C::BladeReach))return D::BladeReach;
    if(Offers(m,C::ImpactDrive))return D::ImpactDrive;
    if(Offers(m,C::MeleeBlade)||Offers(m,C::MeleeTempo)||Offers(m,C::FinisherCharge))return D::Melee;
    if(Offers(m,C::Drones)||Offers(m,C::DroneFocus)||Offers(m,C::DroneGuard))return D::Drone;
    if(Offers(m,C::Ricochet))return D::Ricochet;
    if(Offers(m,C::Homing))return D::Homing;
    if(Offers(m,C::Pierce))return D::Pierce;
    if(Offers(m,C::Heavy)||Offers(m,C::Rapid))return m.style==tankbuild::Style::Drone?D::Drone:m.style==tankbuild::Style::Melee?D::Melee:D::Shooter;
    if(!m.authoredVariant.empty())return m.style==tankbuild::Style::Drone?D::Drone:m.style==tankbuild::Style::Melee?D::Melee:D::Shooter;
    return D::Info;
}
std::string DemoNote(const TankRewardCardModel& m) {
    using D=tankreward::DemoKind;
    if(!m.previewKnown)return "詳細は上の説明を確認してください";
    if(!m.authoredVariant.empty())return "模式実演 / 動作速度・威力を比較";
    switch(Kind(m)) {
    case D::RailCannon:return "左長押しでチャージ → 離して貫通射撃";
    case D::DroneLaserLink:return "隊形を重ね、レーザー線で敵を捉える";
    case D::SlashWave:return "3段目で斬撃波 / 奥の敵にも届く";
    case D::ParryBlade:return "斬撃で弾を切る / 開始直後は反射";
    case D::Shooter:return "左クリックで射撃 / 1砲門につき1発";
    case D::Drone:return "左クリック中だけ発射 / カーソルで照準";
    case D::Melee:return "左クリックで3連斬 / 3段目で押し出す";
    case D::Homing:return "追尾には距離・旋回の上限があります";
    case D::Ricochet:return "壁で跳ね返る / 弾数は増えません";
    case D::Pierce:return "敵を貫いて奥の敵へ届く";
    case D::BladeReach:return "取得前後の届く範囲を比較";
    case D::ImpactDrive:return "体当たり・斬撃の押し出しを比較";
    default:return "強化の効果は上の説明で確認";
    }
}
} // namespace

void TankRewardCard::Initialize(SpriteCommon* spriteCommon) {
    if(spriteCommon_)return;
    StartupTrace::Scope scope("RewardCard.Initialize");
    spriteCommon_=spriteCommon;
    {
        StartupTrace::Scope poolScope("RewardCard.SpritePool");
        wchar_t flag[8]{};
        const bool baseline=GetEnvironmentVariableW(L"CG2_STARTUP_CACHE",flag,8)>0&&flag[0]==L'0';
        // The HDR preview replaced the sprite-based demonstration. Its frame
        // needs at most 85 solids and 28 glows, including legendary acquisition.
        // Reserve above those bounds so opening any rarity adds no allocations.
        // The legacy fallback may grow the existing pool only during Update.
        const std::size_t solidReserve=baseline?kSolidCapacity:96;
        const std::size_t glowReserve=baseline?kGlowCapacity:32;
        for(std::size_t i=0;i<solidReserve;++i) EnsureSprite(solids_[i],kWhite);
        for(std::size_t i=0;i<glowReserve;++i) EnsureSprite(glows_[i],kGlow);
        StartupTrace::Count("rewardCard.reservedSprites",static_cast<double>(solidReserve+glowReserve));
    }
    for(std::size_t i=0;i<labels_.size();++i) {
        TextStyle style;style.fontSize=i==1?24.0f:i==2?16.0f:i==3?18.0f:12.0f;
        style.fontWeight=i==1||i==3?700:400;style.padding=2;style.outlineThickness=0;
        style.color=i==2?Vector4{0.78f,0.84f,0.91f,1}:Vector4{0.94f,0.97f,1,1};
        labels_[i]=std::make_unique<TextLabel>();labels_[i]->Initialize(spriteCommon," ",style);
        labels_[i]->SetAnchorPoint({0.5f,0.5f});
    }
    dirty_=true;
}
void TankRewardCard::SetModel(const TankRewardCardModel& model) {
    if(SameModel(model_,model))return;
    model_=model;model_.rarity=(std::clamp)(model_.rarity,0,4);dirty_=true;
    clock_.Reset();acquireTime_=-1.0f;previewDirty_=true;
}
void TankRewardCard::InitializePreview(SrvManager* srvManager) {
    if(preview_||!spriteCommon_||!srvManager)return;
    StartupTrace::Scope scope("RewardCard.InitializePreview");
    preview_=std::make_unique<TankRewardPreviewRenderer>();preview_->Initialize(spriteCommon_->GetDxCommon(),srvManager);
    previewSprite_=std::make_unique<Sprite>();previewSprite_->Initialize(spriteCommon_,preview_->GetSrvIndex(),srvManager);
    previewSprite_->SetAnchorPoint({.5f,.5f});previewSprite_->SetColor({1,1,1,1});previewDirty_=true;
}
void TankRewardCard::SetPreviewAppearance(const TankRewardPreviewAppearance& appearance) {
    if(preview_)preview_->SetAppearance(appearance);
}
void TankRewardCard::PreparePreviewRender(){if(preview_)preview_->Render();}
void TankRewardCard::PlayAcquire() {acquireTime_=0.0f;}

tankreward::DemoConfig TankRewardCard::DemoConfig(bool after)const {
    tankreward::DemoConfig c;c.kind=Kind(model_);
    auto has=[&](tankrun::CardId id) {return model_.ownedEffects[static_cast<std::size_t>(id)]>0||(after&&Offers(model_,id));};
    using C=tankrun::CardId;
    c.homing=has(C::Homing);c.ricochet=has(C::Ricochet)||(after?model_.reflect:model_.currentReflect);
    c.pierce=has(C::Pierce)||(after?model_.penetrate:model_.currentPenetrate);c.bladeReach=has(C::BladeReach);
    c.impactDrive=has(C::ImpactDrive);c.droneFocus=has(C::DroneFocus);c.droneGuard=has(C::DroneGuard);
    c.meleeTempo=has(C::MeleeTempo);c.finisherCharge=has(C::FinisherCharge);
    c.railCannon=has(C::RailCannon);c.droneLaserLink=has(C::DroneLaserLink);
    c.slashWave=has(C::SlashWave);c.parryBlade=has(C::ParryBlade);
    c.heavy=has(C::Heavy);c.rapid=has(C::Rapid);c.thrusters=has(C::Thrusters);c.repair=has(C::Repair);
    c.growth=model_.growth;
    c.meleeStyle=model_.style==tankbuild::Style::Melee;
    c.damageScale=after?model_.damageScale:model_.currentDamageScale;
    c.reloadScale=after?model_.reloadScale:model_.currentReloadScale;
    c.bulletSpeedScale=after?model_.bulletSpeedScale:model_.currentBulletSpeedScale;
    c.effectPower=model_.ownedEffectPower;
    if(after)for(const auto effect:model_.effects)if(static_cast<std::size_t>(effect)<c.effectPower.size())c.effectPower[static_cast<std::size_t>(effect)]=model_.effectPower[static_cast<std::size_t>(effect)];
    c.profile=SanitizeTankCombatStyleProfile(model_.profile);c.useProfile=true;
    c.droneCount=(std::max)(1,(after?model_.drones:model_.currentDrones)+(has(C::Drones)?static_cast<int>(std::lround(2*c.effectPower[6])):0));
    c.barrels=(std::max)(1,after?model_.barrels:model_.currentBarrels);
    c.fanAngle=after?model_.fanAngle:model_.currentFanAngle;c.alternate=after?model_.alternate:model_.currentAlternate;
    // The card describes the changed behavior in isolation. For reflection and
    // piercing comparisons, unrelated steering must not mask the difference.
    if(c.kind==tankreward::DemoKind::Ricochet||c.kind==tankreward::DemoKind::Pierce)c.homing=false;
    return c;
}
void TankRewardCard::RefreshText() {
    labels_[0]->SetText(model_.styleChoice?"BASIC / 基本スタイル":tankbuild::RarityName(model_.rarity));
    labels_[1]->SetText(Wrap(model_.title,28,2));
    labels_[2]->SetText(Wrap(model_.description,40,3));
    labels_[3]->SetText(model_.footer.empty()?"選択して装備":Wrap(model_.footer,40,1));
    const auto before=DemoConfig(false),after=DemoConfig(true);
    const bool representative=(std::max)(before.droneCount,after.droneCount)>6||(std::max)(before.barrels,after.barrels)>6;
    labels_[4]->SetText(Wrap(representative?"最大6機・6砲門を代表表示 / 詳細は説明":DemoNote(model_),48,1));
    labels_[5]->SetText("現在");labels_[6]->SetText("取得後");
    labels_[7]->SetText("LEFT CLICK");dirty_=false;
}
void TankRewardCard::Update(const Vector2& center,const Vector2& size,float dt,bool hovered,bool enabled) {
    if(!spriteCommon_)return;
    if(!std::isfinite(dt))dt=0;dt=(std::clamp)(dt,0.0f,0.1f);
    center_=center;size_={(std::max)(240.0f,size.x),(std::max)(280.0f,size.y)};
    hovered_=hovered;enabled_=enabled;alpha_=enabled?1.0f:0.52f;
    hoverBlend_+=(static_cast<float>(hovered)-hoverBlend_)*(std::min)(1.0f,dt*12.0f);
    visualTime_+=dt;if(acquireTime_>=0)acquireTime_+=dt;
    clock_.Update(dt,hovered&&enabled);
    if(dirty_)RefreshText();
    solidCount_=0;glowCount_=0;labelVisible_.fill(false);
    accent_=RarityColor(model_.styleChoice?0:model_.rarity,visualTime_*1.6f);
    BuildFrame();BuildDemo();
    const float top=center_.y-size_.y*0.5f;
    Label(0,{center_.x,top+20},size_.x-56,17,0.76f);
    Label(1,{center_.x,top+51},size_.x-38,49);
    Label(2,{center_.x,top+106},size_.x-40,64);
    Label(3,{center_.x,top+size_.y-22},size_.x-46,23);
    Label(4,{center_.x,top+size_.y-58},size_.x-38,17,0.66f+0.20f*hoverBlend_);
}
void TankRewardCard::Rect(Vector2 center,Vector2 size,Vector4 color,float rotation) {
    if(solidCount_>=solids_.size())return;
    auto& s=solids_[solidCount_++];EnsureSprite(s,kWhite);s->SetPosition(center);s->SetSize(size);s->SetRotation(rotation);
    s->SetColor(Tint(color,alpha_));s->Update();
}
void TankRewardCard::Line(Vector2 a,Vector2 b,float width,Vector4 color) {
    const float dx=b.x-a.x,dy=b.y-a.y;
    Rect({(a.x+b.x)*0.5f,(a.y+b.y)*0.5f},{std::sqrt(dx*dx+dy*dy),width},color,std::atan2(dy,dx));
}
void TankRewardCard::Glow(Vector2 center,Vector2 size,Vector4 color,float rotation) {
    if(glowCount_>=glows_.size())return;
    auto& s=glows_[glowCount_++];EnsureSprite(s,kGlow);s->SetPosition(center);s->SetSize(size);s->SetRotation(rotation);
    s->SetColor(Tint(color,alpha_));s->Update();
}
void TankRewardCard::EnsureSprite(std::unique_ptr<Sprite>& sprite,const char* texture) {
    if(sprite)return;
    sprite=std::make_unique<Sprite>();sprite->Initialize(spriteCommon_,texture);sprite->SetAnchorPoint({0.5f,0.5f});
    StartupTrace::Count("rewardCard.allocatedSprites");
}
void TankRewardCard::Label(std::size_t index,Vector2 position,float maxWidth,float maxHeight,float alpha) {
    auto& label=labels_[index];label->SetPosition(position);label->SetAlpha(alpha*alpha_);label->PrepareForDraw();
    auto* sprite=label->GetSprite();if(!sprite)return;
    const Vector2 native=sprite->GetTextureSize();
    const float scale=(std::min)({1.0f,maxWidth/(std::max)(1.0f,native.x),maxHeight/(std::max)(1.0f,native.y)});
    sprite->SetSize({native.x*scale,native.y*scale});sprite->Update();labelVisible_[index]=true;
}
void TankRewardCard::BuildFrame() {
    const float x=center_.x-size_.x*0.5f,y=center_.y-size_.y*0.5f,w=size_.x,h=size_.y;
    const float acquire=IsAcquireAnimating()?std::sin(acquireTime_/0.62f*kPi):0;
    const float focus=0.46f+hoverBlend_*0.50f+acquire*0.4f;
    const int rarity=model_.styleChoice?0:model_.rarity;
    constexpr std::array<float,5> edgeGlows{0.0f,0.10f,0.20f,0.34f,0.50f};
    Rect({center_.x+2,center_.y+7},{w+10,h+10},{0,0.005f,0.012f,0.54f});
    Rect(center_,size_,{0.016f+hoverBlend_*0.009f,0.024f+hoverBlend_*0.008f,0.040f+hoverBlend_*0.008f,0.985f});
    Rect({center_.x,y+17},{w-22,22},{0.035f,0.049f,0.07f,0.8f});
    constexpr float cut=10;
    const std::array<Vector2,8> frame{{{x+cut,y},{x+w-cut,y},{x+w,y+cut},{x+w,y+h-cut},
        {x+w-cut,y+h},{x+cut,y+h},{x,y+h-cut},{x,y+cut}}};
    for(std::size_t i=0;i<frame.size();++i) {
        const auto a=frame[i],b=frame[(i+1)%frame.size()];
        Vector4 color=RarityColor(model_.styleChoice?0:model_.rarity,visualTime_*1.6f+static_cast<float>(i)*0.72f);
        Line(a,b,1.2f+hoverBlend_*0.7f+acquire,Tint(color,focus));
        if(i%2==0&&(rarity>0||acquire>0.0f)) {
            const float length=std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y));
            Glow({(a.x+b.x)*0.5f,(a.y+b.y)*0.5f},{length+24,16+static_cast<float>(rarity)*6+hoverBlend_*10},
                Tint(color,edgeGlows[static_cast<std::size_t>(rarity)]*(1+hoverBlend_)+acquire*0.10f),std::atan2(b.y-a.y,b.x-a.x));
        }
    }
    if(rarity>=2) {
        // Rare gains a distinct inner circuit, not just another tint.
        for(std::size_t i=0;i<frame.size();++i) {
            auto a=frame[i],b=frame[(i+1)%frame.size()];
            a.x+=(a.x<center_.x?5.0f:-5.0f);a.y+=(a.y<center_.y?5.0f:-5.0f);
            b.x+=(b.x<center_.x?5.0f:-5.0f);b.y+=(b.y<center_.y?5.0f:-5.0f);
            Line(a,b,rarity==4?1.4f:0.8f,Tint(RarityColor(rarity,visualTime_*1.6f+static_cast<float>(i)*0.72f),0.21f+hoverBlend_*0.24f));
        }
        if(hoverBlend_>0.02f)for(int n=0;n<2;++n) {
            const float path=std::fmod(visualTime_*1.4f+static_cast<float>(n)*4,8.0f);
            const auto index=static_cast<std::size_t>(path);const float blend=path-static_cast<float>(index);
            const auto a=frame[index],b=frame[(index+1)%frame.size()];
            const Vector2 at{a.x+(b.x-a.x)*blend,a.y+(b.y-a.y)*blend};
            Glow(at,{15,15},Tint(accent_,0.7f*hoverBlend_));Rect(at,{3,3},Tint(accent_,hoverBlend_));
        }
    }
    // Corner brackets retain a sharp silhouette, while the glow is restricted
    // to edges. The center stays dark at every rarity and interaction state.
    for(int side=0;side<2;++side)for(int bottom=0;bottom<2;++bottom) {
        const float bx=x+(side?w-7:7),by=y+(bottom?h-7:7),sx=side?-1.0f:1.0f,sy=bottom?-1.0f:1.0f;
        Line({bx+sx*4,by},{bx+sx*27,by},2,Tint(accent_,0.72f));
        Line({bx,by+sy*4},{bx,by+sy*22},2,Tint(accent_,0.72f));
        Rect({bx+sx*9,by+sy*9},{3,3},Tint(accent_,0.84f));
        if(rarity>=3) {
            // Epic corner sockets, with short moving electric arcs on hover.
            Line({bx+sx*15,by+sy*3},{bx+sx*33,by+sy*3},2,Tint(accent_,0.55f));
            Line({bx+sx*3,by+sy*15},{bx+sx*3,by+sy*33},2,Tint(accent_,0.55f));
            Rect({bx+sx*8,by+sy*8},{6,6},Tint(accent_,0.45f),kPi*0.25f);
            if(hoverBlend_>0.03f) {
                const float jitter=std::sin(visualTime_*24+static_cast<float>(side+bottom*2)*2)*3;
                const Vector2 a{bx+sx*36,by+sy*2},m{bx+sx*(46+jitter),by-sy*3},b{bx+sx*58,by+sy*2};
                Line(a,m,1.1f,Tint(accent_,hoverBlend_*0.72f));Line(m,b,1.1f,Tint(accent_,hoverBlend_*0.72f));
            }
        }
    }
    Line({x+20,y+76},{x+w-20,y+76},1,{0.26f,0.34f,0.43f,0.42f});
    Line({x+20,y+h-42},{x+w-20,y+h-42},1,Tint(accent_,0.23f+hoverBlend_*0.24f));
    const int ornaments=model_.styleChoice?1:(std::clamp)(model_.rarity+1,1,5);
    for(int i=0;i<ornaments;++i) {
        const float px=x+22+static_cast<float>(i)*6;
        Rect({px,y+h-17},{2,7+static_cast<float>(i%2)*3},Tint(RarityColor(model_.rarity,visualTime_+static_cast<float>(i)),0.65f));
    }
    if(rarity==4&&(hoverBlend_>0.05f||acquire>0.0f)) {
        const int amount=10;
        for(int i=0;i<amount;++i) {
            const float fi=static_cast<float>(i);
            const float p=std::fmod(visualTime_*0.16f+fi/static_cast<float>(amount),1.0f);
            const float py=y+19+p*(h-38),px=(i%2==0?x-4:x+w+4);
            Glow({px,py},{11+acquire*9,11+acquire*9},Tint(RarityColor(model_.rarity,visualTime_+fi),hoverBlend_*0.35f+acquire*0.3f));
            Rect({px,py},{2,2},Tint(accent_,0.85f*hoverBlend_));
        }
    }
    if(acquire>0.0f) {
        const float inset=5+acquireTime_*16;
        Line({x+inset,y+h-inset},{x+w-inset,y+h-inset},2,Tint(accent_,acquire));
        Line({x+inset,y+inset},{x+w-inset,y+inset},2,Tint(accent_,acquire));
        if(rarity==4)for(int i=0;i<12;++i) {
            const float p=static_cast<float>(i)/12.0f*2*kPi;
            const float outward=acquireTime_*14;
            const Vector2 at{center_.x+std::cos(p)*(w*0.47f+outward),center_.y+std::sin(p)*(h*0.47f+outward)};
            Line(at,{at.x+std::cos(p)*7,at.y+std::sin(p)*7},2,Tint(RarityColor(4,p),acquire));
            Glow(at,{19,19},Tint(RarityColor(4,p),acquire*0.65f));
        }
    }
}
void TankRewardCard::Tank(Vector2 center,float radius,float angle,Vector4 color,bool drone,int barrels) {
    const int segments=drone?4:12;
    if(!drone) {
        Rect({center.x,center.y-radius*0.72f},{radius*1.8f,radius*0.38f},{0.17f,0.24f,0.32f,0.9f});
        Rect({center.x,center.y+radius*0.72f},{radius*1.8f,radius*0.38f},{0.17f,0.24f,0.32f,0.9f});
    }
    for(int i=0;i<segments;++i) {
        const float a=static_cast<float>(i)/static_cast<float>(segments)*2*kPi;
        const float b=static_cast<float>(i+1)/static_cast<float>(segments)*2*kPi;
        Line({center.x+std::cos(a)*radius,center.y+std::sin(a)*radius},
            {center.x+std::cos(b)*radius,center.y+std::sin(b)*radius},1.4f,color);
    }
    for(int n=0;n<barrels;++n) {
        const float offset=(static_cast<float>(n)-static_cast<float>(barrels-1)*0.5f)*radius*0.25f;
        const Vector2 base{center.x-std::sin(angle)*offset,center.y+std::cos(angle)*offset};
        Line(base,{base.x+std::cos(angle)*radius*1.55f,base.y+std::sin(angle)*radius*1.55f},drone?2.0f:2.6f,color);
    }
    Glow(center,{radius*3.3f,radius*3.3f},Tint(color,0.22f));
}
void TankRewardCard::DrawLane(const tankreward::DemoSnapshot& s,const Vector2& origin,const Vector2& size,bool after) {
    auto point=[&](tankreward::Point p)->Vector2{return {origin.x+p.x*size.x,origin.y+p.y*size.y};};
    const Vector4 actor=after?Vector4{0.30f,0.90f,1.0f,0.95f}:Vector4{0.55f,0.63f,0.70f,0.67f};
    const float radius=(std::min)(9.0f,size.y*0.16f);
    if(s.wall)Line(point({0.35f,0.18f}),point({0.71f,0.18f}),3,{0.52f,0.63f,0.76f,0.65f});
    // Reticle: a real cursor target, distinct from the enemy and bullet path.
    const auto cursor=point(s.cursor);
    Line({cursor.x-4,cursor.y},{cursor.x+4,cursor.y},1,{0.5f,0.68f,0.75f,0.34f});
    Line({cursor.x,cursor.y-4},{cursor.x,cursor.y+4},1,{0.5f,0.68f,0.75f,0.34f});
    for(int n=0;n<s.targetCount;++n) {
        auto at=point(s.targets[n]);const float r=radius*0.88f;
        const Vector4 enemy{1.0f,0.27f+s.hit[n]*0.42f,0.30f+s.hit[n]*0.4f,0.82f};
        Rect(at,{r*1.2f,r*1.2f},{0.13f,0.026f,0.048f,0.98f},0.17f);
        const std::array<Vector2,4> ends{{{at.x-r,at.y-r},{at.x+r,at.y-r},{at.x+r,at.y+r},{at.x-r,at.y+r}}};
        for(std::size_t i=0;i<4;++i)Line(ends[i],ends[(i+1)%4],1.2f,enemy);
        if(s.hit[n]>0.0f) {
            Glow(at,{radius*4,radius*4},Tint(enemy,s.hit[n]*0.45f));
            for(int k=0;k<3;++k) {const float a=static_cast<float>(k)*2.0944f;Line(
                {at.x+std::cos(a)*r*1.6f,at.y+std::sin(a)*r*1.6f},
                {at.x+std::cos(a)*r*2.3f,at.y+std::sin(a)*r*2.3f},1,Tint(enemy,s.hit[n]));}
        }
        Line({at.x-r,at.y-r-4},{at.x+r,at.y-r-4},2,{0.30f,0.13f,0.17f,0.8f});
        Line({at.x-r,at.y-r-4},{at.x-r+2*r*(1.0f-tankreward::Saturate(s.damage[n])),at.y-r-4},2,{0.95f,0.37f,0.37f,0.8f});
    }
    const auto player=point(s.player);
    if(s.dashing) {
        Line({player.x-22,player.y},player,4,Tint(actor,0.30f));
        Line({player.x-29,player.y+4},{player.x-9,player.y+4},1,Tint(actor,0.45f));
    }
    Tank(player,radius,std::atan2(cursor.y-player.y,cursor.x-player.x),actor,false,s.barrelCount);
    for(int n=0;n<s.droneCount;++n) {
        const auto drone=point(s.drones[n]);
        Line(player,drone,0.7f,Tint(actor,0.18f));
        Tank(drone,radius*0.55f,std::atan2(cursor.y-drone.y,cursor.x-drone.x),actor,true);
    }
    for(const auto& bullet:s.bullets) {
        if(!bullet.visible)continue;
        for(std::size_t i=1;i<bullet.trailCount;++i)Line(point(bullet.trail[i-1]),point(bullet.trail[i]),1.2f,Tint(actor,0.12f+0.44f*static_cast<float>(i)/8.0f));
        const auto at=point(bullet.position);Rect(at,{5,2.5f},actor,bullet.angle);Glow(at,{13,13},Tint(actor,0.5f));
        if(s.reinforcedProjectiles) {Line({at.x-4,at.y-3},{at.x+4,at.y-3},1,Tint(actor,0.65f));Line({at.x-4,at.y+3},{at.x+4,at.y+3},1,Tint(actor,0.65f));}
    }
    if(s.slashing) {
        // The horizontal range is intentionally compared in the same scale in
        // both lanes. A separate vertical scale keeps the arc inside the card.
        constexpr int segments=12;const float halfArc=s.slashArc*0.26f;
        const float start=(std::max)(-s.slashArc*0.5f,s.slashAngle-halfArc);
        const float end=(std::min)(s.slashArc*0.5f,s.slashAngle+halfArc);
        for(int i=0;i<segments;++i) {
            const float a=start+(end-start)*static_cast<float>(i)/segments;
            const float b=start+(end-start)*static_cast<float>(i+1)/segments;
            const float rx=s.slashReach*size.x,ry=(std::min)(size.y*0.37f,rx);
            Line({player.x+std::cos(a)*rx,player.y+std::sin(a)*ry},
                {player.x+std::cos(b)*rx,player.y+std::sin(b)*ry},s.comboStep==2?2.8f:1.8f,Tint(actor,0.78f));
        }
        Glow({player.x+std::cos(s.slashAngle)*s.slashReach*size.x,player.y+std::sin(s.slashAngle)*size.y*0.3f},
            {20,20},Tint(actor,0.38f));
    }
}
void TankRewardCard::BuildDemo() {
    using D=tankreward::DemoKind;
    const float x=center_.x-size_.x*0.5f,y=center_.y-size_.y*0.5f;
    const float demoY=y+148,demoH=size_.y-220;
    Rect({center_.x,demoY+demoH*0.5f},{size_.x-38,demoH},{0.009f,0.016f,0.027f,1});
    const auto kind=Kind(model_);
    before_=tankreward::SampleDemo(DemoConfig(false),clock_.Elapsed());
    after_=tankreward::SampleDemo(DemoConfig(true),clock_.Elapsed());
    if(preview_) {
        const bool compare=!model_.styleChoice&&kind!=D::Info;
        const auto beforeConfig=DemoConfig(false),afterConfig=DemoConfig(true);
        preview_->Update(before_,after_,compare,beforeConfig.ricochet,afterConfig.ricochet,
            model_.style==tankbuild::Style::Melee||kind==D::Melee||kind==D::BladeReach,
            hovered_&&enabled_,clock_.Elapsed(),previewDirty_);
        previewDirty_=false;
        previewSprite_->SetPosition({center_.x,demoY+demoH*.5f});previewSprite_->SetSize({size_.x-38,demoH});
        previewSprite_->SetColor({1,1,1,alpha_});previewSprite_->Update();
        if(compare) {
            Label(5,{x+size_.x*.25f+8,demoY+7},70,12,.85f);Label(6,{x+size_.x*.75f-8,demoY+7},70,12,1);
        } else if(kind!=D::Info)Label(7,{x+size_.x-75,demoY+demoH-8},95,12,after_.leftClick?.9f:.3f);
        return;
    }
    if(kind==D::Info) {
        // Generic statistics do not invent extra shots, range or effects.
        const float r=18;Tank({center_.x,demoY+demoH*0.46f},r,0,Tint(accent_,0.70f));
        for(int i=0;i<3;++i)Rect({center_.x-13+static_cast<float>(i)*13,demoY+demoH*0.82f},{9,2+static_cast<float>(i)*3},Tint(accent_,0.42f));
        return;
    }
    if(model_.styleChoice) {
        DrawLane(after_,{x+28,demoY+2},{size_.x-56,demoH-17},true);
        Label(7,{x+size_.x-73,demoY+demoH-8},95,12,after_.leftClick?0.9f:0.3f);
    } else {
        const float laneH=demoH*0.5f;
        Line({x+25,demoY+laneH},{x+size_.x-25,demoY+laneH},1,{0.19f,0.27f,0.36f,0.38f});
        DrawLane(before_,{x+39,demoY},{size_.x-64,laneH},false);
        DrawLane(after_,{x+39,demoY+laneH},{size_.x-64,laneH},true);
        Label(5,{x+37,demoY+8},42,12,0.5f);Label(6,{x+37,demoY+laneH+8},42,12,0.87f);
    }
    Line({x+25,demoY+demoH+3},{x+size_.x-25,demoY+demoH+3},1,{0.2f,0.29f,0.38f,0.34f});
    if(hovered_)Line({x+25,demoY+demoH+3},{x+25+(size_.x-50)*after_.progress,demoY+demoH+3},1.7f,Tint(accent_,0.65f));
}
void TankRewardCard::Draw() {
    if(!spriteCommon_)return;
    spriteCommon_->PreDraw(kNormal);
    for(std::size_t i=0;i<solidCount_;++i)solids_[i]->Draw();
    if(preview_&&preview_->HasRendered()&&previewSprite_)previewSprite_->Draw();
    spriteCommon_->PreDraw(kAdd);
    for(std::size_t i=0;i<glowCount_;++i)glows_[i]->Draw();
    spriteCommon_->PreDraw(kNormal);
    // TextLabel::Draw may rebuild a texture after a font change. Drawing only
    // prepared sprites guarantees no texture creation inside the render pass.
    for(std::size_t i=0;i<labels_.size();++i)if(labelVisible_[i])if(auto* sprite=labels_[i]->GetSprite())sprite->Draw();
}
void TankRewardCard::DrawBloomSource() {
    if(!spriteCommon_)return;
    spriteCommon_->PreDrawForScene(kAdd);
    for(std::size_t i=0;i<glowCount_;++i)glows_[i]->Draw();
}
