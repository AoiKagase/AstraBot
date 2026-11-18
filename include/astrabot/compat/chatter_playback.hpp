#ifndef ASTRABOT_COMPAT_CHATTER_PLAYBACK_HPP
#define ASTRABOT_COMPAT_CHATTER_PLAYBACK_HPP
#include "astrabot/compat/chatter_catalog.hpp"
#include <array>
#include <cmath>
namespace astrabot
{
namespace compat
{
enum class ChatterPartKind
{
	Fixed,
	Place,
	CurrentEnemyCount,
	RemainingEnemyCount,
	ShortDelay,
	LongDelay,
	AccumulateEnemies,
	Named
};
struct ChatterPart
{
	ChatterPartKind kind = ChatterPartKind::Fixed;
	ChatterPhrase phrase = ChatterPhrase::None;
	std::uint16_t place = 0;
	std::uint32_t placePhrase = 0;
	std::uint32_t namedPhrase = 0;
};
struct ChatterProgram
{
	static constexpr std::size_t kMaximumParts = 4;
	std::array<ChatterPart, kMaximumParts> parts{};
	std::size_t count = 0;
	bool append(ChatterPart part)
	{
		if (count >= parts.size() || (part.kind == ChatterPartKind::Fixed && part.phrase == ChatterPhrase::None) ||
			(part.kind == ChatterPartKind::Place && (!part.place || !part.placePhrase)) ||
			(part.kind == ChatterPartKind::Named && !part.namedPhrase))
			return false;
		parts[count++] = part;
		return true;
	}
};
struct ChatterPlaybackInput
{
	float now = 0;
	int enemies = 0;
	int nearbyFriends = 0;
	int enemiesRemaining = 0;
	int friendsRemaining = 0;
	bool emote = false;
};
struct ChatterPlaybackState
{
	int index = -1;
	bool speaking = false;
	float speakTimestamp = 0;
	float nextTime = 0;
	bool inFlight = false;
	bool finished = false;
	std::uint64_t revision = 0;
	float pendingTime = 0;
	ChatterPartKind pendingKind = ChatterPartKind::Fixed;
};
enum class ChatterPlaybackKind
{
	Waiting,
	Phrase,
	Delay,
	Null,
	Finished
};
struct ChatterPlaybackDecision
{
	ChatterPlaybackKind kind = ChatterPlaybackKind::Waiting;
	ChatterPhrase phrase = ChatterPhrase::None;
	int enemyCount = -1;
	std::uint32_t placePhrase = 0;
	std::uint32_t namedPhrase = 0;
	std::uint64_t step = 0;
	float low = 0;
	float high = 0;
};
inline void advanceChatterPlaybackRevision(ChatterPlaybackState &state)
{
	if (++state.revision == 0)
		++state.revision;
}
inline ChatterPlaybackDecision advanceChatterPlayback(
	const ChatterProgram &program, ChatterPlaybackState &state, const ChatterPlaybackInput &in)
{
	if (state.finished || state.inFlight || !std::isfinite(in.now) || in.now < 0 ||
		program.count > program.parts.size())
		return {};
	if ((in.friendsRemaining == 0 && !in.emote) || program.count == 0)
	{
		state.finished = true;
		state.speaking = false;
		advanceChatterPlaybackRevision(state);
		ChatterPlaybackDecision out{};
		out.kind = ChatterPlaybackKind::Finished;
		return out;
	}
	if (!state.speaking)
	{
		state.speaking = true;
		state.speakTimestamp = in.now;
		advanceChatterPlaybackRevision(state);
	}
	constexpr float kAccumulationSeconds = 2.0f;
	constexpr int kImmediateReportCount = 3;
	if (state.index >= 0 && static_cast<std::size_t>(state.index) < program.count &&
		program.parts[static_cast<std::size_t>(state.index)].kind == ChatterPartKind::AccumulateEnemies &&
		(in.enemies > kImmediateReportCount || in.now - state.speakTimestamp > kAccumulationSeconds))
	{
		if (state.nextTime != 0)
		{
			state.nextTime = 0;
			advanceChatterPlaybackRevision(state);
		}
	}
	if (in.now <= state.nextTime)
		return {};
	++state.index;
	advanceChatterPlaybackRevision(state);
	if (static_cast<std::size_t>(state.index) == program.count)
	{
		state.finished = true;
		state.speaking = false;
		ChatterPlaybackDecision out{};
		out.kind = ChatterPlaybackKind::Finished;
		out.step = state.revision;
		return out;
	}
	const auto &part = program.parts[static_cast<std::size_t>(state.index)];
	state.pendingKind = part.kind;
	state.pendingTime = in.now;
	ChatterPlaybackDecision out{};
	out.step = state.revision;
	switch (part.kind)
	{
	case ChatterPartKind::Fixed:
		out.phrase = part.phrase;
		break;
	case ChatterPartKind::Place:
		out.placePhrase = part.placePhrase;
		break;
	case ChatterPartKind::Named:
		out.namedPhrase = part.namedPhrase;
		break;
	case ChatterPartKind::CurrentEnemyCount:
		if (in.enemies > 1 && in.enemies - 1 > in.nearbyFriends)
			out.phrase = ChatterPhrase::Help;
		else if (in.enemies > 1)
		{
			out.phrase = ChatterPhrase::EnemySpotted;
			out.enemyCount = in.enemies;
		}
		break;
	case ChatterPartKind::RemainingEnemyCount:
		out.phrase = remainingEnemyPhrase(in.enemiesRemaining);
		break;
	case ChatterPartKind::ShortDelay:
		out.kind = ChatterPlaybackKind::Delay;
		out.low = .1f;
		out.high = .5f;
		state.inFlight = true;
		return out;
	case ChatterPartKind::LongDelay:
		out.kind = ChatterPlaybackKind::Delay;
		out.low = 1.0f;
		out.high = 2.0f;
		state.inFlight = true;
		return out;
	case ChatterPartKind::AccumulateEnemies:
		state.nextTime = 99999999.9f;
		return out;
	}
	if (out.phrase == ChatterPhrase::None && !out.placePhrase && !out.namedPhrase)
	{
		state.nextTime = 0;
		out.kind = ChatterPlaybackKind::Null;
		return out;
	}
	state.inFlight = true;
	out.kind = ChatterPlaybackKind::Phrase;
	return out;
}
inline bool finishChatterPlaybackPart(
	ChatterPlaybackState &state, std::uint64_t step, float duration, bool skip = false)
{
	if (!state.inFlight || !step || step != state.revision || state.finished || !std::isfinite(duration) ||
		duration < 0 || state.pendingKind == ChatterPartKind::ShortDelay ||
		state.pendingKind == ChatterPartKind::LongDelay)
		return false;
	constexpr float kPartGapSeconds = .1f;
	const float next = skip ? 0.0f : state.pendingTime + duration + kPartGapSeconds;
	if (!std::isfinite(next))
		return false;
	state.nextTime = next;
	state.inFlight = false;
	advanceChatterPlaybackRevision(state);
	return true;
}
inline bool finishChatterPlaybackDelay(ChatterPlaybackState &state, std::uint64_t step, float duration)
{
	if (!state.inFlight || !step || step != state.revision || state.finished || !std::isfinite(duration))
		return false;
	const bool shortDelay = state.pendingKind == ChatterPartKind::ShortDelay;
	const bool longDelay = state.pendingKind == ChatterPartKind::LongDelay;
	if ((!shortDelay && !longDelay) || duration < (shortDelay ? .1f : 1.0f) || duration > (shortDelay ? .5f : 2.0f))
		return false;
	const float next = state.pendingTime + duration;
	if (!std::isfinite(next))
		return false;
	state.nextTime = next;
	state.inFlight = false;
	advanceChatterPlaybackRevision(state);
	return true;
}
inline bool cancelChatterPlaybackStep(ChatterPlaybackState &state, std::uint64_t step)
{
	if (!state.inFlight || !step || step != state.revision || state.finished)
		return false;
	--state.index;
	state.inFlight = false;
	state.nextTime = 0;
	advanceChatterPlaybackRevision(state);
	return true;
}
}
}
#endif
