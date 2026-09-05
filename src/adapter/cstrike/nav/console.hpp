// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "adapter/metamod/plugin_entry.hpp"
#include "nav/runtime/route_session.hpp"
#include "nav/query/spatial_index.hpp"
namespace astrabot::adapter::metamod { class LifecycleCoordinator; }
namespace astrabot::adapter::cstrike {
enum class NavCommand { Load, GoTo, Status, Cancel };
class NavConsole final : public nav::runtime::IWorldQueries {
public:
    void configure(enginefuncs_t*, mutil_funcs_t*, globalvars_t*) noexcept;
    void reset() noexcept;
    void invalidate(nav::runtime::SessionReason) noexcept;
    void observe(metamod::LifecycleCoordinator&) noexcept;
    void execute(NavCommand, metamod::LifecycleCoordinator&) noexcept;
    // Publication binds an independently obtained immutable mesh to the current map.
    nav::diagnostics::NavError publish(core::MapGeneration,
        std::shared_ptr<const nav::model::NavMeshSnapshot>) noexcept;
    const nav::runtime::DecisionTrace* trace() const noexcept { return session_ ? &session_->trace():nullptr; }
    nav::runtime::WorldQueryResult query(const nav::runtime::QueryRequest&) override;
private:
    nav::runtime::MovementSnapshot snapshot(metamod::LifecycleCoordinator&) noexcept;
    void printUpdate(const nav::runtime::SessionUpdate&) noexcept;
    void line(const char*) noexcept;
    static void sink(void*,const char*) noexcept;
    bool load(const char*,core::MapGeneration) noexcept;
    enginefuncs_t* engine_{};
    mutil_funcs_t* utility_{};
    globalvars_t* globals_{};
    nav::runtime::NavigationSnapshot navigation_{};
    std::shared_ptr<const nav::query::NavSpatialIndex> index_{};
    std::optional<nav::runtime::RouteSession> session_{};
    bool inRequest_{};
    std::optional<nav::runtime::SessionReason> deferredInvalidation_{};
    edict_t* queryingEntity_{}; // borrowed only for synchronous request
};
}
