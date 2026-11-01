#include "eureka_visual_clock.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
    using pocket_amp::EurekaVisualClock;
    EurekaVisualClock a, b;
    // Silence should not produce periodic animation changes.
    for (int i=0;i<300;++i) {
        const auto r=a.step(0.0f,false,0.5f);
        assert(r.frame == 0 && !r.changed && !r.cut && r.speed_fps == 0.0f);
    }
    const auto first=a.step(0.2f,true,0.8f);
    assert(first.cut && first.frame>=0 && first.frame<396);
    int cut_count = 1;
    bool changed = first.changed;
    // Independent copies with the same known input trace are deterministic.
    for (int i=0;i<300;++i) b.step(0.0f,false,0.5f);
    const auto first_b=b.step(0.2f,true,0.8f);
    assert(first_b.frame==first.frame && first_b.cut==first.cut);
    for (int i=0;i<12000;++i) {
        const float level=(i%117<96)?0.24f:0.0f;
        const float x = (i%11)/10.0f;
        const auto p=a.step(level,false,x), q=b.step(level,false,x);
        assert(p.frame>=0 && p.frame<396);
        assert(p.frame==q.frame && p.cut==q.cut && p.changed==q.changed);
        assert(std::isfinite(p.speed_fps) && p.speed_fps>=0.0f && p.speed_fps<=4.5f);
        if(p.cut) ++cut_count;
        if(p.changed) changed=true;
    }
    assert(changed && cut_count>3);
    // Insufficient material cannot access a nonexistent frame.
    EurekaVisualClock empty(0);
    const auto e=empty.step(0.6f,true,1.0f);
    assert(e.frame==0 && !e.changed);
    std::printf("PASS: deterministic Eureka scheduler, silence, range, cut, bounds; cuts=%d\n",cut_count);
}
