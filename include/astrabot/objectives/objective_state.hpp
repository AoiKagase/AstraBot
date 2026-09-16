#ifndef ASTRABOT_OBJECTIVES_OBJECTIVE_STATE_HPP
#define ASTRABOT_OBJECTIVES_OBJECTIVE_STATE_HPP

#include "astrabot/world/world_snapshot.hpp"

#include <cstdint>

namespace astrabot
{
namespace objectives
{
struct ObjectiveLimits
{
	static constexpr std::uint32_t kMaximumLifetimeTicks = 4096U;
};

enum class ScenarioKind
{
	Unknown,
	Bomb,
	Hostage
};

enum class TeamRole
{
	Unknown,
	Terrorist,
	CounterTerrorist
};

enum class ObjectiveKind
{
	None,
	Attack,
	Defend,
	Retake,
	Save,
	Rescue,
	Escort,
	Buy,
	Plant,
	Defuse
};

enum class ObjectivePriority
{
	Low,
	Normal,
	High,
	Critical
};

enum class ObjectiveStatus
{
	Unknown,
	Proposed,
	Active,
	Completed,
	Failed,
	Expired
};

enum class ObjectiveStateResult
{
	Accepted,
	Found,
	NotFound,
	InvalidArgument,
	InvalidIdentity,
	InvalidProposal,
	StaleGeneration,
	StaleFrame,
	FeedbackUnavailable,
	Completed,
	Expired
};

struct ScenarioIdentity
{
	world::FrameIdentity frame;
	std::uint32_t scenarioGeneration;
	ScenarioKind kind;
	TeamRole team;

	bool isValid() const;
	bool operator==(const ScenarioIdentity &other) const;
};

struct ObjectiveIdentity
{
	ScenarioIdentity scenario;
	ObjectiveKind kind;
	std::uint32_t subjectId;

	bool isValid() const;
	bool operator==(const ObjectiveIdentity &other) const;
};

struct ObjectiveProposal
{
	world::ActorKey actor;
	ScenarioIdentity scenario;
	ObjectiveIdentity objective;
	ObjectivePriority priority;
	std::uint32_t issuedTick;
	std::uint32_t expiresAtTick;

	bool isValid() const;
	bool isExpired(const world::FrameIdentity &frame) const;
	bool isProposal() const;
	bool isCompletionFeedback() const;
};

struct ObjectiveCompletionFeedback
{
	world::ActorKey actor;
	ScenarioIdentity scenario;
	ObjectiveIdentity objective;
	ObjectiveStatus status;

	bool isValid() const;
	bool isConfirmed() const;
};

struct ObjectiveStateRecord
{
	world::ActorKey actor;
	ObjectiveIdentity objective;
	ObjectivePriority priority;
	ObjectiveStatus status;
	std::uint32_t issuedTick;
	std::uint32_t expiresAtTick;
};

class ObjectiveState
{
public:
	ObjectiveState();
	explicit ObjectiveState(const world::ActorKey &actor);

	ObjectiveStateResult acceptProposal(const ObjectiveProposal &proposal);
	ObjectiveStateResult applyFeedback(
		const ObjectiveCompletionFeedback &feedback);
	ObjectiveStateResult expire(const world::FrameIdentity &frame);
	ObjectiveStateResult current(ObjectiveStateRecord *record) const;

	bool isComplete() const;
	const world::ActorKey &actor() const;

private:
	world::ActorKey actor_;
	ObjectiveStateRecord record_;
	bool hasRecord_;
};
}
}

#endif
