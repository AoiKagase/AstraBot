#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/compat/bomb_status.hpp"
#include "astrabot/perception/perception.hpp"
#include <cstdio>
#include <cstring>
namespace
{
bool testPlantPlan()
{
	using namespace astrabot::compat;
	const char definitions[] = "Chatter GoingToPlantBomb End Chatter CoveringFriend End Chatter PlantingBomb End";
	ChatterRadioCatalog catalog;
	if (!catalog.loadText(definitions, sizeof(definitions) - 1))
		return false;

	OutgoingRadioScheduler scheduler;
	OutgoingRadioInput input{};
	input.actor = {1, 1};
	input.frame = {1, 1, 1};
	input.now = 12;
	input.alive = true;
	input.team = 1;
	input.friendCount = 1;
	input.plantMoveEpoch = 1;
	input.plantMoveAge = 2;
	input.plantGoalPlace = 17;
	input.plantGoalPlacePhrase = 3;
	const auto prepare = [&](float now)
	{
		input.now = now;
		++input.frame.tick;
		scheduler.prepare(input, catalog);
	};
	prepare(12);
	if (!scheduler.statements().empty())
		return false;
	input.plantMoveAge = 2.125f;
	prepare(12.125f);
	ChatterStatementHandle handle{};
	scheduler.statements().visitSchedules(
		[&](const ChatterScheduleEntry &entry)
		{
			handle = {entry.order, entry};
			return false;
		});
	ChatterPendingStatement node{};
	if (!scheduler.statements().read(handle, &node) || node.request.phrase != ChatterPhrase::GoingToPlantBomb ||
		node.request.meme.kind != ChatterMemeKind::Follow ||
		node.schedule.topic != ChatterStatementTopic::CriticalEvent || node.schedule.place != 17 ||
		node.request.criteriaPlace != 3 || node.request.expires != 22.125f)
		return false;
	ChatterPlaybackInput playback{};
	playback.now = input.now;
	playback.friendsRemaining = 1;
	auto step = scheduler.statements().advance(handle, playback);
	if (step.completion.natural || !scheduler.statements().finishPart(step.token, 0))
		return false;
	playback.now += 0.125f;
	step = scheduler.statements().advance(handle, playback);
	if (!step.completion.natural || step.completion.meme.kind != ChatterMemeKind::Follow)
		return false;
	prepare(23);
	if (!scheduler.statements().empty())
		return false;

	// The native invalidated interval is measured from -1.
	scheduler.reset();
	prepare(8.875f);
	if (!scheduler.statements().empty())
		return false;
	++input.plantMoveEpoch;
	prepare(9);
	if (scheduler.statements().empty())
		return false;

	// Cooldown refusal still consumes this MoveTo entry's attempt.
	scheduler.reset();
	input.verbosity = ChatterVerbosity::Off;
	prepare(30);
	input.verbosity = ChatterVerbosity::Normal;
	input.plantMoveEpoch = 2;
	prepare(39.875f);
	prepare(40);
	if (!scheduler.statements().empty())
		return false;
	input.plantMoveEpoch = 3;
	input.plantCoverDue = true;
	prepare(40);
	std::vector<ChatterPhrase> admitted;
	scheduler.statements().visitSchedules(
		[&](const ChatterScheduleEntry &entry)
		{
			ChatterPendingStatement pending{};
			if (scheduler.statements().read({entry.order, entry}, &pending))
				admitted.push_back(pending.request.phrase);
			return true;
		});
	return admitted == std::vector<ChatterPhrase>{ChatterPhrase::PlantingBomb, ChatterPhrase::GoingToPlantBomb};
}

bool testPlantingDefendRequest()
{
	using namespace astrabot::compat;
	const char definitions[] = "Chatter PlantingBomb End";
	ChatterRadioCatalog catalog;
	if (!catalog.loadText(definitions, sizeof(definitions) - 1))
		return false;
	for (bool approaching : {false, true})
	{
		OutgoingRadioInput input{};
		input.actor = {1, 1};
		input.frame = {1, 1, 1};
		input.now = 12;
		input.alive = true;
		input.team = 1;
		input.friendCount = approaching ? 1 : 0;
		input.plantingBomb = !approaching;
		input.plantMoveEpoch = approaching ? 1U : 0U;
		input.plantCoverDue = approaching;
		input.ownPlace = 17;
		input.ownPlacePhrase = 3;
		input.plantPosition = {100.0f, 200.0f, 300.0f};
		OutgoingRadioScheduler scheduler;
		scheduler.prepare(input, catalog);
		ChatterStatementHandle handle{};
		scheduler.statements().visitSchedules([&](const ChatterScheduleEntry &entry)
		{
			handle = {entry.order, entry};
			return false;
		});
		ChatterPendingStatement node{};
		if (!scheduler.statements().read(handle, &node) || node.request.phrase != ChatterPhrase::PlantingBomb ||
				node.schedule.topic != ChatterStatementTopic::CriticalEvent || node.schedule.place != 17 ||
				node.request.criteriaPlace != 3 || node.request.expires != 22 ||
				node.request.meme.kind != ChatterMemeKind::DefendHere)
			return false;
		input.plantPosition = {400.0f, 500.0f, 600.0f};
		ChatterPlaybackInput playback{};
		playback.now = input.now;
		playback.friendsRemaining = 1;
		auto step = scheduler.statements().advance(handle, playback);
		if (step.completion.natural || !scheduler.statements().finishPart(step.token, 0))
			return false;
		playback.now += 0.125f;
		step = scheduler.statements().advance(handle, playback);
		const auto &completion = step.completion;
		if (!completion.natural || completion.meme.kind != ChatterMemeKind::DefendHere ||
				completion.meme.position.x != 100 || completion.meme.position.y != 200 || completion.meme.position.z != 300)
			return false;
		scheduler.reset();
		input.roundOver = true;
		scheduler.prepare(input, catalog);
		if (!scheduler.statements().empty())
			return false;
	}
	return true;
}
}
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
	check(testPlantPlan(), "plant plan timing, same-update cover ordering, cooldown and silent Follow completion");
	check(testPlantingDefendRequest(), "planting reports die at round end or complete with captured DefendHere position");
	check(reportingInContactPhrase(true, {2, 1}, 0, 0) == ChatterPhrase::InCombat &&
		reportingInContactPhrase(true, {3, 1}, 0, 0) == ChatterPhrase::Help,
		"ReportingIn counts the reporting bot when deciding if it needs help");
	check(reportingInContactPhrase(false, {}, 9.5f, 9.0f) == ChatterPhrase::EnemyDown &&
		reportingInContactPhrase(false, {}, 9.49f, 9.0f) == ChatterPhrase::EnemySpotted &&
		reportingInContactPhrase(false, {}, 10.0f, 9.0f) == ChatterPhrase::EnemySpotted &&
		reportingInContactPhrase(false, {}, 0.0f, 10.0f) == ChatterPhrase::Clear,
		"ReportingIn preserves native death timestamp and last sight boundaries");
	ChatterRadioCatalog catalog;
	const char db[] = "// source-owned resource supplied by host\nChatter EnemySpotted\n Radio "
					  "EVENT_RADIO_ENEMY_SPOTTED\n enemy.wav\nEnd\nChatter Affirmative\n Radio "
					  "EVENT_RADIO_AFFIRMATIVE\nEnd\nChatter Help\n Radio EVENT_RADIO_NEED_BACKUP\nEnd\nChatter "
					  "RequestReport\n Radio EVENT_RADIO_REPORT_IN_TEAM\nEnd\n";
	check(catalog.loadText(db, sizeof(db) - 1), "public chatter radio metadata accepted");
	{
		ChatterRadioCatalog sites;
		const char names[] = "Place BombsiteA site.wav End Chatter BombsiteClear clear.wav End";
		check(sites.loadText(names, sizeof(names) - 1), "site report catalog");
		OutgoingRadioScheduler sender;
		OutgoingRadioInput update{};
		update.actor = {1, 1};
		update.frame = {1, 1, 1};
		update.now = 20;
		update.team = update.friendCount = 1;
		update.alive = true;
		update.clearedBombsite = {9, 4};
		update.clearedBombsitePlace = 2;
		update.clearedBombsitePhrase = sites.nameToId("BombsiteA");
		sender.prepare(update, sites);
		bool emitted = false;
		sender.statements().visitSchedules([&](const ChatterScheduleEntry &entry)
		{
			ChatterPendingStatement node{};
			emitted = sender.statements().read({entry.order, entry}, &node) &&
				entry.topic == ChatterStatementTopic::Information && node.request.expires == 30 &&
				node.request.program.count == 2 && node.request.program.parts[0].place == 2 &&
				node.request.program.parts[1].phrase == ChatterPhrase::BombsiteClear &&
				node.request.meme.kind == ChatterMemeKind::BombsiteClear && node.request.meme.site == update.clearedBombsite;
			return false;
		});
		check(emitted, "site-clear report carries the zone identity and speaks its place first");
	}
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
	const char defaultOnly[] = "Chatter WhereIsTheBomb End";
	check(empty.loadText(defaultOnly, sizeof(defaultOnly) - 1),
		"default definition exists independently of radio mapping/profile speakables");
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
	in.now = 31;
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
	expiryInput.now = 40;
	expiryInput.teamBusyUntil = 70;
	(void)vipExpiry.update(expiryInput, catalog);
	expiryInput.now = 45;
	expiryInput.vipEscaping = true;
	(void)vipExpiry.update(expiryInput, catalog);
	expiryInput.now = 50.125f;
	expiryInput.vipEscaping = false;
	out = vipExpiry.update(expiryInput, catalog);
	vipExpiry.confirmSent(expiryInput, out);
	expiryInput.teamBusyUntil = 0;
	check(vipExpiry.update(expiryInput, catalog).status == OutgoingRadioStatus::None,
		"an unattempted direct request cannot extend an older ordinary statement lifetime");
	OutgoingRadioScheduler firstDirect;
	auto directClock = in;
	directClock.visibleEnemyCount = 0;
	directClock.nearbyFriends = directClock.friendCount;
	directClock.now = 30;
	check(firstDirect.update(directClock, catalog).status == OutgoingRadioStatus::None,
		"zero-initialized native FollowMe clock blocks the exact first30seconds");
	directClock.now = 30.125f;
	check(firstDirect.update(directClock, catalog).directToken != 0,
		"direct FollowMe becomes eligible strictly after30seconds");
	OutgoingRadioScheduler peerDirect;
	directClock.actor = {2, 8};
	directClock.teamFollowRadioAt = 40;
	directClock.now = 70;
	check(peerDirect.update(directClock, catalog).directToken == 0,
		"a new managed owner respects another teammate attempt at the exact30second boundary");
	directClock.now = 70.125f;
	check(peerDirect.update(directClock, catalog).directToken != 0,
		"shared team FollowMe interval expires strictly after30seconds");
	peerDirect.confirmSent(directClock,peerDirect.update(directClock,catalog));
	directClock.team=2; directClock.teamFollowRadioAt=0; directClock.now=70.25f;
	check(peerDirect.update(directClock,catalog).directToken!=0,"same actor changing teams uses its new team's clock without a stale per-actor interval");
	OutgoingRadioScheduler stableDirect;
	auto stableClock=directClock; stableClock.now=40;
	const auto firstToken=stableDirect.update(stableClock,catalog).directToken;
	stableClock.now=40.125f;
	check(firstToken!=0 && stableDirect.update(stableClock,catalog).directToken==firstToken,"unattempted direct request retains its exact token while awaiting this owner's action");
	OutgoingRadioScheduler directSilence;
	auto silenceInput = directClock;
	silenceInput.actor = {1, 1};
	silenceInput.teamFollowRadioAt = 0;
	silenceInput.vipEscaping = false;
	silenceInput.now = 1;
	(void)directSilence.update(silenceInput, catalog);
	silenceInput.vipEscaping = true;
	silenceInput.now = 31.25f;
	auto directOutput = directSilence.update(silenceInput, catalog);
	directSilence.confirmSent(silenceInput, directOutput);
	silenceInput.vipEscaping = false;
	silenceInput.nearbyFriends = 0;
	silenceInput.now = 31.5f;
	const auto reportRequest = directSilence.update(silenceInput, catalog);
	check(reportRequest.phrase == ChatterPhrase::RequestReport,
		"direct radio does not reset the independent Chatter silence interval");
	ChatterPlaybackInput reportPlayback{};
	reportPlayback.now = silenceInput.now;
	reportPlayback.friendsRemaining = 1;
	const auto reportStep = directSilence.statements().advance(reportRequest.statement, reportPlayback);
	check(!reportStep.completion.natural && directSilence.statements().finishPart(reportStep.token, 0),
		"the request is spoken before it reaches teammates");
	reportPlayback.now += 0.125f;
	const auto delivered = directSilence.statements().advance(reportRequest.statement, reportPlayback).completion;
	check(delivered.natural && delivered.meme.kind == ChatterMemeKind::RequestReport,
		"natural completion of RequestReport delivers a report request");
	const auto calm = reportingInStatement(30, false, {}, 0, 10, 3, 5);
	const auto fighting = reportingInStatement(30, true, {2, 0}, 0, 10, 3, 5);
	ChatterRadioCatalog taskCatalog;
	const auto plantedSite = plantedBombsiteReport(30.0f, {9, 4}, 2, 5);
	BombStatusMemory spotted{};
	check(spotted.isNewCarrierPosition({0, 0, 32}), "the first sighting of a bomb carrier is new");
	spotted.rememberCarrier({0, 0, 32});
	check(!spotted.isNewCarrierPosition({999, 0, 32}) && spotted.isNewCarrierPosition({0, 0, 1032}),
		"moving bomb news compares 3D distance to the last reported position using a strict 1000-unit radius");
	check(spotted.spotLooseBomb({80, 0, 0}, 20), "the first drop changes memory and starts the reporting timer");
	spotted.rememberCarrier({80, 0, 0});
	check(!spotted.spotLooseBomb({90, 0, 0}, 30) && spotted.status == BombStatusMemory::Status::Loose &&
		spotted.position.x == 90, "at exactly ten seconds a new drop changes memory without another report");
	spotted.rememberCarrier({90, 0, 0});
	check(spotted.spotLooseBomb({110, 0, 0}, 30.125f), "a new drop after the interval can be reported again");
	check(!spotted.spotLooseBomb({110, 0, 0}, 50), "a known loose bomb is old news even after its timer expires");
	const auto carrierReport = bombStatusReport(ChatterPhrase::SpottedBomber, 10.0f, {100, 50, 32}, 2, 5, 7);
	check(carrierReport.start == 10.0f && carrierReport.expires == 20.0f && carrierReport.subject == 7 &&
		carrierReport.topic == ChatterStatementTopic::Information && carrierReport.program.count == 2 &&
		carrierReport.program.parts[0].kind == ChatterPartKind::Place &&
		carrierReport.program.parts[1].phrase == ChatterPhrase::SpottedBomber &&
		carrierReport.meme.kind == ChatterMemeKind::BombMoving && carrierReport.meme.position.y == 50,
		"carrier report names the place before the sighting and shares the observed position");
	check(plantedSite.topic == ChatterStatementTopic::Information &&
		plantedSite.phrase == ChatterPhrase::PlantedBombPlace && plantedSite.start == 30.0f &&
		plantedSite.expires == 33.0f && plantedSite.place == 2 && plantedSite.criteriaPlace == 5 &&
		plantedSite.meme.kind == ChatterMemeKind::BombsitePlanted && plantedSite.meme.site.id == 9 &&
		plantedBombsiteReport(30.0f, {}, 2, 5).source == ChatterScheduleSource::None,
		"a planted-site answer names a known zone and retains its identity for three seconds");
	const char taskPhrases[] = "Chatter Clear End Chatter GoingToPlantBomb End Chatter DefusingBomb End Chatter EscortingHostages End";
	check(taskCatalog.loadText(taskPhrases, sizeof(taskPhrases) - 1), "load task report phrases");
	for (const auto &task : {std::make_pair(CompatibilityTaskId::PlantBomb, ChatterPhrase::GoingToPlantBomb),
			 std::make_pair(CompatibilityTaskId::DefuseBomb, ChatterPhrase::DefusingBomb),
			 std::make_pair(CompatibilityTaskId::RescueHostages, ChatterPhrase::EscortingHostages)})
	{
		OutgoingRadioScheduler reporter;
		auto reporterInput = silenceInput;
		reporterInput.now = calm.created;
		reporterInput.plantGoalPlace = 17;
		reporterInput.plantGoalPlacePhrase = 9;
		reporter.reportIn(calm, task.first, reporterInput, taskCatalog);
		std::vector<ChatterPhrase> parts;
		reporter.statements().visitSchedules([&](const ChatterScheduleEntry &entry)
		{
			ChatterPendingStatement pending{};
			if (reporter.statements().read({entry.order, entry}, &pending))
			{
				parts.push_back(pending.request.phrase);
				if (pending.request.phrase == ChatterPhrase::GoingToPlantBomb)
					check(entry.place == 0 && pending.request.criteriaPlace == 0 &&
						pending.request.meme.kind == ChatterMemeKind::Follow, "reported plant plan uses undefined place");
			}
			return true;
		});
		check(parts == std::vector<ChatterPhrase>{task.second, ChatterPhrase::Clear},
			"task-specific announcement precedes the delayed contact report");
	}
	check(calm.start == 32 && calm.expires == 40 && calm.topic == ChatterStatementTopic::Information &&
		calm.program.parts[0].kind == ChatterPartKind::Place && calm.program.parts[0].placePhrase == 5 &&
		calm.program.parts[1].phrase == ChatterPhrase::Clear && calm.meme.kind == ChatterMemeKind::None,
		"a quiet report announces the receiver location then contact status after two seconds");
	check(fighting.start == 30 && fighting.expires == 40 && fighting.program.parts[1].phrase == ChatterPhrase::Help &&
		fighting.meme.kind == ChatterMemeKind::Help && fighting.meme.place == 3,
		"an outnumbered receiver immediately reports and attaches a Help request for its own location");
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
	ChatterRadioCatalog acknowledgementCatalog;
	const char *acknowledgementText = "Chatter Affirmative ack.wav End Chatter OnMyWay way.wav End";
	check(acknowledgementCatalog.loadText(acknowledgementText, std::strlen(acknowledgementText)),
		"reset fixture declares both native acknowledgement phrases");
	const auto acknowledgementVoices = (1U << static_cast<unsigned>(ChatterPhrase::Affirmative)) |
		(1U << static_cast<unsigned>(ChatterPhrase::OnMyWay));
	for (bool help : {false, true})
	{
		for (unsigned reset = 0; reset < 4; ++reset)
		{
			OutgoingRadioScheduler delayed;
			ChatterAdmissionClock clock;
			OutgoingRadioInput order{};
			order.actor = {1, 2};
			order.frame = {3, 4, 5};
			order.now = 10;
			order.alive = true;
			order.team = 1;
			order.friendCount = order.nearbyFriends = 1;
			order.humanOrderSequence = 1;
			order.humanOrderAccepted = true;
			order.humanOrderHelp = help;
			order.humanOrderReceived = 10;
			order.reactionSeconds = .3f;
			order.admission = {{order.actor, order.frame, 6, 10, true}, 7, 8, &clock};
			check(delayed.update(order, acknowledgementCatalog, acknowledgementVoices).status ==
					OutgoingRadioStatus::None,
				"human response is reserved before its reaction delay");
			if (reset == 0)
				delayed.discardPending();
			if (reset == 1)
				++order.admission.ownerRevision;
			if (reset == 2)
				++order.admission.connection;
			if (reset == 3)
				++order.admission.owner.entityGeneration;
			order.now = 12;
			++order.frame.tick;
			order.admission.owner.frame = order.frame;
			order.admission.owner.now = 12;
			auto old = delayed.update(order, acknowledgementCatalog, acknowledgementVoices);
			check(old.status == OutgoingRadioStatus::None,
				"reset or changed owner receipt cannot materialize the old delayed acknowledgement");
			if (old.status == OutgoingRadioStatus::Ready)
				delayed.confirmSent(order, old);
			order.humanOrderSequence = 2;
			order.humanOrderReceived = 12;
			check(delayed.update(order, acknowledgementCatalog, acknowledgementVoices).status ==
					OutgoingRadioStatus::None,
				"new post-reset order retains its own reaction delay");
			order.now = 14;
			++order.frame.tick;
			order.admission.owner.frame = order.frame;
			order.admission.owner.now = 14;
			auto fresh = delayed.update(order, acknowledgementCatalog, acknowledgementVoices);
			check(fresh.status == OutgoingRadioStatus::Ready &&
					fresh.phrase == (help ? ChatterPhrase::OnMyWay : ChatterPhrase::Affirmative),
				"fresh order after cancellation still produces the correct native response");
		}
		OutgoingRadioScheduler advancing;
		OutgoingRadioInput forward{};
		forward.actor = {1, 2};
		forward.frame = {3, 4, 5};
		forward.now = 10;
		forward.alive = true;
		forward.team = 1;
		forward.friendCount = forward.nearbyFriends = 1;
		forward.humanOrderSequence = 1;
		forward.humanOrderAccepted = true;
		forward.humanOrderHelp = help;
		forward.humanOrderReceived = 10;
		forward.reactionSeconds = .3f;
		(void)advancing.update(forward, acknowledgementCatalog, acknowledgementVoices);
		forward.now = 12;
		++forward.frame.tick;
		const auto ready = advancing.update(forward, acknowledgementCatalog, acknowledgementVoices);
		check(ready.status == OutgoingRadioStatus::Ready &&
				ready.phrase == (help ? ChatterPhrase::OnMyWay : ChatterPhrase::Affirmative),
			"ordinary forward time and legacy admission0 preserve a valid delayed response");
	}
	return failures ? 1 : 0;
}
