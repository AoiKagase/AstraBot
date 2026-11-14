#ifndef ASTRABOT_COMPAT_GUNSHOT_EVENTS_HPP
#define ASTRABOT_COMPAT_GUNSHOT_EVENTS_HPP
#include <array>
#include <cstdint>
#include <cstring>
namespace astrabot
{
namespace compat
{
// Only standard server gun scripts establish a firing receipt. No private weapon state.
class GunshotEventCatalog
{
	enum class Kind : std::uint8_t
	{
		None,
		Ordinary,
		Tmp,
		Awp,
		M4a1,
		Usp
	};
	struct Entry
	{
		std::uint16_t index{};
		Kind kind{};
	};
	std::array<Entry, 32> entries_{};
	static Kind classify(const char *script)
	{
		if (!script)
			return Kind::None;
		if (std::strcmp(script, "events/tmp.sc") == 0)
			return Kind::Tmp;
		if (std::strcmp(script, "events/awp.sc") == 0)
			return Kind::Awp;
		if (std::strcmp(script, "events/m4a1.sc") == 0)
			return Kind::M4a1;
		if (std::strcmp(script, "events/usp.sc") == 0)
			return Kind::Usp;
		static const char *const ordinary[] = {"events/ak47.sc", "events/aug.sc", "events/deagle.sc",
			"events/elite_left.sc", "events/elite_right.sc", "events/famas.sc", "events/fiveseven.sc",
			"events/g3sg1.sc", "events/galil.sc", "events/glock18.sc", "events/m249.sc", "events/m3.sc",
			"events/mac10.sc", "events/mp5n.sc", "events/p228.sc", "events/p90.sc", "events/scout.sc",
			"events/sg550.sc", "events/sg552.sc", "events/ump45.sc", "events/xm1014.sc"};
		for (const char *known : ordinary)
			if (std::strcmp(script, known) == 0)
				return Kind::Ordinary;
		return Kind::None;
	}

public:
	void reset()
	{
		entries_ = {};
	}
	void observe(std::uint16_t index, const char *script)
	{
		if (index == 0)
			return;
		const auto kind = classify(script);
		for (auto &entry : entries_)
			if (entry.index == index)
			{
				entry = kind == Kind::None ? Entry{} : Entry{index, kind};
				return;
			}
		if (kind == Kind::None)
			return;
		for (auto &entry : entries_)
			if (entry.index == 0)
			{
				entry = {index, kind};
				return;
			}
	}
	float range(std::uint16_t index, int bparam1, int bparam2) const
	{
		if (index == 0)
			return 0;
		for (const auto &entry : entries_)
			if (entry.index == index)
			{
				switch (entry.kind)
				{
				case Kind::Ordinary:
					return 2000;
				case Kind::Tmp:
					return 1000;
				case Kind::Awp:
					return 99999;
				case Kind::M4a1:
					return bparam1 ? 1000.0f : 2000.0f;
				case Kind::Usp:
					return bparam2 ? 1000.0f : 2000.0f;
				default:
					return 0;
				}
			}
		return 0;
	}
};
}
}
#endif
