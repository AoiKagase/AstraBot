#ifndef ASTRABOT_COMPAT_CHATTER_CATALOG_HPP
#define ASTRABOT_COMPAT_CHATTER_CATALOG_HPP

#include "astrabot/compat/chatter_schedule.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace astrabot
{
namespace compat
{
enum class ChatterPhrase : unsigned
{
	None,
	EnemySpotted,
	Help,
	RequestReport,
	Affirmative,
	OnMyWay,
	PlantingBomb,
	WhereIsTheBomb,
	EscortingHostages,
	HeardNoise,
	NoEnemiesLeft,
	OneEnemyLeft,
	TwoEnemiesLeft,
	ThreeEnemiesLeft,
	NiceShot,
	NiceShotSir,
	NiceShotCommander,
	CommanderDown,
	KilledFriend,
	Pain,
	AgreeWithPlan,
	GoingToPlantBomb,
	CoveringFriend,
	InCombat,
	EnemyDown,
	Clear,
	DefusingBomb,
	BombsiteClear,
	PlantedBombPlace,
	SpottedBomber,
	SpottedLooseBomb,
	TheyPickedUpTheBomb,
	GoingToGuardLooseBomb,
	GuardingLooseBomb,
	GoingToDefendBombsite,
	HostagesBeingTaken,
	Count
};
inline ChatterPhrase remainingEnemyPhrase(int count)
{
	constexpr int kMaximumReportedEnemies = 3;
	return count >= 0 && count <= kMaximumReportedEnemies
		? static_cast<ChatterPhrase>(static_cast<unsigned>(ChatterPhrase::NoEnemiesLeft) + static_cast<unsigned>(count))
		: ChatterPhrase::None;
}

inline bool isRemainingEnemyPhrase(ChatterPhrase phrase)
{
	return phrase >= ChatterPhrase::NoEnemiesLeft && phrase <= ChatterPhrase::ThreeEnemiesLeft;
}

inline bool isKillPraisePhrase(ChatterPhrase phrase)
{
	return phrase >= ChatterPhrase::NiceShot && phrase <= ChatterPhrase::NiceShotCommander;
}

inline bool isIntentionPhrase(ChatterPhrase phrase)
{
	return isKillPraisePhrase(phrase) || phrase == ChatterPhrase::CommanderDown;
}

inline bool usesUndefinedChatterPlace(ChatterPhrase phrase)
{
	return phrase != ChatterPhrase::EnemySpotted && phrase != ChatterPhrase::Help &&
		phrase != ChatterPhrase::PlantingBomb && phrase != ChatterPhrase::GoingToPlantBomb &&
		phrase != ChatterPhrase::HeardNoise && phrase != ChatterPhrase::GoingToGuardLooseBomb &&
		phrase != ChatterPhrase::GoingToDefendBombsite;
}

inline ChatterStatementTopic chatterStatementTopic(ChatterPhrase phrase)
{
	if (isRemainingEnemyPhrase(phrase))
		return ChatterStatementTopic::EnemiesRemaining;
	if (isIntentionPhrase(phrase))
		return ChatterStatementTopic::MyIntention;
	switch (phrase)
	{
	case ChatterPhrase::EnemySpotted:
		return ChatterStatementTopic::VisibleEnemies;
	case ChatterPhrase::Help:
		return ChatterStatementTopic::RequestHelp;
	case ChatterPhrase::RequestReport:
	case ChatterPhrase::WhereIsTheBomb:
		return ChatterStatementTopic::RequestInformation;
	case ChatterPhrase::Affirmative:
		return ChatterStatementTopic::Acknowledge;
	case ChatterPhrase::OnMyWay:
	case ChatterPhrase::DefusingBomb:
	case ChatterPhrase::CoveringFriend:
		return ChatterStatementTopic::MyIntention;
	case ChatterPhrase::PlantingBomb:
	case ChatterPhrase::GoingToPlantBomb:
		return ChatterStatementTopic::CriticalEvent;
	case ChatterPhrase::EscortingHostages:
	case ChatterPhrase::GoingToGuardLooseBomb:
	case ChatterPhrase::GoingToDefendBombsite:
		return ChatterStatementTopic::MyPlan;
	case ChatterPhrase::HeardNoise:
	case ChatterPhrase::SpottedBomber:
	case ChatterPhrase::SpottedLooseBomb:
	case ChatterPhrase::TheyPickedUpTheBomb:
	case ChatterPhrase::HostagesBeingTaken:
	case ChatterPhrase::GuardingLooseBomb:
		return ChatterStatementTopic::Information;
	case ChatterPhrase::KilledFriend:
		return ChatterStatementTopic::KilledFriend;
	default:
		return ChatterStatementTopic::None;
	}
}

struct RadioMenuChoice
{
	int menu = 0, slot = 0;
};
struct ChatterPlaceDefinition
{
	std::uint32_t id = 0;
	std::string name;
	RadioMenuChoice choice;
	bool important = false;
	std::uint32_t occurrence = 0;
};
// Reads definition metadata from the host's own DB.
// No voice samples, proprietary phrase text, or presumed fallback equivalents.
class ChatterRadioCatalog
{

public:
	bool loadText(const char *data, std::size_t length)
	{
		constexpr std::size_t kMaximumFileBytes = 1024U * 1024U;
		if ((!data && length) || length > kMaximumFileBytes)
			return false;
		ChatterRadioCatalog candidate;
		std::size_t pos = 0;
		std::uint32_t id = 0;
		std::string token;
		while (next(data, length, pos, token))
		{
			if (equal(token, "BaseDir"))
			{
				if (!value(data, length, pos, token))
					return false;
				continue;
			}
			if (!equal(token, "Chatter") && !equal(token, "Place"))
				continue;
			const bool chatter = equal(token, "Chatter");
			if (!next(data, length, pos, token))
				return false;
			const std::string definitionName = token;
			const ChatterPhrase phrase = chatter ? phraseFor(token) : ChatterPhrase::None;
			++id;
			ChatterPlaceDefinition place;
			place.id = id;
			place.name = definitionName;
			for (const auto &previous : candidate.places_)
				if (equal(previous.name, definitionName.c_str()))
					++place.occurrence;
			if (phrase != ChatterPhrase::None)
				candidate.defined_[static_cast<unsigned>(phrase)] = true;
			bool ended = false;
			while (next(data, length, pos, token))
			{
				if (equal(token, "Place") || equal(token, "Count"))
				{
					if (!value(data, length, pos, token))
						return false;
					continue;
				}
				if (equal(token, "End"))
				{
					ended = true;
					break;
				}
				if (equal(token, "Important"))
				{
					place.important = true;
					if (phrase != ChatterPhrase::None)
						candidate.important_[static_cast<unsigned>(phrase)] = true;
				}
				if (equal(token, "Radio"))
				{
					if (!value(data, length, pos, token))
						return false;
					const auto choice = eventFor(token);
					if (!choice.menu)
						return false;
					place.choice = choice;
					if (phrase != ChatterPhrase::None)
						candidate.choices_[static_cast<unsigned>(phrase)] = choice;
				}
			}
			if (!ended)
				return false;
			if (chatter)
				candidate.phraseNames_.push_back({id, definitionName});
			else
				candidate.places_.push_back(std::move(place));
		}
		if (pos > length)
			return false;
		candidate.loaded_ = true;
		*this = std::move(candidate);
		return true;
	}

	const ChatterPlaceDefinition *findPlace(const char *name) const
	{
		if (name)
			for (const auto &place : places_)
				if (equal(place.name, name))
					return &place;
		return nullptr;
	}
	const ChatterPlaceDefinition *place(std::uint32_t id) const
	{
		for (const auto &entry : places_)
			if (entry.id == id)
				return &entry;
		return nullptr;
	}
	bool definedPhraseName(const char *name) const
	{
		if (name)
			for (const auto &entry : phraseNames_)
				if (equal(entry.name, name))
					return true;
		return false;
	}
	std::uint32_t phraseId(ChatterPhrase phrase) const
	{
		if (defined(phrase))
			for (const auto &entry : phraseNames_)
				if (equal(entry.name, name(phrase)))
					return entry.id;
		return 0;
	}
	std::uint32_t nameToId(const char *name, std::uint32_t beforeId = (std::numeric_limits<std::uint32_t>::max)()) const
	{
		if (!name)
			return 0;
		for (const auto &entry : places_)
			if (entry.id < beforeId && equal(entry.name, name))
				return entry.id;
		for (const auto &entry : phraseNames_)
			if (entry.id < beforeId && equal(entry.name, name))
				return entry.id;
		return 0;
	}
	bool hasPlaceBefore(std::uint32_t beforeId = (std::numeric_limits<std::uint32_t>::max)()) const
	{
		for (const auto &entry : places_)
			if (entry.id < beforeId)
				return true;
		return false;
	}
	static bool validRadioEvent(const std::string &name)
	{
		return eventFor(name).menu != 0;
	}

	RadioMenuChoice lookup(ChatterPhrase phrase) const
	{
		const auto i = static_cast<unsigned>(phrase);
		return i < choices_.size() ? choices_[i] : RadioMenuChoice{};
	}

	bool defined(ChatterPhrase phrase) const
	{
		const auto i = static_cast<unsigned>(phrase);
		return i < defined_.size() && defined_[i];
	}

	bool loaded() const
	{
		return loaded_;
	}

	bool important(ChatterPhrase p) const
	{
		const auto i = static_cast<unsigned>(p);
		return i < important_.size() && important_[i];
	}

	static const char *name(ChatterPhrase p)
	{
		const char *names[] = {"None", "EnemySpotted", "Help", "RequestReport", "Affirmative", "OnMyWay",
			"PlantingBomb", "WhereIsTheBomb", "EscortingHostages", "HeardNoise", "NoEnemiesLeft", "OneEnemyLeft",
			"TwoEnemiesLeft", "ThreeEnemiesLeft", "NiceShot", "NiceShotSir", "NiceShotCommander", "CommanderDown",
			"KilledFriend", "pain", "AgreeWithPlan", "GoingToPlantBomb", "CoveringFriend", "InCombat", "EnemyDown",
			"Clear", "DefusingBomb", "BombsiteClear", "PlantedBombPlace", "SpottedBomber", "SpottedLooseBomb",
			"TheyPickedUpTheBomb", "GoingToGuardLooseBomb", "GuardingLooseBomb", "GoingToDefendBombsite", "HostagesBeingTaken"};
		const auto i = static_cast<unsigned>(p);
		return i < static_cast<unsigned>(ChatterPhrase::Count) ? names[i] : "Unknown";
	}

private:
	static bool equal(const std::string &a, const char *b)
	{
		if (a.size() != std::strlen(b))
			return false;
		for (std::size_t i = 0; i < a.size(); ++i)
		{
			const char x = a[i] >= 'a' && a[i] <= 'z' ? static_cast<char>(a[i] - 'a' + 'A') : a[i];
			const char y = b[i] >= 'a' && b[i] <= 'z' ? static_cast<char>(b[i] - 'a' + 'A') : b[i];
			if (x != y)
				return false;
		}
		return true;
	}

	static bool next(const char *d, std::size_t n, std::size_t &p, std::string &out)
	{
		constexpr int kMaximumTokenLength = 512;
		out.clear();
		while (p < n)
		{
			if (d[p] == ' ' || d[p] == '\t' || d[p] == '\r' || d[p] == '\n')
			{
				++p;
				continue;
			}
			if (p + 1 < n && d[p] == '/' && d[p + 1] == '/')
			{
				while (p < n && d[p] != '\n')
					++p;
				continue;
			}
			break;
		}
		if (p >= n)
			return false;
		bool closed = false;
		const bool quote = d[p] == '"';
		if (quote)
			++p;
		while (p < n)
		{
			const char c = d[p++];
			if ((quote && c == '"') || (!quote && (c == ' ' || c == '\t' || c == '\r' || c == '\n')))
			{
				closed = true;
				break;
			}
			out.push_back(c);
			if (out.size() > kMaximumTokenLength)
			{
				p = n + 1;
				return false;
			}
		}
		if (quote && !closed)
		{
			p = n + 1;
			return false;
		}
		return quote || !out.empty();
	}

	static bool value(const char *data, std::size_t length, std::size_t &position, std::string &out)
	{
		if (!next(data, length, position, out))
			return false;
		return out != "=" || next(data, length, position, out);
	}

	static ChatterPhrase phraseFor(const std::string &s)
	{
		for (unsigned i = 1; i < static_cast<unsigned>(ChatterPhrase::Count); ++i)
			if (equal(s, name(static_cast<ChatterPhrase>(i))))
				return static_cast<ChatterPhrase>(i);
		return ChatterPhrase::None;
	}

	static RadioMenuChoice eventFor(std::string s)
	{
		constexpr int kEventPrefixLength = 6;
		constexpr int kRadioEventCount = 21;
		constexpr int kFirstMenuEventLimit = 6;
		constexpr int kSecondMenuEventLimit = 12;
		constexpr int kCommandRadioMenu = 1;
		constexpr int kGroupRadioMenu = 2;
		constexpr int kReportRadioMenu = 3;
		constexpr int kSecondMenuSlotOffset = 5;
		constexpr int kThirdMenuSlotOffset = 11;
		if (s.size() > kEventPrefixLength && equal(s.substr(0, kEventPrefixLength), "EVENT_"))
			s = s.substr(kEventPrefixLength);
		const char *events[] = {"RADIO_COVER_ME", "RADIO_YOU_TAKE_THE_POINT", "RADIO_HOLD_THIS_POSITION",
			"RADIO_REGROUP_TEAM", "RADIO_FOLLOW_ME", "RADIO_TAKING_FIRE", "RADIO_GO_GO_GO", "RADIO_TEAM_FALL_BACK",
			"RADIO_STICK_TOGETHER_TEAM", "RADIO_GET_IN_POSITION_AND_WAIT", "RADIO_STORM_THE_FRONT",
			"RADIO_REPORT_IN_TEAM", "RADIO_AFFIRMATIVE", "RADIO_ENEMY_SPOTTED", "RADIO_NEED_BACKUP",
			"RADIO_SECTOR_CLEAR", "RADIO_IN_POSITION", "RADIO_REPORTING_IN", "RADIO_GET_OUT_OF_THERE", "RADIO_NEGATIVE",
			"RADIO_ENEMY_DOWN"};
		for (unsigned i = 0; i < kRadioEventCount; ++i)
		{
			if (equal(s, events[i]))
				return {i < kFirstMenuEventLimit ? kCommandRadioMenu
												 : (i < kSecondMenuEventLimit ? kGroupRadioMenu : kReportRadioMenu),
					static_cast<int>(i < kFirstMenuEventLimit
							? i + 1
							: (i < kSecondMenuEventLimit ? i - kSecondMenuSlotOffset : i - kThirdMenuSlotOffset))};
		}
		return {};
	}

	std::array<RadioMenuChoice, static_cast<unsigned>(ChatterPhrase::Count)> choices_{};

	std::array<bool, static_cast<unsigned>(ChatterPhrase::Count)> important_{};

	std::array<bool, static_cast<unsigned>(ChatterPhrase::Count)> defined_{};

	struct PhraseName
	{
		std::uint32_t id;
		std::string name;
	};
	std::vector<PhraseName> phraseNames_;
	std::vector<ChatterPlaceDefinition> places_;
	bool loaded_ = false;
};
}
}
#endif
