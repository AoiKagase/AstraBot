#ifndef ASTRABOT_ADAPTER_MOVEMENT_SOUND_BRIDGE_HPP
#define ASTRABOT_ADAPTER_MOVEMENT_SOUND_BRIDGE_HPP

#include "astrabot/metamod/abi_contract.hpp"
#include "pm_defs.h"
#include <array>
#include <cstdint>

namespace astrabot
{
namespace metamod
{
class MovementSoundBridge
{
public:
	using SoundFunction = void (*)(int, const char *, float, float, int, int);
	static constexpr unsigned kMaximumScopes = 8;

	struct Frame
	{
		playermove_t *move = nullptr;
		edict_t *source = nullptr;
		SoundFunction restore = nullptr;
		SoundFunction forward = nullptr;
		SoundFunction thunk = nullptr;
		std::uint64_t id = 0;
		std::uint32_t map = 0;
		std::uint32_t round = 0;
		float seconds = 0;
		int serial = -1;
		int playerIndex = -1;
		unsigned forwarding = 0;
		bool eligible = false;
		bool owned = false;
	};

	bool begin(playermove_t *move, edict_t *source, std::uint32_t map, std::uint32_t round,
		bool server, SoundFunction thunk, float seconds = 0)
	{
		if (overflow_ != 0 || depth_ == frames_.size())
		{
			reset();
			++overflow_;
			return false;
		}
		Frame frame{};
		frame.move = move;
		frame.source = source;
		frame.restore = move ? move->PM_PlaySound : nullptr;
		frame.forward = frame.restore;
		frame.thunk = thunk;
		frame.map = map;
		frame.round = round;
		frame.seconds = seconds;
		frame.serial = source ? source->serialnumber : -1;
		frame.playerIndex = move ? move->player_index : -1;
		frame.id = ++nextId_;
		frame.eligible = server && move && source && frame.serial >= 0;
		if (frame.forward == thunk)
		{
			frame.forward = nullptr;
			for (unsigned i = depth_; i > 0; --i)
				if (frames_[i - 1].move == move)
				{
					frame.forward = frames_[i - 1].forward;
					break;
				}
		}
		frame.eligible = frame.eligible && frame.forward;
		frame.owned = move && frame.restore && frame.forward && thunk;
		if (frame.owned)
			move->PM_PlaySound = thunk;
		frames_[depth_++] = frame;
		return frame.owned;
	}

	bool end(playermove_t *move)
	{
		if (overflow_ != 0)
		{
			--overflow_;
			return false;
		}
		if (depth_ == 0)
			return false;
		if (frames_[depth_ - 1].move != move)
		{
			reset();
			return false;
		}
		restore(frames_[depth_ - 1]);
		frames_[--depth_] = {};
		return true;
	}

	void reset()
	{
		for (unsigned i = depth_; i > 0; --i)
		{
			auto &frame = frames_[i - 1];
			restore(frame);
			frame.eligible = false;
			frame.source = nullptr;
		}
	}

	bool active() const
	{
		return depth_ != 0 || overflow_ != 0 || forwarding_ != 0;
	}

	const Frame *current() const
	{
		return depth_ != 0 && overflow_ == 0 ? &frames_[depth_ - 1] : nullptr;
	}

	const Frame *capture() const
	{
		const auto *frame = current();
		return frame && frame->eligible && frame->forwarding == 0 ? frame : nullptr;
	}

	void forward(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
	{
		Frame *frame = depth_ != 0 ? &frames_[depth_ - 1] : nullptr;
		if (frame && frame->forwarding != 0)
		{
			// A foreign wrapper can call a saved thunk from an outer scope.
			// Resolve that chain without re-entering a callback already on it.
			const auto *move = frame->move;
			frame = nullptr;
			for (unsigned i = depth_ - 1; i > 0; --i)
			{
				auto &candidate = frames_[i - 1];
				if (candidate.move != move || candidate.forwarding != 0 || !candidate.forward)
					continue;
				bool inFlight = false;
				for (unsigned j = i; j < depth_; ++j)
					if (frames_[j].move == move && frames_[j].forwarding != 0 &&
						frames_[j].forward == candidate.forward)
						inFlight = true;
				if (!inFlight)
				{
					frame = &candidate;
					break;
				}
			}
		}
		if (!frame)
			return;
		const auto function = frame ? frame->forward : nullptr;
		if (!function || (frame && function == frame->thunk))
			return;
		const std::uint64_t id = frame ? frame->id : 0;
		if (frame)
			++frame->forwarding;
		++forwarding_;
		function(channel, sample, volume, attenuation, flags, pitch);
		--forwarding_;
		for (unsigned i = 0; i < depth_; ++i)
			if (frames_[i].id == id && frames_[i].forwarding != 0)
			{
				--frames_[i].forwarding;
				break;
			}
	}

private:
	static void restore(Frame &frame)
	{
		if (frame.owned && frame.move && frame.move->PM_PlaySound == frame.thunk)
			frame.move->PM_PlaySound = frame.restore;
		frame.owned = false;
	}

	std::array<Frame, kMaximumScopes> frames_{};
	unsigned depth_ = 0;
	unsigned overflow_ = 0;
	unsigned forwarding_ = 0;
	std::uint64_t nextId_ = 0;
};
}
}

#endif
