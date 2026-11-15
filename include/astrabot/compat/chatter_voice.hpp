#ifndef ASTRABOT_COMPAT_CHATTER_VOICE_HPP
#define ASTRABOT_COMPAT_CHATTER_VOICE_HPP
#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/compat/random_source.hpp"
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
			const auto phrase = chatter ? phraseFor(token) : ChatterPhrase::None;
			const ChatterPlaceDefinition *definition = nullptr;
			if (chatter)
			{
				if (definitions && !definitions->definedPhraseName(token.c_str()))
					return false;
			}
			else
			{
				definition =
					definitions ? definitions->findPlace(token.c_str()) : candidate.definitions_.place(blockId);
				if (!definition)
					return false;
			}
			PlaceVariants *placeVariants = nullptr;
			if (definition)
			{
				for (auto &group : candidate.places_)
					if (group.occurrence == definition->occurrence && equal(group.name, definition->name.c_str()))
						placeVariants = &group;
				if (!placeVariants)
				{
					candidate.places_.push_back({definition->name, definition->occurrence, {}, 0});
					placeVariants = &candidate.places_.back();
				}
			}
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
				if (!placeVariants && phrase == ChatterPhrase::None)
					continue;
				auto &variants =
					placeVariants ? placeVariants->variants : candidate.variants_[static_cast<unsigned>(phrase)];
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
		const auto i = static_cast<unsigned>(phrase);
		return i < variants_.size() && !variants_[i].empty();
	}
	bool hasPlace(std::uint32_t id, const ChatterRadioCatalog &definitions) const
	{
		const auto *group = findPlace(id, definitions);
		return group && !group->variants.empty();
	}
	std::uint64_t revision() const
	{
		return revision_;
	}
	template <typename Available> void retainAvailable(Available available)
	{
		const auto generation = revision_;
		auto places = places_;
		auto phrases = variants_;
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
		for (auto &group : places)
		{
			if (!filter(group.variants))
				return;
			group.cursor = 0;
		}
		for (auto &variants : phrases)
			if (!filter(variants))
				return;
		places_ = std::move(places);
		variants_ = std::move(phrases);
		cursors_ = {};
		++revision_;
	}
	ChatterSpeakable *select(ChatterPhrase phrase, int count, const char *place)
	{
		return selectAtPlace(phrase, count, resolvePlace(place));
	}
	ChatterSpeakable *selectAtPlace(ChatterPhrase phrase, int count, std::uint32_t placeId)
	{
		const auto i = static_cast<unsigned>(phrase);
		return i < variants_.size() ? selectVariant(variants_[i], cursors_[i], count, placeId) : nullptr;
	}
	ChatterSpeakable *selectPlace(
		std::uint32_t id, const ChatterRadioCatalog &definitions, int count, const char *criteria)
	{
		return selectPlaceAt(id, definitions, count, resolvePlace(criteria));
	}
	ChatterSpeakable *selectPlaceAt(
		std::uint32_t id, const ChatterRadioCatalog &definitions, int count, std::uint32_t criteria)
	{
		auto *group = const_cast<PlaceVariants *>(findPlace(id, definitions));
		return group ? selectVariant(group->variants, group->cursor, count, criteria) : nullptr;
	}
	bool shuffle(ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing)
	{
		// Engine RNG remains an explicit portable approximation of native CRT order.
		const auto generation = revision_;
		auto places = places_;
		auto phrases = variants_;
		const auto shuffleVariants = [&](std::vector<ChatterSpeakable> &variants)
		{
			for (std::size_t i = 1; i < variants.size(); ++i)
			{
				const auto result = random.nextLong(
					RandomRequest::longRequest("chatter.speakable_shuffle", actor, timing, 0, static_cast<int>(i)));
				if (revision_ != generation || result.status != RandomStatus::Ok || result.value < 0 ||
					result.value > static_cast<int>(i))
					return false;
				std::swap(variants[i], variants[static_cast<std::size_t>(result.value)]);
			}
			return true;
		};
		for (auto &group : places)
			if (!shuffleVariants(group.variants))
				return false;
		for (auto &variants : phrases)
			if (!shuffleVariants(variants))
				return false;
		// A same-generation selection callback may advance a live cursor.
		for (std::size_t i = 0; i < places.size(); ++i)
			places[i].cursor = places_[i].cursor;
		places_ = std::move(places);
		variants_ = std::move(phrases);
		++revision_;
		return true;
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
	struct PlaceVariants
	{
		std::string name;
		std::uint32_t occurrence;
		std::vector<ChatterSpeakable> variants;
		std::size_t cursor;
	};
	const PlaceVariants *findPlace(std::uint32_t id, const ChatterRadioCatalog &definitions) const
	{
		const auto *definition = definitions.place(id);
		if (definition)
			for (const auto &group : places_)
				if (group.occurrence == definition->occurrence && equal(group.name, definition->name.c_str()))
					return &group;
		return nullptr;
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
	static ChatterPhrase phraseFor(const std::string &name)
	{
		for (unsigned i = 1; i < static_cast<unsigned>(ChatterPhrase::Count); ++i)
			if (equal(name, ChatterRadioCatalog::name(static_cast<ChatterPhrase>(i))))
				return static_cast<ChatterPhrase>(i);
		return ChatterPhrase::None;
	}
	ChatterRadioCatalog definitions_;
	std::vector<PlaceVariants> places_;
	std::uint64_t revision_ = 0;
	std::array<std::vector<ChatterSpeakable>, static_cast<unsigned>(ChatterPhrase::Count)> variants_{};
	std::array<std::size_t, static_cast<unsigned>(ChatterPhrase::Count)> cursors_{};
};
} // namespace compat
} // namespace astrabot
#endif
