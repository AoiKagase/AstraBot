// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "core/economy.hpp"
#include "core/tactical_planner.hpp"
#include "core/team_director.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace astrabot::adapter::metamod
{

template <typename T> struct ObservedValue final
{
	T value{};
	bool known{false};
};

enum class ObjectiveFamily : std::uint8_t
{
	BombDefusal = 0,
	VipEscort,
	HostageRescue,
	Escape,
};

struct ObjectiveObservationStamp final
{
	core::MapGeneration map{};
	core::perception::RoundGeneration round{};
	core::TickId tick{};
	std::uint64_t nowMicros{0};
	core::PlayerId player{};
	core::BotAgentId agent{};

	constexpr bool valid() const noexcept
	{
		return map.isValid() && round.isValid() && tick.isValid() && nowMicros != 0 && player.isValid() &&
			   agent.isValid();
	}
	friend constexpr bool operator==(const ObjectiveObservationStamp& left,
									 const ObjectiveObservationStamp& right) noexcept
	{
		return left.map == right.map && left.round == right.round && left.tick == right.tick &&
			   left.nowMicros == right.nowMicros && left.player == right.player && left.agent == right.agent;
	}
	friend constexpr bool operator!=(const ObjectiveObservationStamp& left,
									 const ObjectiveObservationStamp& right) noexcept
	{
		return !(left == right);
	}
};

struct BombAuthorityObservation final
{
	ObservedValue<core::tactical::BombState> state{};
	ObservedValue<core::PlayerId> carrier{};
	ObservedValue<std::uint32_t> site{};
	ObservedValue<core::perception::Point> position{};
	ObservedValue<std::uint64_t> remainingMicros{};
	ObservedValue<bool> actorHasC4{};
	ObservedValue<bool> actorInBombZone{};
	ObservedValue<bool> actorHasDefuseKit{};
};

struct VipAuthorityObservation final
{
	ObservedValue<core::PlayerId> vip{};
	ObservedValue<bool> alive{};
	ObservedValue<bool> escaped{};
};

struct HostageAuthorityObservation final
{
	std::array<core::team::ObjectiveTarget, core::team::kMaxObjectiveTargets> values{};
	std::size_t count{0};
	bool known{false};
};

struct RoundAuthorityObservation final
{
	ObservedValue<core::economy::RoundPhase> phase{};
	ObservedValue<std::uint32_t> roundNumber{};
	ObservedValue<bool> buyTimeOpen{};
};

struct EconomyAuthorityObservation final
{
	ObservedValue<std::int32_t> money{};
	ObservedValue<std::int32_t> weaponValue{};
	ObservedValue<bool> lowFunds{};
	ObservedValue<bool> inBuyZone{};
};

// Filled synchronously by the ReGameDLL/CS integration layer. Every field is
// independently qualified: absence of one authority must not create a false
// default for another objective or economy fact.
struct ObjectiveAuthoritySnapshot final
{
	ObjectiveObservationStamp stamp{};
	ObservedValue<ObjectiveFamily> family{};
	BombAuthorityObservation bomb{};
	VipAuthorityObservation vip{};
	HostageAuthorityObservation hostages{};
	RoundAuthorityObservation round{};
	EconomyAuthorityObservation economy{};
};

class IObjectiveObservationSource
{
public:
	virtual ~IObjectiveObservationSource() = default;
	virtual bool read(const ObjectiveObservationStamp& expected, ObjectiveAuthoritySnapshot& output) const noexcept = 0;
};

struct ObjectiveEconomyObservation final
{
	ObjectiveObservationStamp stamp{};
	ObservedValue<ObjectiveFamily> family{};
	BombAuthorityObservation bomb{};
	VipAuthorityObservation vip{};
	HostageAuthorityObservation hostages{};
	RoundAuthorityObservation round{};
	EconomyAuthorityObservation economy{};

	bool objectiveAvailable() const noexcept;
	bool roundAvailable() const noexcept;
	bool economyAvailable() const noexcept;
	bool buyAvailable() const noexcept;
};

enum class ObjectiveObservationError : std::uint8_t
{
	None = 0,
	InvalidExpectedStamp,
	SourceUnavailable,
	StaleSnapshot,
	InvalidObjective,
	InvalidHostage,
	InvalidRound,
	InvalidEconomy,
};

struct ObjectiveObservationResult final
{
	ObjectiveEconomyObservation value{};
	ObjectiveObservationError error{ObjectiveObservationError::None};
	bool accepted{false};

	constexpr explicit operator bool() const noexcept
	{
		return accepted && error == ObjectiveObservationError::None;
	}
};

class ObjectiveObservationAdapter final
{
public:
	static ObjectiveObservationResult observe(const ObjectiveObservationStamp& expected,
											  const IObjectiveObservationSource& source) noexcept;
};

} // namespace astrabot::adapter::metamod
