// SPDX-License-Identifier: MPL-2.0
#include "adapter/cstrike/vision.hpp"
#include "adapter/metamod/lifecycle.hpp"
#include <cmath>
#include <limits>

namespace astrabot::adapter::cstrike {
namespace {
namespace p = core::perception;
bool connected(edict_t* e) noexcept {
    return e && !e->free && (e->v.flags & (FL_CLIENT | FL_FAKECLIENT)) != 0;
}
bool alive(edict_t* e) noexcept {
    return connected(e) && e->v.deadflag == DEAD_NO && std::isfinite(e->v.health) &&
        e->v.health > 0 && e->v.iuser1 == 0 && (e->v.flags & FL_SPECTATOR) == 0;
}
p::Point point(const Vector& v) noexcept { return {v.x,v.y,v.z}; }
p::Point eye(edict_t* e) noexcept {
    return {double(e->v.origin.x)+e->v.view_ofs.x,
        double(e->v.origin.y)+e->v.view_ofs.y, double(e->v.origin.z)+e->v.view_ofs.z};
}
// Uses local trigonometry: pfnMakeVectors would mutate global engine vectors.
p::Point forward(const Vector& v) noexcept {
    constexpr double radians = 3.14159265358979323846/180;
    if (!p::finite(point(v))) return {(std::numeric_limits<double>::quiet_NaN)(),0,0};
    const double pitch = double(v.x)*radians, yaw = double(v.y)*radians;
    return {std::cos(pitch)*std::cos(yaw), std::cos(pitch)*std::sin(yaw), -std::sin(pitch)};
}
struct FrameEntity { edict_t* entity{}; int serial{}; core::PlayerId player{}; };
class Queries final : public p::IVisibilityQueries {
public:
    metamod::LifecycleCoordinator& owner;
    enginefuncs_t& engine;
    std::array<FrameEntity,p::kPlayerCapacity> entities{};
    Queries(metamod::LifecycleCoordinator& o, enginefuncs_t& e) noexcept : owner(o),engine(e) {}
    bool current(core::PlayerId player, const p::Stamp& stamp) const noexcept {
        if (!player.isValid() || player.slot > entities.size()) return false;
        const auto& b = entities[player.slot-1U];
        return owner.registry().isMapActive() && owner.registry().mapGeneration() == stamp.map &&
            owner.registry().currentTick() == stamp.tick && b.player == player &&
            owner.registry().currentPlayer(player.slot) == player && alive(b.entity) &&
            b.entity->serialnumber == b.serial && engine.pfnPEntityOfEntIndex &&
            engine.pfnPEntityOfEntIndex(player.slot) == b.entity && !owner.removalPending(player);
    }
    p::Reason trace(const p::SightRequest& request) noexcept override {
        if (!engine.pfnTraceLine) return p::Reason::MissingEngine;
        if (!current(request.stamp.observer,request.stamp) || !current(request.target,request.stamp))
            return p::Reason::StaleIdentity;
        if (!p::finite(request.start) || !p::finite(request.end)) return p::Reason::InvalidGeometry;
        const float start[]{static_cast<float>(request.start.x),static_cast<float>(request.start.y),static_cast<float>(request.start.z)};
        const float end[]{static_cast<float>(request.end.x),static_cast<float>(request.end.y),static_cast<float>(request.end.z)};
        for (int i=0; i<3; ++i)
            if (!std::isfinite(start[i]) || !std::isfinite(end[i])) return p::Reason::InvalidGeometry;
        // Initialize required outputs to invalid sentinels; a silent engine
        // stub/malformed result cannot become an unobstructed observation.
        TraceResult result{};
        result.flFraction = (std::numeric_limits<float>::quiet_NaN)();
        result.vecEndPos = Vector(result.flFraction,result.flFraction,result.flFraction);
        auto* observer = entities[request.stamp.observer.slot-1U].entity;
        auto* target = entities[request.target.slot-1U].entity;
        engine.pfnTraceLine(start,end,0,observer,&result); // collide with players and glass
        if (!current(request.stamp.observer,request.stamp) || !current(request.target,request.stamp))
            return p::Reason::StaleIdentity;
        if (!std::isfinite(result.flFraction) || result.flFraction < 0 || result.flFraction > 1 ||
            !p::finite(point(result.vecEndPos)) ||
            (result.fStartSolid != 0 && result.fStartSolid != 1) ||
            (result.fAllSolid != 0 && result.fAllSolid != 1)) return p::Reason::InvalidTrace;
        if (result.fStartSolid || result.fAllSolid) return p::Reason::Occluded;
        const double fraction = result.flFraction;
        if (std::hypot(double(result.vecEndPos.x)-(start[0]+(double(end[0])-start[0])*fraction),
                       double(result.vecEndPos.y)-(start[1]+(double(end[1])-start[1])*fraction),
                       double(result.vecEndPos.z)-(start[2]+(double(end[2])-start[2])*fraction)) > 0.125)
            return p::Reason::InvalidTrace;
        if (result.pHit == observer) return p::Reason::InvalidTrace;
        if (result.pHit == target) return p::Reason::None;
        if (result.flFraction < 1 || result.pHit) return p::Reason::Occluded;
        // The engine must actually have reached the requested endpoint.
        if (std::hypot(double(result.vecEndPos.x)-end[0],double(result.vecEndPos.y)-end[1],
                       double(result.vecEndPos.z)-end[2]) > 0.125) return p::Reason::InvalidTrace;
        return p::Reason::None;
    }
};
}
void VisionAdapter::reset() noexcept {
    ++revision_; memory_.reset();
    roster_ = {}; vision_.reset(); map_ = {}; error_ = p::Reason::None;
}
void VisionAdapter::forget(core::PlayerId player) noexcept {
    ++revision_; memory_.forget(player);
    vision_.forget(player);
    if (player.isValid() && player.slot <= roster_.size() && roster_[player.slot-1U].player == player)
        roster_[player.slot-1U] = {};
}
void VisionAdapter::frame(metamod::LifecycleCoordinator& owner, enginefuncs_t* engine, float time) noexcept {
    error_ = p::Reason::None;
    auto& registry = owner.registry();
    if (!registry.isMapActive()) { reset(); return; }
    if (map_ != registry.mapGeneration()) { reset(); map_ = registry.mapGeneration(); }
    if (!engine || !engine->pfnPEntityOfEntIndex || !engine->pfnIndexOfEdict) {
        memory_.invalidate(core::world::MemoryReason::MissingEngine);
        vision_.reset(); error_ = p::Reason::MissingEngine; return;
    }
    const double micros = double(time)*1000000;
    if (!std::isfinite(micros) || micros < 0 || micros >= 18446744073709551616.0) {
        memory_.invalidate(core::world::MemoryReason::InvalidFrame);
        vision_.reset(); error_ = p::Reason::InvalidFrame; return;
    }
    p::InputFrame input{};
    input.map = map_; input.tick = registry.currentTick(); input.timeMicros = static_cast<std::uint64_t>(micros);
    Queries queries(owner,*engine);
    const auto revision = revision_;
    for (std::uint16_t slot=1; slot<=registry.clientMax(); ++slot) {
        const auto i = static_cast<std::size_t>(slot-1U);
        auto* entity = engine->pfnPEntityOfEntIndex(slot);
        auto player = registry.currentPlayer(slot);
        const auto binding = owner.agents().findByPlayer(player);
        const bool managed = binding.isValid();
        const bool valid = connected(entity) && engine->pfnIndexOfEdict(entity) == slot;
        const auto prior = roster_[i];
        if (prior.player.isValid() && (!valid || prior.entity != entity || prior.serial != entity->serialnumber || prior.player != player)) {
            vision_.forget(prior.player);
            memory_.forget(prior.player);
            // Never steal lifecycle ownership of a managed bot.
            if (!managed && player == prior.player) {
                (void)registry.disconnectPlayer(player); player = {};
            }
            roster_[i] = {};
        }
        if (!valid) continue;
        if (managed && (owner.entityFor(player) != entity || owner.removalPending(player))) continue;
        if (!player.isValid()) {
            // Perception discovers only observation-eligible foreign players.
            // A dormant/dead slot is not evidence of a newly joined player and
            // must not preempt another subsystem's pending registration.
            if (!alive(entity)) continue;
            const auto registered = registry.registerPlayer(slot);
            if (!registered) continue;
            player = registered.event.player;
        }
        roster_[i] = {entity,entity->serialnumber,player};
        queries.entities[i] = {entity,entity->serialnumber,player};
        auto& sample = input.players[i];
        sample.player = player; sample.alive = alive(entity);
        if (managed) {
            const auto* join = owner.joinState(player);
            sample.alive = sample.alive && join && join->phase() == JoinPhase::Joined;
            if (sample.alive) sample.agent = binding.agent;
        }
        if (!sample.alive) continue;
        sample.eye = eye(entity);
        sample.center = {double(entity->v.origin.x)+(double(entity->v.mins.x)+entity->v.maxs.x)/2,
            double(entity->v.origin.y)+(double(entity->v.mins.y)+entity->v.maxs.y)/2,
            double(entity->v.origin.z)+(double(entity->v.mins.z)+entity->v.maxs.z)/2};
        sample.forward = forward(entity->v.v_angle);
    }
    vision_.update(input,queries);
    // Engine callbacks may retire a different candidate/observer while tracing.
    // Revalidate before consumers can read any publication from this frame.
    core::world::MemoryFrame memoryFrame{};
    memoryFrame.map = input.map; memoryFrame.tick = input.tick; memoryFrame.timeMicros = input.timeMicros;
    for (const auto& sample : input.players) {
        if (!sample.player.isValid()) continue;
        const p::Stamp stamp{{},sample.player,input.map,input.tick,input.timeMicros};
        if (!queries.current(sample.player,stamp)) {
            vision_.forget(sample.player); memory_.forget(sample.player);
        } else {
            memoryFrame.players[sample.player.slot-1U] = {sample.player,sample.agent,sample.alive};
        }
    }
    error_ = vision_.frameReason();
    // A lifecycle callback may have reset/retired data during any engine query.
    if (revision_ != revision) return;
    if (error_ == p::Reason::InvalidFrame || error_ == p::Reason::InvalidSettings) {
        memory_.invalidate(core::world::MemoryReason::InvalidFrame); return;
    }
    if (!memory_.advance(memoryFrame)) return;
    for (const auto& player : memoryFrame.players) {
        if (!player.eligible || !player.agent.isValid()) continue;
        const auto* batch = vision_.latest(player.player);
        // Cached publications are not new evidence. Decay still ran above.
        if (batch && batch->stamp.tick == input.tick) (void)memory_.observe(*batch);
    }
}
} // namespace astrabot::adapter::cstrike
