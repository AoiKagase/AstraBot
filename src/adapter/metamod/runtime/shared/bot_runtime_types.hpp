#ifndef ASTRABOT_ADAPTER_BOT_RUNTIME_TYPES_HPP
#define ASTRABOT_ADAPTER_BOT_RUNTIME_TYPES_HPP

// Preserve SDK macro visibility at the original include boundary.
// clang-format off
#include "astrabot/metamod/abi_contract.hpp"
#include "usercmd.h"
#include "astrabot/metamod/compat_surface.hpp"
#include "astrabot/metamod/fake_client_manager.hpp"
#include "astrabot/metamod/join_controller.hpp"
#include "astrabot/metamod/round_lifecycle_tracker.hpp"
#include "astrabot/metamod/input_dispatcher.hpp"
#include "astrabot/metamod/native_bot_guard.hpp"
#include "astrabot/metamod/nav_loader.hpp"
#include "astrabot/combat/combat_intent.hpp"
#include "astrabot/compat/state_machine.hpp"
#include "astrabot/compat/public_observation.hpp"
#include "astrabot/compat/buy_controller.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/compat/combat_behavior.hpp"
#include "astrabot/compat/weapon_actions.hpp"
#include "astrabot/compat/grenade_trajectory.hpp"
#include "astrabot/compat/scenario_behavior.hpp"
#include "astrabot/compat/public_bomb_clock.hpp"
#include "astrabot/compat/public_bomb_radar.hpp"
#include "astrabot/compat/bomb_guard.hpp"
#include "astrabot/compat/bomb_search.hpp"
#include "astrabot/compat/teamwork_policy.hpp"
#include "astrabot/objectives/round_objectives.hpp"
#include "astrabot/objectives/plant_attempt.hpp"
#include "astrabot/objectives/bomb_site_route_selection.hpp"
#include "astrabot/team/round_objective_coordinator.hpp"
#include "astrabot/perception/perception.hpp"
#include "astrabot/runtime/nav_roam_controller.hpp"
#include "astrabot/runtime/player_avoidance.hpp"
#include "astrabot/nav/danger_memory.hpp"
#include "astrabot/runtime/movement_physics.hpp"
#include "astrabot/runtime/bot_timing_scheduler.hpp"
#include "astrabot/runtime/jump_crouch_sequencer.hpp"
#include "astrabot/runtime/lifecycle.hpp"
#include "bridge/engine_random_source.hpp"
#include "bridge/action_adapter.hpp"
#include "bridge/observation_adapter.hpp"
#include "bridge/regamedll_observation_provider.hpp"
#include "astrabot/metamod/runtime_profiler.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

// clang-format on

namespace astrabot
{
namespace metamod
{
constexpr auto kObjectiveKnowledgeStateCount = 2U;
constexpr auto kTeamScoreEntryCount = 3U;
enum class CompatibilityCommandResult
{
	Handled,
	Unknown,
	InvalidArguments,
	InvalidOutput,
	NotActive,
	Disabled,
	Stopped,
	QuotaReached,
	NativeGuardDenied,
	NoTarget,
	ProfileUnavailable,
	NameTaken,
	ActorOperationFailed
};

struct ThreatObservation
{
	static constexpr auto kMaximumThreatCandidates = 32U;
	std::array<compat::ThreatCandidate, kMaximumThreatCandidates> candidates{};
	std::size_t count = 0U;
	bool observerAvailable = false;
};

struct ManagedObjectiveTargetCache
{
	bool valid;
	runtime::LifecycleGeneration mapGeneration;
	runtime::LifecycleGeneration roundGeneration;
	std::uint8_t team;
	bool carryingBomb;
	nav::NavVector target;
	int selectedEntityIndex;
	std::uint32_t selectedSiteIdentity;
	std::uint32_t generation;
	std::uint32_t teamObjectiveAssignmentGeneration;
	team::TeamObjectiveKind teamObjectiveKind;
	bool diagnosticsEmitted;
	bool lastCacheHit;
	float approachRouteSeconds;
	bool routeOverride = false;
	nav::NavRouteType routeType = nav::NavRouteType::Safest;
};

struct ManagedObjectiveActionState
{
	bool active;
	world::ActorKey actor;
	team::TeamObjectiveKind kind;
	std::uint32_t mapGeneration;
	std::uint32_t roundGeneration;
	std::uint32_t assignmentGeneration;
	int targetEntityIndex;
	std::uint32_t targetSiteIdentity;
	bool c4SelectionCommandDispatched;
	bool c4AttackInputDispatched;
	objectives::PlantAttemptController plantAttempt;
};

struct ManagedCurrentWeaponObservation
{
	bool available;
	bool active;
	std::uint8_t weaponId;
	std::uint32_t actorGeneration;
	std::uint32_t mapGeneration;
	std::uint32_t roundGeneration;
	std::uint32_t frame;
};

struct ManagedObjectiveSiteRegistry
{
	static constexpr std::size_t kMaximumSites = 4U;
	struct Entry
	{
		bool registered;
		std::uint32_t targetId;
		int entityIndex;
		std::uint32_t siteIdentity;
		compat::BombSearchSite zone;
	};

	runtime::LifecycleGeneration mapGeneration;
	runtime::LifecycleGeneration roundGeneration;
	bool initialized;
	std::array<Entry, kMaximumSites> entries;
	std::uint32_t registeredCount;
	std::uint32_t registeredFuncBombTargetCount;
	std::uint32_t registeredInfoBombTargetCount;
};

enum class UserMessageKind : std::uint8_t
{
	None,
	ShowMenu,
	VguiMenu,
	TeamInfo,
	CurWeapon
};
struct MovementReversalDiagnosticState
{
	bool valid;
	runtime::PhysicsVector feetPosition;
	runtime::PhysicsVector direction;
	nav::NavVector localTarget;
	nav::AreaId currentArea;
	nav::AreaId targetArea;
	std::uint32_t pathSequence;
	std::uint32_t reversalCount;
};
struct PendingTraversalDiagnostic
{
	bool active;
	std::uint32_t actorGeneration;
	std::uint32_t commandFrame;
	std::uint32_t navUpdateSequence;
	std::uint32_t goalGeneration;
	std::uint32_t mapGeneration;
	std::uint32_t roundGeneration;
	std::uint32_t corridorIndex;
	runtime::NavGoalSelectionReason goalSelectionReason;
	runtime::NavGoalSelectionStrategy goalSelectionStrategy;
	nav::AreaId currentArea;
	nav::AreaId targetArea;
	nav::NavVector targetPosition;
	nav::NavVector intentDirection;
	float intentSpeed;
	float sourceFloorZ;
	float targetFloorZ;
	float dropHeight;
	float horizontalGap;
	float maximumSafeDropHeight;
	bool safeDropHeightAvailable;
	std::uint8_t linkDirection;
	std::uint8_t linkHow;
	nav::TraversalAction action;
	std::uint16_t buttons;
	runtime::PhysicsVector startPosition;
	float startVelocityZ;
	bool startGrounded;
	float startHealth;
};

struct PublicScoreStatus
{
	bool known = false;
	std::uint32_t serial = 0U;
	world::FrameIdentity frame{};
	int flags = 0;
};

struct PublicSmoke
{
	world::EntityKey entity{};
	world::WorldVector position{};
};

struct ManagedNavigationFailure
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	std::uint32_t goalGeneration;
};

struct ObjectiveKnowledge
{
	world::EntityKey entity{};
	world::WorldVector position{};
	float firstHeardAt = -1.0f;
};
enum class HuntApproachLookUpdate
{
	Unchanged,
	Clear,
	Track,
	TrackEncounter
};
struct HuntApproachLook
{
	HuntApproachLookUpdate update = HuntApproachLookUpdate::Unchanged;
	nav::NavVector spot{};
	bool tracking() const
	{
		return update == HuntApproachLookUpdate::Track || update == HuntApproachLookUpdate::TrackEncounter;
	}
};
struct NavigationUpdate
{
	runtime::NavRoamObservation observation{};
	runtime::LandingDamageObservation landingFeedback{};
	runtime::GroundLookaheadRoute probeRoute{};
	nav::LocomotionIntent locomotionIntent{};
	runtime::NavRoamDecision roamDecision{};
	runtime::NavRoamResult roamResult = runtime::NavRoamResult::NoRoute;
	HuntApproachLook huntApproachLook{};
};
// Stack-local command preparation; no owner state or observations are cached across updates.
enum class BlindMove : std::uint8_t
{
	None,
	Right,
	Backward,
	Left,
	Crouch
};

struct BlindCommand
{
	BlindMove move = BlindMove::None;
	bool fire = false;
	bool active = false;
};

struct MovementCommandDraft
{
	runtime::BotCommand command{};
	runtime::MovementPhysicsState before{};
	runtime::CommandReceipt receipt{};
	bool ready = false;
};
enum class MovementPreparationKind
{
	Ready,
	Skip,
	Neutral
};
enum class MovementPreparationCompletion
{
	None,
	ResetRoute,
	ResetRouteAndWarmup,
	AdvanceWarmup
};
struct MovementPreparation
{
	MovementPreparationKind kind;
	MovementPreparationCompletion completion;
};
enum class NavigationRequestKind
{
	PublishStep,
	ClearStep,
	ClearFailure,
	ResetMovement,
	DestroyPath,
	ResetProgress
};
struct NavigationRequest
{
	std::size_t index;
	NavigationRequestKind kind;
	compat::NavigationStep step{};
};
} // namespace metamod
} // namespace astrabot

#endif
