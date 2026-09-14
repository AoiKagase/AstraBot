// SPDX-License-Identifier: MPL-2.0

#include "adapter/metamod/objective_observation.hpp"

#include <cassert>

namespace
{
namespace m = astrabot::adapter::metamod;
namespace c = astrabot::core;

constexpr c::PlayerId actor{1, {4}};
constexpr c::BotAgentId agent{9};
constexpr c::PlayerId carrier{2, {7}};

m::ObjectiveObservationStamp stamp()
{
	return {{3}, {5}, {11}, 2'000'000, actor, agent};
}

struct Source final : m::IObjectiveObservationSource
{
	m::ObjectiveAuthoritySnapshot snapshot{};
	bool available{true};

	bool read(const m::ObjectiveObservationStamp& expected,
			  m::ObjectiveAuthoritySnapshot& output) const noexcept override
	{
		(void)expected;
		if (!available)
			return false;
		output = snapshot;
		return true;
	}
};

void testUnavailableSourceFailsClosed()
{
	Source source{};
	source.available = false;
	const auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::SourceUnavailable);
	assert(!result.value.objectiveAvailable());
	assert(!result.value.economyAvailable());
	assert(!result.value.buyAvailable());
}

void testStaleSourceStampIsRejected()
{
	Source source{};
	source.snapshot.stamp = stamp();
	++source.snapshot.stamp.player.generation.value;
	const auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::StaleSnapshot);
}

void testInvalidWireEnumsAreRejected()
{
	Source source{};
	source.snapshot.stamp = stamp();
	source.snapshot.family = {static_cast<m::ObjectiveFamily>(255), true};
	auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::InvalidObjective);

	source.snapshot.family = {};
	source.snapshot.round.phase = {static_cast<c::economy::RoundPhase>(255), true};
	result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::InvalidRound);
}

void testUnknownFactsRemainUnavailable()
{
	Source source{};
	source.snapshot.stamp = stamp();
	source.snapshot.family.value = m::ObjectiveFamily::HostageRescue;
	source.snapshot.bomb.actorHasC4.value = true;
	source.snapshot.economy.money.value = 9'999;
	const auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(result);
	assert(!result.value.objectiveAvailable());
	assert(!result.value.roundAvailable());
	assert(!result.value.economyAvailable());
	assert(!result.value.buyAvailable());
	assert(!result.value.bomb.actorHasC4.known);
	assert(!result.value.bomb.actorHasC4.value);
	assert(result.value.economy.money.value == 0);
	assert(!result.value.vip.vip.known);
	assert(!result.value.hostages.known);
}

void testCarriedC4RequiresExplicitCarrier()
{
	Source source{};
	source.snapshot.stamp = stamp();
	source.snapshot.family = {m::ObjectiveFamily::BombDefusal, true};
	source.snapshot.bomb.state = {c::tactical::BombState::Carried, true};

	auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::InvalidObjective);

	source.snapshot.bomb.carrier = {carrier, true};
	source.snapshot.bomb.actorHasC4 = {false, true};
	result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(result);
	assert(result.value.objectiveAvailable());
	assert(result.value.bomb.carrier.value == carrier);
	assert(!result.value.bomb.actorHasC4.value);
}

void testPlantedBombRequiresObservedPositionAndTime()
{
	Source source{};
	source.snapshot.stamp = stamp();
	source.snapshot.family = {m::ObjectiveFamily::BombDefusal, true};
	source.snapshot.bomb.state = {c::tactical::BombState::Planted, true};
	source.snapshot.bomb.position = {{128.0, -64.0, 8.0}, true};

	auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::InvalidObjective);

	source.snapshot.bomb.remainingMicros = {12'000'000, true};
	source.snapshot.bomb.site = {2, true};
	result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(result);
	assert(result.value.objectiveAvailable());
	assert(result.value.bomb.site.value == 2);
}

void testVipAndHostagesPreserveAuthoritativeIdentities()
{
	Source vip{};
	vip.snapshot.stamp = stamp();
	vip.snapshot.family = {m::ObjectiveFamily::VipEscort, true};
	vip.snapshot.vip.vip = {carrier, true};
	vip.snapshot.vip.alive = {true, true};
	vip.snapshot.vip.escaped = {false, true};
	auto result = m::ObjectiveObservationAdapter::observe(stamp(), vip);
	assert(result && result.value.objectiveAvailable());
	assert(result.value.vip.vip.value == carrier);

	Source hostages{};
	hostages.snapshot.stamp = stamp();
	hostages.snapshot.family = {m::ObjectiveFamily::HostageRescue, true};
	hostages.snapshot.hostages.known = true;
	hostages.snapshot.hostages.count = 1;
	auto& hostage = hostages.snapshot.hostages.values[0];
	hostage.id = {41, {2}};
	hostage.position = {32.0, 48.0, 0.0};
	hostage.alive = true;
	result = m::ObjectiveObservationAdapter::observe(stamp(), hostages);
	assert(result && result.value.objectiveAvailable());
	assert(result.value.hostages.values[0].id == hostage.id);

	hostages.snapshot.hostages.count = 2;
	hostages.snapshot.hostages.values[1] = hostage;
	result = m::ObjectiveObservationAdapter::observe(stamp(), hostages);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::InvalidHostage);
}

void testRoundBuyAndEconomyNeedIndependentEvidence()
{
	Source source{};
	source.snapshot.stamp = stamp();
	source.snapshot.round.phase = {c::economy::RoundPhase::Live, true};
	source.snapshot.round.roundNumber = {8, true};
	source.snapshot.round.buyTimeOpen = {true, true};
	source.snapshot.economy.money = {3'250, true};
	source.snapshot.economy.weaponValue = {1'700, true};
	source.snapshot.economy.lowFunds = {false, true};

	auto result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(result);
	assert(result.value.roundAvailable());
	assert(result.value.economyAvailable());
	assert(!result.value.buyAvailable());

	source.snapshot.economy.inBuyZone = {true, true};
	result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(result && result.value.buyAvailable());
	assert(result.value.economy.money.value == 3'250);

	source.snapshot.economy.money.value = c::economy::kMaxMoney + 1;
	result = m::ObjectiveObservationAdapter::observe(stamp(), source);
	assert(!result);
	assert(result.error == m::ObjectiveObservationError::InvalidEconomy);
}

} // namespace

int main()
{
	testUnavailableSourceFailsClosed();
	testStaleSourceStampIsRejected();
	testInvalidWireEnumsAreRejected();
	testUnknownFactsRemainUnavailable();
	testCarriedC4RequiresExplicitCarrier();
	testPlantedBombRequiresObservedPositionAndTime();
	testVipAndHostagesPreserveAuthoritativeIdentities();
	testRoundBuyAndEconomyNeedIndependentEvidence();
	return 0;
}
