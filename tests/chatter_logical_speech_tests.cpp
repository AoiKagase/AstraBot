#include "astrabot/compat/friend_death_chatter.hpp"
#include <cstdio>
int main()
{
	using namespace astrabot::compat;
	const auto check = [](bool ok, const char *text)
	{
		if (!ok)
			std::fprintf(stderr, "check failed: %s\n", text);
		return ok;
	};
	ReactionContext context{{1, 2}, {3, 4, 5}, 6, 10, true};
	ConfirmedChatterSpeech speech;
	speech.synchronizeOwner(context);
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission admission{context, 7, speech.lifetimeRevision(), &clock};
	auto schedule = admission.make(ChatterScheduleSource::Generic, ChatterStatementTopic::VisibleEnemies, 10, 10);
	const auto revision = speech.revision();
	if (!check(speech.beginStatement(context, schedule, revision) && speech.wasTalking(context) &&
				speech.scheduledStatement().speaking,
			"logical speaking starts before a delay or silent part without an audio phrase"))
		return 1;
	auto later = context;
	later.now = 20;
	later.frame.tick = 6;
	if (!check(!speech.completeIfElapsed(later) && speech.wasTalking(later),
			"part timeout cannot complete a managed multipart statement"))
		return 1;
	auto dead = later;
	dead.alive = false;
	if (!check(speech.wasTalking(dead), "silent logical owner authorizes native death interruption"))
		return 1;
	if (!check(speech.beginSpeaking(later, ChatterPhrase::None, 21, revision) &&
				speech.record(later, ChatterPhrase::None, 21, revision),
			"Place audio keeps logical ownership without inventing a fixed phrase"))
		return 1;
	auto wrong = schedule;
	++wrong.order;
	if (!check(!speech.finishStatement(later, wrong, speech.revision()) && speech.wasTalking(later),
			"different statement cannot clear logical owner"))
		return 1;
	if (!check(speech.finishStatement(later, schedule, speech.revision()) && !speech.wasTalking(later),
			"exact final statement completion releases logical owner once"))
		return 1;
	const auto old = speech.revision();
	speech.reset();
	speech.synchronizeOwner(context);
	if (!check(!speech.beginStatement(context, schedule, old), "same owner reset rejects stale logical begin receipt"))
		return 1;
	return 0;
}
