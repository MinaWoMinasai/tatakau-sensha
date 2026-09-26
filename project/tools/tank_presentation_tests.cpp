#include "../game/run/TankExpeditionTransition.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    tankexp::PresentationTransition t;
    assert(!t.IsActive()&&!t.Advance(1));
    assert(t.Begin()&&!t.Begin());
    assert(!t.Advance(-1)&&!t.Advance(std::numeric_limits<float>::quiet_NaN()));
    assert(t.Age()==0);
    int commits=0;bool revealSeen=false;
    for(int i=0;i<80;++i) {
        const bool commit=t.Advance(1.0f/60);
        if(commit) {++commits;assert(t.Cover()==1.0f&&t.IsActive());}
        if(t.IsCommitted()&&t.IsActive()&&t.Cover()<1) revealSeen=true;
        assert(t.Cover()>=0&&t.Cover()<=1);
        assert(t.LabelAlpha()>=0&&t.LabelAlpha()<=1);
    }
    assert(commits==1&&revealSeen&&!t.IsActive());
    assert(t.Begin());commits=0;
    // A hitch must not skip the visible warning, commit twice or unfreeze early.
    for(int i=0;i<11;++i) {if(t.Advance(60)) {++commits;assert(t.IsActive()&&t.Cover()==1);}}
    assert(commits==1&&!t.IsActive());
    std::cout<<"Presentation: single opaque commit, input gate, reveal, reuse, invalid dt and hitches passed.\n";
}
