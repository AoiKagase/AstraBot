#ifndef ASTRABOT_COMPAT_OUTGOING_RADIO_HPP
#define ASTRABOT_COMPAT_OUTGOING_RADIO_HPP

#include "astrabot/world/world_snapshot.hpp"
#include "astrabot/compat/noise_report.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace astrabot
{
namespace compat
{
enum class ChatterPhrase : unsigned
{
	None,
	EnemySpotted,
	Help,
	RequestReport,
	Affirmative,
	OnMyWay,
	PlantingBomb,
	WhereIsTheBomb,
	EscortingHostages,
	HeardNoise,
	Count
};
struct RadioMenuChoice
{
	int menu = 0, slot = 0;
};
// Reads only public phrase-to-standard-radio metadata from the host's own DB.
// No voice samples, proprietary phrase text, or presumed fallback equivalents.
class ChatterRadioCatalog
{

public:
	bool loadText(const char *data, std::size_t length)
	{
		if (!data && length)
			return false;
		auto candidate = *this;
		candidate.choices_ = {};
		candidate.important_ = {};
		candidate.loaded_ = false;
		std::size_t pos = 0;
		std::string token;
		while (next(data, length, pos, token))
		{
			if (!equal(token, "Chatter") && !equal(token, "Place"))
				continue;
			const bool chatter = equal(token, "Chatter");
			if (!next(data, length, pos, token))
				return false;
			const ChatterPhrase phrase = chatter ? phraseFor(token) : ChatterPhrase::None;
			bool ended = false;
			while (next(data, length, pos, token))
			{
				if (equal(token, "End"))
				{
					ended = true;
					break;
				}
				if (equal(token, "Important") && phrase != ChatterPhrase::None)
					candidate.important_[static_cast<unsigned>(phrase)] = true;
				if (equal(token, "Radio"))
				{
					if (!next(data, length, pos, token))
						return false;
					if (equal(token, "=") && !next(data, length, pos, token))
						return false;
					const auto choice = eventFor(token);
					if (!choice.menu)
						return false;
					if (phrase != ChatterPhrase::None)
						candidate.choices_[static_cast<unsigned>(phrase)] = choice;
				}
			}
			if (!ended)
				return false;
		}
		candidate.loaded_ = true;
		*this = candidate;
		return true;
	}

	RadioMenuChoice lookup(ChatterPhrase phrase) const
	{
		const auto i = static_cast<unsigned>(phrase);
		return i < choices_.size() ? choices_[i] : RadioMenuChoice{};
	}

	bool loaded() const
	{
		return loaded_;
	}

	bool important(ChatterPhrase p) const
	{
		const auto i = static_cast<unsigned>(p);
		return i < important_.size() && important_[i];
	}

	static const char *name(ChatterPhrase p)
	{
		const char *names[] = {"None", "EnemySpotted", "Help", "RequestReport", "Affirmative", "OnMyWay",
			"PlantingBomb", "WhereIsTheBomb", "EscortingHostages", "HeardNoise"};
		const auto i = static_cast<unsigned>(p);
		return i < static_cast<unsigned>(ChatterPhrase::Count) ? names[i] : "Unknown";
	}

private:
	static bool equal(const std::string &a, const char *b)
	{
		if (a.size() != std::strlen(b))
			return false;
		for (std::size_t i = 0; i < a.size(); ++i)
		{
			const char x = a[i] >= 'a' && a[i] <= 'z' ? static_cast<char>(a[i] - 'a' + 'A') : a[i];
			const char y = b[i] >= 'a' && b[i] <= 'z' ? static_cast<char>(b[i] - 'a' + 'A') : b[i];
			if (x != y)
				return false;
		}
		return true;
	}

	static bool next(const char *d, std::size_t n, std::size_t &p, std::string &out)
	{
		constexpr int kMaximumTokenLength = 512;
		out.clear();
		while (p < n)
		{
			if (d[p] == ' ' || d[p] == '\t' || d[p] == '\r' || d[p] == '\n')
			{
				++p;
				continue;
			}
			if (p + 1 < n && d[p] == '/' && d[p + 1] == '/')
			{
				while (p < n && d[p] != '\n')
					++p;
				continue;
			}
			break;
		}
		if (p >= n)
			return false;
		const bool quote = d[p] == '"';
		if (quote)
			++p;
		while (p < n)
		{
			const char c = d[p++];
			if ((quote && c == '"') || (!quote && (c == ' ' || c == '\t' || c == '\r' || c == '\n')))
				break;
			out.push_back(c);
			if (out.size() > kMaximumTokenLength)
				return false;
		}
		return !out.empty();
	}

	static ChatterPhrase phraseFor(const std::string &s)
	{
		for (unsigned i = 1; i < static_cast<unsigned>(ChatterPhrase::Count); ++i)
			if (equal(s, name(static_cast<ChatterPhrase>(i))))
				return static_cast<ChatterPhrase>(i);
		return ChatterPhrase::None;
	}

	static RadioMenuChoice eventFor(std::string s)
	{
		constexpr int kEventPrefixLength = 6;
		constexpr int kRadioEventCount = 21;
		constexpr int kFirstMenuEventLimit = 6;
		constexpr int kSecondMenuEventLimit = 12;
		constexpr int kCommandRadioMenu = 1;
		constexpr int kGroupRadioMenu = 2;
		constexpr int kReportRadioMenu = 3;
		constexpr int kSecondMenuSlotOffset = 5;
		constexpr int kThirdMenuSlotOffset = 11;
		if (s.size() > kEventPrefixLength && equal(s.substr(0, kEventPrefixLength), "EVENT_"))
			s = s.substr(kEventPrefixLength);
		const char *events[] = {"RADIO_COVER_ME", "RADIO_YOU_TAKE_THE_POINT", "RADIO_HOLD_THIS_POSITION",
			"RADIO_REGROUP_TEAM", "RADIO_FOLLOW_ME", "RADIO_TAKING_FIRE", "RADIO_GO_GO_GO", "RADIO_TEAM_FALL_BACK",
			"RADIO_STICK_TOGETHER_TEAM", "RADIO_GET_IN_POSITION_AND_WAIT", "RADIO_STORM_THE_FRONT",
			"RADIO_REPORT_IN_TEAM", "RADIO_AFFIRMATIVE", "RADIO_ENEMY_SPOTTED", "RADIO_NEED_BACKUP",
			"RADIO_SECTOR_CLEAR", "RADIO_IN_POSITION", "RADIO_REPORTING_IN", "RADIO_GET_OUT_OF_THERE", "RADIO_NEGATIVE",
			"RADIO_ENEMY_DOWN"};
		for (unsigned i = 0; i < kRadioEventCount; ++i)
		{
			if (equal(s, events[i]))
				return {i < kFirstMenuEventLimit ? kCommandRadioMenu
												 : (i < kSecondMenuEventLimit ? kGroupRadioMenu : kReportRadioMenu),
					static_cast<int>(i < kFirstMenuEventLimit
							? i + 1
							: (i < kSecondMenuEventLimit ? i - kSecondMenuSlotOffset : i - kThirdMenuSlotOffset))};
		}
		return {};
	}

	std::array<RadioMenuChoice, static_cast<unsigned>(ChatterPhrase::Count)> choices_{};

	std::array<bool, static_cast<unsigned>(ChatterPhrase::Count)> important_{};

	bool loaded_ = false;
};
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
		constexpr float kContactWatchSeconds = 3.0f;
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
};
struct OutgoingRadioDecision
{
	OutgoingRadioStatus status = OutgoingRadioStatus::None;
	ChatterPhrase phrase = ChatterPhrase::None;
	int menu = 0, slot = 0;
	std::uint16_t place = 0;
};
class OutgoingRadioScheduler
{

public:
	OutgoingRadioDecision update(const OutgoingRadioInput &in, const ChatterRadioCatalog &catalog,
		std::uint32_t voicePhrases = 0U)
	{
		constexpr int kTerroristTeam = 1;
		constexpr int kCounterTerroristTeam = 2;
		constexpr float kAcknowledgementBaseSeconds = 1.0f;
		constexpr int kAcknowledgementReactionScale = 2;
		constexpr int kAcknowledgementLifetimeSeconds = 3;
		constexpr int kVipReportCooldownSeconds = 30;
		constexpr int kVipFollowLifetimeSeconds = 10;
		constexpr int kHelpCooldownSeconds = 10;
		constexpr int kReportLifetimeSeconds = 10;
		constexpr float kEnemyReportDelaySeconds = 2.0f;
		constexpr int kEscortCooldownSeconds = 10;
		constexpr int kEscortReportLifetimeSeconds = 5;
		constexpr int kTeamSilenceSeconds = 30;
		constexpr int kImmediateEnemyReportCount = 3;
		constexpr int kFollowMenu = 1;
		constexpr int kFollowSlot = 5;
		if (!in.actor.isValid() || !in.frame.isValid() || !std::isfinite(in.now) || in.now < 0)
			return {};
		const bool clockRewound = in.actor == actor_ && in.frame.mapGeneration == frame_.mapGeneration &&
			in.frame.roundGeneration == frame_.roundGeneration && in.now < lastUpdate_;
		if (!(in.actor == actor_) || in.frame.mapGeneration != frame_.mapGeneration ||
			in.frame.roundGeneration != frame_.roundGeneration || in.now < lastUpdate_)
		{
			*this = OutgoingRadioScheduler();
			actor_ = in.actor;
			frame_ = in.frame;
			start_ = in.now;
			if (clockRewound && in.noiseReport.active)
			{
				lastNoiseReport_ = in.noiseReport.created;
				lastNoiseReportTick_ = in.noiseReport.frame.tick;
			}
		}
		frame_ = in.frame;
		lastUpdate_ = in.now;
		if (!in.alive || in.team < kTerroristTeam || in.team > kCounterTerroristTeam)
		{
			pending_ = ChatterPhrase::None;
			lastNoiseReport_ = in.noiseReport.created;
			lastNoiseReportTick_ = in.noiseReport.frame.tick;
			return {};
		}
		if (pending_ == ChatterPhrase::HeardNoise &&
			(in.roundOver || in.navRevision != pendingNoiseRevision_ || !in.noiseReport.active ||
				!(in.noiseReport.actor == in.actor) ||
				in.noiseReport.frame.mapGeneration != in.frame.mapGeneration ||
				in.noiseReport.frame.roundGeneration != in.frame.roundGeneration ||
				in.noiseReport.created != lastNoiseReport_ || in.noiseReport.frame.tick != lastNoiseReportTick_))
		{
			pending_ = ChatterPhrase::None;
		}
		if (!in.visibleEnemyCount)
		{
			reportedEnemies_ = false;
			if (pending_ == ChatterPhrase::EnemySpotted)
				pending_ = ChatterPhrase::None;
		}
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
			queue(ack_, in.now, kAcknowledgementLifetimeSeconds);
			ack_ = ChatterPhrase::None;
		}
		if (in.vipEscaping && in.friendCount > 0 && (lastVip_ < 0 || in.now - lastVip_ > kVipReportCooldownSeconds))
		{
			directFollow_ = true;
			lastVip_ = in.now;
			expires_ = in.now + kVipFollowLifetimeSeconds;
		}
		if (in.friendCount > 0)
		{
			if (!reportedEnemies_ && in.visibleEnemyCount > 0)
			{
				reportedEnemies_ = true;
				if (in.visibleEnemyCount > in.nearbyFriends + 1 &&
					(lastHelp_ < 0 || in.now - lastHelp_ >= kHelpCooldownSeconds))
				{
					queue(ChatterPhrase::Help, in.now, kReportLifetimeSeconds);
					lastHelp_ = in.now;
				}
				else
				{
					queue(ChatterPhrase::EnemySpotted, in.now, kReportLifetimeSeconds);
					enemyReportAt_ = in.now + kEnemyReportDelaySeconds;
				}
			}
			if (in.plantingBomb && !wasPlanting_)
				queue(ChatterPhrase::PlantingBomb, in.now, kReportLifetimeSeconds);
			if (in.bombLocationUnknown && !askedBomb_)
			{
				queue(ChatterPhrase::WhereIsTheBomb, in.now, kReportLifetimeSeconds);
				askedBomb_ = true;
			}
			if (in.escortingHostages && (lastEscort_ < 0 || in.now - lastEscort_ >= kEscortCooldownSeconds))
			{
				queue(ChatterPhrase::EscortingHostages, in.now, kEscortReportLifetimeSeconds);
				lastEscort_ = in.now;
			}
			const float silence = in.teamLastSpeech >= 0 ? in.teamLastSpeech : (lastSent_ >= 0 ? lastSent_ : start_);
			if (in.enemiesRemaining > 0 && in.nearbyFriends != in.friendCount &&
				in.now - silence > kTeamSilenceSeconds && pending_ == ChatterPhrase::None)
				queue(ChatterPhrase::RequestReport, in.now, kReportLifetimeSeconds);
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
			else if (report.navRevision != 0 && report.navRevision == in.navRevision &&
				pending_ == ChatterPhrase::None && !directFollow_)
			{
				queue(ChatterPhrase::HeardNoise, report.created, kNoiseReportLifetimeSeconds);
				pendingPlace_ = report.place;
				pendingNoiseRevision_ = report.navRevision;
				lastNoiseReport_ = report.created;
				lastNoiseReportTick_ = report.frame.tick;
			}
		}
		const bool expired = pending_ == ChatterPhrase::HeardNoise ? in.now > expires_ : in.now >= expires_;
		if ((pending_ != ChatterPhrase::None || directFollow_) && expired)
		{
			pending_ = ChatterPhrase::None;
			directFollow_ = false;
		}
		if (pending_ == ChatterPhrase::None && !directFollow_)
			return {};
		if (pending_ == ChatterPhrase::EnemySpotted && !in.inCombat)
		{
			pending_ = ChatterPhrase::None;
			return {};
		}
		if (pending_ == ChatterPhrase::EnemySpotted && in.visibleEnemyCount <= kImmediateEnemyReportCount &&
			in.now <= enemyReportAt_)
			return {};
		if (in.now < in.teamBusyUntil)
		{
			// Same topic without place/subject is redundant while a teammate says it.
			const bool samePlace = (pendingPlace_ == 0 && in.teamCurrentPlace == 0) ||
				(pendingPlace_ != 0 && pendingPlace_ == in.teamCurrentPlace);
			if (pending_ == in.teamCurrentPhrase && !directFollow_ &&
				(pending_ != ChatterPhrase::HeardNoise || samePlace))
			{
				pending_ = ChatterPhrase::None;
				return {};
			}
			return {OutgoingRadioStatus::WaitingTeam, pending_, 0, 0};
		}
		const auto choice = directFollow_ ? RadioMenuChoice{kFollowMenu, kFollowSlot} : catalog.lookup(pending_);
		if (!choice.menu && pending_ != ChatterPhrase::None &&
			(voicePhrases & (1U << static_cast<unsigned>(pending_))) != 0U)
			return {OutgoingRadioStatus::Ready, pending_, 0, 0, pendingPlace_};
		if (!choice.menu)
		{
			const auto missing = pending_;
			pending_ = ChatterPhrase::None;
			return {OutgoingRadioStatus::MissingPhrase, missing, 0, 0};
		}
		return {OutgoingRadioStatus::Ready, directFollow_ ? ChatterPhrase::None : pending_, choice.menu, choice.slot,
			directFollow_ ? std::uint16_t{0} : pendingPlace_};
	}

	void confirmSent(const OutgoingRadioInput &in, const OutgoingRadioDecision &out)
	{
		if (out.status != OutgoingRadioStatus::Ready)
			return;
		lastSent_ = in.now;
		pending_ = ChatterPhrase::None;
		directFollow_ = false;
	}

	// AddStatement drops a new OFF/non-important MINIMAL statement independently of older topics.
	void discardNoiseReport(const HeardNoiseReport &report)
	{
		if (!report.active || !(report.actor == actor_) || report.frame.mapGeneration != frame_.mapGeneration ||
			report.frame.roundGeneration != frame_.roundGeneration)
		{
			return;
		}
		lastNoiseReport_ = report.created;
		lastNoiseReportTick_ = report.frame.tick;
		if (pending_ == ChatterPhrase::HeardNoise)
		{
			pending_ = ChatterPhrase::None;
		}
	}

	void discardPending()
	{
		pending_ = ChatterPhrase::None;
		directFollow_ = false;
	}

private:
	void queue(ChatterPhrase p, float now, float lifetime)
	{
		if (pending_ != ChatterPhrase::None)
			return;
		pending_ = p;
		pendingPlace_ = 0;
		expires_ = now + lifetime;
	}

	world::ActorKey actor_{};
	world::FrameIdentity frame_{};
	float lastUpdate_ = 0, start_ = 0, lastSent_ = -1, lastHelp_ = -1, lastEscort_ = -1, lastVip_ = -1, ackAt_ = 0,
		  expires_ = 0, enemyReportAt_ = 0;
	bool reportedEnemies_ = false, wasPlanting_ = false, askedBomb_ = false, directFollow_ = false;
	std::uint32_t orderSequence_ = 0;
	ChatterPhrase pending_ = ChatterPhrase::None, ack_ = ChatterPhrase::None;
	float lastNoiseReport_ = -1;
	std::uint32_t lastNoiseReportTick_ = 0;
	std::uint16_t pendingPlace_ = 0;
	std::uint64_t pendingNoiseRevision_ = 0;
};
} // namespace compat
} // namespace astrabot
#endif
