#ifndef ASTRABOT_TEST_NOISE_RUNTIME_NAV_HPP
#define ASTRABOT_TEST_NOISE_RUNTIME_NAV_HPP
namespace
{
float hearingFixtureRandomFloat(float low, float high)
{
	return low < 0 && high > 0 ? 0.0f : jumpFixtureRandomFloat(low, high);
}
inline bool loadHearingNav(astrabot::metamod::PluginRuntime &runtime, const char *path = "astrabot_hearing_fixture.nav")
{
	std::vector<std::uint8_t> bytes;
	const auto u32 = [&](std::uint32_t v)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<std::uint8_t>(v >> shift));
	};
	const auto f32 = [&](float v)
	{
		std::uint32_t b = 0;
		std::memcpy(&b, &v, sizeof(b));
		u32(b);
	};
	u32(0xFEEDFACEU);
	u32(1);
	u32(1);
	u32(1);
	bytes.push_back(0);
	for (float v : {-32768.0f, -32768.0f, 0.0f, 32768.0f, 32768.0f, 0.0f, 0.0f, 0.0f})
		f32(v);
	for (unsigned i = 0; i < 4; ++i)
		u32(0);
	bytes.push_back(0);
	bytes.push_back(0);
	u32(0);
	auto *file = openFixture(path, "wb");
	if (!file)
		return false;
	const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	const bool closed = std::fclose(file) == 0;
	const astrabot::metamod::NavLoadRequest request = {
		path, "astrabot_hearing_fixture", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const bool loaded =
		written && closed && runtime.loadNavigationFile(&request) == astrabot::metamod::NavLoadResult::Loaded;
	std::remove(path);
	return loaded;
}
inline bool loadInvestigationNav(astrabot::metamod::PluginRuntime &runtime, bool approaches = false)
{
	const char *path = "astrabot_investigation_fixture.nav";
	std::vector<std::uint8_t> bytes;
	const auto u32 = [&](std::uint32_t v)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
			bytes.push_back(static_cast<std::uint8_t>(v >> shift));
	};
	const auto f32 = [&](float v)
	{
		std::uint32_t b = 0;
		std::memcpy(&b, &v, sizeof(b));
		u32(b);
	};
	u32(0xFEEDFACEU);
	u32(1);
	u32(2);
	for (unsigned id = 1; id <= 2; ++id)
	{
		u32(id);
		bytes.push_back(0);
		for (float v :
			{-128.0f, id == 1 ? -128.0f : 256.0f, 0.0f, 128.0f, id == 1 ? 256.0f : 1200.0f, 0.0f, 0.0f, 0.0f})
			f32(v);
		for (unsigned direction = 0; direction < 4; ++direction)
		{
			const bool connected = (id == 1 && direction == 2) || (id == 2 && direction == 0);
			u32(connected ? 1 : 0);
			if (connected)
				u32(id == 1 ? 2 : 1);
		}
		bytes.push_back(0);
		bytes.push_back(approaches ? 1 : 0);
		if (approaches)
		{
			u32(id == 1 ? 2 : 1);
			u32(id);
			bytes.push_back(id == 1 ? 2 : 0);
			u32(0);
			bytes.push_back(0);
		}
		u32(0);
	}
	auto *file = openFixture(path, "wb");
	if (!file)
		return false;
	const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	const bool closed = std::fclose(file) == 0;
	const astrabot::metamod::NavLoadRequest request = {
		path, "astrabot_investigation_fixture", runtime.snapshot().mapGeneration, false, 0, false, 0};
	const bool loaded =
		written && closed && runtime.loadNavigationFile(&request) == astrabot::metamod::NavLoadResult::Loaded;
	std::remove(path);
	return loaded;
}

}
#endif
