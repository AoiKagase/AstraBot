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
		Timeout,
		CommandDispatchFailed,
		InvalidActor,
		WrongTeam
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
		JoinAction onMenu(bool classMenu, std::uint32_t frame) noexcept;
		JoinAction onTeamInfo(const char *teamName) noexcept;
		JoinAction onFrame(edict_t *entity, std::uint32_t frame) noexcept;
		JoinAction commandCompleted(std::uint32_t frame) noexcept;
		JoinAction commandFailed(JoinError error) noexcept;
		JoinAction cancel(JoinError error) noexcept;
		void reset() noexcept;

		bool active() const noexcept;
		bool isCurrent(const runtime::ActorId &actor) const noexcept;
		JoinPhase phase() const noexcept;
		JoinError error() const noexcept;

	private:
		static constexpr std::uint8_t kMaximumAttempts = 3U;
		static constexpr std::uint32_t kTimeoutFrames = 128U;
		static constexpr std::uint32_t kInitialFallbackFrames = 3U;
		static constexpr std::uint32_t kRetryFrames = 8U;

		JoinAction fail(JoinError error) noexcept;
		JoinAction retryTeam(std::uint32_t frame) noexcept;
		std::uint8_t teamSelection() const noexcept;
		bool expectedTeamName(const char *teamName) const noexcept;
		bool oppositeTeamName(const char *teamName) const noexcept;
		bool readyEntity(edict_t *entity) const noexcept;
		static bool textEqualsIgnoreCase(const char *left, const char *right) noexcept;

		JoinPhase phase_;
		JoinError error_;
		runtime::ActorId actor_;
		compat::CommandTeam team_;
		std::uint32_t deadline_;
		std::uint32_t nextFrame_;
		std::uint8_t attempts_;
		bool teamMenuReceived_;
		bool classMenuReceived_;
		bool teamConfirmed_;
	};
}
}

#endif
