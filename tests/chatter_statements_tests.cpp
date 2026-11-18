#include "astrabot/compat/chatter_statements.hpp"
#include "astrabot/compat/chatter_voice.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include <cstdio>
#include <cstring>
#include <functional>
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
struct PlanRandom : ICompatibilityRandomSource
{
	std::vector<RandomRequest> requests;
	std::function<void()> callback;
	RandomStatus status = RandomStatus::Ok;
	RandomFloatResult nextFloat(const RandomRequest &request) override
	{
		requests.push_back(request);
		if (callback)
			callback();
		return {status, (request.floatLower + request.floatUpper) * .5f};
	}
	RandomLongResult nextLong(const RandomRequest &) override
	{
		return {RandomStatus::Unavailable, 0};
	}
};
bool testNamedSay()
{
	constexpr char definitions[] = "Place SyntheticRoom End "
		"Chatter FixtureIntent Important Radio = EVENT_RADIO_COVER_ME End";
	constexpr char samples[] = "Chatter FixtureIntent cue.wav End";
	ChatterRadioCatalog catalog;
	ChatterVoiceCatalog voice;
	if (!catalog.loadText(definitions, sizeof(definitions) - 1) ||
		!voice.loadText(samples, sizeof(samples) - 1, &catalog))
		return false;
	const auto *definition = catalog.findPhrase("fixtureintent");
	if (!check(definition && definition->choice.menu == 1 && definition->choice.slot == 1 &&
			!catalog.findPhrase("SyntheticRoom") && !catalog.findPhrase("Missing"),
			"Say resolves a case-insensitive chatter definition and its radio fallback"))
		return false;
	ChatterPendingQueue queue;
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission owner{{{1, 1}, {1, 1, 5}, 1, 20.0f, true}, 1, 1, &clock};
	const auto say = makeSayStatement(*definition, 20.0f, 3.0f, 1.0f);
	const auto handle = queue.admit(say, owner, catalog, ChatterVerbosity::Minimal);
	if (!check(handle.id && handle.owner.topic == ChatterStatementTopic::MyIntention &&
			handle.owner.firstPhrase == definition->id && say.expires == 23.0f,
			"important named Say retains native intention and creation-based lifetime"))
		return false;
	ChatterPlaybackInput frame{};
	frame.now = 20.5f;
	frame.friendsRemaining = 1;
	if (!check(queue.advance(handle, frame).playback.kind == ChatterPlaybackKind::Waiting,
			"named Say observes its requested delay"))
		return false;
	frame.now = 21.0f;
	const auto part = queue.advance(handle, frame);
	const auto *clip = voice.selectNamedAt(part.playback.namedPhrase, catalog, -1, 0);
	return check(part.playback.kind == ChatterPlaybackKind::Phrase && clip && clip->sample == "cue.wav",
		"profile audio can play a named Say beyond the built-in phrases");
}

bool testRetainedRoundEnd()
{
	ChatterRadioCatalog catalog;
	const char text[] = "Chatter Affirmative End Chatter WonRoundQuickly End";
	if (!catalog.loadText(text, sizeof(text) - 1))
		return false;
	ChatterPendingQueue queue;
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission owner{{{1, 1}, {1, 1, 5}, 1, 20.0f, true}, 1, 1, &clock};
	auto result = fixed(ChatterPhrase::Affirmative, 20.0f, 24.0f, 35.0f);
	result.topic = ChatterStatementTopic::RoundEnd;
	const auto resultHandle = queue.admit(result, owner, catalog, ChatterVerbosity::Normal);
	queue.admit(fixed(ChatterPhrase::Affirmative, 20, 21, 35), owner, catalog, ChatterVerbosity::Normal);
	queue.admit(fixed(ChatterPhrase::WonRoundQuickly, 20, 22, 35), owner, catalog, ChatterVerbosity::Normal);
	++owner.owner.frame.roundGeneration;
	++owner.owner.frame.tick;
	++owner.ownerRevision;
	queue.retainAfterReset(owner, true);
	ChatterPendingStatement node{};
	const auto afterEnd = schedules(queue);
	if (!check(resultHandle.id && !queue.read(resultHandle, &node) && afterEnd.size() == 3,
			"administrative Round_End rebinds the queue before the first post-result playback and maintenance"))
		return false;
	owner.owner.alive = false;
	++owner.owner.frame.tick;
	++owner.ownerRevision;
	queue.retainAfterReset(owner, false);
	const auto dead = schedules(queue);
	if (!check(dead.size() == 1 && dead[0].topic == ChatterStatementTopic::RoundEnd &&
			validChatterScheduleEntry(dead[0]) && !dead[0].owner.alive &&
			!queue.admit(result, owner, catalog, ChatterVerbosity::Normal).id,
			"native reset retains only RoundEnd, permits its dead owner to speak and blocks new dead admissions"))
		return false;
	owner.owner.now = 24.0f;
	++owner.owner.frame.tick;
	queue.retainAfterReset(owner, false);
	ChatterPlaybackInput frame{};
	frame.now = 24.0f;
	frame.friendsRemaining = 1;
	const auto playback = queue.advance(queue.handle(dead[0]), frame);
	if (!check(playback.playback.kind == ChatterPlaybackKind::Phrase &&
			playback.playback.phrase == ChatterPhrase::Affirmative,
			"the dead owner's retained statement reaches playback at its original start"))
		return false;
	++owner.connection;
	queue.retainAfterReset(owner, false);
	return check(queue.empty(), "slot reuse drops retained speech");
}

bool testPlanConversion()
{
	ChatterRadioCatalog catalog;
	const char *text = "Place EscortingHostages place.wav End Chatter EscortingHostages escort.wav End "
					   "Chatter AgreeWithPlan agree.wav End Chatter Affirmative ack.wav End Chatter Help help.wav End";
	bool ok = check(catalog.loadText(text, std::strlen(text)), "conversion default catalog loads");
	ok = check(catalog.phraseId(ChatterPhrase::EscortingHostages) == 2 &&
				 catalog.phraseId(ChatterPhrase::AgreeWithPlan) == 3,
			 "plan phrase identity uses chatter namespace despite a same-name Place") &&
		ok;
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission own{{{1, 2}, {3, 4, 5}, 6, 10, true}, 7, 8, &clock};
	auto peer = own;
	peer.owner.actor = {2, 3};
	const auto request = [&](std::uint16_t place)
	{
		auto out = fixed(ChatterPhrase::EscortingHostages, 10, 10, 30);
		out.place = place;
		out.subject = 9;
		out.program.append({ChatterPartKind::Fixed, ChatterPhrase::EscortingHostages, 0, 0});
		out.program.append({ChatterPartKind::Fixed, ChatterPhrase::Affirmative, 0, 0});
		return out;
	};
	for (const auto place : {0U, 7U})
	{
		ChatterPendingQueue queue, other;
		const auto id = queue.admit(request(static_cast<std::uint16_t>(place)), own, catalog, ChatterVerbosity::Normal);
		const auto peerId =
			other.admit(request(static_cast<std::uint16_t>(place)), peer, catalog, ChatterVerbosity::Normal);
		ChatterPendingStatement before{}, node{}, friendNode{};
		queue.read(id, &before);
		other.read(peerId, &friendNode);
		PlanRandom random;
		ChatterPendingMaintenance in{};
		in.admission = own;
		in.teammate = friendNode.schedule;
		in.random = &random;
		in.agreementPhraseId = catalog.phraseId(ChatterPhrase::AgreeWithPlan);
		queue.maintain(in);
		queue.read(id, &node);
		ok =
			check(node.request.program.parts[0].phrase == ChatterPhrase::AgreeWithPlan &&
					node.request.program.parts[1].phrase == ChatterPhrase::Affirmative &&
					node.schedule.start == 10.75f && node.request.start == 10.75f && node.schedule.firstPhrase == 3 &&
					node.schedule.created == 10 && node.request.expires == 30 && node.request.subject == 9 &&
					node.schedule.order == before.schedule.order,
				"same plan/place including undefined becomes delayed agreement without replacing its identity or tail") &&
			ok;
		ok = check(random.requests.size() == 1 && random.requests[0].floatLower == .5f &&
					 random.requests[0].floatUpper == 1,
				 "agreement consumes exactly the native short-range draw") &&
			ok;
		queue.maintain(in);
		ok = check(random.requests.size() == 1 && queue.read(id, &node) && node.schedule.start == 10.75f,
				 "already converted agreement neither redraws nor shifts start again") &&
			ok;
		ChatterPlaybackInput playback{};
		playback.now = 10.5f;
		playback.friendsRemaining = 1;
		ok = check(queue.advance(id, playback).playback.kind == ChatterPlaybackKind::Waiting,
				 "converted agreement cannot start before its new time") &&
			ok;
		playback.now = 10.75f;
		ok = check(queue.advance(id, playback).playback.phrase == ChatterPhrase::AgreeWithPlan,
				 "actual stored playback speaks agreement at the native start boundary") &&
			ok;
	}
	ChatterPendingQueue queue, other;
	const auto id = queue.admit(request(7), own, catalog, ChatterVerbosity::Normal);
	const auto later =
		queue.admit(fixed(ChatterPhrase::Affirmative, 10, 11, 30), own, catalog, ChatterVerbosity::Normal);
	const auto peerId = other.admit(request(8), peer, catalog, ChatterVerbosity::Normal);
	ChatterPendingStatement node{}, friendNode{};
	other.read(peerId, &friendNode);
	PlanRandom random;
	ChatterPendingMaintenance in{};
	in.admission = own;
	in.teammate = friendNode.schedule;
	in.random = &random;
	in.agreementPhraseId = 3;
	queue.maintain(in);
	queue.read(id, &node);
	const auto order = schedules(queue);
	ok = check(node.schedule.start == 13.5f &&
				 node.request.program.parts[0].phrase == ChatterPhrase::EscortingHostages && order.size() == 2 &&
				 order[0].order == id.id && order[1].order == later.id,
			 "different place delays original plan in place without resorting the linked list") &&
		ok;
	in.admission.owner.now = 11;
	queue.maintain(in);
	queue.read(id, &node);
	ok =
		check(random.requests.size() == 2 && random.requests[0].floatLower == 3 && random.requests[0].floatUpper == 4 &&
				node.schedule.start == 14.5f && node.schedule.created == 10 && node.request.expires == 30,
			"different-place plan redraws and postpones each maintenance without extending expiry") &&
		ok;
	for (unsigned mode = 0; mode < 5; ++mode)
	{
		ChatterPendingQueue untouched;
		auto input = request(8);
		if (mode == 1)
			input.topic = ChatterStatementTopic::Information;
		if (mode == 2)
			input.requiresCombat = true;
		if (mode == 3)
			input.expires = 10;
		if (mode == 4)
			input.program.parts[0].phrase = ChatterPhrase::Help;
		const auto kept = untouched.admit(input, own, catalog, ChatterVerbosity::Normal);
		PlanRandom noDraw;
		auto excluded = in;
		excluded.random = &noDraw;
		if (mode == 0)
		{
			ChatterPendingStatement current{};
			untouched.read(kept, &current);
			excluded.speaking = current.schedule;
			excluded.speaking.speaking = true;
		}
		untouched.maintain(excluded);
		ok = check(noDraw.requests.empty(),
				 "speaking/different topic/invalid/obsolete/different first phrase consumes no conversion draw") &&
			ok;
	}
	for (unsigned mode = 0; mode < 3; ++mode)
	{
		ChatterPendingQueue guarded;
		const auto old = guarded.admit(request(8), own, catalog, ChatterVerbosity::Normal);
		ChatterStatementHandle fresh{};
		PlanRandom draw;
		auto guardedInput = in;
		guardedInput.random = &draw;
		guardedInput.admission = own;
		if (mode == 0)
			draw.status = RandomStatus::Unavailable;
		if (mode == 1)
			draw.callback = [&]()
			{
				guarded.clear();
				fresh = guarded.admit(request(8), own, catalog, ChatterVerbosity::Normal);
			};
		guarded.maintain(guardedInput,
			[&]()
			{
				return mode != 2;
			});
		const auto survivor = mode == 1 ? fresh : old;
		ok = check(guarded.read(survivor, &node) && node.schedule.start == 10 &&
					 node.request.program.parts[0].phrase == ChatterPhrase::EscortingHostages,
				 "unavailable RNG, queue reset or invalid peer receipt cannot publish stale conversion") &&
			ok;
	}
	ChatterPendingQueue missing;
	const auto nullId = missing.admit(request(8), own, catalog, ChatterVerbosity::Normal);
	PlanRandom nullRandom;
	auto noAgreement = in;
	noAgreement.admission = own;
	noAgreement.random = &nullRandom;
	noAgreement.agreementPhraseId = 0;
	missing.maintain(noAgreement);
	missing.read(nullId, &node);
	ok = check(node.schedule.start == 10.75f && node.request.program.parts[0].phrase == ChatterPhrase::None &&
				 node.request.program.count == 2 && node.request.program.parts[1].phrase == ChatterPhrase::Affirmative,
			 "missing agreement definition replaces only the first part with native null and still delays") &&
		ok;
	return ok;
}
}
int main()
{
	if (!testNamedSay() || !testRetainedRoundEnd())
		return 1;
	using namespace astrabot::compat;
	if (!testPlanConversion())
		return 1;
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
