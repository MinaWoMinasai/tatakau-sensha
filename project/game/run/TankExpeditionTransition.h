#pragma once
#include <algorithm>
#include <cmath>

namespace tankexp {
// A room is committed once, behind a fully opaque curtain. Gameplay and input
// remain stopped until the reveal finishes; long frames cannot skip that gate.
class PresentationTransition {
public:
    bool Begin() {
        if(active_) return false;
        active_=true;committed_=false;age_=0;return true;
    }
    bool Advance(float dt) {
        if(!active_||!std::isfinite(dt)||dt<=0) return false;
        age_+=(std::min)(dt,0.10f);
        if(!committed_&&age_>=0.50f) {committed_=true;return true;}
        if(age_>=1.05f) active_=false;
        return false;
    }
    bool IsActive() const {return active_;}
    bool IsCommitted() const {return committed_;}
    float Age() const {return age_;}
    float Cover() const {
        if(!active_) return 0;
        return age_<0.65f?Smooth((age_-0.14f)/0.30f):1.0f-Smooth((age_-0.65f)/0.40f);
    }
    float LabelAlpha() const {
        return active_?(std::min)(Smooth(age_/0.12f),1.0f-Smooth((age_-0.76f)/0.29f)):0;
    }
    static float Smooth(float t) {t=(std::clamp)(t,0.0f,1.0f);return t*t*(3-2*t);}
private:
    bool active_=false,committed_=false;
    float age_=0;
};
}
