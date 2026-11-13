#include "runtime/behavior/bot_behavior_runtime.hpp"

namespace astrabot
{
namespace metamod
{
bool BotBehaviorRuntime::prepareManagedPerception(std::size_t index,
	world::WorldSnapshot *snapshot, world::ActorKey *targetActor,
	world::WorldPosition *targetPosition)
{
	ThreatObservation threats{};
	if (!services_->observation_collectManagedPerception(index, snapshot, &threats))
	{
		return false;
	}
	if (!threats.observerAvailable)
	{
		return true;
	}
	const auto &belief = *snapshot;
	const auto observer = belief.identity().observer;
	const auto &candidates = threats.candidates;
	const auto candidateCount = threats.count;
	if (actors_[index].recognitionSequences !=
		services_->service_managedBotFullUpdateSequences()[index])
	{
		actors_[index].recognitionSequences =
			services_->service_managedBotFullUpdateSequences()[index];
		compat::ThreatCandidate threat{};
		compat::PublicActorState publicState{};
		const bool blinded =
			services_->observation_publicObservations().read(observer, belief.identity().frame, &publicState) &&
			services_->service_globals() != nullptr &&
			services_->service_globals()->time < publicState.blindUntil;
		if (!blinded && candidateCount != 0U && services_->service_globals() != nullptr)
		{
			actors_[index].lastVisibleEnemyTimes = services_->service_globals()->time;
		}
		if (services_->service_globals() != nullptr &&
			actors_[index].reactionQueues.update(
				observer, belief.identity().frame, services_->service_globals()->time,
				actors_[index].managedProfiles.reactionTimeSeconds,
				candidates.data(), candidateCount, blinded, &threat))
		{
			if (!(actors_[index].recognizedTargets == threat.actor))
			{
				observeRecognizedTargetChange(
					index, services_->service_globals()->time);
			}
			actors_[index].recognizedTargets = threat.actor;
			actors_[index].recognizedPoints = threat.visiblePoint;
		}
		else
		{
			actors_[index].recognizedTargets = {};
		}
	}
	if (actors_[index].recognizedTargets.isValid())
	{
		*targetActor = actors_[index].recognizedTargets;
		*targetPosition = actors_[index].recognizedPoints;
	}
	return true;
}

void BotBehaviorRuntime::resetRoundRecognition()
{
	for (auto &actor : actors_)
	{
		actor.recognizedTargets = {};
		actor.recognitionSequences = 0U;
		actor.reactionQueues.reset();
	}
}

void BotBehaviorRuntime::observePublicDeath(std::size_t index)
{
	actors_[index].reactionQueues.reset();
}
} // namespace metamod
} // namespace astrabot
