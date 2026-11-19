#include "astrabot/compat/chatter_voice.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
namespace
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
void nativeShuffle(std::vector<std::string> &clips)
{
	std::random_shuffle(clips.begin(), clips.end());
}
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

bool matchesNativeRoundShuffle()
{
	using namespace astrabot::compat;
	const char base[] = "Chatter Help h0.wav h1.wav h2.wav h3.wav End "
						"Place Arena a0.wav a1.wav a2.wav a3.wav a4.wav End "
						"Chatter Custom c0.wav c1.wav c2.wav End "
						"Chatter Help d0.wav d1.wav d2.wav End "
						"Chatter Affirmative y0.wav y1.wav y2.wav End";
	const char custom[] = "Chatter Affirmative p0.wav p1.wav p2.wav End "
						  "Place Arena q0.wav q1.wav q2.wav End "
						  "Chatter Help r0.wav r1.wav r2.wav End";
	ChatterRadioCatalog definitions;
	ChatterVoiceCatalog primary, profile;
	if (!definitions.loadText(base, sizeof(base) - 1) || !primary.loadText(base, sizeof(base) - 1) ||
		!profile.loadText(custom, sizeof(custom) - 1, &definitions))
		return false;

	struct Collection
	{
		ChatterVoiceCatalog &bank;
		std::uint32_t id;
		std::vector<std::string> clips;
		std::size_t cursor = 0;
	};
	Collection help{primary, 1, {"h0.wav", "h1.wav", "h2.wav", "h3.wav"}};
	Collection arena{primary, 2, {"a0.wav", "a1.wav", "a2.wav", "a3.wav", "a4.wav"}};
	Collection named{primary, 3, {"c0.wav", "c1.wav", "c2.wav"}};
	Collection duplicate{primary, 4, {"d0.wav", "d1.wav", "d2.wav"}};
	Collection affirmative{primary, 5, {"y0.wav", "y1.wav", "y2.wav"}};
	Collection profileArena{profile, 2, {"q0.wav", "q1.wav", "q2.wav"}};
	Collection profileHelp{profile, 1, {"r0.wav", "r1.wav", "r2.wav"}};
	Collection profileAffirmative{profile, 5, {"p0.wav", "p1.wav", "p2.wav"}};
	const std::vector<Collection *> nativePass{
		&arena, &profileArena, &arena, &profileArena,
		&help, &profileHelp, &named, &duplicate, &affirmative, &profileAffirmative,
		&affirmative, &profileAffirmative, &help, &profileHelp};
	const std::vector<Collection *> selected{
		&help, &arena, &named, &duplicate, &affirmative, &profileArena, &profileHelp, &profileAffirmative};
	const auto verify = [&](Collection &entry)
	{
		const auto *clip = entry.id == 2 ? entry.bank.selectPlaceAt(entry.id, definitions, -1, 0)
										: entry.bank.selectNamedAt(entry.id, definitions, -1, 0);
		const bool matches = clip && clip->sample == entry.clips[entry.cursor];
		entry.cursor = (entry.cursor + 1U) % entry.clips.size();
		return matches;
	};
	std::srand(1);
	ChatterVoiceRandom random;
	for (unsigned round = 0; round < 32; ++round)
	{
		for (auto *entry : nativePass)
			nativeShuffle(entry->clips);
		ChatterVoiceCatalog::shuffleBanks({&primary, &profile}, random);
		for (auto *entry : selected)
			if (!verify(*entry) || !verify(*entry))
				return false;
	}
	std::srand(739);
	const int next = std::rand();
	std::srand(739);
	ChatterVoiceCatalog::shuffleBanks({&primary, &profile}, random);
	return std::rand() == next;
}
}
int main(int argc, char **argv)
{
	using namespace astrabot::compat;
	unsigned failures = 0;
	const auto check = [&](bool ok, const char *message)
	{
		if (!ok)
		{
			++failures;
			std::fprintf(stderr, "FAIL %s\n", message);
		}
	};
	ChatterVoiceCatalog voice;
	const char db[] = "BaseDir \"radio/bot/\"\nPlace BombsiteA place.wav End\n"
					  "Chatter EnemySpotted Radio EVENT_RADIO_ENEMY_SPOTTED Count 1 one.wav Count Many many.wav End\n"
					  "Chatter Affirmative Important a.wav b.wav End\n"
					  "Chatter PlantingBomb Place ANY generic.wav Place BombsiteA at_a.wav End\n";
	check(voice.loadText(db, sizeof(db) - 1), "host voice metadata parses without packaged assets");
	auto *one = voice.select(ChatterPhrase::EnemySpotted, 1, "");
	check(one && one->sample == "radio/bot/one.wav", "BaseDir and exact count select the single-enemy clip");
	auto *many = voice.select(ChatterPhrase::EnemySpotted, 9, "");
	check(many && many->sample == "radio/bot/many.wav", "Many saturates the count at four like CSBot");
	check(!voice.select(ChatterPhrase::EnemySpotted, 2, ""), "unmatched count does not invent a phrase");
	auto *first = voice.select(ChatterPhrase::Affirmative, 0, "");
	std::string firstName = first ? first->sample : "";
	auto *second = voice.select(ChatterPhrase::Affirmative, 0, "");
	check(second && second->sample != firstName, "shared phrase cursor avoids immediate repetition");
	auto *third = voice.select(ChatterPhrase::Affirmative, 0, "");
	check(third && third->sample == firstName, "variants cycle after exhaustion");
	auto *generic = voice.select(ChatterPhrase::PlantingBomb, 0, "");
	check(generic && generic->sample == "radio/bot/generic.wav", "unknown location selects only ANY");
	auto *atA = voice.select(ChatterPhrase::PlantingBomb, 0, "BombsiteA");
	check(atA && atA->sample == "radio/bot/at_a.wav", "known named NAV place permits matching variant");
	const char broken[] = "Chatter Help \"unterminated";
	check(!voice.loadText(broken, sizeof(broken) - 1) && voice.has(ChatterPhrase::Affirmative),
		"malformed load preserves the previous bank atomically");
	const char escape[] = "BaseDir ../ Chatter Help stolen.wav End";
	check(!voice.loadText(escape, sizeof(escape) - 1), "a parent-directory audio path is rejected");
	const char noEnd[] = "Chatter Help help.wav";
	check(!voice.loadText(noEnd, sizeof(noEnd) - 1), "missing End does not publish a partial bank");
	ChatterVoiceCatalog shuffled;
	check(shuffled.loadText(db, sizeof(db) - 1), "shuffle fixture loads");
	check(matchesNativeRoundShuffle(),
		"successive multi-bank rounds match native random_shuffle and leave host rand untouched");
	shuffled.retainAvailable(
		[](const std::string &sample)
		{
			return sample.find("a.wav") == std::string::npos;
		});
	const auto *retained = shuffled.select(ChatterPhrase::Affirmative, 0, "");
	check(retained && retained->sample == "radio/bot/b.wav", "missing sample removal retains available alternatives");
	std::string tooMany = "Chatter Help ";
	for (unsigned i = 0; i < 129; ++i)
		tooMany += "help.wav ";
	tooMany += "End";
	check(!voice.loadText(tooMany.data(), tooMany.size()), "per-concept sample capacity is bounded");
	ChatterVoiceCatalog durationBank;
	const char durationText[] = "Chatter Help cached.wav End";
	check(durationBank.loadText(durationText, sizeof(durationText) - 1), "duration cache fixture loads real bank");
	const auto *durationBorrowed = durationBank.select(ChatterPhrase::Help, -1, "");
	const auto immutableClip = durationBorrowed ? *durationBorrowed : ChatterSpeakable{};
	const auto durationRevision = durationBank.revision();
	check(durationBank.cacheDuration("cached.wav", durationRevision, .5f),
		"current bank generation accepts measured sample duration");
	durationBorrowed = durationBank.select(ChatterPhrase::Help, -1, "");
	check(durationBorrowed && durationBorrowed->duration == .5f && immutableClip.duration == -1.0f,
		"real selected duration cache updates without modifying immutable transport copy");
	check(durationBank.loadText(durationText, sizeof(durationText) - 1),
		"replacement bank can reuse exact same asset name");
	check(!durationBank.cacheDuration("cached.wav", durationRevision, 1.0f),
		"old waveform callback cannot update fresh same-named bank");
	durationBorrowed = durationBank.select(ChatterPhrase::Help, -1, "");
	check(durationBorrowed && durationBorrowed->duration < 0,
		"fresh bank duration remains unknown after stale cache callback");
	if (argc > 1)
	{
		std::ifstream file(argv[1], std::ios::binary);
		std::string host((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		check(file.is_open() && voice.loadText(host.data(), host.size()), "installed host database parses");
		for (unsigned i = 1; i < static_cast<unsigned>(ChatterPhrase::Count); ++i)
			check(voice.has(static_cast<ChatterPhrase>(i)), "installed host supplies supported chatter concept");
	}
	return failures ? 1 : 0;
}
