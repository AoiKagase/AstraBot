// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "nav/local/ground_probe.hpp"
#include <cmath>

namespace astrabot::nav::local {
// NAV guides XY travel. Height is proved from current physical support below,
// never from interpolation between a stair's endpoints.
inline bool groundSegmentAllows(model::NavVector3 start,model::NavVector3 end,
    model::NavVector3 position,double travel) noexcept {
    if(!start.isFinite() || !end.isFinite() || !position.isFinite() || !std::isfinite(travel) || travel<0) return false;
    const double dx=double(end.x)-start.x,dy=double(end.y)-start.y,length=std::hypot(dx,dy);
    if(length<=0) return false;
    const double px=double(position.x)-start.x,py=double(position.y)-start.y;
    const double along=(px*dx+py*dy)/length,lateral=std::abs(px*dy-py*dx)/length;
    return along>=-0.01 && along<=length && lateral<=0.5 && along+travel<=length+0.001;
}

inline ProbeResult inspectGroundFrame(const runtime::MovementSnapshot& s,std::uint64_t generation,
    model::NavAreaId source,model::NavAreaId target,float x,float y,const query::NavSpatialIndex& index,
    core::MapGeneration map,runtime::IWorldQueries& port,GroundProbeLimits limits) noexcept {
    // Restrict every sampled support, not only the endpoint, to this route step.
    struct CorridorQueries final : runtime::IWorldQueries {
        runtime::IWorldQueries& port;
        const query::NavSpatialIndex& index;
        model::NavAreaId source,target;
        bool outside{};
        std::uint32_t issued{};
        CorridorQueries(runtime::IWorldQueries& p,const query::NavSpatialIndex& i,
            model::NavAreaId a,model::NavAreaId b) : port(p),index(i),source(a),target(b) {}
        runtime::WorldQueryResult query(const runtime::QueryRequest& original) override {
            auto q=original; q.stamp.ordinal=++issued;
            auto r=port.query(q);
            if(r.stamp==q.stamp && r.kind==q.kind && r.error==runtime::QueryError::None) {
                std::optional<model::NavAreaId> area;
                if(r.ground) area=r.ground->area;
                if(r.floor) {
                    auto match=index.containing({q.end.x,q.end.y,r.floor->height},q.navTolerance);
                    if(match && *match.value) area=(**match.value).areaId;
                }
                if(area && *area!=source && *area!=target) {
                    outside=true; r.ground.reset(); r.floor.reset();
                }
            }
            if(r.stamp==q.stamp) r.stamp=original.stamp;
            return r;
        }
    } queries(port,index,source,target);
    ProbeResult result;
    if(limits.maxQueries<2) { result.reason=ProbeReason::BudgetExceeded; return result; }
    auto ground=GroundProbe::locate(s,generation,index,map,queries,limits);
    if(!ground) return ground;
    limits.maxQueries-=ground.queries;
    result=GroundProbe::inspect(s,generation,ground.target->area,x,y,index,map,queries,limits);
    result.queries+=ground.queries;
    if(queries.outside) { result.reason=ProbeReason::NoArea; result.target.reset(); }
    return result;
}
}
