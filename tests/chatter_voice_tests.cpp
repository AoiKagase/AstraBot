#include "astrabot/compat/chatter_voice.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
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
	const RandomActor actor{1, 2};
	const RandomTimingContext timing{1, 0, 1};
	std::vector<RandomTapeEntry> tape;
	for (unsigned i = 0; i < 3; ++i)
		tape.push_back(RandomTapeEntry::longEntry("chatter.speakable_shuffle", actor, timing, 0, 1, 0));
	ScriptedRandomSource random(tape);
	check(shuffled.shuffle(random, actor, timing) && random.verifyComplete(),
		"round shuffle consumes bounded shared random requests");
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
