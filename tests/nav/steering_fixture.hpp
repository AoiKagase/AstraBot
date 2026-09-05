// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/runtime/world_query.hpp"
#include <algorithm>
namespace steering_fixture {
// Independent synthetic physics. Bounds describe the hull center after Minkowski expansion.
inline astrabot::nav::runtime::HullObservation sweep(int mode,astrabot::nav::model::NavVector3 a,
    astrabot::nav::model::NavVector3 b) {
    using namespace astrabot::nav;
    runtime::HullObservation h{1,b,{},false};
    if(mode==0) {
        if(a.y<48 || a.y>52) { h.startSolid=true; return h; }
        if(b.y>52) { h.fraction=(52-a.y)/(b.y-a.y); h.normal={0,-1,0}; }
        if(b.y<48) { h.fraction=(48-a.y)/(b.y-a.y); h.normal={0,1,0}; }
    } else {
        const float low[]{74,mode==2 ? -100.0f:28.0f},high[]{112,mode==2 ? 200.0f:72.0f};
        const float p[]{a.x,a.y},d[]{b.x-a.x,b.y-a.y};
        double enter=0,leave=1; int axis=-1; float normal=0; bool misses=false;
        for(int i=0;i<2;++i) {
            if(d[i]==0) { if(p[i]<=low[i] || p[i]>=high[i]) misses=true; continue; }
            double first=(low[i]-p[i])/d[i],last=(high[i]-p[i])/d[i];
            const float n=d[i]>0 ? -1.0f:1.0f;
            if(first>last) std::swap(first,last);
            if(first>enter) { enter=first; axis=i; normal=n; }
            leave=(std::min)(leave,last);
        }
        if(!misses && enter<leave && enter<1 && axis>=0) {
            h.fraction=static_cast<float>(enter);
            h.normal=axis==0 ? model::NavVector3{normal,0,0}:model::NavVector3{0,normal,0};
        }
    }
    h.end={a.x+(b.x-a.x)*h.fraction,a.y+(b.y-a.y)*h.fraction,a.z+(b.z-a.z)*h.fraction};
    return h;
}
}
