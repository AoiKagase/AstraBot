// SPDX-License-Identifier: MPL-2.0
#include "debug/nav_command.hpp"
#include <charconv>
#include <cstdio>
namespace astrabot::debug {
std::optional<nav::model::NavAreaId> parseNavGoal(std::string_view text) noexcept {
    if (text.empty() || text.size()>10 || text.front()<'0' || text.front()>'9') return {};
    std::uint32_t value{};
    const auto r=std::from_chars(text.data(),text.data()+text.size(),value);
    if (r.ec!=std::errc{} || r.ptr!=text.data()+text.size() || !value) return {};
    return nav::model::NavAreaId{value};
}
namespace {
const char* reasonName(nav::runtime::SessionReason reason) noexcept {
    using R=nav::runtime::SessionReason;
    switch(reason) {
#define RNAME(n) case R::n: return #n;
    RNAME(None) RNAME(InvalidActor) RNAME(ActorChanged) RNAME(MapChanged)
    RNAME(Disconnected) RNAME(Dead) RNAME(NotJoined) RNAME(InvalidSnapshot)
    RNAME(StaleSnapshot) RNAME(MissingGraph) RNAME(InvalidGoal)
    RNAME(QueryBudgetExceeded) RNAME(StaleQuery) RNAME(QueryFailed)
    RNAME(UnknownGround) RNAME(NoCurrentArea) RNAME(Unreachable)
    RNAME(ExpansionLimit) RNAME(NavFailure) RNAME(AllocationFailure)
    RNAME(GoalReplaced) RNAME(Cancelled) RNAME(GenerationExhausted)
#undef RNAME
    }
    return "Unknown";
}
const char* stateName(nav::runtime::SessionState state) noexcept {
    using S=nav::runtime::SessionState;
    switch(state) {
    case S::Idle:return "Idle"; case S::Ready:return "Ready";
    case S::Failed:return "Failed"; case S::Cancelled:return "Cancelled";
    }
    return "Unknown";
}
}
void printNavTrace(const nav::runtime::DecisionTrace& t, NavLineSink sink, void* ctx) noexcept {
    if (!sink) return;
    char line[512]{};
    std::snprintf(line,sizeof(line),"nav actor=%u:%u agent=%u map=%u tick=%llu route=%llu current=%u goal=%u state=%s reason=%s terminal=%u arrival=unverified nav_error=%u field=%u offset=%llu",
        unsigned(t.actor.slot),unsigned(t.actor.generation.value),unsigned(t.agent.value),unsigned(t.map.value),
        static_cast<unsigned long long>(t.tick.value),static_cast<unsigned long long>(t.routeGeneration),
        t.currentArea ? unsigned(t.currentArea->value):0U,unsigned(t.goal.value),stateName(t.state),reasonName(t.reason),unsigned(t.terminal),
        unsigned(t.navError.kind),unsigned(t.navError.field),static_cast<unsigned long long>(t.navError.offset));
    sink(ctx,line);
    if (!t.route) return;
    const auto& route=*t.route;
    char cost[64]{};
    const auto converted=std::to_chars(cost,cost+sizeof(cost)-1,route.total,std::chars_format::general,17);
    if(converted.ec!=std::errc{}) return;
    const char* status=route.status==nav::query::NavRouteStatus::Complete ? "Complete":
        route.status==nav::query::NavRouteStatus::Unreachable ? "Unreachable":"ExpansionLimit";
    const auto count=route.steps.size()<64 ? route.steps.size():64;
    std::snprintf(line,sizeof(line),"nav cost=%s edges=%zu shown=%zu omitted=%zu expansions=%zu status=%s",
        cost,route.steps.size(),count,route.steps.size()-count,route.metrics.expansions,status);
    sink(ctx,line);
    for (std::size_t i=0;i<count;++i) {
        const auto& e=route.steps[i].edge;
        std::snprintf(line,sizeof(line),"nav edge=%zu from=%u to=%u direction=%u traversal=%u source=%llu generation=%llu link=%llu",
            i,unsigned(e.source.value),unsigned(e.target.value),unsigned(e.direction),unsigned(e.traversal),
            e.external ? static_cast<unsigned long long>(e.external->sourceId):0ULL,
            e.external ? static_cast<unsigned long long>(e.external->generation):0ULL,
            e.external ? static_cast<unsigned long long>(e.external->linkId):0ULL);
        sink(ctx,line);
    }
}
}
