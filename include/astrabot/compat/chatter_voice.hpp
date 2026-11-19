#ifndef ASTRABOT_COMPAT_CHATTER_VOICE_HPP
#define ASTRABOT_COMPAT_CHATTER_VOICE_HPP
#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/compat/chatter_voice_random.hpp"
#include "astrabot/compat/chatter_place_names.hpp"
#include <utility>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <limits>
namespace astrabot
{
namespace compat
{
struct ChatterSpeakable
{
	std::string sample;
	std::string place = "ANY";
	std::uint32_t placeId = 0xFFFF;
	int count = -1;
	float duration = -1.0f;
};
// Metadata only. Audio stays in the host's own filesystem.
class ChatterVoiceCatalog
{
public:
	static constexpr std::size_t kMaximumFileBytes = 1024U * 1024U;
	bool loadText(const char *data, std::size_t length, const ChatterRadioCatalog *definitions = nullptr)
	{
		if (!data || length > kMaximumFileBytes)
			return false;
		ChatterVoiceCatalog candidate;
		if (definitions)
			candidate.definitions_ = *definitions;
		else if (!candidate.definitions_.loadText(data, length))
			return false;
		Tokens tokens{data, length};
		std::string token, base;
		std::uint32_t blockId = 0;
		while (tokens.next(token))
		{
			if (equal(token, "BaseDir"))
			{
				if (!tokens.value(base))
					return false;
				continue;
			}
			if (!equal(token, "Chatter") && !equal(token, "Place"))
				return false;
			const bool chatter = equal(token, "Chatter");
			if (!tokens.next(token))
				return false;
			++blockId;
			std::uint32_t id = 0;
			if (chatter)
			{
				const auto *definition = definitions ? definitions->findPhrase(token.c_str())
													: candidate.definitions_.phrase(blockId);
				if (!definition)
					return false;
				id = definition->id;
				if (definition->known != ChatterPhrase::None && candidate.definitions_.phraseId(definition->known) == id)
					candidate.knownIds_[static_cast<unsigned>(definition->known)] = id;
			}
			else
			{
				const auto *definition =
					definitions ? definitions->findPlace(token.c_str()) : candidate.definitions_.place(blockId);
				if (!definition)
					return false;
				id = definition->id;
			}
			(chatter ? candidate.phraseOrder_ : candidate.placeOrder_).push_back(id);
			if (id > candidate.collections_.size())
				candidate.collections_.resize(id);
			auto &variants = candidate.collections_[id - 1].variants;
			const auto beforeId = definitions ? (std::numeric_limits<std::uint32_t>::max)() : blockId;
			std::string place = "ANY";
			std::uint32_t placeId = kAnyPlace;
			int count = -1;
			bool ended = false;
			while (tokens.next(token))
			{
				if (equal(token, "End"))
				{
					ended = true;
					break;
				}
				if (equal(token, "Important"))
					continue;
				if (equal(token, "Radio"))
				{
					if (!tokens.value(token) || !ChatterRadioCatalog::validRadioEvent(token))
						return false;
					continue;
				}
				if (equal(token, "Place"))
				{
					if (!tokens.value(place))
						return false;
					placeId = candidate.resolvePlace(place.c_str(), beforeId);
					continue;
				}
				if (equal(token, "Count"))
				{
					if (!tokens.value(token))
						return false;
					if (equal(token, "Many"))
						count = 4;
					else
					{
						char *end = nullptr;
						const long value = std::strtol(token.c_str(), &end, 10);
						if (!end || *end || end == token.c_str() || value < 0 ||
							value > (std::numeric_limits<int>::max)())
							return false;
						count = static_cast<int>(value);
					}
					continue;
				}
				std::string path = base + token;
				for (auto &c : path)
					if (c == '\\')
						c = '/';
				if (!validPath(path))
					return false;
				if (variants.size() >= 128U)
					return false;
				ChatterSpeakable speakable;
				speakable.sample = path;
				speakable.place = place;
				speakable.placeId = placeId;
				speakable.count = count;
				variants.push_back(std::move(speakable));
			}
			if (!ended)
				return false;
		}
		if (!tokens.valid)
			return false;
		candidate.revision_ = revision_ + 1;
		*this = std::move(candidate);
		return true;
	}
	bool has(ChatterPhrase phrase) const
	{
		const auto *group = findGroup(knownId(phrase));
		return group && !group->variants.empty();
	}
	bool hasPlace(std::uint32_t id, const ChatterRadioCatalog &definitions) const
	{
		const auto *group = definitions.place(id) ? findGroup(id) : nullptr;
		return group && !group->variants.empty();
	}
	std::uint64_t revision() const
	{
		return revision_;
	}
	template <typename Available> void retainAvailable(Available available)
	{
		const auto generation = revision_;
		auto collections = collections_;
		const auto filter = [&](std::vector<ChatterSpeakable> &variants)
		{
			for (auto it = variants.begin(); it != variants.end();)
			{
				const bool keep = available(it->sample);
				if (revision_ != generation)
					return false;
				if (keep)
					++it;
				else
					it = variants.erase(it);
			}
			return true;
		};
		for (auto &group : collections)
		{
			if (!filter(group.variants))
				return;
			group.cursor = 0;
		}
		collections_ = std::move(collections);
		++revision_;
	}
	ChatterSpeakable *select(ChatterPhrase phrase, int count, const char *place)
	{
		return selectAtPlace(phrase, count, resolvePlace(place));
	}
	ChatterSpeakable *selectAtPlace(ChatterPhrase phrase, int count, std::uint32_t placeId)
	{
		return selectGroup(knownId(phrase), count, placeId);
	}
	ChatterSpeakable *selectPlace(
		std::uint32_t id, const ChatterRadioCatalog &definitions, int count, const char *criteria)
	{
		return selectPlaceAt(id, definitions, count, resolvePlace(criteria));
	}
	ChatterSpeakable *selectPlaceAt(
		std::uint32_t id, const ChatterRadioCatalog &definitions, int count, std::uint32_t criteria)
	{
		return definitions.place(id) ? selectGroup(id, count, criteria) : nullptr;
	}
	ChatterSpeakable *selectNamedAt(
		std::uint32_t id, const ChatterRadioCatalog &definitions, int count, std::uint32_t criteria)
	{
		return definitions.phrase(id) ? selectGroup(id, count, criteria) : nullptr;
	}
	bool cacheDuration(const std::string &sample, std::uint64_t revision, float duration)
	{
		if (revision != revision_ || !std::isfinite(duration) || duration <= 0)
			return false;
		bool found = false;
		const auto cache = [&](std::vector<ChatterSpeakable> &variants)
		{
			for (auto &speakable : variants)
				if (speakable.sample == sample)
				{
					speakable.duration = duration;
					found = true;
				}
		};
		for (auto &group : collections_)
			cache(group.variants);
		return found;
	}
	static void shuffleBanks(const std::vector<ChatterVoiceCatalog *> &banks, ChatterVoiceRandom &random)
	{
		// Each successful custom block adds another visit to the same phrase.
		// On each visit CSBot shuffles that phrase in every bank in index order.
		const auto shuffleList = [&](const std::vector<std::uint32_t> ChatterVoiceCatalog::*order)
		{
			for (const auto *source : banks)
				for (const auto id : source->*order)
					for (auto *bank : banks)
						if (id <= bank->collections_.size())
						{
							auto &variants = bank->collections_[id - 1].variants;
							random.shuffle(variants.begin(), variants.end());
						}
		};
		shuffleList(&ChatterVoiceCatalog::placeOrder_);
		shuffleList(&ChatterVoiceCatalog::phraseOrder_);
		for (auto *bank : banks)
			++bank->revision_;
	}

	static bool validPath(const std::string &path)
	{
		if (path.empty() || path.size() >= 128U || path.front() == '/' || path.find(':') != std::string::npos ||
			path.find("..") != std::string::npos)
			return false;
		for (const auto c : path)
			if (static_cast<unsigned char>(c) < 32U || c == '"' || c == ';')
				return false;
		return true;
	}

private:
	static constexpr std::uint32_t kAnyPlace = 0xFFFF;
	struct PhraseVariants
	{
		std::vector<ChatterSpeakable> variants;
		std::size_t cursor = 0;
	};
	std::uint32_t knownId(ChatterPhrase phrase) const
	{
		const auto i = static_cast<unsigned>(phrase);
		return i < knownIds_.size() ? knownIds_[i] : 0U;
	}
	const PhraseVariants *findGroup(std::uint32_t id) const
	{
		return id && id <= collections_.size() ? &collections_[id - 1] : nullptr;
	}
	ChatterSpeakable *selectGroup(std::uint32_t id, int count, std::uint32_t place)
	{
		auto *group = const_cast<PhraseVariants *>(findGroup(id));
		return group ? selectVariant(group->variants, group->cursor, count, place) : nullptr;
	}
	std::uint32_t resolvePlace(
		const char *name, std::uint32_t beforeId = (std::numeric_limits<std::uint32_t>::max)()) const
	{
		if (!name || !*name || equal(name, "UNDEFINED"))
			return 0;
		if (equal(name, "ANY"))
			return kAnyPlace;
		const auto id = definitions_.nameToId(name, beforeId);
		return id || definitions_.hasPlaceBefore(beforeId) ? id : defaultChatterPlaceId(name);
	}
	static ChatterSpeakable *selectVariant(
		std::vector<ChatterSpeakable> &variants, std::size_t &cursor, int count, std::uint32_t place)
	{
		constexpr int kUndefinedCount = 0xFFFF;
		constexpr int kManyCount = 4;
		const int criteria = count < 0 ? kUndefinedCount : count;
		for (std::size_t attempt = 0; attempt < variants.size(); ++attempt)
		{
			auto &variant = variants[cursor];
			cursor = (cursor + 1U) % variants.size();
			if ((variant.placeId == kAnyPlace || variant.placeId == place) &&
				(variant.count < 0 || variant.count == (std::min)(criteria, kManyCount)))
				return &variant;
		}
		return nullptr;
	}
	struct Tokens
	{
		const char *data;
		std::size_t length;
		std::size_t position = 0;
		bool valid = true;
		bool next(std::string &out)
		{
			out.clear();
			while (position < length)
			{
				if (static_cast<unsigned char>(data[position]) <= 32U)
				{
					++position;
					continue;
				}
				if (position + 1 < length && data[position] == '/' && data[position + 1] == '/')
				{
					while (position < length && data[position] != '\n')
						++position;
					continue;
				}
				break;
			}
			if (position >= length)
				return false;
			const bool quote = data[position] == '"';
			if (quote)
				++position;
			while (position < length)
			{
				const char c = data[position++];
				if ((quote && c == '"') || (!quote && static_cast<unsigned char>(c) <= 32U))
					return true;
				out.push_back(c);
				if (out.size() > 512U)
				{
					valid = false;
					return false;
				}
			}
			if (quote)
			{
				valid = false;
				return false;
			}
			return !out.empty();
		}
		bool value(std::string &out)
		{
			if (!next(out))
				return false;
			return out != "=" || next(out);
		}
	};
	static bool equal(const std::string &left, const char *right)
	{
		if (!right || left.size() != std::strlen(right))
			return false;
		for (std::size_t i = 0; i < left.size(); ++i)
		{
			const auto upper = [](char c)
			{
				return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
			};
			if (upper(left[i]) != upper(right[i]))
				return false;
		}
		return true;
	}
	ChatterRadioCatalog definitions_;
	std::vector<PhraseVariants> collections_;
	std::vector<std::uint32_t> placeOrder_, phraseOrder_;
	std::array<std::uint32_t, static_cast<unsigned>(ChatterPhrase::Count)> knownIds_{};
	std::uint64_t revision_ = 0;
};
} // namespace compat
} // namespace astrabot
#endif
