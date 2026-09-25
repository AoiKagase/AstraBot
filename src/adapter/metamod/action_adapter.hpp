#ifndef ASTRABOT_ADAPTER_METAMOD_ACTION_ADAPTER_HPP
#define ASTRABOT_ADAPTER_METAMOD_ACTION_ADAPTER_HPP

#include "astrabot/runtime/bot_command.hpp"

#include <in_buttons.h>

#include <cstdint>

namespace astrabot
{
namespace metamod
{
enum class ActionKind : std::uint8_t
{
	None,
	Fire,
	Reload,
	Plant,
	PlantContinue,
	SelectC4,
	Defuse
};

struct ActionProposal
{
	ActionKind kind;
	runtime::ViewAngles viewAngles;
	std::uint16_t movementButtons;
};

struct ActionDispatch
{
	ActionKind kind;
	runtime::ViewAngles viewAngles;
	std::uint16_t buttons;
	const char *clientCommand;
	bool stopMovement;
};

struct MovementProjection
{
	float forward;
	float side;
};

struct PlantTargetBounds {
	float minimumX;
	float minimumY;
	float minimumZ;
	float maximumX;
	float maximumY;
	float maximumZ;
};

struct PlantAttemptState {
	bool active;
	float startedAt;
	float retryAfter;
};

enum class PlantAttemptResult : std::uint8_t {
	OutsideSite,
	RetryPending,
	Ready,
	Continuing,
	TimedOut,
	Confirmed,
	InvalidTime
};

class ActionAdapter
{
  public:
	static constexpr std::uint16_t kAttackButton = static_cast<std::uint16_t>(IN_ATTACK);
	static constexpr std::uint16_t kUseButton = static_cast<std::uint16_t>(IN_USE);
	static constexpr std::uint16_t kForwardButton = static_cast<std::uint16_t>(IN_FORWARD);
	static constexpr std::uint16_t kBackButton = static_cast<std::uint16_t>(IN_BACK);
	static constexpr std::uint16_t kMoveLeftButton = static_cast<std::uint16_t>(IN_MOVELEFT);
	static constexpr std::uint16_t kMoveRightButton = static_cast<std::uint16_t>(IN_MOVERIGHT);
	static constexpr const char *kReloadCommand = "reload";
	static constexpr const char *kSelectC4Command = "weapon_c4";
	// Public WEAPON_C4 value in the pinned ReGameDLL-CS CurWeapon protocol.
	static constexpr std::uint8_t kC4WeaponId = 6U;
	static constexpr float kPlantAttemptTimeoutSeconds = 5.0f;
	static constexpr float kPlantAttemptRetrySeconds = 1.0f;
	static bool validPlantTargetBounds(const PlantTargetBounds &bounds) noexcept;
	static bool overlapsPlantTarget(const PlantTargetBounds &site,
		const PlantTargetBounds &actor) noexcept;
	static bool plantAttemptExpired(float elapsedSeconds) noexcept;
	static PlantAttemptResult evaluatePlantAttempt(PlantAttemptState &state,
		bool actorOverlapsSite, bool plantedConfirmed, float now) noexcept;
	static void recordPlantAttemptDispatched(PlantAttemptState &state, float now) noexcept;
	static ActionKind plantActionForWeaponObservation(bool observationAvailable,
		bool active, std::uint8_t weaponId, bool attackAlreadyDispatched) noexcept;

	static ActionDispatch translate(const ActionProposal &proposal);
	static ActionProposal forLiveDispatch(
		const ActionProposal &proposal,
		bool activeWeaponAvailable,
		const runtime::ViewAngles &movementViewAngles);
	static MovementProjection projectMovement(
		float worldDirectionX,
		float worldDirectionY,
		float speed,
		float viewYaw);
	static std::uint16_t movementButtons(float forward, float side);
};
} // namespace metamod
} // namespace astrabot

#endif
