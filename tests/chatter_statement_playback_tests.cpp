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
}
int main()
{
	using namespace astrabot::compat;
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
