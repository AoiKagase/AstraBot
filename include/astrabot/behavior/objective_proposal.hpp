#ifndef ASTRABOT_BEHAVIOR_OBJECTIVE_PROPOSAL_HPP
#define ASTRABOT_BEHAVIOR_OBJECTIVE_PROPOSAL_HPP

#include "astrabot/world/world_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace behavior
{
struct ObjectiveLimits
{
	static constexpr std::size_t kMaximumProposals = 16U;
	static constexpr std::uint32_t kMaximumLifetimeTicks = 4096U;
};

enum class ObjectiveKind
{
	None,
	Roam,
	Seek,
	Engage,
	Retreat
};

enum class ObjectivePriority
{
	Low,
	Normal,
	High,
	Critical
};

enum class ProposalResult
{
	Accepted,
	InvalidArgument,
	InvalidActor,
	StaleGeneration,
	InvalidProposal,
	DuplicateProposal,
	ResourceLimit,
	StaleFrame,
	Selected,
	NoProposal
};

struct ObjectiveCompletionFeedback
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	ObjectiveKind objective;
	bool completed;
};

struct ObjectiveProposal
{
	world::ActorKey actor;
	world::FrameIdentity sourceFrame;
	std::uint64_t sourceNavRevision;
	ObjectiveKind objective;
	ObjectivePriority priority;
	std::uint32_t issuedTick;
	std::uint32_t expiresAtTick;
	bool hasTarget;
	world::ActorKey target;

	bool isValid() const;
	bool isExpired(const world::FrameIdentity &frame) const;
	bool isProposal() const;
	bool isActionIntent() const;
	bool isCompletionFeedback() const;
};

class ObjectiveProposalSet
{
public:
	ObjectiveProposalSet();
	explicit ObjectiveProposalSet(const world::ActorKey &owner);

	ProposalResult setOwner(const world::ActorKey &owner);
	ProposalResult add(const ObjectiveProposal &proposal);
	ProposalResult select(
		const world::FrameIdentity &frame,
		ObjectiveProposal *proposal) const;

	void clear();
	std::size_t size() const;
	const world::ActorKey &owner() const;

private:
	static bool isValidObjective(ObjectiveKind objective);
	static bool isValidPriority(ObjectivePriority priority);
	static bool sameProposal(
		const ObjectiveProposal &left,
		const ObjectiveProposal &right);
	static int priorityValue(ObjectivePriority priority);
	static int objectiveValue(ObjectiveKind objective);
	static bool isBetter(
		const ObjectiveProposal &candidate,
		const ObjectiveProposal &current);

	world::ActorKey owner_;
	std::array<ObjectiveProposal, ObjectiveLimits::kMaximumProposals>
		proposals_;
	std::size_t proposalCount_;
};
}
}

#endif
