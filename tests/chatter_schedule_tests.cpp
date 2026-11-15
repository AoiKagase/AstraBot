#include "astrabot/compat/chatter_schedule.hpp"
#include "astrabot/compat/outgoing_radio.hpp"
#include <array>
#include <cstdio>
#include <limits>

namespace
{
using namespace astrabot;
compat::ChatterScheduleEntry entry(
	unsigned slot, float created, float start, std::uint64_t order, bool speaking = false)
{
	compat::ChatterScheduleEntry out{};
	out.owner = {{slot, 1}, {1, 1, 1}, 1, created, true};
	out.connection = 1;
	out.ownerRevision = 1;
	out.source = compat::ChatterScheduleSource::Generic;
	out.topic = compat::ChatterStatementTopic::Information;
	out.created = created;
	out.start = start;
	out.order = order;
	out.speaking = speaking;
	return out;
}
bool check(bool value, const char *what)
{
	if (!value)
		std::fprintf(stderr, "check failed: %s\n", what);
	return value;
}
}
int main()
{
	using namespace astrabot::compat;
	std::array<ChatterScheduleEntry, 3> entries{{entry(5, 10, 10, 1), entry(1, 11, 11, 1), {}}};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 12).owner.actor.slot == 5,
			"older teammate outranks current lower-slot updater"))
		return 1;
	entries = {{entry(5, 11, 11, 1), entry(1, 10, 14, 1), {}}};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 12).source == ChatterScheduleSource::None,
			"native timestamp assignment retains selected future blocker"))
		return 1;
	if (!check(selectActiveChatter(entries.data(), entries.size(), 14).owner.actor.slot == 1,
			"future blocker starts at its inclusive deadline"))
		return 1;
	entries = {{entry(1, 9, 13, 2), entry(1, 10, 12, 1), entry(5, 11, 11, 1)}};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 14).owner.actor.slot == 1,
			"scan compares starts against retained creation timestamp, not global minimum start"))
		return 1;
	entries = {{entry(1, 10, 12, 2), entry(1, 9, 12, 1), entry(5, 10, 11, 1, true)}};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 13).owner.actor.slot == 5,
			"retained speaking outranks all pending timestamps"))
		return 1;
	entries[2] = {};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 13).order == 1,
			"equal owner starts preserve actual insertion order"))
		return 1;
	entries = {{entry(5, 10, 10, 1), entry(1, 10, 10, 1), {}}};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 12).owner.actor.slot == 1,
			"equal cross-owner start preserves client-slot scan order"))
		return 1;
	entries[0].start = std::numeric_limits<float>::quiet_NaN();
	entries[1].owner.entityGeneration = 0;
	if (!check(selectActiveChatter(entries.data(), entries.size(), 12).source == ChatterScheduleSource::None,
			"malformed schedule receipts cannot authorize a speaker"))
		return 1;
	entries[0] = entry(1, 10, 10, 1, true);
	if (!check(selectActiveChatter(entries.data(), entries.size(), std::numeric_limits<float>::quiet_NaN()).source ==
				ChatterScheduleSource::None,
			"nonfinite scan time fails closed"))
		return 1;
	ChatterAdmissionClock clock;
	ChatterScheduleAdmission admission{entry(1, 10, 10, 1).owner, 1, 1, &clock};
	const auto first =
		admission.make(ChatterScheduleSource::EnemyCount, ChatterStatementTopic::EnemiesRemaining, 10, 12);
	const auto second =
		admission.make(ChatterScheduleSource::CommanderDown, ChatterStatementTopic::MyIntention, 12, 12);
	entries = {{second, first, {}}};
	if (!check(selectActiveChatter(entries.data(), entries.size(), 12).source == ChatterScheduleSource::EnemyCount,
			"cross-topic equal starts retain actual actor-wide admission order"))
		return 1;
	auto onMyWay = entry(1, 10, 10, 1);
	auto commander = entry(5, 10, 10, 1);
	onMyWay.topic = chatterStatementTopic(ChatterPhrase::OnMyWay);
	commander.topic = chatterStatementTopic(ChatterPhrase::CommanderDown);
	if (!check(chatterStatementsRedundant(onMyWay, commander),
			"native Say OnMyWay shares intention redundancy with CommanderDown"))
		return 1;
	const ChatterStatementTopic ordinary[] = {ChatterStatementTopic::Information, ChatterStatementTopic::MyIntention,
		ChatterStatementTopic::EnemiesRemaining, ChatterStatementTopic::KilledFriend};
	for (const auto topic : ordinary)
	{
		auto a = entry(1, 10, 10, 1);
		auto b = entry(5, 10, 10, 1);
		a.topic = b.topic = topic;
		if (!check(chatterStatementsRedundant(a, b), "unqualified equal native topics are redundant"))
			return 1;
		a.place = 7;
		if (!check(!chatterStatementsRedundant(a, b), "one explicit place is not redundant with no place"))
			return 1;
		b.place = 7;
		if (!check(chatterStatementsRedundant(a, b), "equal places make equal native topics redundant"))
			return 1;
		b.place = 8;
		a.subject = b.subject = 4;
		if (!check(chatterStatementsRedundant(a, b), "matching subject suffices despite differing places"))
			return 1;
		b.subject = 5;
		if (!check(!chatterStatementsRedundant(a, b), "different places and subjects remain distinct"))
			return 1;
		b.topic = ChatterStatementTopic::EnemyAction;
		b.place = a.place;
		if (!check(!chatterStatementsRedundant(a, b), "different native topics never become redundant"))
			return 1;
	}
	for (auto topic : {ChatterStatementTopic::MyPlan, ChatterStatementTopic::RequestHelp,
			 ChatterStatementTopic::CriticalEvent, ChatterStatementTopic::Acknowledge})
	{
		auto a = entry(1, 10, 10, 1);
		a.topic = topic;
		if (!check(!chatterStatementsRedundant(a, a),
				"native plan/help/critical/acknowledge exceptions retain duplicates"))
			return 1;
	}
	return 0;
}
