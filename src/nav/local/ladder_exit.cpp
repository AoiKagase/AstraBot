// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ladder_exit.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot::nav::local {
namespace {
LadderExitResult planExit(const LadderPlan& p,const runtime::MovementSnapshot& s,bool touching,
    double releaseZ,LadderAirPhysics physics,std::uint8_t msec,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,bool jumping,std::optional<core::BotCommand> firstCommand) noexcept {
    LadderExitResult result;
    const auto fail=[&](LadderExitReason reason) { result.reason=reason; return result; };
    if(!s.map.isValid() || !s.tick.isValid() || !s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() ||
       !s.hull || s.hull->minimum!=model::NavVector3{-16,-16,-36} || s.hull->maximum!=model::NavVector3{16,16,36} ||
       s.ducked!=false || !p.end.isFinite() || !p.normal.isFinite() || p.normal.z!=0 ||
       std::abs(double(p.normal.x)*p.normal.x+double(p.normal.y)*p.normal.y-1)>0.001 ||
       !std::isfinite(releaseZ) || releaseZ<p.end.z || releaseZ>double(p.end.z)+96 || !msec || msec>120 ||
       (firstCommand && firstCommand->msec!=msec) || (jumping && s.grounded!=false) || std::abs(double(s.position->z)-p.end.z)>96 ||
       std::hypot(double(s.position->x)-p.end.x,double(s.position->y)-p.end.y)>96) return result;
    if(s.map!=indexMap) return fail(LadderExitReason::StaleNavigation);
    if(p.link.traversal!=model::NavTraversalKind::Ladder ||
       (p.link.direction!=enrichment::NavLinkDirection::Up && (!jumping || p.link.direction!=enrichment::NavLinkDirection::Down)) ||
       !p.link.to.isValid()) return fail(LadderExitReason::Unsupported);
    core::BotCommand neutral; neutral.msec=msec;
    if(!ladderAirStep(neutral,*s.velocity,physics)) return result;
    LadderExitCandidate candidate;
    auto position=*s.position,velocity=*s.velocity;
    auto low=position,high=position;
    const auto include=[&](model::NavVector3 v) {
        low.x=(std::min)(low.x,v.x); low.y=(std::min)(low.y,v.y); low.z=(std::min)(low.z,v.z);
        high.x=(std::max)(high.x,v.x); high.y=(std::max)(high.y,v.y); high.z=(std::max)(high.z,v.z);
    };
    const auto flush=[&]() {
        const unsigned nx=high.x==low.x ? 1:2,ny=high.y==low.y ? 1:2;
        if(candidate.columnCount+nx*ny>candidate.columns.size()) return false;
        for(unsigned x=0;x<nx;++x) for(unsigned y=0;y<ny;++y) {
            const float px=x ? high.x:low.x,py=y ? high.y:low.y;
            candidate.columns[candidate.columnCount++]={{px,py,low.z+0.05f},{px,py,high.z+0.05f}};
        }
        return true;
    };
    bool attached=touching;
    constexpr double pi=3.14159265358979323846;
    for(unsigned frame=0;frame<256 && (frame+1)*unsigned(msec)<=2000;++frame) {
        MovementIntent intent;
        if(attached && jumping) {
            intent.jump=ActionRequest::Press;
        } else if(attached) {
            intent.view=core::IntentVector{-45,std::atan2(-p.normal.y,-p.normal.x)*180/pi,0};
            intent.direction={-p.normal.x,-p.normal.y,0}; intent.speed=(std::min)(physics.maximumSpeed,200.0);
            intent.forward=ActionRequest::Hold;
        } else {
            double x=double(p.end.x)-position.x,y=double(p.end.y)-position.y;
            const double distance=std::hypot(x,y);
            if(distance>0.001) {
                x/=distance; y/=distance;
                // Cancel lateral momentum while adding a forward component.
                // Fixed60-degree wish alternation can build forward air speed
                // without pretending the30 component cap is a total-speed cap.
                const double side=(-y*velocity.x+x*velocity.y)>0 ? -1:1;
                intent.direction={0.5*x-side*std::sqrt(0.75)*y,0.5*y+side*std::sqrt(0.75)*x,0};
                intent.speed=(std::min)(physics.maximumSpeed,double(core::kMaxMovement));
            }
            intent.view=core::IntentVector{0,0,0};
        }
        auto motor=core::Motor::command(intent,{},static_cast<float>(physics.maximumSpeed),std::uint64_t(msec)*1000,true);
        if(!motor) return result;
        if(frame==0 && firstCommand) motor.command=firstCommand;
        if(frame==0) { candidate.suppliedFirstCommand=firstCommand.has_value(); candidate.intent=firstCommand ? MovementIntent{}:intent; candidate.command=*motor.command; }
        model::NavVector3 displacement{};
        if(attached && (motor.command->buttons&static_cast<core::ButtonMask>(core::Button::Jump))) {
            const auto step=ladderJumpAirStep(*motor.command,p.normal,physics);
            if(!step) return result;
            displacement=step->displacement; velocity=step->velocity; attached=false;
        } else if(attached) {
            const auto v=ladderVelocity(*motor.command,p.normal,physics.maximumSpeed,false);
            if(!v || v->z<=0) return fail(LadderExitReason::Unsupported);
            velocity=*v; const double dt=double(msec)/1000;
            displacement={static_cast<float>(v->x*dt),static_cast<float>(v->y*dt),static_cast<float>(v->z*dt)};
        } else {
            const auto step=ladderAirStep(*motor.command,velocity,physics);
            if(!step) return result;
            displacement=step->displacement; velocity=step->velocity;
        }
        model::NavVector3 next{position.x+displacement.x,position.y+displacement.y,position.z+displacement.z};
        if(!next.isFinite() || double(next.z)-p.end.z>96) return fail(LadderExitReason::Unsupported);
        if(jumping && displacement.x*p.normal.x+displacement.y*p.normal.y<0)
            return fail(LadderExitReason::Unsupported); // No forecasted re-entry into the ladder face.
        const bool landing=!attached && displacement.z<0 && position.z>=p.end.z && next.z<=p.end.z;
        if(landing) {
            const double fraction=(double(position.z)-p.end.z)/-displacement.z;
            next={static_cast<float>(position.x+displacement.x*fraction),static_cast<float>(position.y+displacement.y*fraction),p.end.z};
        }
        if(std::abs(double(next.x)-position.x)>31 || std::abs(double(next.y)-position.y)>31)
            return fail(LadderExitReason::Unsupported);
        // Four vertical standing-hull sweeps cover an entire box whenever its
        // XY spans are below the32-unit hull width; successive boxes share an
        // endpoint. Vertical extent is covered continuously by each sweep.
        if((std::max)(high.x,next.x)-(std::min)(low.x,next.x)>31 ||
           (std::max)(high.y,next.y)-(std::min)(low.y,next.y)>31) {
            if(!flush()) return fail(LadderExitReason::BudgetExceeded);
            low=high=position;
        }
        include(next); ++candidate.simulatedFrames;
        if(landing) {
            const auto area=index.containing({next.x,next.y,next.z-36},2);
            if(!area || !area.value->has_value() || (**area.value).areaId!=p.link.to) return fail(LadderExitReason::NoLanding);
            if(!flush()) return fail(LadderExitReason::BudgetExceeded);
            candidate.landing=next; result.reason=LadderExitReason::None; result.value=candidate; return result;
        }
        position=next;
        if(attached && position.z>releaseZ) {
            if(!flush()) return fail(LadderExitReason::BudgetExceeded);
            low=high=position; attached=false;
        }
    }
    return fail(LadderExitReason::NoLanding);
}
}
LadderExitResult planUpperLadderExit(const LadderPlan& p,const runtime::MovementSnapshot& s,bool touching,
    double releaseZ,LadderAirPhysics physics,std::uint8_t msec,const query::NavSpatialIndex& index,
    core::MapGeneration indexMap,std::optional<core::BotCommand> firstCommand) noexcept {
    return planExit(p,s,touching,releaseZ,physics,msec,index,indexMap,false,firstCommand);
}
LadderExitResult planJumpLadderExit(const LadderPlan& p,const runtime::MovementSnapshot& s,bool touching,
    LadderAirPhysics physics,std::uint8_t msec,const query::NavSpatialIndex& index,core::MapGeneration indexMap,
    std::optional<core::BotCommand> firstCommand) noexcept {
    return planExit(p,s,touching,p.end.z,physics,msec,index,indexMap,true,firstCommand);
}
}
