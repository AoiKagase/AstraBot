#ifndef ASTRABOT_COMPAT_CHATTER_VOICE_HPP
#define ASTRABOT_COMPAT_CHATTER_VOICE_HPP
#include "astrabot/compat/outgoing_radio.hpp"
#include "astrabot/compat/random_source.hpp"
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
	int count = -1;
	float duration = -1.0f;
};
// Metadata only. Audio stays in the host's own filesystem.
class ChatterVoiceCatalog
{
public:
	static constexpr std::size_t kMaximumFileBytes = 1024U * 1024U;
	bool loadText(const char *data, std::size_t length)
	{
		if (!data || length > kMaximumFileBytes)
			return false;
		ChatterVoiceCatalog candidate;
		Tokens tokens{data, length};
		std::string token, base;
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
			const auto phrase = chatter ? phraseFor(token) : ChatterPhrase::None;
			std::string place = "ANY";
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
					if (!tokens.value(token))
						return false;
					continue;
				}
				if (equal(token, "Place"))
				{
					if (!tokens.value(place))
						return false;
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
				if (phrase == ChatterPhrase::None)
					continue;
				std::string path = base + token;
				for (auto &c : path)
					if (c == '\\')
						c = '/';
				if (!validPath(path))
					return false;
				auto &variants = candidate.variants_[static_cast<unsigned>(phrase)];
				if (variants.size() >= 128U)
					return false;
				ChatterSpeakable speakable;
				speakable.sample = path;
				speakable.place = place;
				speakable.count = count;
				variants.push_back(speakable);
			}
			if (!ended)
				return false;
		}
		if (!tokens.valid)
			return false;
		*this = std::move(candidate);
		return true;
	}
	bool has(ChatterPhrase phrase) const
	{
		const auto i = static_cast<unsigned>(phrase);
		return i < variants_.size() && !variants_[i].empty();
	}
	template <typename Available> void retainAvailable(Available available)
	{
		for (auto &variants : variants_)
			variants.erase(std::remove_if(variants.begin(), variants.end(),
							   [&](const ChatterSpeakable &speakable)
							   {
								   return !available(speakable.sample);
							   }),
				variants.end());
		cursors_ = {};
	}
	ChatterSpeakable *select(ChatterPhrase phrase, int count, const char *place)
	{
		const auto i = static_cast<unsigned>(phrase);
		if (i >= variants_.size() || variants_[i].empty())
			return nullptr;
		auto &variants = variants_[i];
		for (std::size_t attempt = 0; attempt < variants.size(); ++attempt)
		{
			auto &variant = variants[cursors_[i]];
			cursors_[i] = (cursors_[i] + 1U) % variants.size();
			const bool placeMatches = equal(variant.place, "ANY") ||
				(equal(variant.place, "UNDEFINED") && (!place || !*place)) || (place && equal(variant.place, place));
			if (placeMatches && (variant.count < 0 || variant.count == (std::min)(count, 4)))
				return &variant;
		}
		return nullptr;
	}
	bool shuffle(ICompatibilityRandomSource &random, RandomActor actor, RandomTimingContext timing)
	{
		// Reference shuffles each shared phrase once per round. Engine RNG is an explicit
		// portable ordering approximation of its CRT stream, not reference tape parity.
		auto candidate = variants_;
		for (auto &variants : candidate)
			for (std::size_t i = 1; i < variants.size(); ++i)
			{
				const auto result = random.nextLong(
					RandomRequest::longRequest("chatter.speakable_shuffle", actor, timing, 0, static_cast<int>(i)));
				if (result.status != RandomStatus::Ok || result.value < 0 || result.value > static_cast<int>(i))
					return false;
				std::swap(variants[i], variants[static_cast<std::size_t>(result.value)]);
			}
		variants_ = std::move(candidate);
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
	std::array<std::vector<ChatterSpeakable>, static_cast<unsigned>(ChatterPhrase::Count)> variants_{};
	std::array<std::size_t, static_cast<unsigned>(ChatterPhrase::Count)> cursors_{};
};
} // namespace compat
} // namespace astrabot
#endif
