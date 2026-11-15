#include "astrabot/compat/friend_death_chatter.hpp"
#include "astrabot/compat/chatter_voice.hpp"
#include <cstdio>
#include <cstring>
#include <limits>

int main()
{
	using namespace astrabot::compat;
	int failures = 0;
	const auto check = [&](bool ok, const char *message)
	{
		if (!ok)
		{
			std::printf("FAIL: %s\n", message);
			++failures;
		}
	};
	ReactionContext context{{1, 2}, {3, 4, 5}, 6, 10, true};
	ConfirmedChatterSpeech speech;
	check(!speech.wasTalking(context), "pending/no statement is not native speaking");
	speech.maintain(context);
	const auto transportRevision = speech.revision();
	check(speech.beginSpeaking(context, ChatterPhrase::CommanderDown, 10.3f, transportRevision) &&
			speech.wasTalking(context) && speech.revision() == transportRevision,
		"native speaking begins before transport callback completion");
	check(speech.record(context, ChatterPhrase::CommanderDown, 10.3f, transportRevision), "real speech confirms");
	auto exactEnd = context;
	exactEnd.now = 10.3f;
	auto boundarySpeech = speech;
	boundarySpeech.maintain(exactEnd);
	check(boundarySpeech.wasTalking(exactEnd),
		"native speaking survives exact nextTime boundary until strictly later owner update");
	auto death = context;
	death.alive = false;
	death.frame.tick = 6;
	death.now = 10.1f;
	check(speech.wasTalking(death), "already dead owner retains interrupted statement");
	check(speech.ownsSpeech(death, ChatterPhrase::CommanderDown, 10.3f), "death owns its exact team speech lock");
	check(!speech.ownsSpeech(death, ChatterPhrase::Help, 10.3f) &&
			!speech.ownsSpeech(death, ChatterPhrase::CommanderDown, 11.3f),
		"another team statement survives death");
	check(sameChatterUpdateReceipt(death, death) && !sameChatterReceipt(death, death),
		"death transport identity is distinct from living normal speech");
	for (int change = 0; change < 7; ++change)
	{
		auto other = death;
		if (change == 0)
			++other.actor.generation;
		if (change == 1)
			++other.entityGeneration;
		if (change == 2)
			++other.frame.mapGeneration;
		if (change == 3)
			++other.frame.roundGeneration;
		if (change == 4)
			other.frame.tick = 4;
		if (change == 5)
			other.now = 9;
		if (change == 6)
			other.now = (std::numeric_limits<float>::quiet_NaN)();
		check(!speech.wasTalking(other) && !sameChatterUpdateReceipt(death, other), "stale death cannot borrow speech");
	}
	auto changedLife = death;
	changedLife.alive = true;
	check(!sameChatterUpdateReceipt(death, changedLife), "revival during death callback cancels old transport");
	death.now = 20;
	check(speech.wasTalking(death), "native speaking persists until its next update removes the node");
	auto update = death;
	update.alive = true;
	speech.maintain(update);
	check(!speech.wasTalking(death), "completed statement no longer produces pain after update");
	speech.maintain(context);
	check(speech.record(context, ChatterPhrase::Help, 11, speech.revision()), "another real speech starts");
	speech.reset();
	check(!speech.wasTalking(context), "public death consumes speech once");
	for (const char *mode : {static_cast<const char *>(nullptr), "normal", "MINIMAL", "m", "N", "unknown", ""})
		check(deathInterruptVoiceEnabled(mode), "native normal/minimal first-letter mode allows pain");
	for (const char *mode : {"off", "OFF", "o", "radio", "Radio", "r"})
		check(!deathInterruptVoiceEnabled(mode), "native off/radio first-letter mode suppresses pain");
	const char *metadata = "Chatter pain Radio = EVENT_RADIO_NEED_BACKUP End";
	ChatterRadioCatalog catalog;
	check(catalog.loadText(metadata, std::strlen(metadata)) && catalog.defined(ChatterPhrase::Pain),
		"default catalog registers native pain definition");
	ChatterVoiceCatalog bank;
	const char *clips =
		"Chatter pain Place = UNDEFINED Count = Many wrong_place.wav Place = ANY Count = 0 wrong_count.wav Count = Many pain.wav End";
	check(bank.loadText(clips, std::strlen(clips)), "profile voice bank parses native pain");
	auto *clip = bank.select(ChatterPhrase::Pain, -1, "ANY");
	check(clip && clip->sample == "pain.wav", "direct pain uses default ANY place and native undefined CountMany");
	OutgoingRadioScheduler queued;
	OutgoingRadioInput input{};
	input.actor = context.actor;
	input.frame = context.frame;
	input.now = 10;
	input.alive = true;
	input.team = 1;
	input.friendCount = 1;
	input.humanOrderSequence = 1;
	input.humanOrderAccepted = true;
	input.humanOrderReceived = 10;
	input.reactionSeconds = .3f;
	(void)queued.update(input, catalog);
	queued.cancelOnDeath(1);
	input.now = 12;
	(void)queued.update(input, catalog, 1U << static_cast<unsigned>(ChatterPhrase::Affirmative));
	check(!queued.hasPending(), "death cancels delayed acknowledgement without re-admitting consumed order");
	input.humanOrderSequence = 2;
	input.humanOrderReceived = 12;
	(void)queued.update(input, catalog);
	input.now = 14;
	check(queued.update(input, catalog, 1U << static_cast<unsigned>(ChatterPhrase::Affirmative)).phrase ==
			ChatterPhrase::Affirmative,
		"fresh post-death order can still acknowledge");
	OutgoingRadioScheduler planting;
	input.humanOrderSequence = 0;
	input.plantingBomb = true;
	input.teamBusyUntil = 20;
	(void)planting.update(input, catalog, 1U << static_cast<unsigned>(ChatterPhrase::PlantingBomb));
	check(planting.hasPending(), "ordinary statement is pending before death");
	planting.cancelOnDeath(2);
	input.now = 15;
	input.teamBusyUntil = 0;
	(void)planting.update(input, catalog, 1U << static_cast<unsigned>(ChatterPhrase::PlantingBomb));
	check(!planting.hasPending(), "death clears ordinary pending while preserving trigger history");
	OutgoingRadioScheduler direct;
	input.plantingBomb = false;
	input.vipEscaping = true;
	(void)direct.update(input, catalog);
	check(direct.hasPending(), "VIP direct request is pending before death");
	direct.cancelOnDeath();
	input.now = 16;
	(void)direct.update(input, catalog);
	check(!direct.hasPending(), "death clears direct requests while preserving cooldown history");
	return failures == 0 ? 0 : 1;
}
