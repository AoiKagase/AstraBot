#ifndef ASTRABOT_METAMOD_JOIN_CONTROLLER_HPP
#define ASTRABOT_METAMOD_JOIN_CONTROLLER_HPP

#include "astrabot/compat/command_registry.hpp"
#include "astrabot/metamod/abi_contract.hpp"
#include "astrabot/runtime/actor_registry.hpp"

#include <cstdint>

namespace astrabot
{
namespace metamod
{
	enum class JoinPhase : std::uint8_t
	{
		Idle,
		WaitingTeamMenu,
		TeamCommandPending,
		WaitingClassMenu,
		ClassCommandPending,
		WaitingConfirmation,
		Joined,
		Failed,
		Cancelled
	};

	enum class JoinError : std::uint8_t
	{
		None,
		InvalidRequest,
		AlreadyJoining,
		Timeout,
		CommandDispatchFailed,
		InvalidActor,
		WrongTeam,
		MenuOptionUnavailable,
		CommandAttemptsExhausted,
		Disconnected,
		MapDeactivated,
		CleanupFailed
	};

	enum class JoinMenuKind : std::uint8_t
	{
		Unknown,
		Team,
		TerroristClass,
		CounterTerroristClass
	};

	enum class JoinMenuSource : std::uint8_t
	{
		Unknown,
		LegacyShowMenu,
		Vgui
	};

	enum class JoinActionKind : std::uint8_t
	{
		None,
		SendMenuSelect,
		Joined,
		Failed,
		Cancelled
	};

	enum class JoinCommandKind : std::uint8_t
	{
		MenuSelect,
		JoinTeam,
		JoinClass
	};

	struct JoinAction
	{
		JoinActionKind kind;
		JoinError error;
		JoinCommandKind command;
		std::uint8_t selection;

		static JoinAction noOp() noexcept;
		static JoinAction send(std::uint8_t selection) noexcept;
		static JoinAction send(JoinCommandKind command, std::uint8_t selection) noexcept;
		static JoinAction joined() noexcept;
		static JoinAction failed(JoinError error) noexcept;
		static JoinAction cancelled(JoinError error) noexcept;
	};

	class JoinController final
	{
	public:
		JoinController() noexcept;

		JoinAction begin(
			const runtime::ActorId &actor,
			compat::CommandTeam team,
			std::uint32_t startFrame) noexcept;
		JoinAction onMenu(JoinMenuKind menu, std::uint16_t validSlots,
						  std::uint32_t frame,
						  JoinMenuSource source = JoinMenuSource::Unknown) noexcept;
		JoinAction onTeamInfo(const char *teamName) noexcept;
		JoinAction onFrame(std::uint32_t frame) noexcept;
		JoinAction onFrame(const edict_t *entity, std::uint32_t frame) noexcept;
		JoinAction commandCompleted(std::uint32_t frame) noexcept;
		JoinAction commandFailed(JoinError error) noexcept;
		JoinAction cancel(JoinError error) noexcept;
		void reset() noexcept;

		bool active() const noexcept;
		bool isCurrent(const runtime::ActorId &actor) const noexcept;
		JoinPhase phase() const noexcept;
		JoinError error() const noexcept;
		std::uint8_t attempts() const noexcept;
		bool teamConfirmed() const noexcept;
		bool classSelectionCompleted() const noexcept;
		compat::CommandTeam requestedTeam() const noexcept;

	private:
		static constexpr std::uint8_t kMaximumAttempts = 3U;
		static constexpr std::uint8_t kClassSelection = 1U;
		static constexpr std::uint32_t kTimeoutFrames = 128U;
		static constexpr std::uint32_t kInitialFallbackFrames = 3U;
		static constexpr std::uint32_t kRetryFrames = 8U;
		static constexpr std::uint16_t kAllMenuSelections = 0xFFFFU;

		JoinAction fail(JoinError error) noexcept;
		JoinAction sendPendingSelection() noexcept;
		std::uint8_t teamSelection() const noexcept;
		JoinCommandKind pendingCommand() const noexcept;
		bool selectionAvailable(std::uint16_t validSlots, std::uint8_t selection) const noexcept;
		bool readyEntity(const edict_t *entity) const noexcept;
		bool expectedTeamName(const char *teamName) const noexcept;
		bool oppositeTeamName(const char *teamName) const noexcept;
		static bool textEqualsIgnoreCase(const char *left, const char *right) noexcept;

		JoinPhase phase_;
		JoinError error_;
		runtime::ActorId actor_;
		compat::CommandTeam team_;
		std::uint32_t deadline_;
		std::uint32_t nextFrame_;
		std::uint8_t attempts_;
		bool teamConfirmed_;
		bool teamInfoReceived_;
		bool classSelectionCompleted_;
		bool postClassFrameAdvanced_;
		bool teamMenuReceived_;
		bool classMenuReceived_;
		JoinMenuSource teamMenuSource_;
		JoinMenuSource classMenuSource_;
		bool pendingSelection_;
		bool repeatedPrompt_;
		std::uint8_t pendingValue_;
		std::uint32_t pendingFrame_;
		std::uint8_t promptGraceFrames_;
	};
}
}

#endif
