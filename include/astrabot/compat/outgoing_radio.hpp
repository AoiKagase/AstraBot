#ifndef ASTRABOT_COMPAT_OUTGOING_RADIO_HPP
#define ASTRABOT_COMPAT_OUTGOING_RADIO_HPP

#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/noise_report.hpp"
#include "astrabot/compat/chatter_schedule.hpp"
#include "astrabot/compat/chatter_statements.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace astrabot
{
namespace compat
{
enum class OutgoingRadioStatus
{
	None,
	Ready,
	WaitingTeam,
	MissingPhrase
};

struct RadioContactCounts
{
	int enemies = 0;
	int friends = 0;
};

// CSBot's nearby counts are a three-second visual watch window, not a
// distance radius. Retain only contacts actually seen by this observer.
class RadioContactMemory
{
	static constexpr int kClientWatchCapacity = 32;

public:
	RadioContactCounts update(const world::ActorKey &observer, const world::FrameIdentity &frame, float now,
		const world::WorldSnapshot *belief, bool blind, int remainingFriends, int remainingEnemies)
	{
		if (!(observer == observer_) || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || now < previousTime_)
		{
			*this = RadioContactMemory();
			observer_ = observer;
		}
		frame_ = frame;
		previousTime_ = now;
		if (!blind && belief != nullptr && belief->isValid() &&
			belief->identity().frame.mapGeneration == frame.mapGeneration &&
			belief->identity().frame.roundGeneration == frame.roundGeneration &&
			belief->identity().frame.tick == frame.tick)
		{
			for (std::size_t i = 0; i < belief->actorCount(); ++i)
			{
				const auto *contact = belief->actorAt(i);
				if (contact == nullptr || contact->actor == observer || !contact->visible ||
					!contact->hasCurrentPosition() || contact->actor.slot == 0 || contact->actor.slot > watches_.size())
					continue;
				if (contact->relation != world::TeamRelation::Hostile &&
					contact->relation != world::TeamRelation::Friendly)
					continue;
				watches_[contact->actor.slot - 1] = {contact->actor, now, contact->relation};
			}
		}
		return counts(now, remainingFriends, remainingEnemies);
	}

	RadioContactCounts counts(const world::ActorKey &observer, const world::FrameIdentity &frame, float now,
		int remainingFriends, int remainingEnemies) const
	{
		if (!(observer == observer_) || frame.mapGeneration != frame_.mapGeneration ||
			frame.roundGeneration != frame_.roundGeneration || !std::isfinite(now) || now < previousTime_)
			return {};
		return counts(now, remainingFriends, remainingEnemies);
	}

	RadioContactCounts counts(float now, int remainingFriends, int remainingEnemies) const
	{
		constexpr float kContactWatchSeconds = 3.0f;
		RadioContactCounts counts{};
		for (const auto &watch : watches_)
		{
			if (watch.timestamp <= 0 || now < watch.timestamp || now - watch.timestamp >= kContactWatchSeconds)
				continue;
			if (watch.relation == world::TeamRelation::Hostile)
				++counts.enemies;
			else if (watch.relation == world::TeamRelation::Friendly)
				++counts.friends;
		}
		counts.enemies = (std::max)(0, (std::min)(counts.enemies, remainingEnemies));
		counts.friends = (std::max)(0, (std::min)(counts.friends, remainingFriends));
		return counts;
	}

private:
	struct Watch
	{
		world::ActorKey actor{};
		float timestamp = 0;
		world::TeamRelation relation = world::TeamRelation::Unknown;
	};
	std::array<Watch, kClientWatchCapacity> watches_{};
	world::ActorKey observer_{};
	world::FrameIdentity frame_{};
	float previousTime_ = 0;
};
struct OutgoingRadioInput
{
	world::ActorKey actor{};
	world::FrameIdentity frame{};
	float now = 0;
	bool alive = false;
	int team = 0;
	int friendCount = 0, nearbyFriends = 0, visibleEnemyCount = 0, enemiesRemaining = 0;
	std::uint16_t ownPlace = 0, enemyPlace = 0;
	std::uint32_t ownPlacePhrase = 0, enemyPlacePhrase = 0, noisePlacePhrase = 0;
	bool inCombat = false;
	float teamLastSpeech = -1, teamBusyUntil = 0;
	ChatterPhrase teamCurrentPhrase = ChatterPhrase::None;
	std::uint16_t teamCurrentPlace = 0;
	std::uint64_t navRevision = 0;
	bool roundOver = false;
	HeardNoiseReport noiseReport{};
	std::uint32_t humanOrderSequence = 0;
	float humanOrderReceived = 0, reactionSeconds = .3f;
	bool humanOrderAccepted = false, humanOrderHelp = false;
	bool vipEscaping = false, plantingBomb = false, bombLocationUnknown = false, escortingHostages = false;
	bool canStart = true;
	ChatterScheduleAdmission admission{};
	ChatterVerbosity verbosity = ChatterVerbosity::Normal;
	ChatterScheduleEntry speaking{};
};
struct OutgoingRadioDecision
{
	OutgoingRadioStatus status = OutgoingRadioStatus::None;
	ChatterPhrase phrase = ChatterPhrase::None;
	int menu = 0, slot = 0;
	std::uint16_t place = 0;
	ChatterStatementHandle statement{};
	std::uint64_t directToken = 0;
};
class OutgoingRadioScheduler
{
public:
	void reset()
	{
		pending_.clear();
		resetMetadata();
	}

	ChatterPendingQueue &statements()
	{
		return pending_;
	}
	const ChatterPendingQueue &statements() const
	{
		return pending_;
	}
	bool needsEnemyPlace(const OutgoingRadioInput &in) const
	{
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0 || !in.alive || in.friendCount <= 0 || in.visibleEnemyCount <= 0)
			return false;
		const bool fresh = !(in.actor == actor_) || in.frame.mapGeneration != frame_.mapGeneration ||
			in.frame.roundGeneration != frame_.roundGeneration || in.now < lastUpdate_;
		if (!fresh && reportedEnemies_)
			return false;
		constexpr float kHelpCooldownSeconds = 10.0f;
		const float lastHelp = fresh ? -1.0f : lastHelp_;
		return !(in.visibleEnemyCount > in.nearbyFriends + 1 && (lastHelp < 0 || in.now - lastHelp >= kHelpCooldownSeconds));
	}
	void prepare(const OutgoingRadioInput &in, const ChatterRadioCatalog &catalog)
	{
		constexpr int kAcknowledgementBaseSeconds = 1, kAcknowledgementReactionScale = 2,
					  kAcknowledgementLifetimeSeconds = 3;
		constexpr int kVipReportCooldownSeconds = 30, kVipFollowLifetimeSeconds = 10, kHelpCooldownSeconds = 10;
		constexpr int kReportLifetimeSeconds = 10, kEscortCooldownSeconds = 10, kEscortReportLifetimeSeconds = 5,
					  kTeamSilenceSeconds = 30;
		constexpr float kEnemyReportDelaySeconds = 2;
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0)
			return;
		const bool rewound = in.actor == actor_ && in.frame.mapGeneration == frame_.mapGeneration &&
			in.frame.roundGeneration == frame_.roundGeneration && in.now < lastUpdate_;
		if (actor_.isValid() &&
			(!(in.actor == actor_) || in.frame.mapGeneration != frame_.mapGeneration ||
				in.frame.roundGeneration != frame_.roundGeneration || in.now < lastUpdate_))
		{
			// Event admission may already own this life; reset only ordinary trigger metadata.
			resetMetadata();
			actor_ = in.actor;
			start_ = in.now;
			if (rewound && in.noiseReport.active)
			{
				lastNoiseReport_ = in.noiseReport.created;
				lastNoiseReportTick_ = in.noiseReport.frame.tick;
			}
		}
		if (!actor_.isValid())
		{
			actor_ = in.actor;
			start_ = in.now;
		}
		frame_ = in.frame;
		lastUpdate_ = in.now;
		if (!in.alive || in.team < 1 || in.team > 2)
		{
			cancelOnDeath(in.humanOrderSequence);
			lastNoiseReport_ = in.noiseReport.created;
			lastNoiseReportTick_ = in.noiseReport.frame.tick;
			return;
		}
		pending_.synchronize(admission(in));
		if (!in.visibleEnemyCount)
			reportedEnemies_ = false;
		if (in.humanOrderSequence > orderSequence_ && in.humanOrderAccepted && std::isfinite(in.reactionSeconds) &&
			in.reactionSeconds >= 0)
		{
			orderSequence_ = in.humanOrderSequence;
			ackAt_ = in.humanOrderReceived + kAcknowledgementBaseSeconds +
				kAcknowledgementReactionScale * in.reactionSeconds;
			ack_ = in.humanOrderHelp ? ChatterPhrase::OnMyWay : ChatterPhrase::Affirmative;
		}
		if (ack_ != ChatterPhrase::None && in.now >= ackAt_)
		{
			queue(ack_, in.now, kAcknowledgementLifetimeSeconds, in, catalog);
			ack_ = ChatterPhrase::None;
		}
		if (in.vipEscaping && in.friendCount > 0 && (lastVip_ < 0 || in.now - lastVip_ > kVipReportCooldownSeconds))
		{
			directFollow_ = true;
			++directToken_;
			directFollowStart_ = in.now;
			lastVip_ = in.now;
			directFollowExpires_ = in.now + kVipFollowLifetimeSeconds;
		}
		if (in.friendCount > 0)
		{
			if (!reportedEnemies_ && in.visibleEnemyCount > 0)
			{
				reportedEnemies_ = true;
				if (in.visibleEnemyCount > in.nearbyFriends + 1 &&
					(lastHelp_ < 0 || in.now - lastHelp_ >= kHelpCooldownSeconds))
				{
					queue(ChatterPhrase::Help, in.now, kReportLifetimeSeconds, in, catalog);
					lastHelp_ = in.now;
				}
				else
				{
					queue(ChatterPhrase::EnemySpotted, in.now, kReportLifetimeSeconds, in, catalog, 0,
						in.now + kEnemyReportDelaySeconds);
				}
			}
			if (in.plantingBomb && !wasPlanting_)
				queue(ChatterPhrase::PlantingBomb, in.now, kReportLifetimeSeconds, in, catalog);
			if (in.bombLocationUnknown && !askedBomb_)
			{
				queue(ChatterPhrase::WhereIsTheBomb, in.now, kReportLifetimeSeconds, in, catalog);
				askedBomb_ = true;
			}
			if (in.escortingHostages && (lastEscort_ < 0 || in.now - lastEscort_ >= kEscortCooldownSeconds))
			{
				queue(ChatterPhrase::EscortingHostages, in.now, kEscortReportLifetimeSeconds, in, catalog);
				lastEscort_ = in.now;
			}
			const float silence = in.teamLastSpeech >= 0 ? in.teamLastSpeech : (lastSent_ >= 0 ? lastSent_ : start_);
			if (in.enemiesRemaining > 0 && in.nearbyFriends != in.friendCount && in.now - silence > kTeamSilenceSeconds)
				queue(ChatterPhrase::RequestReport, in.now, kReportLifetimeSeconds, in, catalog);
		}
		wasPlanting_ = in.plantingBomb;
		const auto &report = in.noiseReport;
		constexpr float kNoiseReportLifetimeSeconds = 5.0f;
		if (report.active && report.actor == in.actor && report.frame.mapGeneration == in.frame.mapGeneration &&
			report.frame.roundGeneration == in.frame.roundGeneration && std::isfinite(report.created) &&
			report.created >= 0 && report.created <= in.now &&
			(report.created != lastNoiseReport_ || report.frame.tick != lastNoiseReportTick_))
		{
			if (in.roundOver || in.now > report.created + kNoiseReportLifetimeSeconds)
			{
				lastNoiseReport_ = report.created;
				lastNoiseReportTick_ = report.frame.tick;
			}
			else if (report.navRevision != 0 && report.navRevision == in.navRevision && true)
			{
				queue(
					ChatterPhrase::HeardNoise, report.created, kNoiseReportLifetimeSeconds, in, catalog, report.place);
				lastNoiseReport_ = report.created;
				lastNoiseReportTick_ = report.frame.tick;
			}
		}

		if (directFollow_ && in.now >= directFollowExpires_)
			directFollow_ = false;
	}
	OutgoingRadioDecision decide(const OutgoingRadioInput &in, const ChatterRadioCatalog &catalog,
		std::uint32_t voicePhrases, const ChatterStatementHandle &selected)
	{
		if (directFollow_)
			return {OutgoingRadioStatus::Ready, ChatterPhrase::None, 1, 5, 0, {}, directToken_};
		ChatterPendingStatement node{};
		if (!pending_.read(selected, &node))
			return {};
		legacySelected_ = selected;
		if (node.request.phrase == ChatterPhrase::HeardNoise &&
			(in.roundOver || !in.noiseReport.active || node.request.navRevision != in.navRevision ||
				node.request.created != in.noiseReport.created || node.request.noiseTick != in.noiseReport.frame.tick))
		{
			pending_.erase(selected);
			return {};
		}
		if (!in.canStart || in.now < in.teamBusyUntil)
			return {OutgoingRadioStatus::WaitingTeam, node.request.phrase, 0, 0, node.request.place, selected, 0};
		if (in.friendCount <= 0 || (node.request.requiresCombat && !in.inCombat))
		{
			pending_.erase(selected);
			return {};
		}
		auto phrase = node.request.phrase;
		if (node.request.content == ChatterStatementContent::RemainingEnemies)
		{
			phrase = remainingEnemyPhrase(in.enemiesRemaining);
			if (phrase == ChatterPhrase::None)
			{
				pending_.erase(selected);
				return {};
			}
		}
		constexpr int kImmediateEnemyReportCount = 3;
		if (phrase == ChatterPhrase::EnemySpotted && in.visibleEnemyCount <= kImmediateEnemyReportCount &&
			in.now <= node.request.readyAt)
			return {};
		if (in.now < node.request.start)
			return {};
		const auto choice = catalog.lookup(phrase);
		const bool available = choice.menu || ((voicePhrases & (1U << static_cast<unsigned>(phrase))) != 0);
		return {available ? OutgoingRadioStatus::Ready : OutgoingRadioStatus::MissingPhrase, phrase, choice.menu,
			choice.slot, node.request.place, selected, 0};
	}
	void maintain(const OutgoingRadioInput &in, const ChatterScheduleEntry &teammate = {})
	{
		ChatterPendingMaintenance maintenance{};
		maintenance.admission = admission(in);
		maintenance.inCombat = in.inCombat;
		maintenance.speaking = in.speaking;
		maintenance.teammate = teammate;
		maintenance.roundOver = in.roundOver;
		maintenance.noiseActive = in.noiseReport.active && in.noiseReport.actor == in.actor &&
			in.noiseReport.frame.mapGeneration == in.frame.mapGeneration &&
			in.noiseReport.frame.roundGeneration == in.frame.roundGeneration;
		maintenance.noiseCreated = in.noiseReport.created;
		maintenance.noiseTick = in.noiseReport.frame.tick;
		maintenance.navRevision = in.navRevision;
		pending_.maintain(maintenance);
	}
	OutgoingRadioDecision update(
		const OutgoingRadioInput &in, const ChatterRadioCatalog &catalog, std::uint32_t voicePhrases = 0)
	{
		prepare(in, catalog);
		ChatterTeamSelection selection;
		pending_.visitSchedules(
			[&](const ChatterScheduleEntry &entry)
			{
				return selection.consider(entry);
			});
		const auto selected = selection.finish(in.now);
		auto out = decide(in, catalog, voicePhrases, pending_.handle(selected));
		ChatterScheduleEntry teammate{};
		if (in.now < in.teamBusyUntil && in.teamCurrentPhrase != ChatterPhrase::None)
			teammate.topic = chatterStatementTopic(in.teamCurrentPhrase), teammate.place = in.teamCurrentPlace;
		maintain(in, teammate);
		if (out.status == OutgoingRadioStatus::MissingPhrase)
			pending_.erase(out.statement);
		ChatterPendingStatement retained{};
		if (out.status == OutgoingRadioStatus::WaitingTeam && !pending_.read(out.statement, &retained))
			return {};
		return out;
	}
	ChatterScheduleEntry scheduledStatement() const
	{
		ChatterScheduleEntry first{};
		pending_.visitSchedules(
			[&](const ChatterScheduleEntry &entry)
			{
				first = entry;
				return false;
			});
		return first;
	}
	bool hasDirect() const
	{
		return directFollow_;
	}
	std::uint64_t directToken() const
	{
		return directToken_;
	}
	void discardDirect()
	{
		directFollow_ = false;
		++directToken_;
	}
	void discardDirect(std::uint64_t token)
	{
		if (token == directToken_)
			discardDirect();
	}
	void discardStatement()
	{
		pending_.erase(legacySelected_);
	}
	bool hasPending() const
	{
		return !pending_.empty() || directFollow_;
	}
	float scheduledStart() const
	{
		return directFollow_ ? directFollowStart_ : scheduledStatement().start;
	}
	void confirmSent(const OutgoingRadioInput &in, const OutgoingRadioDecision &out)
	{
		if (out.status != OutgoingRadioStatus::Ready)
			return;
		if (out.directToken)
		{
			if (out.directToken != directToken_)
				return;
			discardDirect();
		}
		else if (!pending_.erase(out.statement))
			return;
		lastSent_ = in.now;
	}
	void discardNoiseReport(const HeardNoiseReport &report)
	{
		if (!report.active || !(report.actor == actor_) || report.frame.mapGeneration != frame_.mapGeneration ||
			report.frame.roundGeneration != frame_.roundGeneration)
			return;
		lastNoiseReport_ = report.created;
		lastNoiseReportTick_ = report.frame.tick;
	}
	void cancelOnDeath(std::uint32_t consumedOrder = 0)
	{
		pending_.clear();
		ack_ = ChatterPhrase::None;
		discardDirect();
		orderSequence_ = (std::max)(orderSequence_, consumedOrder);
	}
	void discardPending()
	{
		pending_.clear();
		discardDirect();
	}

private:
	void resetMetadata()
	{
		actor_ = {};
		frame_ = {};
		lastUpdate_ = 0;
		start_ = 0;
		lastSent_ = -1;
		lastHelp_ = -1;
		lastEscort_ = -1;
		lastVip_ = -1;
		ackAt_ = 0;
		reportedEnemies_ = false;
		wasPlanting_ = false;
		askedBomb_ = false;
		directFollow_ = false;
		orderSequence_ = 0;
		ack_ = ChatterPhrase::None;
		directFollowStart_ = 0;
		directFollowExpires_ = 0;
		lastNoiseReport_ = -1;
		lastNoiseReportTick_ = 0;
		legacySelected_ = {};
		++directToken_;
	}
	ChatterScheduleAdmission admission(const OutgoingRadioInput &in) const
	{
		if (in.admission.owner.actor.isValid())
			return in.admission;
		return {{in.actor, in.frame, 1, in.now, in.alive}, 0, 0, nullptr};
	}
	void queue(ChatterPhrase phrase, float created, float lifetime, const OutgoingRadioInput &in,
		const ChatterRadioCatalog &catalog, std::uint16_t place = 0, float readyAt = 0)
	{
		ChatterStatementRequest request{};
		request.source = ChatterScheduleSource::Generic;
		request.topic = chatterStatementTopic(phrase);
		request.phrase = phrase;
		request.created = created;
		request.start = created;
		request.expires = created + lifetime;
		request.readyAt = readyAt;
		request.place = phrase == ChatterPhrase::PlantingBomb ? in.ownPlace : place;
		request.criteriaPlace = phrase == ChatterPhrase::PlantingBomb ? in.ownPlacePhrase
			: phrase == ChatterPhrase::HeardNoise					  ? in.noisePlacePhrase
																	  : 0;
		if (phrase == ChatterPhrase::Help)
		{
			request.program.append({ChatterPartKind::Place, ChatterPhrase::None, in.ownPlace, in.ownPlacePhrase});
			request.program.append({ChatterPartKind::Fixed, ChatterPhrase::Help, 0, 0});
		}
		else if (phrase == ChatterPhrase::EnemySpotted)
		{
			request.program.append({ChatterPartKind::Place, ChatterPhrase::None, in.enemyPlace, in.enemyPlacePhrase});
			request.program.append({ChatterPartKind::AccumulateEnemies, ChatterPhrase::None, 0, 0});
			request.program.append({ChatterPartKind::CurrentEnemyCount, ChatterPhrase::None, 0, 0});
		}
		request.requiresCombat = phrase == ChatterPhrase::EnemySpotted;
		request.content =
			request.requiresCombat ? ChatterStatementContent::EnemyReport : ChatterStatementContent::Fixed;
		request.navRevision = in.noiseReport.navRevision;
		request.noiseTick = in.noiseReport.frame.tick;
		pending_.admit(request, admission(in), catalog, in.verbosity, in.speaking);
	}
	ChatterPendingQueue pending_{};
	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	float lastUpdate_ = 0, start_ = 0, lastSent_ = -1, lastHelp_ = -1, lastEscort_ = -1, lastVip_ = -1, ackAt_ = 0;
	bool reportedEnemies_ = false, wasPlanting_ = false, askedBomb_ = false, directFollow_ = false;
	std::uint32_t orderSequence_ = 0;
	ChatterPhrase ack_ = ChatterPhrase::None;
	float directFollowStart_ = 0, directFollowExpires_ = 0, lastNoiseReport_ = -1;
	std::uint32_t lastNoiseReportTick_ = 0;
	std::uint64_t directToken_ = 0;
	ChatterStatementHandle legacySelected_{};
};
}
}
#endif
