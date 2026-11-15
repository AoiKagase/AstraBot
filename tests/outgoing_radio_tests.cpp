#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/perception/perception.hpp"
#include <cstdio>
#include <cstring>
int main()
{
	using namespace astrabot::compat;
	int failures = 0;
	const auto check = [&failures](bool ok, const char *s)
	{
		if (!ok)
		{
			++failures;
			std::fprintf(stderr, "FAIL %s\n", s);
		}
	};
	ChatterRadioCatalog catalog;
	const char db[] = "// source-owned resource supplied by host\nChatter EnemySpotted\n Radio "
					  "EVENT_RADIO_ENEMY_SPOTTED\n enemy.wav\nEnd\nChatter Affirmative\n Radio "
					  "EVENT_RADIO_AFFIRMATIVE\nEnd\nChatter Help\n Radio EVENT_RADIO_NEED_BACKUP\nEnd\nChatter "
					  "RequestReport\n Radio EVENT_RADIO_REPORT_IN_TEAM\nEnd\n";
	check(catalog.loadText(db, sizeof(db) - 1), "public chatter radio metadata accepted");
	OutgoingRadioScheduler scheduler;
	OutgoingRadioInput in{};
	in.actor = {1, 1};
	in.frame = {1, 1, 1};
	in.now = 1;
	in.alive = true;
	in.inCombat = true;
	in.team = 1;
	in.friendCount = 2;
	in.nearbyFriends = 1;
	in.visibleEnemyCount = 1;
	in.enemiesRemaining = 3;
	auto out = scheduler.update(in, catalog);
	check(out.status == OutgoingRadioStatus::None, "new enemy waits for reference two second accumulation");
	in.now = 3.1f;
	in.frame.tick++;
	out = scheduler.update(in, catalog);
	check(out.status == OutgoingRadioStatus::Ready && out.menu == 3 && out.slot == 2,
		"accumulated observed enemy produces mapped standard radio");
	scheduler.confirmSent(in, out);
	in.now = 3.2f;
	in.frame.tick++;
	check(scheduler.update(in, catalog).status == OutgoingRadioStatus::None,
		"same visible contact does not repeat report");
	in.now = 4;
	in.frame.tick++;
	in.humanOrderSequence = 1;
	in.humanOrderReceived = 4;
	in.humanOrderAccepted = true;
	in.reactionSeconds = .3f;
	check(scheduler.update(in, catalog).status == OutgoingRadioStatus::None,
		"human command acknowledgement waits for reaction delay");
	in.now = 5.7f;
	in.frame.tick++;
	out = scheduler.update(in, catalog);
	check(out.status == OutgoingRadioStatus::Ready && out.slot == 1 && out.menu == 3,
		"delayed acknowledgement uses mapped radio");
	scheduler.confirmSent(in, out);
	in.now = 37;
	in.frame.tick++;
	in.visibleEnemyCount = 0;
	out = scheduler.update(in, catalog);
	check(out.status == OutgoingRadioStatus::Ready && out.menu == 2 && out.slot == 6,
		"thirty second radio silence requests team report");
	ChatterRadioCatalog empty;
	OutgoingRadioScheduler unknown;
	in.now = 1;
	in.frame = {1, 2, 1};
	in.visibleEnemyCount = 1;
	in.humanOrderSequence = 0;
	unknown.update(in, empty);
	in.now = 3.1f;
	in.frame.tick++;
	check(unknown.update(in, empty).status == OutgoingRadioStatus::MissingPhrase,
		"missing host phrase mapping is explicit");
	in.frame.roundGeneration = 3;
	in.frame.tick = 1;
	in.visibleEnemyCount = 0;
	in.vipEscaping = true;
	in.now = 2;
	out = unknown.update(in, empty);
	check(out.status == OutgoingRadioStatus::Ready && out.menu == 1 && out.slot == 5,
		"VIP escape uses reference direct FollowMe radio");
	OutgoingRadioScheduler vipWithHelp;
	in.frame.roundGeneration = 30;
	in.visibleEnemyCount = 4;
	in.nearbyFriends = 0;
	out = vipWithHelp.update(in, catalog, 1U << static_cast<unsigned>(ChatterPhrase::Help));
	check(
		out.status == OutgoingRadioStatus::Ready && out.menu == 1 && out.slot == 5 && out.phrase == ChatterPhrase::None,
		"direct VIP FollowMe cannot be substituted by queued Help voice");
	vipWithHelp.confirmSent(in, out);
	auto afterVip = in;
	afterVip.vipEscaping = false;
	afterVip.now += .1f;
	out = vipWithHelp.update(afterVip, catalog);
	check(out.status == OutgoingRadioStatus::Ready && out.phrase == ChatterPhrase::Help,
		"confirmed direct VIP radio preserves independent queued statement");
	OutgoingRadioScheduler vipExpiry;
	auto expiryInput = in;
	expiryInput.vipEscaping = false;
	expiryInput.now = 10;
	expiryInput.teamBusyUntil = 30;
	(void)vipExpiry.update(expiryInput, catalog);
	expiryInput.now = 15;
	expiryInput.vipEscaping = true;
	(void)vipExpiry.update(expiryInput, catalog);
	expiryInput.now = 20.1f;
	expiryInput.vipEscaping = false;
	out = vipExpiry.update(expiryInput, catalog);
	vipExpiry.confirmSent(expiryInput, out);
	expiryInput.teamBusyUntil = 0;
	check(vipExpiry.update(expiryInput, catalog).status == OutgoingRadioStatus::None,
		"direct VIP retry lifetime cannot extend an older statement lifetime");
	OutgoingRadioScheduler blockedGeneric;
	auto blockedInput = afterVip;
	blockedInput.frame.roundGeneration = 39;
	blockedInput.visibleEnemyCount = 0;
	blockedInput.bombLocationUnknown = true;
	blockedInput.canStart = false;
	check(blockedGeneric.update(blockedInput, empty).status != OutgoingRadioStatus::MissingPhrase &&
			blockedGeneric.hasPending(),
		"blocked generic maintenance cannot consume missing-phrase readiness");
	blockedInput.canStart = true;
	check(
		blockedGeneric.update(blockedInput, empty, 1U << static_cast<unsigned>(ChatterPhrase::WhereIsTheBomb)).phrase ==
			ChatterPhrase::WhereIsTheBomb,
		"selected generic statement remains deliverable after blocked maintenance");
	OutgoingRadioScheduler voiceOnly;
	auto voiceInput = in;
	voiceInput.frame.roundGeneration = 31;
	voiceInput.vipEscaping = false;
	voiceInput.visibleEnemyCount = 0;
	voiceInput.bombLocationUnknown = true;
	out = voiceOnly.update(voiceInput, empty, 1U << static_cast<unsigned>(ChatterPhrase::WhereIsTheBomb));
	check(out.status == OutgoingRadioStatus::Ready && out.menu == 0 && out.phrase == ChatterPhrase::WhereIsTheBomb,
		"a host voice without a standard-radio equivalent is still speakable");
	OutgoingRadioScheduler backup;
	in.vipEscaping = false;
	in.now = 2;
	in.frame = {1, 4, 1};
	in.visibleEnemyCount = 4;
	in.nearbyFriends = 0;
	out = backup.update(in, catalog);
	check(out.status == OutgoingRadioStatus::Ready && out.phrase == ChatterPhrase::Help,
		"multiple public visible enemies trigger outnumbered help");
	OutgoingRadioScheduler duplicate;
	in.frame.roundGeneration = 5;
	in.visibleEnemyCount = 1;
	in.nearbyFriends = 1;
	in.now = 1;
	duplicate.update(in, catalog);
	in.now = 3.2f;
	in.frame.tick++;
	in.teamBusyUntil = 5;
	in.teamCurrentPhrase = ChatterPhrase::EnemySpotted;
	check(duplicate.update(in, catalog).status == OutgoingRadioStatus::None,
		"redundant teammate contact statement is discarded");
	astrabot::world::SnapshotIdentity identity{{1, 10, 1}, {1, 1}, 0};
	astrabot::perception::PerceptionInput perceptionInput(identity);
	for (unsigned actorSlot = 2; actorSlot <= 6; ++actorSlot)
	{
		astrabot::world::ActorObservation contact{};
		contact.actor = {actorSlot, 1};
		contact.state = astrabot::world::ObservationState::ObservedPresent;
		contact.relation =
			actorSlot == 6 ? astrabot::world::TeamRelation::Friendly : astrabot::world::TeamRelation::Hostile;
		contact.position = {100, 0, 0};
		contact.confidence = {1, 0};
		contact.visible = true;
		contact.fovPassed = true;
		contact.losPassed = true;
		contact.visibleParts = astrabot::world::VisibleChest;
		check(perceptionInput.addActor(contact) == astrabot::perception::PerceptionInputResult::Accepted,
			"visible contact added");
	}
	astrabot::perception::PerceptionAssembler assembler;
	astrabot::world::WorldSnapshot belief;
	check(assembler.publish(perceptionInput, &belief) == astrabot::perception::PerceptionResult::Published,
		"public contact snapshot published");
	RadioContactMemory watch;
	auto counts = watch.update(identity.observer, identity.frame, 1, &belief, false, 1, 4);
	check(counts.enemies == 4 && counts.friends == 1, "nearby counts come from all public visible contacts");
	check(watch.counts(identity.observer, identity.frame, 1.1f, 1, 4).enemies == 4,
		"event count reuses current visual watch without fresh traces");
	check(watch.counts({1, 99}, identity.frame, 1.1f, 1, 4).enemies == 0,
		"event count rejects former observer generation");
	auto otherRound = identity.frame;
	otherRound.roundGeneration++;
	check(
		watch.counts(identity.observer, otherRound, 1.1f, 1, 4).enemies == 0, "event count rejects former round watch");
	check(watch.counts(identity.observer, identity.frame, .5f, 1, 4).enemies == 0,
		"event count rejects clock rollback before watch update");
	identity.frame.tick++;
	counts = watch.update(identity.observer, identity.frame, 3.9f, nullptr, true, 1, 4);
	check(counts.enemies == 4 && counts.friends == 1,
		"recent visual watches persist while blind for less than three seconds");
	identity.frame.tick++;
	counts = watch.update(identity.observer, identity.frame, 4, nullptr, false, 1, 4);
	check(counts.enemies == 0 && counts.friends == 0, "visual watch expires exactly at three seconds");
	return failures ? 1 : 0;
}
