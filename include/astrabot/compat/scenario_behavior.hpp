#ifndef ASTRABOT_COMPAT_SCENARIO_BEHAVIOR_HPP
#define ASTRABOT_COMPAT_SCENARIO_BEHAVIOR_HPP

#include "astrabot/compat/state_machine.hpp"
#include "astrabot/objectives/objective_state.hpp"
#include <cmath>
#include <cstdint>

namespace astrabot
{
namespace compat
{
// The existing objective planner covers Bomb/Hostage. VIP/Escape are adapter
// scenario identities here, without changing the persistent objective schema.
enum class PublicScenarioKind
{
	Unknown,
	Bomb,
	Hostage,
	Vip,
	Escape
};
enum class ScenarioKnowledge
{
	Unknown,
	Visible,
	Remembered
};
enum class ScenarioAction
{
	None,
	Move,
	Use,
	Guard,
	Escape,
	Follow,
	Stop,
	Hunt
};
enum class ScenarioRadioCommand
{
	None,
	FollowMe,
	CoverMe,
	StickTogether,
	Regroup,
	NeedBackup,
	HoldPosition,
	GoGoGo,
	GetOut,
	ReportIn,
	TeamFallBack
};
struct ScenarioTarget
{
	ScenarioKnowledge knowledge;
	world::FrameIdentity frame;
	world::EntityKey entity;
	world::ActorKey actor;
	world::WorldPosition position;
	float pathCost;
	bool inUseRange;
};
struct ScenarioLeaderObservation
{
	world::ActorKey actor;
	objectives::TeamRole team;
	bool human;
	bool lifeKnown;
	bool alive;
	ScenarioTarget location;
};
struct ScenarioRadioObservation
{
	std::uint32_t sequence;
	ScenarioRadioCommand command;
	world::ActorKey sender;
	objectives::TeamRole team;
	bool senderHuman;
	float receivedTime;
	ScenarioTarget position;
};
struct ScenarioFallBackResponse
{
	world::ActorKey sender{};
	bool decline = false;
};
struct ScenarioSenderValidation
{
	world::ActorKey actor = {};
	bool known = false;
	bool valid = false;
};
struct ScenarioBehaviorInput
{
	world::ActorKey actor;
	world::FrameIdentity frame;
	float timestamp;
	objectives::TeamRole team;
	bool lifeKnown;
	bool alive;
	bool blind;
	PublicScenarioKind scenario;
	ScenarioTarget hostage;
	ScenarioTarget rescueZone;
	ScenarioTarget escapeZone;
	ScenarioTarget vipZone;
	ScenarioTarget vip;
	bool escortKnown;
	std::uint32_t escortCount;
	bool isVipKnown;
	bool isVip;
	const ScenarioRadioObservation *radio;
	ScenarioLeaderObservation leader;
	float reactionSeconds;
	bool busy;
	bool rogue;
	bool bombPlantedKnown;
	bool bombPlanted;
	ScenarioTarget bomb;
	bool timerKnown;
	float bombSecondsRemaining;
	bool defuseKitKnown;
	bool defuseKit;
	bool travelTimeKnown;
	float travelSecondsToBomb;
	bool humanDefusingKnown;
	bool humanDefusing;
	bool selfPositionKnown = false;
	world::WorldPosition selfPosition = {};
	bool hostageCompetitorNearby = false;
	bool timerEstimated = false;
	bool travelTimeEstimated = false;
	ScenarioTarget bombSearch = {};
	ScenarioTarget plantedBombsite = {};
	float aggression = 0.5f;
	bool attacking = false;
	bool currentDefusing = false;
	bool alreadyEscapingFromBomb = false;
	bool automaticFollowRequested = false;
	bool stopFollowingRequested = false;
	bool autoFollowPermissionInferred = false;
	ScenarioLeaderObservation automaticLeader = {};
	ScenarioLeaderObservation teammateLeader = {};
	ScenarioSenderValidation radioSenderValidation = {};
	ScenarioSenderValidation pendingRadioSenderValidation = {};
	bool deferToHuman = false;
};
struct ScenarioBehaviorDecision
{
	CompatibilityStateId state;
	ScenarioAction action;
	bool hasTarget;
	ScenarioTarget target;
	bool awaitingReceipt;
	bool unavailable;
	bool escortInferred = false;
	bool followPermissionInferred = false;
	CompatibilityTaskId hideTask = CompatibilityTaskId::None;
};

inline CompatibilityTaskId nativeScenarioMoveTask(
	const ScenarioBehaviorInput &input, const ScenarioBehaviorDecision &decision, bool carryingC4 = false)
{
	if (decision.state != CompatibilityStateId::MoveTo || !decision.hasTarget)
		return CompatibilityTaskId::None;
	if (input.scenario == PublicScenarioKind::Hostage && input.team == objectives::TeamRole::CounterTerrorist)
	{
		if ((input.escortKnown && input.escortCount) || decision.escortInferred)
			return CompatibilityTaskId::RescueHostages;
		if (input.hostage.entity.isValid() && decision.target.entity == input.hostage.entity)
			return CompatibilityTaskId::CollectHostages;
	}
	if (input.scenario == PublicScenarioKind::Bomb)
	{
		if (input.team == objectives::TeamRole::Terrorist && carryingC4)
			return CompatibilityTaskId::PlantBomb;
		if (input.team == objectives::TeamRole::CounterTerrorist && input.bombPlantedKnown && input.bombPlanted)
		{
			if (input.bomb.knowledge != ScenarioKnowledge::Unknown && input.bomb.entity.isValid() &&
				decision.target.entity == input.bomb.entity)
				return CompatibilityTaskId::DefuseBomb;
			if (input.bombSearch.knowledge != ScenarioKnowledge::Unknown && input.bombSearch.entity.isValid() &&
				decision.target.entity == input.bombSearch.entity)
				return CompatibilityTaskId::FindTickingBomb;
			if (input.plantedBombsite.entity.isValid() && decision.target.entity == input.plantedBombsite.entity)
				return CompatibilityTaskId::DefuseBomb;
		}
	}
	return CompatibilityTaskId::None;
}

class ScenarioBehaviorPlanner
{

public:
	bool isFollowing() const
	{
		return following_;
	}

	world::ActorKey followLeader() const
	{
		return leader_;
	}

	void followTeammate(world::ActorKey leader, float now)
	{
		if (!following_ || !followingTeammate_ || !(leader_ == leader))
			followSince_ = now;
		following_ = true;
		holding_ = false;
		leader_ = leader;
		followPermissionInferred_ = false;
		followingTeammate_ = true;
	}

	void holdPosition(const ScenarioTarget &position, float duration = -1.0f, float ambushDelay = 0.0f,
		bool keepFollowing = false)
	{
		holding_ = true;
		if (!keepFollowing)
			following_ = false;
		holdPosition_ = position;
		holdDuration_ = duration;
		ambushDelay_ = ambushDelay;
		firstHeardEnemyTime_ = -1.0f;
	}

	bool delayDefenseInvestigation(float now)
	{
		if (!holding_ || !holdPosition_.inUseRange || ambushDelay_ <= 0.0f)
			return false;
		if (firstHeardEnemyTime_ < 0.0f)
			firstHeardEnemyTime_ = now;
		return now - firstHeardEnemyTime_ <= ambushDelay_;
	}

	bool defenseAmbushExpired(float now) const
	{
		return holding_ && firstHeardEnemyTime_ >= 0 && now - firstHeardEnemyTime_ > ambushDelay_;
	}

	bool atDefenseSpot() const { return holding_ && holdPosition_.inUseRange; }

	bool defenseExpired(float elapsed, bool reloading) const
	{
		return holding_ && holdDuration_ >= 0.0f && !reloading && elapsed > holdDuration_;
	}

	bool finishDefense(float elapsed, bool reloading)
	{
		if (!defenseExpired(elapsed, reloading))
			return false;
		holding_ = false;
		return true;
	}

	void cancelHoldPosition()
	{
		holding_ = false;
		holdPosition_ = {};
		holdDuration_ = -1.0f;
	}

	void observeDefenderFeet(float x, float y, float z)
	{
		if (!holding_ || holdPosition_.inUseRange)
			return;
		const float dx = holdPosition_.position.x - x;
		const float dy = holdPosition_.position.y - y;
		const float dz = holdPosition_.position.z - z;
		constexpr float arrivalRadius = 20.0f;
		holdPosition_.inUseRange = dx * dx + dy * dy + dz * dz < arrivalRadius * arrivalRadius;
	}

	float followSince() const
	{
		return followSince_;
	}

	bool awaitingHostageEscort() const
	{
		return pendingUse_.isValid();
	}

	world::ActorKey pendingRadioSender() const
	{
		return pendingRadio_.sender;
	}

	bool takeReportRequest(const world::FrameIdentity &frame)
	{
		const bool due = reportRequested_ && frame_.mapGeneration == frame.mapGeneration &&
			frame_.roundGeneration == frame.roundGeneration && frame_.tick == frame.tick;
		reportRequested_ = false;
		return due;
	}

	bool hasFollowingLeader(const ScenarioBehaviorInput &input) const
	{
		const auto &leader = followingLeader(input);
		return following_ && leader.actor == leader_ && leader.lifeKnown && leader.alive && leader.team == input.team &&
			targetKnown(input, leader.location);
	}

	static bool autoEscapeRequested(const ScenarioBehaviorInput &input)
	{
		constexpr float kEscapeThresholdSeconds = 8.0f;
		constexpr float kAggressionEscapeReductionSeconds = 2.0f;
// Keep literal evaluation precision on x87; each macro is undefined below.
#define ASTRABOT_SCENARIO_BEHAVIOR_DEFUSE_KIT_ESCAPE_SCALE 0.66f
		if (input.scenario != PublicScenarioKind::Bomb || input.alreadyEscapingFromBomb || input.attacking ||
			input.currentDefusing || !input.bombPlantedKnown || !input.bombPlanted || !targetKnown(input, input.bomb) ||
			!(input.timerKnown || input.timerEstimated) || !std::isfinite(input.bombSecondsRemaining) ||
			input.bombSecondsRemaining < 0.0f || !std::isfinite(input.aggression) || input.aggression < 0.0f ||
			input.aggression > 1.0f)
			return false;
		float threshold = kEscapeThresholdSeconds - kAggressionEscapeReductionSeconds * input.aggression;
		if ((input.defuseKitKnown && input.defuseKit) ||
			(!input.defuseKitKnown && input.team == objectives::TeamRole::CounterTerrorist))
			threshold *=
				ASTRABOT_SCENARIO_BEHAVIOR_DEFUSE_KIT_ESCAPE_SCALE; // Unknown CT kit: trigger only below both possible thresholds.
		return input.bombSecondsRemaining < threshold;
	}
#undef ASTRABOT_SCENARIO_BEHAVIOR_DEFUSE_KIT_ESCAPE_SCALE

	void reset()
	{
		*this = ScenarioBehaviorPlanner();
	}

	ScenarioBehaviorDecision update(const ScenarioBehaviorInput &input, ScenarioFallBackResponse *fallBack = nullptr)
	{
		if (fallBack)
			*fallBack = {};
		ScenarioBehaviorDecision result = {};
		if (!input.actor.isValid() || !input.frame.isValid() || !std::isfinite(input.timestamp) ||
			input.timestamp < 0.0f || !std::isfinite(input.reactionSeconds) || input.reactionSeconds < 0.0f)
		{
			result.unavailable = true;
			return result;
		}
		if (!(input.actor == actor_) || input.frame.mapGeneration != frame_.mapGeneration ||
			input.frame.roundGeneration != frame_.roundGeneration || input.timestamp < timestamp_)
		{
			reset();
		}
		else if (frame_.isValid() && input.frame.tick <= frame_.tick)
		{
			return result;
		}
		actor_ = input.actor;
		frame_ = input.frame;
		reportRequested_ = false;
		timestamp_ = input.timestamp;
		if (!input.lifeKnown || !input.alive)
		{
			following_ = false;
			holding_ = false;
			pendingRadio_ = {};
			pendingUse_ = {};
			result.action = ScenarioAction::Stop;
			result.unavailable = !input.lifeKnown;
			return result;
		}
		if (input.pendingRadioSenderValidation.known &&
			input.pendingRadioSenderValidation.actor == pendingRadio_.sender &&
			!input.pendingRadioSenderValidation.valid)
			pendingRadio_ = {};
		if (input.radio && input.radio->command == ScenarioRadioCommand::ReportIn)
			receiveRadio(input);
		if (pendingRadio_.command == ScenarioRadioCommand::ReportIn &&
			(!input.radio || input.radio->sequence <= radioSequence_))
			applyRadio(input, &result, fallBack);
		if (input.scenario == PublicScenarioKind::Bomb && input.bombPlantedKnown && input.bombPlanted &&
			targetKnown(input, input.bomb) && (input.alreadyEscapingFromBomb || autoEscapeRequested(input)))
			return targetDecision(
				input, input.escapeZone, ScenarioAction::Escape, CompatibilityStateId::EscapeFromBomb);
		receiveRadio(input);
		if (applyRadio(input, &result, fallBack))
		{
			return result;
		}
		if (input.stopFollowingRequested)
			following_ = false;
		if (input.automaticFollowRequested && !following_ && input.automaticLeader.human &&
			input.automaticLeader.actor.isValid() && input.automaticLeader.team == input.team &&
			input.automaticLeader.lifeKnown && input.automaticLeader.alive &&
			input.automaticLeader.location.knowledge == ScenarioKnowledge::Visible &&
			targetKnown(input, input.automaticLeader.location))
		{
			following_ = true;
			holding_ = false;
			leader_ = input.automaticLeader.actor;
			followSince_ = input.timestamp;
			followPermissionInferred_ = input.autoFollowPermissionInferred;
			followingTeammate_ = false;
		}
		if (following_ && !holding_)
		{
			const auto &leader = followingLeader(input);
			if (!(leader.actor == leader_) || !leader.lifeKnown || !leader.alive || leader.team != input.team)
			{
				following_ = false;
				result.action = ScenarioAction::Stop;
				result.state = CompatibilityStateId::Idle;
				return result;
			}
			result = targetDecision(input, leader.location, ScenarioAction::Follow, CompatibilityStateId::Follow);
			result.followPermissionInferred = followPermissionInferred_;
			return result;
		}
		if (holding_)
		{
			return targetDecision(input, holdPosition_, ScenarioAction::Guard, CompatibilityStateId::Hide);
		}
		switch (input.scenario)
		{
		case PublicScenarioKind::Hostage:
			return hostageDecision(input);
		case PublicScenarioKind::Vip:
			if (input.team == objectives::TeamRole::CounterTerrorist && input.isVipKnown && input.isVip)
			{
				return targetDecision(input, input.vipZone, ScenarioAction::Escape, CompatibilityStateId::MoveTo);
			}
			return targetDecision(input, input.vip, ScenarioAction::Guard, CompatibilityStateId::Follow);
		case PublicScenarioKind::Escape:
			return targetDecision(input, input.escapeZone,
				input.team == objectives::TeamRole::Terrorist ? ScenarioAction::Escape : ScenarioAction::Guard,
				CompatibilityStateId::MoveTo);
		case PublicScenarioKind::Bomb:
			return bombDecision(input);
		default:
			return result;
		}
	}

private:
	static bool targetKnown(const ScenarioBehaviorInput &input, const ScenarioTarget &target)
	{
		return (target.knowledge == ScenarioKnowledge::Visible || target.knowledge == ScenarioKnowledge::Remembered) &&
			target.frame.mapGeneration == input.frame.mapGeneration &&
			target.frame.roundGeneration == input.frame.roundGeneration && target.frame.tick <= input.frame.tick &&
			!(input.blind && target.knowledge == ScenarioKnowledge::Visible) && std::isfinite(target.position.x) &&
			std::isfinite(target.position.y) && std::isfinite(target.position.z) &&
			std::fabs(target.position.x) <= world::WorldLimits::kMaximumCoordinate &&
			std::fabs(target.position.y) <= world::WorldLimits::kMaximumCoordinate &&
			std::fabs(target.position.z) <= world::WorldLimits::kMaximumCoordinate && std::isfinite(target.pathCost) &&
			target.pathCost >= 0.0f;
	}

	static ScenarioBehaviorDecision targetDecision(const ScenarioBehaviorInput &input, const ScenarioTarget &target,
		ScenarioAction action, CompatibilityStateId state)
	{
		ScenarioBehaviorDecision result = {};
		if (!targetKnown(input, target))
		{
			result.unavailable = true;
			return result;
		}
		result.state = state;
		result.action = action;
		result.hasTarget = true;
		result.target = target;
		return result;
	}

	void receiveRadio(const ScenarioBehaviorInput &input)
	{
		if (!input.radio || !input.radio->sequence || input.radio->sequence <= radioSequence_)
		{
			return;
		}
		radioSequence_ = input.radio->sequence;
		if (input.radioSenderValidation.known && input.radioSenderValidation.actor == input.radio->sender &&
			!input.radioSenderValidation.valid)
		{
			if (pendingRadio_.sender == input.radio->sender)
				pendingRadio_ = {};
			return;
		}
		if (!input.radio->sender.isValid() || input.radio->sender == input.actor || input.radio->team != input.team ||
			!input.radio->senderHuman || (input.busy && input.radio->command != ScenarioRadioCommand::ReportIn) ||
			!std::isfinite(input.radio->receivedTime) ||
			input.radio->receivedTime < 0.0f || input.radio->receivedTime > input.timestamp)
		{
			return;
		}
		pendingRadio_ = *input.radio;
	}

	bool applyRadio(const ScenarioBehaviorInput &input, ScenarioBehaviorDecision *result, ScenarioFallBackResponse *fallBack)
	{
		constexpr float kRadioResponseBaseSeconds = 1.0f;
		constexpr float kRadioReactionScale = 2.0f;
		constexpr float kRogueRadioDelaySeconds = 2.0f;
		if (pendingRadio_.command == ScenarioRadioCommand::None)
		{
			return false;
		}
		if (input.pendingRadioSenderValidation.known &&
			input.pendingRadioSenderValidation.actor == pendingRadio_.sender &&
			!input.pendingRadioSenderValidation.valid)
		{
			pendingRadio_ = {};
			return false;
		}
		const bool report = pendingRadio_.command == ScenarioRadioCommand::ReportIn;
		const bool decline = input.rogue && !following_;
		if (!report && (input.busy || (decline && pendingRadio_.command != ScenarioRadioCommand::TeamFallBack)))
		{
			pendingRadio_ = {};
			return false;
		}
		const float responseDelay = kRadioResponseBaseSeconds + kRadioReactionScale * input.reactionSeconds +
			(input.rogue ? kRogueRadioDelaySeconds : 0.0f);
		if (input.timestamp - pendingRadio_.receivedTime < responseDelay)
		{
			return false;
		}
		const ScenarioRadioObservation order = pendingRadio_;
		pendingRadio_ = {};
		switch (order.command)
		{
		case ScenarioRadioCommand::TeamFallBack:
			if (fallBack)
				*fallBack = {order.sender, decline};
			return false;
		case ScenarioRadioCommand::ReportIn:
			reportRequested_ = !input.rogue || following_;
			return false;
		case ScenarioRadioCommand::FollowMe:
		case ScenarioRadioCommand::CoverMe:
		case ScenarioRadioCommand::StickTogether:
		case ScenarioRadioCommand::Regroup:
		case ScenarioRadioCommand::NeedBackup:
			if (!following_ && input.leader.actor == order.sender && input.leader.lifeKnown && input.leader.alive &&
				input.leader.team == input.team && targetKnown(input, input.leader.location))
			{
				following_ = true;
				holding_ = false;
				leader_ = order.sender;
				followSince_ = input.timestamp;
				followPermissionInferred_ = false;
				followingTeammate_ = false;
			}
			return false;
		case ScenarioRadioCommand::HoldPosition:
			if (targetKnown(input, order.position))
			{
				holdPosition(order.position);
			}
			return false;
		case ScenarioRadioCommand::GoGoGo:
			following_ = false;
			holding_ = false;
			result->action = ScenarioAction::Hunt;
			result->state = CompatibilityStateId::Hunt;
			return true;
		case ScenarioRadioCommand::GetOut:
			if (input.bombPlantedKnown && input.bombPlanted)
			{
				following_ = false;
				holding_ = false;
				*result = targetDecision(
					input, input.escapeZone, ScenarioAction::Escape, CompatibilityStateId::EscapeFromBomb);
				return true;
			}
			return false;
		default:
			return false;
		}
	}

	ScenarioBehaviorDecision hostageDecision(const ScenarioBehaviorInput &input)
	{
		constexpr float kEscortSelfMotionThreshold = 64.0f;
		constexpr float kEscortHostageMotionThreshold = 32.0f;
		constexpr float kMaximumEscortDistance = 256.0f;
		constexpr float kInferredEscortWaitSeconds = 3.0f;
		constexpr float kUseReceiptWaitSeconds = 2.0f;
		constexpr unsigned kMaximumHostageUseAttempts = 3U;
		if (input.team != objectives::TeamRole::CounterTerrorist)
			return {};
		if (input.escortKnown && input.escortCount)
		{
			pendingUse_ = {};
			return targetDecision(input, input.rescueZone, ScenarioAction::Move, CompatibilityStateId::MoveTo);
		}
		if (pendingUse_.isValid())
		{
			const bool sameVisibleHostage = pendingUse_ == input.hostage.entity &&
				input.hostage.knowledge == ScenarioKnowledge::Visible && targetKnown(input, input.hostage);
			if (sameVisibleHostage && input.selfPositionKnown && useSelfKnown_ && finitePosition(input.selfPosition) &&
				!input.hostageCompetitorNearby)
			{
				const float selfTravel = distanceSquared(input.selfPosition, useSelfPosition_);
				const float hostageTravel = distanceSquared(input.hostage.position, useHostage_.position);
				const float oldDistance = distanceSquared(useHostage_.position, input.selfPosition);
				const float newDistance = distanceSquared(input.hostage.position, input.selfPosition);
				if (selfTravel >= kEscortSelfMotionThreshold * kEscortSelfMotionThreshold &&
					hostageTravel >= kEscortHostageMotionThreshold * kEscortHostageMotionThreshold &&
					newDistance < oldDistance && newDistance <= kMaximumEscortDistance * kMaximumEscortDistance)
				{
					// Public motion supports an inference, never an authoritative
					// follow-leader/escort receipt. Refresh it only on new movement.
					inferredEscort_ = true;
					lastFollowMotion_ = input.timestamp;
					useSelfPosition_ = input.selfPosition;
					useHostage_ = input.hostage;
				}
			}
			if (input.hostageCompetitorNearby)
				inferredEscort_ = false;
			const float waitLimit = inferredEscort_ ? kInferredEscortWaitSeconds : kUseReceiptWaitSeconds;
			const float lastEvidence = inferredEscort_ ? lastFollowMotion_ : useSentAt_;
			if (input.timestamp - lastEvidence < waitLimit)
			{
				ScenarioBehaviorDecision moving =
					targetDecision(input, input.rescueZone, ScenarioAction::Move, CompatibilityStateId::MoveTo);
				if (!moving.hasTarget)
					moving = targetDecision(input, sameVisibleHostage ? input.hostage : useHostage_,
						ScenarioAction::Move, CompatibilityStateId::MoveTo);
				moving.awaitingReceipt = true;
				moving.escortInferred = inferredEscort_;
				return moving;
			}
			// No motion confirmation: reapproach rather than waiting forever.
			// The next update can retry Use, at most three times per entity.
			pendingUse_ = {};
			inferredEscort_ = false;
			ScenarioBehaviorDecision approach = targetDecision(input, sameVisibleHostage ? input.hostage : useHostage_,
				ScenarioAction::Move, CompatibilityStateId::MoveTo);
			approach.awaitingReceipt = true;
			return approach;
		}
		if (attemptedHostage_ == input.hostage.entity && useAttempts_ >= kMaximumHostageUseAttempts)
		{
			ScenarioBehaviorDecision exhausted = {};
			exhausted.action = ScenarioAction::Hunt;
			exhausted.state = CompatibilityStateId::Hunt;
			exhausted.unavailable = true;
			return exhausted;
		}
		ScenarioBehaviorDecision result =
			targetDecision(input, input.hostage, ScenarioAction::Move, CompatibilityStateId::MoveTo);
		if (!result.hasTarget)
		{
			return result;
		}
		if (input.hostage.entity.isValid() && input.hostage.knowledge == ScenarioKnowledge::Visible &&
			input.hostage.inUseRange)
		{
			result.awaitingReceipt = true;
			if (!(attemptedHostage_ == input.hostage.entity))
			{
				attemptedHostage_ = input.hostage.entity;
				useAttempts_ = 0U;
			}
			if (useAttempts_ >= kMaximumHostageUseAttempts)
			{
				result = {};
				result.action = ScenarioAction::Hunt;
				result.state = CompatibilityStateId::Hunt;
				result.unavailable = true;
				return result;
			}
			++useAttempts_;
			pendingUse_ = input.hostage.entity;
			useHostage_ = input.hostage;
			useSelfKnown_ = input.selfPositionKnown && finitePosition(input.selfPosition);
			useSelfPosition_ = input.selfPosition;
			useSentAt_ = input.timestamp;
			result.action = ScenarioAction::Use;
			result.state = CompatibilityStateId::UseEntity;
		}
		return result;
	}

	ScenarioBehaviorDecision bombDecision(const ScenarioBehaviorInput &input)
	{
		constexpr float kDefuseWithKitSeconds = 5.0f;
		constexpr float kDefuseWithoutKitSeconds = 10.0f;
		if (!input.bombPlantedKnown || !input.bombPlanted)
		{
			return {};
		}
		if (input.team == objectives::TeamRole::CounterTerrorist)
		{
			if (!input.attacking && !input.currentDefusing && (input.timerKnown || input.timerEstimated) &&
				input.defuseKitKnown && (input.travelTimeKnown || input.travelTimeEstimated) &&
				std::isfinite(input.bombSecondsRemaining) && std::isfinite(input.travelSecondsToBomb) &&
				input.bombSecondsRemaining >= 0.0f && input.travelSecondsToBomb >= 0.0f &&
				input.bombSecondsRemaining <=
					input.travelSecondsToBomb + (input.defuseKit ? kDefuseWithKitSeconds : kDefuseWithoutKitSeconds))
			{
				return targetDecision(
					input, input.escapeZone, ScenarioAction::Escape, CompatibilityStateId::EscapeFromBomb);
			}
			if (input.humanDefusingKnown && input.humanDefusing)
			{
				return targetDecision(input, targetKnown(input, input.bomb) ? input.bomb : input.plantedBombsite,
					ScenarioAction::Guard, CompatibilityStateId::Hide);
			}
			if (input.deferToHuman && !input.currentDefusing)
			{
				auto guard = targetDecision(input, targetKnown(input, input.bomb) ? input.bomb : input.plantedBombsite,
					ScenarioAction::Guard, CompatibilityStateId::Hide);
				if (guard.hasTarget)
				{
					guard.hideTask = CompatibilityTaskId::GuardBombZone;
					return guard;
				}
			}
			if (!targetKnown(input, input.bomb))
				return targetDecision(input, targetKnown(input, input.plantedBombsite) ? input.plantedBombsite : input.bombSearch,
					ScenarioAction::Move, CompatibilityStateId::MoveTo);
			ScenarioBehaviorDecision result =
				targetDecision(input, input.bomb, ScenarioAction::Move, CompatibilityStateId::MoveTo);
			if (result.hasTarget && input.currentDefusing)
			{
				result.action = ScenarioAction::Use;
				result.state = CompatibilityStateId::DefuseBomb;
				result.awaitingReceipt = true;
			}
			return result;
		}
		return targetDecision(input, input.bomb, ScenarioAction::Guard, CompatibilityStateId::Hide);
	}

	world::ActorKey actor_ = {};
	world::FrameIdentity frame_ = {};
	float timestamp_ = 0.0f;
	std::uint32_t radioSequence_ = 0U;
	ScenarioRadioObservation pendingRadio_ = {};
	world::ActorKey leader_ = {};
	bool following_ = false;
	float followSince_ = 0.0f;
	const ScenarioLeaderObservation &followingLeader(const ScenarioBehaviorInput &input) const
	{
		if (followingTeammate_)
			return input.teammateLeader;
		return input.automaticLeader.actor == leader_ ? input.automaticLeader : input.leader;
	}
	bool followPermissionInferred_ = false;
	bool followingTeammate_ = false;
	bool holding_ = false;
	bool reportRequested_ = false;
	float holdDuration_ = -1.0f;
	float ambushDelay_ = 0.0f;
	float firstHeardEnemyTime_ = -1.0f;
	ScenarioTarget holdPosition_ = {};
	world::EntityKey pendingUse_ = {};
	world::EntityKey attemptedHostage_ = {};
	ScenarioTarget useHostage_ = {};
	world::WorldPosition useSelfPosition_ = {};
	bool useSelfKnown_ = false;
	bool inferredEscort_ = false;
	float useSentAt_ = 0.0f;
	float lastFollowMotion_ = 0.0f;
	std::uint32_t useAttempts_ = 0U;
	static bool finitePosition(const world::WorldPosition &position)
	{
		return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
	}

	static float distanceSquared(const world::WorldPosition &a, const world::WorldPosition &b)
	{
		const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
		return x * x + y * y + z * z;
	}
};
} // namespace compat
} // namespace astrabot
#endif
