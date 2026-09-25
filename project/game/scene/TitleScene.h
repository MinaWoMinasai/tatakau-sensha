#pragma once
#include "Player.h"
#include "Fade.h"
#include "IScene.h"
#include "game/ui/NeonTextEffect.h"
#include <array>
#include <string_view>

class GameScene;
class TitleScene : public IScene {
public:
    TitleScene();
    ~TitleScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawShadow() override;
    void DrawPostEffect3D() override;
    void DrawAfterPostEffect3D() override;
    void DrawSprite() override;
    float GetPostGaussianIntensity() const override { return 0.88f; }
    ScreenEffectState GetScreenEffectState() const override;
    bool IsFinished() const override { return finished_; }
    std::string GetNextSceneName() const override { return nextSceneName_; }
private:
    bool IsSceneAvailable(std::string_view name) const;
    bool StartTransitionIfAvailable(std::string_view name,float duration);
    void UpdateMenuVisuals();
    bool IsMenuAvailable(int selection) const;
    int HitTestMenu(const Vector2& mouse) const;
    void WriteDemoValidation(bool fadeComplete);
    std::unique_ptr<GameScene> demo_;
    std::unique_ptr<Fade> fade_;
    std::unique_ptr<Sprite> backgroundVeil_;
    Input* input_=nullptr;
    std::unique_ptr<TextLabel> title_,subtitle_,hint_,demoCaption_;
    std::array<std::unique_ptr<TextLabel>,3> menu_;
    struct DemoSample {
        std::string build;
        int shots=0,kills=0,dashes=0;
        float maxHomingTurnRate=0;
        uint32_t maxDashExplosionsEmitted=0;
        uint64_t wallBouncesObserved=0,actorPiercesObserved=0,splitChildrenObserved=0;
    };
    std::array<DemoSample,4> stageSamples_{};
    std::unique_ptr<NeonTextEffect> titleTextNeonEffect_;
    Phase phase_=Phase::kFadeIn;
    std::string nextSceneName_;
    bool finished_=false,demoFrozen_=false,autoTest_=false;
    uint32_t capturedStages_=0;
    float frozenAt_=0,blinkTimer_=0;
    int menuSelection_=0;
    Vector2 previousMousePosition_{};
};
