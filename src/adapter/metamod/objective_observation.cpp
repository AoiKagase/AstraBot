// SPDX-License-Identifier: MPL-2.0

#include "adapter/metamod/objective_observation.hpp"

namespace astrabot::adapter::metamod
{
namespace
{

bool validFamily(ObjectiveFamily family) noexcept
{
	switch (family)
	{
	case ObjectiveFamily::BombDefusal:
	case ObjectiveFamily::VipEscort:
	case ObjectiveFamily::HostageRescue:
	case ObjectiveFamily::Escape:
		return true;
	}
	return false;
}

bool validBombState(core::tactical::BombState state) noexcept
{
	switch (state)
	{
	case core::tactical::BombState::None:
	case core::tactical::BombState::Carried:
	case core::tactical::BombState::Dropped:
	case core::tactical::BombState::Planted:
	case core::tactical::BombState::Defused:
	case core::tactical::BombState::Exploded:
		return true;
	}
	return false;
}

bool validBomb(const ObjectiveAuthoritySnapshot& snapshot) noexcept
{
	const auto& bomb = snapshot.bomb;
	if (bomb.position.known && !core::perception::finite(bomb.position.value))
		return false;
	if (!bomb.state.known)
		return true;
	if (!validBombState(bomb.state.value))
		return false;

	if (bomb.state.value == core::tactical::BombState::Carried)
	{
		if (!bomb.carrier.known || !bomb.carrier.value.isValid())
			return false;
	}
	else if (bomb.carrier.known && bomb.carrier.value.isValid())
	{
		return false;
	}

	if (bomb.actorHasC4.known && bomb.actorHasC4.value &&
		(bomb.state.value != core::tactical::BombState::Carried || !bomb.carrier.known ||
		 bomb.carrier.value != snapshot.stamp.player))
	{
		return false;
	}

	if (bomb.state.value == core::tactical::BombState::Planted)
	{
		return bomb.position.known && bomb.remainingMicros.known && bomb.remainingMicros.value != 0 &&
			   bomb.site.known && bomb.site.value != 0;
	}
	return true;
}

bool validVip(const ObjectiveAuthoritySnapshot& snapshot) noexcept
{
	if (snapshot.vip.vip.known && !snapshot.vip.vip.value.isValid())
		return false;
	if (!snapshot.family.known || snapshot.family.value != ObjectiveFamily::VipEscort)
		return true;
	return snapshot.vip.vip.known && snapshot.vip.vip.value.isValid() && snapshot.vip.alive.known &&
		   snapshot.vip.escaped.known;
}

ObjectiveObservationError validateHostages(const HostageAuthorityObservation& hostages) noexcept
{
	if (hostages.count > hostages.values.size())
		return ObjectiveObservationError::InvalidHostage;
	if (!hostages.known)
		return hostages.count == 0 ? ObjectiveObservationError::None : ObjectiveObservationError::InvalidHostage;
	for (std::size_t i = 0; i < hostages.count; ++i)
	{
		if (!hostages.values[i].valid())
			return ObjectiveObservationError::InvalidHostage;
		for (std::size_t j = 0; j < i; ++j)
			if (hostages.values[i].id == hostages.values[j].id)
				return ObjectiveObservationError::InvalidHostage;
	}
	return ObjectiveObservationError::None;
}

bool validRound(const RoundAuthorityObservation& round) noexcept
{
	if (round.phase.known)
	{
		switch (round.phase.value)
		{
		case core::economy::RoundPhase::FreezeTime:
		case core::economy::RoundPhase::Live:
		case core::economy::RoundPhase::PostRound:
			break;
		case core::economy::RoundPhase::Unknown:
		default:
			return false;
		}
	}
	if (round.roundNumber.known && round.roundNumber.value == 0)
		return false;
	return true;
}

bool validEconomy(const EconomyAuthorityObservation& economy) noexcept
{
	if (economy.money.known && (economy.money.value < 0 || economy.money.value > core::economy::kMaxMoney))
		return false;
	return !economy.weaponValue.known || economy.weaponValue.value >= 0;
}

template <typename T> void clearUnknown(ObservedValue<T>& value) noexcept
{
	if (!value.known)
		value.value = {};
}

void normalize(ObjectiveEconomyObservation& value) noexcept
{
	clearUnknown(value.family);
	clearUnknown(value.bomb.state);
	clearUnknown(value.bomb.carrier);
	clearUnknown(value.bomb.site);
	clearUnknown(value.bomb.position);
	clearUnknown(value.bomb.remainingMicros);
	clearUnknown(value.bomb.actorHasC4);
	clearUnknown(value.bomb.actorInBombZone);
	clearUnknown(value.bomb.actorHasDefuseKit);
	clearUnknown(value.vip.vip);
	clearUnknown(value.vip.alive);
	clearUnknown(value.vip.escaped);
	if (!value.hostages.known)
		value.hostages = {};
	clearUnknown(value.round.phase);
	clearUnknown(value.round.roundNumber);
	clearUnknown(value.round.buyTimeOpen);
	clearUnknown(value.economy.money);
	clearUnknown(value.economy.weaponValue);
	clearUnknown(value.economy.lowFunds);
	clearUnknown(value.economy.inBuyZone);
}

} // namespace

bool ObjectiveEconomyObservation::objectiveAvailable() const noexcept
{
	if (!family.known)
		return false;
	switch (family.value)
	{
	case ObjectiveFamily::BombDefusal:
		return bomb.state.known;
	case ObjectiveFamily::VipEscort:
		return vip.vip.known && vip.alive.known && vip.escaped.known;
	case ObjectiveFamily::HostageRescue:
		return hostages.known;
	case ObjectiveFamily::Escape:
		return round.phase.known;
	}
	return false;
}

bool ObjectiveEconomyObservation::roundAvailable() const noexcept
{
	return round.phase.known && round.roundNumber.known && round.buyTimeOpen.known;
}

bool ObjectiveEconomyObservation::economyAvailable() const noexcept
{
	return economy.money.known && economy.weaponValue.known && economy.lowFunds.known;
}

bool ObjectiveEconomyObservation::buyAvailable() const noexcept
{
	return round.buyTimeOpen.known && economy.money.known && economy.inBuyZone.known;
}

ObjectiveObservationResult ObjectiveObservationAdapter::observe(const ObjectiveObservationStamp& expected,
																const IObjectiveObservationSource& source) noexcept
{
	ObjectiveObservationResult result{};
	if (!expected.valid())
	{
		result.error = ObjectiveObservationError::InvalidExpectedStamp;
		return result;
	}

	ObjectiveAuthoritySnapshot authority{};
	if (!source.read(expected, authority))
	{
		result.error = ObjectiveObservationError::SourceUnavailable;
		return result;
	}
	if (authority.stamp != expected)
	{
		result.error = ObjectiveObservationError::StaleSnapshot;
		return result;
	}
	if ((authority.family.known && !validFamily(authority.family.value)) || !validBomb(authority) ||
		!validVip(authority))
	{
		result.error = ObjectiveObservationError::InvalidObjective;
		return result;
	}
	if (const auto error = validateHostages(authority.hostages); error != ObjectiveObservationError::None)
	{
		result.error = error;
		return result;
	}
	if (!validRound(authority.round))
	{
		result.error = ObjectiveObservationError::InvalidRound;
		return result;
	}
	if (!validEconomy(authority.economy))
	{
		result.error = ObjectiveObservationError::InvalidEconomy;
		return result;
	}

	result.value.stamp = authority.stamp;
	result.value.family = authority.family;
	result.value.bomb = authority.bomb;
	result.value.vip = authority.vip;
	result.value.hostages = authority.hostages;
	result.value.round = authority.round;
	result.value.economy = authority.economy;
	normalize(result.value);
	result.accepted = true;
	return result;
}

} // namespace astrabot::adapter::metamod
