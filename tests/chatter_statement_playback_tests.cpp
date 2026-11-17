#include "astrabot/compat/chatter_statements.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include <cstdio>
#include <cstring>
namespace
{
using namespace astrabot::compat;
bool check(bool value, const char *text)
{
	if (!value)
		std::fprintf(stderr, "check failed: %s\n", text);
	return value;
}
ChatterStatementRequest fixed(ChatterPhrase phrase)
{
	ChatterStatementRequest out{};
	out.source = ChatterScheduleSource::Generic;
	out.topic = chatterStatementTopic(phrase);
	out.phrase = phrase;
	out.created = out.start = 10;
	out.expires = 20;
	return out;
}

bool testHelpCompletionPayload()
{
	ChatterRadioCatalog catalog;
	const char definitions[] = "Chatter Help help.wav End Chatter Affirmative ack.wav End";
	catalog.loadText(definitions, sizeof(definitions) - 1);
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission admission{{{1, 2}, {3, 4, 5}, 6, 10, true}, 7, 8, &clock};
	ChatterPendingQueue queue;
	auto request = fixed(ChatterPhrase::Help);
	request.meme = {ChatterMemeKind::Help, 77};
	const auto id = queue.admit(request, admission, catalog, ChatterVerbosity::Normal);
	ChatterPlaybackInput input{};
	input.now = 10;
	input.friendsRemaining = 2;
	auto step = queue.advance(id, input);
	bool ok = check(id.id && !step.completion.natural && queue.finishPart(step.token, 0),
		"starting a Help part does not transmit its retained meme");
	input.now = 10.1f;
	ok = check(!queue.advance(id, input).completion.natural, "final strict duration boundary does not transmit") && ok;
	input.now = 10.125f;
	step = queue.advance(id, input);
	ChatterPendingStatement node;
	ok = check(step.playback.kind == ChatterPlaybackKind::Finished && step.completion.natural &&
				 step.completion.meme.kind == ChatterMemeKind::Help && step.completion.meme.place == 77 &&
				 step.completion.source.id == id.id && sameChatterStatement(step.completion.source.owner, id.owner) &&
				 !queue.read(id, &node),
			 "natural completion transfers immutable Help and sender receipt after node erasure") &&
		ok;
	ok = check(!queue.advance(id, input).completion.natural, "erased completion cannot transmit twice") && ok;
	request.meme.place = 999;
	ok = check(step.completion.meme.place == 77, "completion does not alias a later request mutation") && ok;
	for (unsigned cancel : {0U, 1U, 2U, 3U, 4U, 5U})
	{
		ChatterPendingQueue interrupted;
		auto copy = request;
		copy.requiresCombat = cancel == 5;
		const auto handle = interrupted.admit(copy, admission, catalog, ChatterVerbosity::Normal);
		input.now = 10;
		input.friendsRemaining = cancel == 0 ? 0 : 2;
		if (cancel == 0)
		{
			const auto abort = interrupted.advance(handle, input);
			ok = check(abort.playback.kind == ChatterPlaybackKind::Finished && !abort.completion.natural &&
						 abort.completion.meme.kind == ChatterMemeKind::None,
					 "friends-zero abort has no natural completion or meme") &&
				ok;
			continue;
		}
		if (cancel == 1)
			interrupted.erase(handle);
		if (cancel == 2)
			interrupted.clear();
		if (cancel == 3)
		{
			auto dead = admission;
			dead.owner.alive = false;
			interrupted.synchronize(dead);
		}
		if (cancel == 4)
		{
			ChatterPendingMaintenance expired;
			expired.admission = admission;
			expired.admission.owner.now = 21;
			expired.admission.owner.frame.tick = 6;
			interrupted.maintain(expired);
		}
		if (cancel == 5)
		{
			auto active = interrupted.advance(handle, input);
			interrupted.finishPart(active.token, 0);
			ChatterPendingMaintenance invalid;
			invalid.admission = admission;
			interrupted.maintain(invalid);
		}
		input.now = 11;
		ok = check(!interrupted.advance(handle, input).completion.natural && interrupted.empty(),
				 "erase/reset/death/expiry/combat invalidation cannot produce a completion meme") &&
			ok;
	}
	return ok;
}
bool testDynamicHelpAttachment()
{
	bool ok = true;
	for (bool defined : {false, true})
		for (bool needHelp : {false, true})
		{
			ChatterRadioCatalog catalog;
			const char *definitions = defined ? "Chatter Help help.wav End Chatter EnemySpotted spotted.wav End"
											  : "Chatter EnemySpotted spotted.wav End";
			catalog.loadText(definitions, std::strlen(definitions));
			ChatterAdmissionClock clock;
			ChatterScheduleAdmission admission{{{1, 2}, {3, 4, 5}, 6, 10, true}, 7, 8, &clock};
			ChatterPendingQueue queue;
			auto report = fixed(ChatterPhrase::None);
			report.topic = ChatterStatementTopic::VisibleEnemies;
			report.place = 99;
			report.program.append({ChatterPartKind::CurrentEnemyCount, ChatterPhrase::None, 0, 0});
			const auto handle = queue.admit(report, admission, catalog, ChatterVerbosity::Minimal);
			ChatterPlaybackInput input{};
			input.now = 10;
			input.enemies = needHelp ? 4 : 2;
			input.nearbyFriends = 2;
			input.friendsRemaining = 3;
			auto step = queue.advance(handle, input);
			ok = check(handle.id && !step.completion.natural, "dynamic Help starts without early transmission") && ok;
			queue.finishPart(step.token, 0, !defined && needHelp);
			input.now = 10.125f;
			input.enemies = 1;
			input.nearbyFriends = 9;
			step = queue.advance(handle, input);
			ok =
				check(step.completion.natural &&
						step.completion.meme.kind == (needHelp ? ChatterMemeKind::Help : ChatterMemeKind::None) &&
						step.completion.meme.place == 0,
					"dynamic CurrentEnemyCount captures Help undefined-place even without a phrase and retains it after count changes") &&
				ok;
		}
	return ok;
}
bool testNeedBackupCapturesOwnPlace()
{
	ChatterRadioCatalog catalog;
	const char definitions[] = "Place Arena arena.wav End Chatter Help help.wav End";
	catalog.loadText(definitions, sizeof(definitions) - 1);
	OutgoingRadioScheduler scheduler;
	OutgoingRadioInput input;
	input.actor = {1, 2};
	input.frame = {3, 4, 5};
	input.now = 10;
	input.alive = true;
	input.team = 1;
	input.friendCount = 2;
	input.visibleEnemyCount = 3;
	input.nearbyFriends = 0;
	input.ownPlace = 77;
	input.ownPlacePhrase = 1;
	scheduler.prepare(input, catalog);
	ChatterScheduleEntry selected;
	scheduler.statements().visitSchedules(
		[&](const ChatterScheduleEntry &entry)
		{
			selected = entry;
			return false;
		});
	ChatterPendingStatement node;
	return check(scheduler.statements().read(scheduler.statements().handle(selected), &node) &&
			node.request.meme.kind == ChatterMemeKind::Help && node.request.meme.place == 77 &&
			node.request.place == 0 && node.schedule.place == 77,
		"NeedBackup captures own Place in meme independently of undefined statement criteria and implicit spoken Place");
}
}
int main()
{
	using namespace astrabot::compat;
	const bool completion = testHelpCompletionPayload();
	const bool dynamic = testDynamicHelpAttachment();
	const bool backup = testNeedBackupCapturesOwnPlace();
	if (!completion || !dynamic || !backup)
		return 1;
	const char text[] = "Place Arena Important arena.wav End Chatter Help help.wav End Chatter Affirmative ack.wav End";
	ChatterRadioCatalog catalog;
	if (!check(catalog.loadText(text, sizeof(text) - 1), "real default metadata loads"))
		return 1;
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission admission{{{1, 2}, {3, 4, 5}, 6, 10, true}, 7, 8, &clock};
	ChatterPendingQueue queue;
	auto request = fixed(ChatterPhrase::Help);
	request.program.append({ChatterPartKind::Place, ChatterPhrase::None, 77, 1});
	request.program.append({ChatterPartKind::Fixed, ChatterPhrase::Help, 0, 0});
	const auto id = queue.admit(request, admission, catalog, ChatterVerbosity::Minimal);
	if (!check(id.id != 0, "Important implicit Place admits otherwise nonimportant Help"))
		return 1;
	ChatterPendingStatement node;
	if (!check(queue.read(id, &node) && node.schedule.place == 77 && node.request.place == 0 &&
				node.request.program.count == 2,
			"effective implicit NAV place is independent of explicit undefined criteria"))
		return 1;
	ChatterPlaybackInput input{};
	input.now = 10;
	input.friendsRemaining = 2;
	auto step = queue.advance(id, input);
	if (!check(step.playback.kind == ChatterPlaybackKind::Phrase && step.playback.placePhrase == 1 &&
				step.explicitPlace == 0 && step.schedule.speaking && queue.read(id, &node) && node.playback.speaking,
			"actual queue first step retains speaking node and exact Place token"))
		return 1;
	if (!check(
			queue.finishPart(step.token, .25f) && !queue.finishPart(step.token, .25f), "part completion is single-use"))
		return 1;
	queue.read(id, &node);
	input.now = node.playback.nextTime;
	if (!check(queue.advance(id, input).playback.kind == ChatterPlaybackKind::Waiting,
			"queue preserves strict part boundary"))
		return 1;
	input.now = 10.5f;
	step = queue.advance(id, input);
	const auto ack = queue.admit(fixed(ChatterPhrase::Affirmative), admission, catalog, ChatterVerbosity::Normal);
	if (!check(step.playback.phrase == ChatterPhrase::Help && ack.id && queue.finishPart(step.token, 0),
			"unrelated admission does not invalidate exact current part; defined inaudible part reserves zero plus gap"))
		return 1;
	queue.read(id, &node);
	if (!check(node.playback.nextTime > input.now && node.schedule.speaking,
			"defined zero-duration phrase keeps gap and team owner"))
		return 1;
	input.now = node.playback.nextTime;
	if (!check(queue.advance(id, input).playback.kind == ChatterPlaybackKind::Waiting, "no same-boundary completion"))
		return 1;
	input.now += .125f;
	if (!check(queue.advance(id, input).playback.kind == ChatterPlaybackKind::Finished && !queue.read(id, &node),
			"separate strict final update erases only completed statement"))
		return 1;
	const auto fresh = queue.admit(request, admission, catalog, ChatterVerbosity::Normal);
	if (!check(fresh.id > id.id && !queue.finishPart(step.token, 1) && queue.read(fresh, &node),
			"stale old-node part cannot finish fresh speech"))
		return 1;
	queue.clear();
	auto report = fixed(ChatterPhrase::None);
	report.topic = ChatterStatementTopic::VisibleEnemies;
	report.content = ChatterStatementContent::EnemyReport;
	report.requiresCombat = true;
	const auto reportId = queue.admit(report, admission, catalog, ChatterVerbosity::Minimal);
	input.now = 10;
	input.enemies = 2;
	step = queue.advance(reportId, input);
	if (!check(reportId.id && step.schedule.speaking && queue.read(reportId, &node) && node.playback.nextTime > 1000000,
			"EnemyReport context is Important and starts logical accumulation in queue"))
		return 1;
	input.now = 10.5f;
	if (!check(
			queue.advance(reportId, input).playback.kind == ChatterPlaybackKind::Waiting && queue.read(reportId, &node),
			"selected action precedes native combat-condition maintenance"))
		return 1;
	ChatterPendingMaintenance maintenance{};
	maintenance.admission = admission;
	maintenance.admission.owner.now = input.now;
	maintenance.inCombat = false;
	queue.maintain(maintenance);
	if (!check(!queue.read(reportId, &node),
			"combat validity cancels retained logical speaking during post-action maintenance"))
		return 1;
	queue.clear();
	auto delay = fixed(ChatterPhrase::Affirmative);
	delay.program.append({ChatterPartKind::ShortDelay, ChatterPhrase::None, 0, 0});
	delay.program.append({ChatterPartKind::Fixed, ChatterPhrase::Affirmative, 0, 0});
	const auto delayed = queue.admit(delay, admission, catalog, ChatterVerbosity::Normal);
	step = queue.advance(delayed, input);
	if (!check(step.playback.kind == ChatterPlaybackKind::Delay && !queue.finishDelay(step.token, 1) &&
				queue.finishDelay(step.token, .125f),
			"delay token enforces native bounds and cannot use ordinary duration"))
		return 1;
	input.now += .25f;
	step = queue.advance(delayed, input);
	ChatterStatementStep pending;
	if (!check(queue.currentPart(step.token, &pending) && pending.playback.phrase == ChatterPhrase::Affirmative &&
				queue.cancelPart(step.token) && !queue.currentPart(step.token, &pending),
			"explicit in-flight lookup and cancellation qualify the exact part"))
		return 1;
	queue.clear();
	auto missing = fixed(ChatterPhrase::Help);
	missing.program.append({ChatterPartKind::Place, ChatterPhrase::None, 88, 999});
	missing.program.append({ChatterPartKind::Fixed, ChatterPhrase::None, 0, 0});
	if (!check(!queue.admit(missing, admission, catalog, ChatterVerbosity::Normal).id,
			"missing definitions cannot become program parts"))
		return 1;
	OutgoingRadioScheduler scheduler;
	OutgoingRadioInput reportInput{};
	reportInput.actor = admission.owner.actor;
	reportInput.frame = admission.owner.frame;
	reportInput.now = 10;
	reportInput.alive = true;
	reportInput.team = 1;
	reportInput.friendCount = 2;
	reportInput.nearbyFriends = 2;
	reportInput.visibleEnemyCount = 3;
	reportInput.inCombat = true;
	if (!check(
			scheduler.needsEnemyPlace(reportInput), "first native enemy report needs current visible threat locations"))
		return 1;
	scheduler.prepare(reportInput, catalog);
	if (!check(!scheduler.needsEnemyPlace(reportInput),
			"already reported contacts do not repeat expensive NAV location searches"))
		return 1;
	++reportInput.frame.roundGeneration;
	if (!check(scheduler.needsEnemyPlace(reportInput), "fresh round cannot reuse previous report trigger metadata"))
		return 1;
	OutgoingRadioScheduler helpFirst;
	reportInput.nearbyFriends = 0;
	if (!check(!helpFirst.needsEnemyPlace(reportInput),
			"admission-time NeedBackup uses own Place and skips enemy NAV work"))
		return 1;
	return 0;
}
