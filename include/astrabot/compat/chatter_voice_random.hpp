#ifndef ASTRABOT_COMPAT_CHATTER_VOICE_RANDOM_HPP
#define ASTRABOT_COMPAT_CHATTER_VOICE_RANDOM_HPP
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>

namespace astrabot
{
namespace compat
{
// CSBot uses the platform CRT through std::random_shuffle. Keep the default
// CRT stream local so chatter does not perturb the engine or other plugins.
class ChatterVoiceRandom
{
public:
#ifdef _WIN32
	using result_type = std::uint32_t;
	static constexpr result_type min() { return 0; }
	static constexpr result_type max() { return 32767; }
	result_type operator()()
	{
		state_ = state_ * 214013U + 2531011U;
		return (state_ >> 16U) & max();
	}
#else
	ChatterVoiceRandom()
	{
		state_[0] = 1;
		for (std::size_t i = 1; i < state_.size(); ++i)
			state_[i] = static_cast<std::uint32_t>((16807ULL * state_[i - 1]) % 2147483647ULL);
		for (std::size_t i = 0; i < 10 * state_.size(); ++i)
			(void)next();
	}
#endif

	template <typename Iterator> void shuffle(Iterator first, Iterator last)
	{
		if (first == last)
			return;
#ifdef _WIN32
		std::shuffle(first, last, *this);
#else
		// libstdc++ random_shuffle consumes exactly one glibc rand per swap.
		for (auto current = first + 1; current != last; ++current)
		{
			const auto count = static_cast<std::uint32_t>(current - first + 1);
			std::iter_swap(current, first + next() % count);
		}
#endif
	}

private:
#ifdef _WIN32
	std::uint32_t state_ = 1;
#else
	std::uint32_t next()
	{
		state_[front_] += state_[rear_];
		const auto value = state_[front_] >> 1U;
		front_ = (front_ + 1) % state_.size();
		rear_ = (rear_ + 1) % state_.size();
		return value;
	}
	// glibc's default TYPE_3 additive generator, degree 31 and separation 3.
	std::array<std::uint32_t, 31> state_{};
	std::size_t front_ = 3;
	std::size_t rear_ = 0;
#endif
};
} // namespace compat
} // namespace astrabot
#endif
