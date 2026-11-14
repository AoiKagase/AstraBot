#ifndef ASTRABOT_COMPAT_FRIEND_DEATH_CHATTER_HPP
#define ASTRABOT_COMPAT_FRIEND_DEATH_CHATTER_HPP

#include "astrabot/compat/friend_death.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include <array>
#include <cmath>

namespace astrabot
{
namespace compat
{
inline bool sameChatterReceipt(const ReactionContext &expected, const ReactionContext &current)
{
	return expected.valid() && current.valid() && expected.actor == current.actor &&
		expected.entityGeneration == current.entityGeneration && expected.frame == current.frame &&
		expected.now == current.now;
}

// Native keeps a spoken statement in the owner's list until its next update.
// Preserve that admission guard after transport confirms and the pending slot is removed.
class ConfirmedChatterSpeech
{
public:
	void reset()
	{
		context_ = {};
		phrase_ = ChatterPhrase::None;
		until_ = 0;
		advanceRevision();
	}

	void maintain(const ReactionContext &context)
	{
		if (!context.valid())
		{
			reset();
			return;
		}
		if (!matches(context))
		{
			reset();
		}
		context_ = context;
		if (phrase_ != ChatterPhrase::None && context.now >= until_)
		{
			phrase_ = ChatterPhrase::None;
			advanceRevision();
		}
	}

	std::uint64_t revision() const
	{
		return revision_;
	}

	bool canComplete(const ReactionContext &expected, const ReactionContext &current, std::uint64_t revision) const
	{
		return revision == revision_ && matches(expected) && sameChatterReceipt(expected, current);
	}

	bool record(const ReactionContext &context, ChatterPhrase phrase, float until, std::uint64_t revision)
	{
		if (revision != revision_ || !matches(context) || phrase == ChatterPhrase::None || !std::isfinite(until) ||
			until <= context.now)
		{
			return false;
		}
		phrase_ = phrase;
		until_ = until;
		advanceRevision();
		return true;
	}

	bool blocks(ChatterPhrase phrase, const ReactionContext &context) const
	{
		return matches(context) &&
			((isIntentionPhrase(phrase_) && isIntentionPhrase(phrase)) ||
				(phrase_ == ChatterPhrase::KilledFriend && phrase == ChatterPhrase::KilledFriend));
	}

private:
	bool matches(const ReactionContext &context) const
	{
		return context.valid() && context_.valid() && context.actor == context_.actor &&
			context.entityGeneration == context_.entityGeneration &&
			context.frame.mapGeneration == context_.frame.mapGeneration &&
			context.frame.roundGeneration == context_.frame.roundGeneration &&
			context.frame.tick >= context_.frame.tick && context.now >= context_.now;
	}

	void advanceRevision()
	{
		++revision_;
		if (revision_ == 0)
		{
			++revision_;
		}
	}

	ReactionContext context_{};
	ChatterPhrase phrase_ = ChatterPhrase::None;
	float until_ = 0;
	std::uint64_t revision_ = 0;
};

struct FriendDeathChatterEvent
{
	ReactionContext context{};
	RandomTimingContext timing{};
	bool victimFriendly = false;
	bool victimHuman = false;
	bool careerKnown = false;
	bool career = false;
	bool killerHumanFriendly = false;
	bool killerSelf = false;
	bool retaliationReturned = false;
};

inline bool commanderDownEligible(const FriendDeathChatterEvent &in)
{
	return in.context.valid() && !in.retaliationReturned && in.victimFriendly && in.victimHuman && in.careerKnown &&
		in.career;
}

struct KilledFriendDelay
{
	bool valid = false;
	float seconds = 0;
};

inline KilledFriendDelay prepareKilledFriend(const FriendDeathChatterEvent &in, ICompatibilityRandomSource &random)
{
	if (!in.context.valid() || in.retaliationReturned || !in.victimFriendly || !in.killerHumanFriendly || in.killerSelf)
	{
		return {};
	}
	constexpr float kMinimumDelaySeconds = .5f;
	constexpr float kMaximumDelaySeconds = 1.0f;
	const auto delay = random.nextFloat(RandomRequest::floatRequest("RNG-CSBOT-KILLED-FRIEND-DELAY",
		{in.context.actor.slot, in.context.actor.generation}, in.timing, kMinimumDelaySeconds, kMaximumDelaySeconds));
	return {delay.status == RandomStatus::Ok && std::isfinite(delay.value) && delay.value >= kMinimumDelaySeconds &&
			delay.value <= kMaximumDelaySeconds,
		delay.value};
}

struct FriendDeathChatterSpeechInput
{
	ReactionContext context{};
	std::uint32_t allowedPhrases = 0;
	float teamBusyUntil = 0;
	ChatterPhrase teamPhrase = ChatterPhrase::None;
	int friendsRemaining = 0;
	bool roundOver = false;
	bool canStart = true;
};

struct FriendDeathChatterDecision
{
	bool ready = false;
	ChatterPhrase phrase = ChatterPhrase::None;
	float startTime = 0;
	std::uint64_t sequence = 0;
	ReactionContext owner{};
};

class FriendDeathChatterPolicy
{
	static constexpr std::size_t kCommanderSlot = 0;
	static constexpr std::size_t kKilledFriendSlot = 1;
	static constexpr std::size_t kTopicCount = 2;

public:
	void reset()
	{
		// Keep receipt numbers monotonic across same-owner death/round resets.
		context_ = {};
		slots_ = {};
	}

	bool admitCommander(const ReactionContext &context, std::uint32_t allowed, bool intentionPending)
	{
		constexpr float kCommanderLifetimeSeconds = 20.0f;
		return synchronize(context) && !intentionPending &&
			admit(kCommanderSlot, ChatterPhrase::CommanderDown, context, 0, kCommanderLifetimeSeconds, allowed);
	}

	bool admitKilled(
		const ReactionContext &context, KilledFriendDelay delay, std::uint32_t allowed, bool ownSpeaking = false)
	{
		constexpr float kKilledLifetimeSeconds = 2.0f;
		constexpr float kMinimumDelaySeconds = .5f;
		constexpr float kMaximumDelaySeconds = 1.0f;
		return synchronize(context) && !ownSpeaking && delay.valid && std::isfinite(delay.seconds) &&
			delay.seconds >= kMinimumDelaySeconds && delay.seconds <= kMaximumDelaySeconds &&
			admit(kKilledFriendSlot, ChatterPhrase::KilledFriend, context, delay.seconds, kKilledLifetimeSeconds,
				allowed);
	}

	bool hasCommander(const ReactionContext &context) const
	{
		return matches(context) && slots_[kCommanderSlot].sequence != 0;
	}

	bool active() const
	{
		return slots_[0].sequence != 0 || slots_[kKilledFriendSlot].sequence != 0;
	}

	float scheduledStart() const
	{
		return peek().startTime;
	}

	FriendDeathChatterDecision update(const FriendDeathChatterSpeechInput &in)
	{
		if (!synchronize(in.context))
		{
			return {};
		}
		for (auto &slot : slots_)
		{
			const bool redundant = in.context.now < in.teamBusyUntil &&
				((isIntentionPhrase(slot.phrase) && isIntentionPhrase(in.teamPhrase)) ||
					(slot.phrase == ChatterPhrase::KilledFriend && in.teamPhrase == ChatterPhrase::KilledFriend));
			if (slot.sequence != 0 &&
				(in.roundOver || in.context.now > slot.expires || redundant ||
					!(in.allowedPhrases & (1U << static_cast<unsigned>(slot.phrase)))))
			{
				slot = {};
			}
		}
		auto out = peek();
		if (!out.sequence || !in.canStart || in.context.now < out.startTime || in.context.now < in.teamBusyUntil)
		{
			return out;
		}
		if (in.friendsRemaining <= 0)
		{
			discard(out);
			return {};
		}
		out.ready = true;
		return out;
	}

	void discard(const FriendDeathChatterDecision &decision)
	{
		if (!(decision.owner.actor == context_.actor) || decision.owner.entityGeneration != context_.entityGeneration ||
			decision.owner.frame.mapGeneration != context_.frame.mapGeneration ||
			decision.owner.frame.roundGeneration != context_.frame.roundGeneration)
		{
			return;
		}
		for (auto &slot : slots_)
		{
			if (slot.sequence == decision.sequence && slot.phrase == decision.phrase)
			{
				slot = {};
			}
		}
	}

private:
	struct Slot
	{
		ChatterPhrase phrase = ChatterPhrase::None;
		float start = 0;
		float expires = 0;
		std::uint64_t sequence = 0;
	};

	bool matches(const ReactionContext &context) const
	{
		return context.valid() && context_.valid() && context.actor == context_.actor &&
			context.entityGeneration == context_.entityGeneration &&
			context.frame.mapGeneration == context_.frame.mapGeneration &&
			context.frame.roundGeneration == context_.frame.roundGeneration &&
			context.frame.tick >= context_.frame.tick && context.now >= context_.now;
	}

	bool synchronize(const ReactionContext &context)
	{
		if (!context.valid())
		{
			reset();
			return false;
		}
		if (!matches(context))
		{
			reset();
		}
		context_ = context;
		return true;
	}

	bool admit(std::size_t index, ChatterPhrase phrase, const ReactionContext &context, float delay, float lifetime,
		std::uint32_t allowed)
	{
		if (slots_[index].sequence != 0 || !(allowed & (1U << static_cast<unsigned>(phrase))))
		{
			return false;
		}
		++sequence_;
		if (sequence_ == 0)
		{
			++sequence_;
		}
		slots_[index] = {phrase, context.now + delay, context.now + lifetime, sequence_};
		return true;
	}

	FriendDeathChatterDecision peek() const
	{
		const Slot *first = nullptr;
		for (const auto &slot : slots_)
		{
			if (slot.sequence != 0 &&
				(!first || slot.start < first->start ||
					(slot.start == first->start && slot.sequence < first->sequence)))
			{
				first = &slot;
			}
		}
		return first ? FriendDeathChatterDecision{false, first->phrase, first->start, first->sequence, context_}
					 : FriendDeathChatterDecision{};
	}

	ReactionContext context_{};
	std::array<Slot, kTopicCount> slots_{};
	std::uint64_t sequence_ = 0;
};
}
}
#endif
