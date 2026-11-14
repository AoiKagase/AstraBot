#include "bridge/movement_sound_bridge.hpp"
#include <cstdio>
#include <memory>
#include <cstring>

namespace
{
astrabot::metamod::MovementSoundBridge *bridge = nullptr;
unsigned audioCalls = 0;
unsigned wrapperCalls = 0;
astrabot::metamod::MovementSoundBridge::SoundFunction savedSound = nullptr;
bool argumentsPreserved = true;
bool captureSuspended = true;
bool nestedCaptureAvailable = false;
bool resetActive = false;
playermove_t *nestedMove = nullptr;
edict_t *nestedSource = nullptr;
void original(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	++audioCalls;
	captureSuspended = captureSuspended && !bridge->capture();
	const float expected = .8f;
	std::uint32_t receivedBits = 0, expectedBits = 0;
	std::memcpy(&receivedBits, &attenuation, sizeof(receivedBits));
	std::memcpy(&expectedBits, &expected, sizeof(expectedBits));
	argumentsPreserved = argumentsPreserved && channel == 4 && std::strcmp(sample, "player/pl_step1.wav") == 0 &&
		volume == .5f && receivedBits == expectedBits && flags == 32 && pitch == 95;
}
void thunk(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	bridge->forward(channel, sample, volume, attenuation, flags, pitch);
}
void nestedOriginal(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	captureSuspended = captureSuspended && !bridge->capture();
	bridge->begin(nestedMove, nestedSource, 1, 2, true, &thunk);
	nestedCaptureAvailable = bridge->capture() && bridge->capture()->move == nestedMove;
	nestedMove->PM_PlaySound(channel, sample, volume, attenuation, flags, pitch);
	bridge->end(nestedMove);
	original(channel, sample, volume, attenuation, flags, pitch);
}

void resetOriginal(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	bridge->reset();
	resetActive = bridge->active();
	original(channel, sample, volume, attenuation, flags, pitch);
}

void foreignWrapper(int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
	++wrapperCalls;
	savedSound(channel, sample, volume, attenuation, flags, pitch);
}

void foreign(int, const char *, float, float, int, int)
{
}
}

int main()
{
	using namespace astrabot::metamod;
	int failures = 0;
	const auto check = [&](bool ok, const char *name)
	{
		if (!ok)
		{
			++failures;
			std::fprintf(stderr, "FAIL %s\n", name);
		}
	};
	MovementSoundBridge value;
	bridge = &value;
	std::unique_ptr<playermove_t> a(new playermove_t{});
	std::unique_ptr<playermove_t> b(new playermove_t{});
	edict_t source{};
	source.serialnumber = 0;
	a->PM_PlaySound = &original;
	b->PM_PlaySound = &original;
	check(value.begin(a.get(), &source, 1, 2, true, &thunk), "server scope begins");
	check(a->PM_PlaySound == &thunk && value.active() && value.capture(), "scope owns callback and capture");
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(audioCalls == 1 && argumentsPreserved, "all audio arguments forwarded exactly once, including STOP");
	value.begin(b.get(), &source, 1, 2, true, &thunk);
	check(value.current()->move == b.get(), "different-player nesting selects inner scope");
	value.end(b.get());
	check(b->PM_PlaySound == &original && value.current()->move == a.get(), "inner post restores outer scope");
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(audioCalls == 2, "same-move nesting unwraps own thunk without recursion");
	value.end(a.get());
	check(a->PM_PlaySound == &thunk, "same-move inner post resumes outer thunk");
	value.end(a.get());
	check(!value.active() && a->PM_PlaySound == &original, "outer post restores original callback");
	value.begin(a.get(), &source, 1, 2, false, &thunk);
	check(!value.capture(), "client scope cannot create server observation");
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(audioCalls == 3, "client scope still forwards original audio");
	value.end(a.get());
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	a->PM_PlaySound = &foreign;
	value.end(a.get());
	check(a->PM_PlaySound == &foreign && !value.active(), "foreign callback replacement is not overwritten");
	a->PM_PlaySound = &original;
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	value.reset();
	check(a->PM_PlaySound == &original && !value.capture() && value.active(),
		"reset restores callback but retains logical scope to prevent active unload");
	value.end(a.get());
	check(!value.active(), "reset scope becomes inactive only after paired post");
	for (unsigned i = 0; i < MovementSoundBridge::kMaximumScopes + 2; ++i)
		value.begin(a.get(), &source, 1, 2, true, &thunk);
	check(!value.capture() && a->PM_PlaySound == &original, "overflow cancels capture and restores audio callbacks");
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(audioCalls == 4, "overflow still plays original audio");
	for (unsigned i = 0; i < MovementSoundBridge::kMaximumScopes + 2; ++i)
		value.end(a.get());
	check(!value.active() && a->PM_PlaySound == &original, "all overflow scopes unwind");
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	value.end(b.get());
	check(!value.capture() && a->PM_PlaySound == &original && value.active(),
		"unmatched post cancels owned callback without pretending the real scope ended");
	value.end(a.get());
	check(!value.active(), "real post ends cancelled scope");
	nestedMove = b.get();
	nestedSource = &source;
	a->PM_PlaySound = &nestedOriginal;
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	value.end(a.get());
	check(audioCalls == 6 && nestedCaptureAvailable && captureSuspended,
		"nested movement inside audio forwarding can capture while parent forwarding stays suspended");
	a->PM_PlaySound = &resetOriginal;
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(audioCalls == 7 && resetActive && value.active() && !value.capture(),
		"reset during forwarding retains active lifetime until callback and PM_Move unwind");
	value.end(a.get());
	check(!value.active() && argumentsPreserved, "reset-forwarding counters unwind without underflow");
	a->PM_PlaySound = &original;
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	savedSound = a->PM_PlaySound;
	a->PM_PlaySound = &foreignWrapper;
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	value.begin(a.get(), &source, 1, 2, true, &thunk);
	const auto beforeWrapper = audioCalls;
	a->PM_PlaySound(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(wrapperCalls == 1 && audioCalls == beforeWrapper + 1 && argumentsPreserved && captureSuspended,
		"nested foreign wrapper chains to outer original audio exactly once without recapture");
	value.end(a.get());
	check(a->PM_PlaySound == &thunk, "own nested scope resumes wrapped outer thunk");
	value.end(a.get());
	check(a->PM_PlaySound == &foreignWrapper, "inner post restores foreign wrapper");
	value.end(a.get());
	check(a->PM_PlaySound == &foreignWrapper && !value.active(), "outer post preserves foreign ownership");
	const auto finalCalls = audioCalls;
	thunk(4, "player/pl_step1.wav", .5f, .8f, 32, 95);
	check(audioCalls == finalCalls, "orphaned thunk cannot call a past scope callback after its lifetime ended");
	return failures ? 1 : 0;
}
