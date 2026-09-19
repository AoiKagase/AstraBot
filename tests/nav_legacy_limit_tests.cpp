#include "astrabot/nav/legacy_nav_reader.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
class Bytes
{
public:
	void appendU8(std::uint8_t value)
	{
		bytes.push_back(value);
	}

	void appendU16(std::uint16_t value)
	{
		bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
		bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
	}

	void appendU32(std::uint32_t value)
	{
		for (std::size_t index = 0U; index < 4U; ++index)
		{
			bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
		}
	}

	void appendFloat(float value)
	{
		std::uint32_t bits = 0U;
		std::memcpy(&bits, &value, sizeof(bits));
		appendU32(bits);
	}

	std::vector<std::uint8_t> bytes;
};

bool check(bool condition, const char *description)
{
	if (condition)
	{
		return true;
	}

	std::fprintf(stderr, "check failed: %s\n", description);
	return false;
}

std::vector<std::uint8_t> makeNavWithManyEncounters()
{
	Bytes output;
	output.appendU32(astrabot::nav::LegacyNavReader::kMagicNumber);
	output.appendU32(5U);
	output.appendU32(2057288U);
	output.appendU16(0U);
	output.appendU32(1U);
	output.appendU32(1U);
	output.appendU8(0U);
	const float extentAndHeights[] = {0.0f, 0.0f, 0.0f, 64.0f,
			64.0f, 0.0f, 0.0f, 0.0f};
	for (std::size_t index = 0U; index < 8U; ++index)
	{
		output.appendFloat(extentAndHeights[index]);
	}
	for (std::size_t direction = 0U; direction < 4U; ++direction)
	{
		output.appendU32(0U);
	}
	output.appendU8(0U);
	output.appendU8(0U);
	output.appendU32(256U);
	for (std::size_t index = 0U; index < 256U; ++index)
	{
		output.appendU32(1U);
		output.appendU8(0U);
		output.appendU32(1U);
		output.appendU8(0U);
		output.appendU8(0U);
	}
	output.appendU16(0U);
	return output.bytes;
}
}

int main()
{
	const std::vector<std::uint8_t> bytes = makeNavWithManyEncounters();
	astrabot::nav::NavDocument document;
	astrabot::nav::LegacyNavReader reader;
	if (!check(reader.read(bytes.data(), bytes.size(), &document) ==
			astrabot::nav::NavReadResult::Loaded,
			"legacy reader accepts encounter count above one-byte limit"))
	{
		return 1;
	}
	if (!check(document.areaCount() == 1U &&
				document.areas()[0].encounters.size() == 256U,
			"encounter paths are retained"))
	{
		return 1;
	}
	return 0;
}
