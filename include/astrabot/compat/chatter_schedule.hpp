#ifndef ASTRABOT_COMPAT_CHATTER_SCHEDULE_HPP
#define ASTRABOT_COMPAT_CHATTER_SCHEDULE_HPP

#include "astrabot/compat/friend_death.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace astrabot
{
namespace compat
{
enum class ChatterScheduleSource
{
	None,
	Generic,
	EnemyCount,
	KillPraise,
	CommanderDown,
	KilledFriend,
	Speaking
};
enum class ChatterStatementTopic
{
	None,
	VisibleEnemies,
	EnemyAction,
	MyCurrentTask,
	MyIntention,
	CriticalEvent,
	RequestHelp,
	RequestInformation,
	RoundEnd,
	MyPlan,
	Information,
	Emote,
	Acknowledge,
	EnemiesRemaining,
	FriendlyFire,
	KilledFriend
};
struct ChatterScheduleEntry
{
	ReactionContext owner{};
	std::uint32_t connection = 0;
	std::uint64_t ownerRevision = 0;
	ChatterScheduleSource source = ChatterScheduleSource::None;
	ChatterStatementTopic topic = ChatterStatementTopic::None;
	float created = 0;
	float start = 0;
	std::uint64_t order = 0;
	std::uint16_t place = 0;
	int subject = -1;
	bool speaking = false;
	std::uint32_t firstPhrase = 0;
};
class ChatterAdmissionClock
{
public:
	std::uint64_t next()
	{
		++sequence_;
		if (sequence_ == 0)
			++sequence_;
		return sequence_;
	}

private:
	std::uint64_t sequence_ = 0;
};

struct ChatterScheduleAdmission
{
	ReactionContext owner{};
	std::uint32_t connection = 0;
	std::uint64_t ownerRevision = 0;
	ChatterAdmissionClock *clock = nullptr;

	ChatterScheduleEntry make(ChatterScheduleSource source, ChatterStatementTopic topic, float created, float start,
		std::uint16_t place = 0, int subject = -1) const
	{
		if (!clock || !owner.identityValid() || (!owner.alive && topic != ChatterStatementTopic::RoundEnd))
			return {};
		return {owner, connection, ownerRevision, source, topic, created, start, clock->next(), place, subject, false};
	}
};

inline bool validChatterScheduleEntry(const ChatterScheduleEntry &entry)
{
	return entry.source != ChatterScheduleSource::None && entry.topic != ChatterStatementTopic::None &&
		entry.owner.identityValid() && (entry.owner.alive || entry.topic == ChatterStatementTopic::RoundEnd) &&
		entry.order != 0 && std::isfinite(entry.created) && entry.created >= 0 &&
		std::isfinite(entry.start) && entry.start >= 0;
}

class ChatterTeamSelection
{
public:
	bool consider(const ChatterScheduleEntry &entry)
	{
		if (speaking_ || !validChatterScheduleEntry(entry))
			return !speaking_;
		if (entry.speaking)
		{
			earliest_ = entry;
			speaking_ = true;
			return false;
		}
		if (entry.start < earlyTime_)
		{
			earlyTime_ = entry.created;
			earliest_ = entry;
		}
		return true;
	}
	ChatterScheduleEntry finish(float now) const
	{
		if (!std::isfinite(now) || now < 0)
			return {};
		return speaking_ || (validChatterScheduleEntry(earliest_) && earliest_.start <= now) ? earliest_
																							 : ChatterScheduleEntry{};
	}

private:
	float earlyTime_ = 999999999.9f;
	ChatterScheduleEntry earliest_{};
	bool speaking_ = false;
};

inline ChatterScheduleEntry selectActiveChatter(ChatterScheduleEntry *entries, std::size_t count, float now)
{
	if (!entries || !std::isfinite(now) || now < 0)
	{
		return {};
	}
	std::sort(entries, entries + count,
		[](const ChatterScheduleEntry &a, const ChatterScheduleEntry &b)
		{
			const bool aValid = validChatterScheduleEntry(a);
			const bool bValid = validChatterScheduleEntry(b);
			if (aValid != bValid)
				return aValid;
			if (!aValid)
				return false;
			if (a.owner.actor.slot != b.owner.actor.slot)
				return a.owner.actor.slot < b.owner.actor.slot;
			if (a.start != b.start)
				return a.start < b.start;
			return a.order < b.order;
		});
	// Native compares start to this accumulator, but stores creation time.
	constexpr float kInitialEarliestTime = 999999999.9f;
	float earlyTime = kInitialEarliestTime;
	ChatterScheduleEntry earliest{};
	for (std::size_t i = 0; i < count; ++i)
	{
		const auto &entry = entries[i];
		if (!validChatterScheduleEntry(entry))
			continue;
		if (entry.speaking)
			return entry;
		if (entry.start < earlyTime)
		{
			earlyTime = entry.created;
			earliest = entry;
		}
	}
	return earliest.source != ChatterScheduleSource::None && earliest.start <= now ? earliest : ChatterScheduleEntry{};
}

inline bool chatterStatementsRedundant(const ChatterScheduleEntry &a, const ChatterScheduleEntry &b)
{
	if (a.topic == ChatterStatementTopic::None || a.topic == ChatterStatementTopic::MyPlan ||
		a.topic == ChatterStatementTopic::RequestHelp || a.topic == ChatterStatementTopic::CriticalEvent ||
		a.topic == ChatterStatementTopic::Acknowledge || a.topic != b.topic)
	{
		return false;
	}
	constexpr int kUndefinedSubject = -1;
	if (!a.place && !b.place && a.subject == kUndefinedSubject && b.subject == kUndefinedSubject)
		return true;
	return (a.place != 0 && a.place == b.place) || (a.subject != kUndefinedSubject && a.subject == b.subject);
}
}
}
#endif
