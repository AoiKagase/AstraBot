#include "astrabot/compat/chatter_statements.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
using namespace astrabot::compat;
bool check(bool ok, const char *message)
{
	if (!ok)
	{
		std::fprintf(stderr, "check failed: %s\n", message);
	}
	return ok;
}
ChatterStatementRequest fixed(ChatterPhrase phrase, float created, float start, float expires)
{
	ChatterStatementRequest out{};
	out.source = ChatterScheduleSource::Generic;
	out.topic = chatterStatementTopic(phrase);
	out.phrase = phrase;
	out.created = created;
	out.start = start;
	out.expires = expires;
	out.readyAt = start;
	return out;
}
std::vector<ChatterScheduleEntry> schedules(const ChatterPendingQueue &queue)
{
	std::vector<ChatterScheduleEntry> result;
	queue.visitSchedules(
		[&](const ChatterScheduleEntry &entry)
		{
			result.push_back(entry);
			return true;
		});
	return result;
}
}
int main()
{
	using namespace astrabot::compat;
	ChatterRadioCatalog catalog;
	const char *text = "Chatter Affirmative ack.wav End Chatter Help help.wav End "
					   "Chatter EscortingHostages escort.wav End Chatter PlantingBomb Important plant.wav End "
					   "Chatter OnMyWay way.wav End Chatter CommanderDown Important down.wav End "
					   "Chatter NiceShot nice.wav End Chatter HeardNoise noise.wav End";
	if (!check(catalog.loadText(text, std::strlen(text)), "native default catalog loads"))
	{
		return 1;
	}
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission admission{{{1, 2}, {3, 4, 5}, 6, 10, true}, 7, 8, &clock};
	ChatterPendingQueue queue;
	if (!check(queue.synchronize(admission), "queue receives qualified live owner"))
	{
		return 1;
	}
	std::vector<ChatterStatementHandle> handles;
	for (unsigned i = 0; i < 12; ++i)
	{
		handles.push_back(
			queue.admit(fixed(ChatterPhrase::Affirmative, 10, 12, 20), admission, catalog, ChatterVerbosity::Normal));
	}
	auto entries = schedules(queue);
	if (!check(entries.size() == 12 && handles.back().id != 0,
			"native acknowledgement exception retains more than six pending statements"))
	{
		return 1;
	}
	for (std::size_t i = 1; i < entries.size(); ++i)
	{
		if (!check(entries[i - 1].order < entries[i].order, "equal starts retain actual successful insertion order"))
		{
			return 1;
		}
	}
	const auto old = handles[5];
	if (!check(queue.erase(old) && !queue.erase(old), "qualified removal consumes only the exact node once"))
	{
		return 1;
	}
	const auto fresh =
		queue.admit(fixed(ChatterPhrase::Affirmative, 10, 12, 20), admission, catalog, ChatterVerbosity::Normal);
	ChatterPendingStatement node{};
	const auto revision = queue.revision();
	if (!check(!queue.erase(old) && queue.read(fresh, &node) && queue.revision() == revision,
			"stale same-phrase completion cannot remove a fresh node or publish a mutation"))
	{
		return 1;
	}
	queue.clear();
	for (const auto phrase : {ChatterPhrase::Help, ChatterPhrase::PlantingBomb, ChatterPhrase::EscortingHostages})
	{
		const auto first = queue.admit(fixed(phrase, 10, 11, 20), admission, catalog, ChatterVerbosity::Normal);
		const auto second = queue.admit(fixed(phrase, 10, 11, 20), admission, catalog, ChatterVerbosity::Normal);
		if (!check(first.id != 0 && second.id > first.id, "native help/critical/plan exceptions retain duplicates"))
		{
			return 1;
		}
	}
	queue.clear();
	auto intention = fixed(ChatterPhrase::OnMyWay, 10, 10, 20);
	const auto intentionId = queue.admit(intention, admission, catalog, ChatterVerbosity::Normal);
	auto commander = fixed(ChatterPhrase::CommanderDown, 10, 10, 30);
	commander.source = ChatterScheduleSource::CommanderDown;
	if (!check(intentionId.id != 0 && queue.admit(commander, admission, catalog, ChatterVerbosity::Normal).id == 0,
			"same native intention topic is redundant across former independent stores"))
	{
		return 1;
	}
	queue.clear();
	auto speaking = admission.make(ChatterScheduleSource::Speaking, ChatterStatementTopic::MyIntention, 10, 10);
	speaking.speaking = true;
	if (!check(queue.admit(commander, admission, catalog, ChatterVerbosity::Normal, speaking).id == 0,
			"retained own speaking participates in AddStatement redundancy"))
	{
		return 1;
	}
	auto information = fixed(ChatterPhrase::HeardNoise, 10, 10, 20);
	information.place = 4;
	information.subject = 7;
	const auto placed = queue.admit(information, admission, catalog, ChatterVerbosity::Normal);
	information.place = 5;
	information.subject = 8;
	const auto distinct = queue.admit(information, admission, catalog, ChatterVerbosity::Normal);
	information.subject = 7;
	if (!check(
			placed.id && distinct.id && queue.admit(information, admission, catalog, ChatterVerbosity::Normal).id == 0,
			"different places/subjects coexist while a matching subject remains redundant"))
	{
		return 1;
	}
	queue.clear();
	const auto normal =
		queue.admit(fixed(ChatterPhrase::Affirmative, 10, 10, 20), admission, catalog, ChatterVerbosity::Normal);
	if (!check(queue.admit(fixed(ChatterPhrase::Help, 10, 10, 20), admission, catalog, ChatterVerbosity::Off).id == 0 &&
				queue.read(normal, &node),
			"OFF rejects new admission without pruning an already admitted statement"))
	{
		return 1;
	}
	if (!check(queue.admit(fixed(ChatterPhrase::Help, 10, 10, 20), admission, catalog, ChatterVerbosity::Minimal).id ==
					0 &&
				queue.admit(
						 fixed(ChatterPhrase::PlantingBomb, 10, 10, 20), admission, catalog, ChatterVerbosity::Minimal)
						.id != 0,
			"MINIMAL admission uses default catalog Important metadata"))
	{
		return 1;
	}
	auto remaining = fixed(ChatterPhrase::None, 10, 12, 15);
	remaining.content = ChatterStatementContent::RemainingEnemies;
	remaining.source = ChatterScheduleSource::EnemyCount;
	remaining.topic = ChatterStatementTopic::EnemiesRemaining;
	if (!check(queue.admit(remaining, admission, catalog, ChatterVerbosity::Minimal).id == 0,
			"native remaining-count context is non-Important"))
	{
		return 1;
	}
	if (!check(parseChatterVerbosity("Radio") == ChatterVerbosity::Radio &&
				parseChatterVerbosity("r") == ChatterVerbosity::Radio &&
				parseChatterVerbosity("OFF") == ChatterVerbosity::Off &&
				parseChatterVerbosity("m") == ChatterVerbosity::Minimal &&
				parseChatterVerbosity("unknown") == ChatterVerbosity::Normal,
			"native first-character verbosity controls admission and transport modes"))
	{
		return 1;
	}
	queue.clear();
	auto expires = fixed(ChatterPhrase::NiceShot, 10, 10, 12);
	const auto expiring = queue.admit(expires, admission, catalog, ChatterVerbosity::Normal);
	ChatterPendingMaintenance maintenance{};
	maintenance.admission = admission;
	maintenance.admission.owner.now = 12;
	maintenance.inCombat = true;
	queue.maintain(maintenance);
	if (!check(queue.read(expiring, &node), "pending lifetime retains exact expiry boundary"))
	{
		return 1;
	}
	maintenance.admission.owner.now = 12.125f;
	maintenance.speaking = node.schedule;
	maintenance.speaking.speaking = true;
	queue.maintain(maintenance);
	if (!check(queue.read(expiring, &node), "selected retained speaking is not made obsolete by expiry"))
	{
		return 1;
	}
	maintenance.speaking = {};
	queue.maintain(maintenance);
	if (!check(queue.empty(), "unspoken obsolete entry is removed strictly after expiry"))
	{
		return 1;
	}
	queue.synchronize(admission);
	expires.requiresCombat = true;
	const auto conditional = queue.admit(expires, admission, catalog, ChatterVerbosity::Normal);
	queue.read(conditional, &node);
	maintenance.speaking = node.schedule;
	maintenance.speaking.speaking = true;
	maintenance.inCombat = false;
	if (!check(conditional.id != 0, "conditional fixture admitted"))
	{
		return 1;
	}
	queue.maintain(maintenance);
	if (!check(queue.empty(), "native validity runs before the speaking obsolescence exemption"))
	{
		return 1;
	}
	queue.synchronize(admission);
	const auto oldOwner =
		queue.admit(fixed(ChatterPhrase::Affirmative, 10, 10, 20), admission, catalog, ChatterVerbosity::Normal);
	auto nextLife = admission;
	++nextLife.owner.entityGeneration;
	++nextLife.ownerRevision;
	const auto beforeReset = queue.revision();
	queue.synchronize(nextLife);
	const auto nextOwner =
		queue.admit(fixed(ChatterPhrase::Affirmative, 10, 10, 20), nextLife, catalog, ChatterVerbosity::Normal);
	if (!check(queue.revision() > beforeReset && nextOwner.id > oldOwner.id && !queue.erase(oldOwner) &&
				queue.read(nextOwner, &node),
			"reset preserves monotonic identities and rejects old-owner completion"))
	{
		return 1;
	}
	queue.clear();
	auto late = fixed(ChatterPhrase::Affirmative, 10, 14, 20);
	auto early = fixed(ChatterPhrase::Help, 10, 11, 20);
	queue.admit(late, nextLife, catalog, ChatterVerbosity::Normal);
	const auto earlyId = queue.admit(early, nextLife, catalog, ChatterVerbosity::Normal);
	ChatterTeamSelection selection;
	queue.visitSchedules(
		[&](const ChatterScheduleEntry &entry)
		{
			return selection.consider(entry);
		});
	if (!check(selection.finish(12).order == earlyId.id, "streaming selector consumes actual owner list order"))
	{
		return 1;
	}

	// Event admission can precede the first ordinary update of a new round.
	OutgoingRadioScheduler scheduler;
	OutgoingRadioInput ordinary{};
	ordinary.actor = admission.owner.actor;
	ordinary.frame = admission.owner.frame;
	ordinary.now = admission.owner.now;
	ordinary.alive = true;
	ordinary.team = 1;
	ordinary.friendCount = 1;
	ordinary.admission = admission;
	scheduler.prepare(ordinary, catalog);
	auto eventAdmission = admission;
	++eventAdmission.owner.frame.roundGeneration;
	++eventAdmission.ownerRevision;
	const auto earlyEvent = scheduler.statements().admit(
		fixed(ChatterPhrase::Affirmative, 10, 10, 20), eventAdmission, catalog, ChatterVerbosity::Normal);
	ordinary.frame = eventAdmission.owner.frame;
	ordinary.admission = eventAdmission;
	scheduler.prepare(ordinary, catalog);
	if (!check(earlyEvent.id != 0 && scheduler.statements().read(earlyEvent, &node),
			"current-round event survives stale ordinary scheduler metadata reset"))
		return 1;

	return 0;
}
