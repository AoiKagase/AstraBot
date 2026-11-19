#ifndef ASTRABOT_TESTS_RUNTIME_CHARACTERIZATION_HPP
#define ASTRABOT_TESTS_RUNTIME_CHARACTERIZATION_HPP

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace astrabot_test
{
// Test-only, opt-in recording of observable callbacks. This is a before/after
// characterization of AstraBot, not a measured CSBot compatibility oracle.
class RuntimeCharacterization
{
public:
	RuntimeCharacterization()
	{
#ifdef _WIN32
		char *path = nullptr;
		std::size_t length = 0U;
		if (_dupenv_s(&path, &length, "ASTRABOT_CHARACTERIZATION_PATH") == 0 && path != nullptr)
		{
			path_ = path;
			std::free(path);
		}
#else
		const char *path = std::getenv("ASTRABOT_CHARACTERIZATION_PATH");
		if (path != nullptr)
		{
			path_ = path;
		}
#endif
	}

	~RuntimeCharacterization()
	{
		if (path_.empty())
		{
			return;
		}
		std::FILE *file = nullptr;
#ifdef _WIN32
		(void)fopen_s(&file, path_.c_str(), "wb");
#else
		file = std::fopen(path_.c_str(), "wb");
#endif
		if (file == nullptr)
		{
			std::abort();
		}
		for (const std::string &line : lines_)
		{
			if (std::fwrite(line.data(), 1U, line.size(), file) != line.size() || std::fputc('\n', file) == EOF)
			{
				std::abort();
			}
		}
		if (std::fclose(file) != 0)
		{
			std::abort();
		}
	}

	void record(const char *format, ...)
	{
		if (path_.empty())
		{
			return;
		}
		char line[4096] = {};
		va_list args;
		va_start(args, format);
#ifdef _WIN32
		(void)vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);
#else
		(void)std::vsnprintf(line, sizeof(line), format, args);
#endif
		va_end(args);
		lines_.emplace_back(line);
	}

	bool enabled() const
	{
		return !path_.empty();
	}

private:
	std::string path_;
	std::vector<std::string> lines_;
};

inline RuntimeCharacterization &runtimeCharacterization()
{
	static RuntimeCharacterization recorder;
	return recorder;
}

template <typename Integer> Integer recordRandomLong(Integer low, Integer high, Integer value)
{
	runtimeCharacterization().record(
		"rng long %ld %ld %ld", static_cast<long>(low), static_cast<long>(high), static_cast<long>(value));
	return value;
}

inline float recordRandomFloat(float low, float high, float value)
{
	runtimeCharacterization().record(
		"rng float %a %a %a", static_cast<double>(low), static_cast<double>(high), static_cast<double>(value));
	return value;
}
}

#endif
