// SPDX-License-Identifier: MPL-2.0
#include "nav/local/ladder.hpp"
#include <algorithm>
#include <cmath>
namespace astrabot::nav::local {
namespace {
bool terminal(LadderState s) noexcept { return s==LadderState::Complete || s==LadderState::Failed || s==LadderState::Aborted; }
bool same(Binding a,Binding b) noexcept { return a.agent==b.agent && a.actor==b.actor && a.map==b.map && a.routeGeneration==b.routeGeneration && a.step==b.step; }
double distance(model::NavVector3 a,model::NavVector3 b) noexcept { return std::hypot(double(a.x)-b.x,double(a.y)-b.y); }
bool point(enrichment::NavLinkPoint a,model::NavVector3 b) noexcept {
    return std::abs(a.x-b.x)<0.01 && std::abs(a.y-b.y)<0.01 && std::abs(a.z-(double(b.z)-36))<0.01;
}
bool active(ActionRequest a) noexcept { return a==ActionRequest::Press || a==ActionRequest::Hold; }
}
model::NavVector3 Ladder::target(model::NavVector3 origin) const noexcept {
    if(state_==LadderState::Approach || state_==LadderState::Align) return plan_.start;
    if(state_==LadderState::Contact) return plan_.mount;
    if(state_==LadderState::Exit || state_==LadderState::Support) return plan_.end;
    if(state_==LadderState::Reacquire) return {plan_.mount.x,plan_.mount.y,origin.z};
    return plan_.dismount;
}
LadderDecision Ladder::result(LadderReason reason) const noexcept {
    LadderDecision out; out.state=state_; out.reason=reason; out.link=plan_.link; out.reacquires=reacquires_;
    out.intent.forward=out.intent.back=out.intent.jump=out.intent.duck=out.intent.use=ActionRequest::Release; return out;
}
LadderDecision Ladder::finish(LadderState state,LadderReason reason) noexcept {
    const bool first=!terminal(state_); state_=state; auto out=result(reason); out.accepted=out.terminalEvent=first; return out;
}
LadderDecision Ladder::abort() noexcept { return terminal(state_) ? result():finish(LadderState::Aborted,LadderReason::Cancelled); }
LadderDecision Ladder::update(const LadderFeedback& f) noexcept {
    if(terminal(state_)) return result();
    const auto fail=[&](LadderReason r) { return finish(LadderState::Failed,r); };
    const auto positive=[](double v) { return std::isfinite(v) && v>0; };
    const auto& l=plan_.link; const auto& s=f.movement;
    const bool up=l.direction==enrichment::NavLinkDirection::Up;
    if(!binding_.agent.isValid() || !binding_.actor.isValid() || !binding_.map.isValid() || !binding_.routeGeneration ||
       !l.sourceId || !l.generation || !l.linkId || !l.from.isValid() || !l.to.isValid() || l.from==l.to ||
       l.traversal!=model::NavTraversalKind::Ladder || (!up && l.direction!=enrichment::NavLinkDirection::Down) ||
       !plan_.start.isFinite() || !plan_.end.isFinite() || !plan_.mount.isFinite() || !plan_.dismount.isFinite() ||
       !plan_.normal.isFinite() || plan_.normal.z!=0 || std::abs(std::hypot(plan_.normal.x,plan_.normal.y)-1)>0.001 ||
       !point(l.entry,plan_.start) || !point(l.exit,plan_.end) || !std::isfinite(l.additionalCost) || l.additionalCost<0 ||
       !positive(limits_.approachSpeed) || limits_.approachSpeed>200 || !positive(limits_.positionTolerance) ||
       !positive(limits_.heightTolerance) || !positive(limits_.facingDegrees) || limits_.facingDegrees>45 ||
       !positive(limits_.shaftTolerance) || !positive(limits_.maximumHeight) || !positive(limits_.maximumApproach) ||
       !positive(limits_.maximumFallSpeed) || !limits_.maxQueries || limits_.maxQueries>21 || !limits_.approachTimeoutUs || !limits_.contactTimeoutUs ||
       !limits_.climbTimeoutUs || !limits_.exitTimeoutUs || !limits_.supportTimeoutUs || !limits_.reacquireTimeoutUs ||
       (up ? plan_.end.z<=plan_.start.z:plan_.end.z>=plan_.start.z) || std::abs(double(plan_.end.z)-plan_.start.z)>limits_.maximumHeight ||
       distance(plan_.mount,plan_.dismount)>0.01 || distance(plan_.start,plan_.mount)>limits_.maximumApproach ||
       distance(plan_.end,plan_.dismount)>limits_.maximumApproach || std::abs(double(plan_.mount.z)-plan_.start.z)>limits_.heightTolerance ||
       std::abs(double(plan_.dismount.z)-plan_.end.z)>limits_.heightTolerance) return fail(LadderReason::InvalidInput);
    if(!same(f.binding,binding_) || s.agent!=binding_.agent || s.actor!=binding_.actor || s.map!=binding_.map ||
       s.kind!=runtime::ActorKind::ManagedBot || s.connected!=true || s.alive!=true || s.joined!=true)
        return finish(LadderState::Aborted,LadderReason::InvalidActor);
    if(!s.tick.isValid() || (started_ && !s.tick.isAfter(tick_))) return result(LadderReason::StaleTick);
    if(started_ && f.nowUs<=lastUs_) return fail(LadderReason::InvalidInput);
    tick_=s.tick; lastUs_=f.nowUs;
    if(!started_) { started_=true; startedUs_=phaseUs_=f.nowUs; }
    if(!s.position || !s.position->isFinite() || !s.velocity || !s.velocity->isFinite() || !s.view || !s.view->isFinite() ||
       !s.hull || s.hull->minimum!=model::NavVector3{-16,-16,-36} || s.hull->maximum!=model::NavVector3{16,16,36} ||
       !s.grounded || s.ducked!=false || !s.speedLimit || !positive(*s.speedLimit) || !s.ladder || !f.climbing)
        return fail(LadderReason::MissingObservation);
    // MOVETYPE_FLY belongs to the preceding PM update. At the measured exit,
    // contact may already be gone before the next update resets it to WALK.
    // This bounded handoff is not permission to complete or issue exit motion.
    const bool exitHandoff=state_==LadderState::Exit || state_==LadderState::Support ||
        ((state_==LadderState::ClimbUp || state_==LadderState::ClimbDown) &&
         distance(*s.position,plan_.dismount)<=limits_.shaftTolerance &&
         std::abs(double(s.position->z)-plan_.dismount.z)<=limits_.positionTolerance);
    if(s.ladder->sourceId!=l.sourceId || s.ladder->generation!=l.generation || s.ladder->linkId!=l.linkId ||
       (*f.climbing && !s.ladder->touching && !exitHandoff)) return fail(LadderReason::WrongContact);
    const auto* proof=f.inspection ? &*f.inspection:nullptr;
    if(!proof || proof->stamp.agent!=s.agent || proof->stamp.actor!=s.actor || proof->stamp.map!=s.map || proof->stamp.tick!=s.tick ||
       proof->stamp.routeGeneration!=binding_.routeGeneration || proof->stamp.ordinal || proof->step!=binding_.step ||
       proof->sourceId!=l.sourceId || proof->generation!=l.generation || proof->linkId!=l.linkId ||
       !proof->queries || proof->queries>limits_.maxQueries || proof->origin!=*s.position || proof->velocity!=*s.velocity ||
       proof->hull.minimum!=s.hull->minimum || proof->hull.maximum!=s.hull->maximum || proof->target!=target(*s.position)) return fail(LadderReason::StaleInspection);
    const auto supported=[&](model::NavAreaId area) {
        if(s.grounded!=true || !proof->support) return false;
        const auto& p=*proof->support;
        return p.area==area && p.origin.isFinite() && distance(p.origin,*s.position)<0.01 && std::abs(double(p.origin.z)-s.position->z)<0.01 &&
            p.floor.supported && std::isfinite(p.floor.height) && p.floor.normal.isFinite() && p.floor.normal.z>=0.7f &&
            std::abs(double(p.floor.normal.x)*p.floor.normal.x+double(p.floor.normal.y)*p.floor.normal.y+double(p.floor.normal.z)*p.floor.normal.z-1)<=0.02 &&
            std::abs(double(s.position->z)-36-p.floor.height)<=limits_.heightTolerance;
    };
    auto out=result(); out.accepted=true;
    const auto transition=[&](LadderState state) { state_=state; phaseUs_=f.nowUs; out.state=state; out.reacquires=reacquires_; return out; };
    const double yaw=std::atan2(-plan_.normal.y,-plan_.normal.x)*180/3.14159265358979323846;
    const auto move=[&](model::NavVector3 destination) {
        const double range=distance(*s.position,destination);
        if(range>limits_.positionTolerance) { out.intent.direction={(double(destination.x)-s.position->x)/range,(double(destination.y)-s.position->y)/range,0};
            out.intent.speed=(std::min)({limits_.approachSpeed,double(*s.speedLimit),range/0.120}); }
        return out;
    };
    if(s.velocity->z< -limits_.maximumFallSpeed) return fail(LadderReason::Fall);
    if(climbStarted_ && f.nowUs-climbUs_>=limits_.climbTimeoutUs &&
       (state_==LadderState::ClimbUp || state_==LadderState::ClimbDown || state_==LadderState::Reacquire)) return fail(LadderReason::Timeout);
    if(state_==LadderState::Approach || state_==LadderState::Align) {
        if(f.nowUs-startedUs_>=limits_.approachTimeoutUs) return fail(LadderReason::Timeout);
        if(!supported(l.from)) return fail(LadderReason::MissingSupport);
        if(distance(*s.position,plan_.start)>limits_.maximumApproach) return fail(LadderReason::InvalidInput);
        if(state_==LadderState::Approach) {
            if(distance(*s.position,plan_.start)<=limits_.positionTolerance) return transition(LadderState::Align);
            if(proof->pathClear!=true) return fail(LadderReason::Blocked);
            return move(plan_.start);
        }
        if(distance(*s.position,plan_.start)>limits_.positionTolerance) return fail(LadderReason::WrongContact);
        out.intent.view=core::IntentVector{0,yaw,0};
        if(std::abs(std::remainder(double(s.view->y)-yaw,360))<=limits_.facingDegrees) return transition(LadderState::Contact);
        return out;
    }
    if(state_==LadderState::Contact || state_==LadderState::Reacquire) {
        if(distance(*s.position,plan_.mount)>limits_.maximumApproach) return fail(LadderReason::WrongContact);
        if(*f.climbing && s.ladder->touching) {
            if(!climbStarted_) { climbStarted_=true; climbUs_=f.nowUs; }
            return transition(up ? LadderState::ClimbUp:LadderState::ClimbDown);
        }
        const auto timeout=state_==LadderState::Contact ? limits_.contactTimeoutUs:limits_.reacquireTimeoutUs;
        if(f.nowUs-phaseUs_>=timeout) return fail(LadderReason::Timeout);
        if(proof->pathClear!=true) return fail(LadderReason::Blocked);
        out.intent.view=core::IntentVector{0,yaw,0}; return move(target(*s.position));
    }
    if(state_==LadderState::ClimbUp || state_==LadderState::ClimbDown) {
        if(distance(*s.position,plan_.mount)>limits_.shaftTolerance) return fail(LadderReason::WrongContact);
        const double remaining=(up ? 1:-1)*(double(plan_.dismount.z)-s.position->z);
        if(remaining<=limits_.positionTolerance && remaining>=-limits_.heightTolerance) return transition(LadderState::Exit);
        if(remaining< -limits_.heightTolerance) return fail(LadderReason::WrongLanding);
        if(!*f.climbing || !s.ladder->touching) {
            if(reacquires_) return fail(LadderReason::ReacquireExhausted);
            ++reacquires_; return transition(LadderState::Reacquire);
        }
        if(proof->pathClear!=true) return fail(LadderReason::Blocked);
        // Standard CS ladder buttons move at min(200,maxspeed). Pitch changes
        // the vertical component; analog magnitude alone cannot slow climbing.
        const double speed=(std::min)(200.0,double(*s.speedLimit));
        const double ratio=(std::min)(1.0,remaining/(0.120*speed));
        const double pitch=std::acos(ratio/std::sqrt(2.0))*180/3.14159265358979323846-45;
        out.intent.view=core::IntentVector{pitch,yaw,0}; out.intent.direction={-plan_.normal.x,-plan_.normal.y,0}; out.intent.speed=speed;
        if(up) out.intent.forward=ActionRequest::Hold; else out.intent.back=ActionRequest::Hold;
        return out;
    }
    if(state_==LadderState::Exit) {
        if(f.nowUs-phaseUs_>=limits_.exitTimeoutUs) return fail(LadderReason::Timeout);
        const double relativeHeight=double(s.position->z)-plan_.end.z;
        if(relativeHeight< -limits_.heightTolerance || relativeHeight>(up ? 96:limits_.heightTolerance)) return fail(LadderReason::WrongLanding);
        if(s.grounded==true && std::abs(relativeHeight)<=limits_.heightTolerance &&
           distance(*s.position,plan_.end)<=limits_.positionTolerance) return transition(LadderState::Support);
        if(proof->pathClear!=true) return fail(LadderReason::Blocked);
        if(*f.climbing || s.ladder->touching || (up && s.grounded!=true)) {
            if(!proof->exitIntent || !core::Motor::valid(*proof->exitIntent) || active(proof->exitIntent->jump) ||
               active(proof->exitIntent->duck) || active(proof->exitIntent->use)) return fail(LadderReason::MissingObservation);
            out.intent=*proof->exitIntent; return out;
        }
        return move(plan_.end);
    }
    if(state_==LadderState::Support) {
        if(f.nowUs-phaseUs_>=limits_.supportTimeoutUs) return fail(LadderReason::Timeout);
        if(distance(*s.position,plan_.end)>limits_.positionTolerance || std::abs(double(s.position->z)-plan_.end.z)>limits_.heightTolerance)
            return fail(LadderReason::WrongLanding);
        if(*f.climbing || s.ladder->touching) return out; // Support alone is not proof of dismount.
        if(s.grounded==true) return supported(l.to) ? finish(LadderState::Complete,LadderReason::None):fail(LadderReason::MissingSupport);
        return out;
    }
    return fail(LadderReason::InvalidInput);
}
}
