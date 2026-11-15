#include "astrabot/compat/chatter_voice.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>
namespace
{
using namespace astrabot::compat;
bool check(bool ok, const char *message)
{
	if (!ok)
		std::fprintf(stderr, "check failed: %s\n", message);
	return ok;
}
}
int criteriaTests()
{
	using namespace astrabot::compat;
	ChatterVoiceCatalog bank;
	const char text[] = "Place First first.wav End Chatter Help Place Mystery unknown.wav End";
	if (!check(bank.loadText(text, sizeof(text) - 1), "criteria fixture loads with a known prior Place"))
		return 1;
	const auto *unknown = bank.select(ChatterPhrase::Help, -1, "");
	if (!check(unknown && unknown->sample == "unknown.wav",
			"unknown native Place criterion resolves to UNDEFINED at parse time"))
		return 1;
	ChatterVoiceCatalog forward;
	const char ahead[] = "Place First first.wav End Chatter Help Place Later future.wav End Place Later later.wav End";
	if (!check(forward.loadText(ahead, sizeof(ahead) - 1), "forward criterion fixture loads"))
		return 1;
	if (!check(!forward.select(ChatterPhrase::Help, -1, "Later"),
			"a later default Place does not retroactively bind an earlier criterion"))
		return 1;
	ChatterVoiceCatalog shadow;
	const char collision[] =
		"Chatter Arena unused.wav End Chatter Help Place Arena earlier.wav End Place Arena area.wav End";
	if (!check(shadow.loadText(collision, sizeof(collision) - 1), "namespace-shadow fixture loads"))
		return 1;
	if (!check(!shadow.select(ChatterPhrase::Help, -1, "Arena"),
			"earlier Chatter ID criterion cannot alias later same-name Place ID"))
		return 1;
	return 0;
}

namespace
{
struct CallbackRandom final : astrabot::compat::ICompatibilityRandomSource
{
	std::function<void()> hook;
	astrabot::compat::RandomFloatResult nextFloat(const astrabot::compat::RandomRequest &) override
	{
		return {astrabot::compat::RandomStatus::Unavailable, 0};
	}
	astrabot::compat::RandomLongResult nextLong(const astrabot::compat::RandomRequest &) override
	{
		auto callback = hook;
		hook = {};
		if (callback)
			callback();
		return {astrabot::compat::RandomStatus::Ok, 1};
	}
};
}
int metadataTests()
{
	using namespace astrabot::compat;
	const char definitions[] =
		"Chatter Unrepresented unused.wav End Place Arena Important Radio EVENT_RADIO_NEED_BACKUP arena0.wav End Chatter Help help.wav End Place Arena arena1.wav End Place Later later.wav End";
	ChatterRadioCatalog defaults;
	if (!check(defaults.loadText(definitions, sizeof(definitions) - 1), "default mixed metadata loads"))
		return 1;
	const auto *arena = defaults.findPlace("aReNa");
	if (!check(arena && arena->id == 2 && arena->important && arena->choice.menu == 3 && arena->choice.slot == 3,
			"case-insensitive Place lookup preserves source IDs across unrepresented Chatter and default flags"))
		return 1;
	if (!check(defaults.place(4) && defaults.place(4)->occurrence == 1 && !defaults.place(1) &&
				!defaults.findPlace("Help"),
			"Place and Chatter IDs/names remain separate with duplicate default occurrences"))
		return 1;
	ChatterVoiceCatalog base;
	if (!check(base.loadText(definitions, sizeof(definitions) - 1), "default Place speakables load"))
		return 1;
	const auto *first = base.selectPlaceAt(2, defaults, -1, 0);
	const std::string firstSample = first ? first->sample : "";
	const auto *duplicate = base.selectPlaceAt(4, defaults, -1, 0);
	if (!check(firstSample == "arena0.wav" && duplicate && duplicate->sample == "arena1.wav",
			"exact default Place IDs select their distinct occurrence collections"))
		return 1;
	if (!check(!base.selectPlaceAt(1, defaults, -1, 0) && !base.selectPlaceAt(999, defaults, -1, 0),
			"non-Place and missing definition IDs cannot authorize Place audio"))
		return 1;
	ChatterVoiceCatalog profile;
	const char clips[] =
		"BaseDir radio/profile/ Chatter Unrepresented unused.wav End Place Arena Radio EVENT_RADIO_NEGATIVE p0.wav End Place Arena Important p1.wav End";
	if (!check(profile.loadText(clips, sizeof(clips) - 1, &defaults),
			"profile accepts known unrepresented Chatter and repeated known Place"))
		return 1;
	const auto *p0 = profile.selectPlace(2, defaults, -1, "");
	const std::string p0Name = p0 ? p0->sample : "";
	const auto *p1 = profile.selectPlace(2, defaults, -1, "");
	if (!check(p0Name == "radio/profile/p0.wav" && p1 && p1->sample == "radio/profile/p1.wav" &&
				!profile.hasPlace(4, defaults),
			"profile repeated Place names append to the first default occurrence and cycle independently"))
		return 1;
	if (!check(defaults.place(2)->important && defaults.place(2)->choice.slot == 3,
			"profile flags/radio cannot overwrite default metadata"))
		return 1;
	const char invalidPlace[] = "Place Ghost ghost.wav End";
	if (!check(!profile.loadText(invalidPlace, sizeof(invalidPlace) - 1, &defaults) && profile.hasPlace(2, defaults),
			"profile-only Place cannot create a definition or erase a published bank"))
		return 1;
	const char wrongKind[] = "Chatter Arena bogus.wav End";
	if (!check(!profile.loadText(wrongKind, sizeof(wrongKind) - 1, &defaults),
			"Place name does not authorize a same-name Chatter profile definition"))
		return 1;
	const char onlyFixed[] = "Chatter Help fixed.wav End";
	ChatterVoiceCatalog missing;
	if (!check(
			missing.loadText(onlyFixed, sizeof(onlyFixed) - 1, &defaults) && !missing.selectPlace(2, defaults, -1, ""),
			"missing profile Place does not invent a bank0 fallback"))
		return 1;
	const char conditional[] = "Place Arena Place UNDEFINED undefined.wav Place Later later.wav Place ANY any.wav End";
	ChatterVoiceCatalog criteria;
	if (!check(criteria.loadText(conditional, sizeof(conditional) - 1, &defaults),
			"profile Place criteria use complete default definitions"))
		return 1;
	const auto *undefined = criteria.selectPlaceAt(2, defaults, -1, 0);
	const std::string uName = undefined ? undefined->sample : "";
	const auto *named = criteria.selectPlaceAt(2, defaults, -1, 5);
	const std::string nName = named ? named->sample : "";
	const auto *any = criteria.selectPlaceAt(2, defaults, -1, 99);
	if (!check(uName == "undefined.wav" && nName == "later.wav" && any && any->sample == "any.wav",
			"Place samples honor undefined/named/ANY criteria with one shared cursor"))
		return 1;
	const char counted[] = "Place Arena Count 1 one.wav Count Many many.wav End";
	ChatterVoiceCatalog counts;
	if (!check(counts.loadText(counted, sizeof(counted) - 1, &defaults), "profile Place count criteria load"))
		return 1;
	const auto *many = counts.selectPlace(2, defaults, -1, "");
	const std::string mName = many ? many->sample : "";
	const auto *one = counts.selectPlace(2, defaults, 1, "");
	if (!check(mName == "many.wav" && one && one->sample == "one.wav",
			"Place undefined count saturates to Many while explicit count selects its own sample"))
		return 1;
	const char fallback[] = "Chatter Help Place tSpAwN spawn.wav Place Wall wall.wav End";
	ChatterVoiceCatalog legacy;
	if (!check(legacy.loadText(fallback, sizeof(fallback) - 1), "no-Place fallback fixture loads"))
		return 1;
	const auto *spawn = legacy.selectAtPlace(ChatterPhrase::Help, -1, 8);
	const std::string spawnName = spawn ? spawn->sample : "";
	const auto *wall = legacy.selectAtPlace(ChatterPhrase::Help, -1, 104);
	if (!check(spawnName == "spawn.wav" && wall && wall->sample == "wall.wav",
			"native fallback name IDs bind samples only while the Place manager is empty"))
		return 1;
	const char inhibited[] = "Place First f.wav End Chatter Help Place TSpawn unresolved.wav End";
	ChatterVoiceCatalog strict;
	if (!check(strict.loadText(inhibited, sizeof(inhibited) - 1) && !strict.selectAtPlace(ChatterPhrase::Help, -1, 8),
			"a visible Place disables unknown-name fallback"))
		return 1;
	const auto *unresolved = strict.selectAtPlace(ChatterPhrase::Help, -1, 0);
	if (!check(unresolved && unresolved->sample == "unresolved.wav", "unresolved criteria become native UNDEFINED"))
		return 1;
	const char ordered[] = "Chatter Help h0.wav h1.wav End Place Arena a0.wav a1.wav End";
	ChatterRadioCatalog orderDefs;
	orderDefs.loadText(ordered, sizeof(ordered) - 1);
	ChatterVoiceCatalog order;
	order.loadText(ordered, sizeof(ordered) - 1);
	const RandomActor actor{2, 3};
	const RandomTimingContext timing{11, 0, 12};
	std::vector<RandomTapeEntry> tape = {
		RandomTapeEntry::longEntry("chatter.speakable_shuffle", actor, timing, 0, 1, 0),
		RandomTapeEntry::longEntry("chatter.speakable_shuffle", actor, timing, 0, 1, 1)};
	ScriptedRandomSource random(tape);
	if (!check(order.shuffle(random, actor, timing) && random.verifyComplete(),
			"round shuffle consumes Place then Chatter requests with exact type/range/owner"))
		return 1;
	const auto *a = order.selectPlace(2, orderDefs, -1, "");
	const std::string aName = a ? a->sample : "";
	const auto *h = order.select(ChatterPhrase::Help, -1, "");
	if (!check(aName == "a1.wav" && h && h->sample == "h0.wav",
			"Place-before-Chatter shuffle order produces the native collection order"))
		return 1;
	order.retainAvailable(
		[](const std::string &sample)
		{
			return sample != "a0.wav";
		});
	a = order.selectPlace(2, orderDefs, -1, "");
	if (!check(a && a->sample == "a1.wav", "asset filtering removes Place variants and resets its cursor safely"))
		return 1;
	ChatterVoiceCatalog failed;
	failed.loadText(ordered, sizeof(ordered) - 1);
	ScriptedRandomSource shortTape({tape.front()});
	if (!check(!failed.shuffle(shortTape, actor, timing), "exhausted shuffle does not publish a partial bank"))
		return 1;
	a = failed.selectPlace(2, orderDefs, -1, "");
	if (!check(a && a->sample == "a0.wav", "failed shuffle preserves original Place variants"))
		return 1;
	const char fresh[] = "Place Fresh fresh.wav End";
	ChatterRadioCatalog freshDefs;
	freshDefs.loadText(fresh, sizeof(fresh) - 1);
	ChatterVoiceCatalog filterReload;
	filterReload.loadText(ordered, sizeof(ordered) - 1);
	bool replaced = false;
	filterReload.retainAvailable(
		[&](const std::string &)
		{
			if (!replaced)
				replaced = filterReload.loadText(fresh, sizeof(fresh) - 1);
			return false;
		});
	const auto *freshClip = filterReload.selectPlace(1, freshDefs, -1, "");
	if (!check(replaced && freshClip && freshClip->sample == "fresh.wav",
			"reentrant availability reload survives old filtering completion"))
		return 1;
	ChatterVoiceCatalog shuffleReload;
	shuffleReload.loadText(ordered, sizeof(ordered) - 1);
	CallbackRandom mutating;
	mutating.hook = [&]()
	{
		shuffleReload.loadText(fresh, sizeof(fresh) - 1);
	};
	if (!check(
			!shuffleReload.shuffle(mutating, actor, timing), "reentrant bank replacement cancels stale shuffle commit"))
		return 1;
	freshClip = shuffleReload.selectPlace(1, freshDefs, -1, "");
	if (!check(freshClip && freshClip->sample == "fresh.wav", "fresh Place survives old RNG callback completion"))
		return 1;
	ChatterVoiceCatalog cursor;
	cursor.loadText(ordered, sizeof(ordered) - 1);
	CallbackRandom advancing;
	advancing.hook = [&]()
	{
		(void)cursor.selectPlace(2, orderDefs, -1, "");
	};
	if (!check(
			cursor.shuffle(advancing, actor, timing), "same-bank cursor advance does not replace metadata generation"))
		return 1;
	a = cursor.selectPlace(2, orderDefs, -1, "");
	if (!check(a && a->sample == "a1.wav", "shuffle preserves cursor advances performed by a selected callback"))
		return 1;
	const char badPath[] = "Place Arena ../bad.wav End";
	if (!check(!base.loadText(badPath, sizeof(badPath) - 1) && base.hasPlace(2, defaults),
			"invalid Place asset path cannot replace a published bank"))
		return 1;
	const char badProfileRadio[] = "Place Arena Radio EVENT_INVALID bad.wav End";
	if (!check(!profile.loadText(badProfileRadio, sizeof(badProfileRadio) - 1, &defaults),
			"profile radio syntax is validated even though flags remain default-owned"))
		return 1;
	const char broken[] = "Place Broken Important";
	if (!check(!defaults.loadText(broken, sizeof(broken) - 1) && defaults.findPlace("Arena"),
			"malformed default Place load preserves published metadata"))
		return 1;
	return 0;
}

int parserTests()
{
	using namespace astrabot::compat;
	unsigned failures = 0;
	const auto verify = [&](bool value, const char *message)
	{
		if (!check(value, message))
			++failures;
	};
	ChatterRadioCatalog catalog;
	const char important[] = "Place Important p.wav End Chatter Help Place Important h.wav End";
	verify(catalog.loadText(important, sizeof(important) - 1) && !catalog.important(ChatterPhrase::Help),
		"Place criterion named Important does not make Help important");
	const char radio[] = "Place Radio p.wav End Chatter Help Place Radio h.wav End";
	verify(catalog.loadText(radio, sizeof(radio) - 1) && catalog.defined(ChatterPhrase::Help),
		"Place criterion named Radio does not reinterpret a sample as a radio event");
	const char end[] = "Place End p.wav End Chatter Help Place End h.wav Important End";
	verify(catalog.loadText(end, sizeof(end) - 1) && catalog.important(ChatterPhrase::Help),
		"Place criterion named End does not terminate the definition before its Important attribute");
	const char emptyBase[] = "BaseDir \"\" Place Arena p.wav End Chatter Help h.wav End";
	verify(catalog.loadText(emptyBase, sizeof(emptyBase) - 1) && catalog.findPlace("Arena") &&
			catalog.defined(ChatterPhrase::Help),
		"empty quoted BaseDir does not end metadata parsing");
	const char keywordBase[] = "BaseDir Place Chatter Help h.wav End";
	verify(catalog.loadText(keywordBase, sizeof(keywordBase) - 1) && !catalog.findPlace("Chatter") &&
			catalog.defined(ChatterPhrase::Help),
		"BaseDir operand named Place does not create a definition");
	ChatterVoiceCatalog bank;
	verify(bank.loadText(emptyBase, sizeof(emptyBase) - 1) && bank.select(ChatterPhrase::Help, -1, ""),
		"real voice loader retains fixed samples with an empty BaseDir");
	return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && std::strcmp(argv[1], "--parser") == 0)
		return parserTests();
	if (argc > 1 && std::strcmp(argv[1], "--criteria") == 0)
		return criteriaTests();
	using namespace astrabot::compat;
	const char text[] = "BaseDir radio/test/ Place Arena arena.wav End Chatter Help help.wav End";
	ChatterVoiceCatalog bank;
	if (!check(bank.loadText(text, sizeof(text) - 1), "native Place and fixed phrase fixture loads"))
		return 1;
	const auto *help = bank.select(ChatterPhrase::Help, -1, "");
	if (!check(help && help->sample == "radio/test/help.wav", "fixed Help is a real loader/selection control"))
		return 1;
	std::vector<std::string> candidates;
	bank.retainAvailable(
		[&](const std::string &sample)
		{
			candidates.push_back(sample);
			return true;
		});
	if (!check(std::find(candidates.begin(), candidates.end(), "radio/test/help.wav") != candidates.end(),
			"existing fixed sample reaches the availability boundary"))
		return 1;
	if (!check(std::find(candidates.begin(), candidates.end(), "radio/test/arena.wav") != candidates.end(),
			"native Place sample must also reach the availability boundary instead of being dropped"))
		return 1;
	return metadataTests();
}
