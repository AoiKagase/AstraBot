#include "astrabot/metamod/profile_loader.hpp"

#include <array>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace astrabot
{
namespace metamod
{
namespace
{
constexpr int kReactionTimeFieldMask = 1;
constexpr int kAttackDelayFieldMask = 2;
constexpr int kTeamworkFieldMask = 4;
constexpr int kCostFieldMask = 8;
constexpr int kVoicePitchFieldMask = 16;
constexpr int kWeaponPreferenceFieldMask = 32;
constexpr int kVoiceBankFieldMask = 64;
constexpr unsigned kMaximumCsbotTemplates = 64U;
constexpr unsigned kMaximumCsbotTemplateNames = 8U;
constexpr int kDefaultDifficulty = 2;
constexpr int kDefaultSkill = 50;
constexpr int kDefaultAggression = 50;
enum class TokenType
{
	End,
	Word,
	Quoted,
	LeftBrace,
	RightBrace,
	Equals,
	Invalid
};

struct Token
{
	TokenType type;
	char text[ProfileLoader::kMaximumTokenLength + 1U];
};

class Tokenizer
{
public:
	Tokenizer(const char *data, std::size_t size) : data_(data), size_(size), position_(0U)
	{
	}

	Token next()
	{
		Token token = {};
		skipIgnored();
		if (position_ >= size_)
		{
			token.type = TokenType::End;
			return token;
		}

		const char value = data_[position_];
		if (value == '{')
		{
			++position_;
			token.type = TokenType::LeftBrace;
			return token;
		}
		if (value == '}')
		{
			++position_;
			token.type = TokenType::RightBrace;
			return token;
		}
		if (value == '=')
		{
			++position_;
			token.type = TokenType::Equals;
			return token;
		}
		if (value == '"')
		{
			return readQuoted();
		}
		return readWord();
	}

private:
	void skipIgnored()
	{
		bool skipped = true;
		while (skipped)
		{
			skipped = false;
			while (position_ < size_ && std::isspace(static_cast<unsigned char>(data_[position_])) != 0)
			{
				++position_;
				skipped = true;
			}
			if (position_ + 1U < size_ && data_[position_] == '/' && data_[position_ + 1U] == '/')
			{
				position_ += 2U;
				while (position_ < size_ && data_[position_] != '\n')
				{
					++position_;
				}
				skipped = true;
			}
			else if (position_ < size_ && data_[position_] == '#')
			{
				while (position_ < size_ && data_[position_] != '\n')
				{
					++position_;
				}
				skipped = true;
			}
			else if (position_ + 1U < size_ && data_[position_] == '/' && data_[position_ + 1U] == '*')
			{
				position_ += 2U;
				while (position_ + 1U < size_ && !(data_[position_] == '*' && data_[position_ + 1U] == '/'))
				{
					++position_;
				}
				if (position_ + 1U < size_)
				{
					position_ += 2U;
				}
				skipped = true;
			}
		}
	}

	Token readQuoted()
	{
		Token token = {};
		token.type = TokenType::Quoted;
		++position_;
		std::size_t length = 0U;
		while (position_ < size_)
		{
			const char value = data_[position_++];
			if (value == '"')
			{
				token.text[length] = '\0';
				return token;
			}
			if (value == '\\' && position_ < size_)
			{
				const char escaped = data_[position_++];
				if (escaped == '"' || escaped == '\\')
				{
					if (length >= ProfileLoader::kMaximumTokenLength)
					{
						token.type = TokenType::Invalid;
						return token;
					}
					token.text[length] = escaped;
					++length;
					continue;
				}
				if (length >= ProfileLoader::kMaximumTokenLength - 1U)
				{
					token.type = TokenType::Invalid;
					return token;
				}
				token.text[length] = '\\';
				++length;
				token.text[length] = escaped;
				++length;
				continue;
			}
			if (value == '\n' || value == '\r' || length >= ProfileLoader::kMaximumTokenLength)
			{
				token.type = TokenType::Invalid;
				return token;
			}
			token.text[length] = value;
			++length;
		}
		token.type = TokenType::Invalid;
		return token;
	}

	Token readWord()
	{
		Token token = {};
		token.type = TokenType::Word;
		std::size_t length = 0U;
		while (position_ < size_ && std::isspace(static_cast<unsigned char>(data_[position_])) == 0 &&
			data_[position_] != '{' && data_[position_] != '}' && data_[position_] != '=')
		{
			if (length >= ProfileLoader::kMaximumTokenLength)
			{
				token.type = TokenType::Invalid;
				while (position_ < size_ && std::isspace(static_cast<unsigned char>(data_[position_])) == 0 &&
					data_[position_] != '{' && data_[position_] != '}' && data_[position_] != '=')
				{
					++position_;
				}
				return token;
			}
			token.text[length] = data_[position_];
			++length;
			++position_;
		}
		token.text[length] = '\0';
		return token;
	}

	const char *data_;
	std::size_t size_;
	std::size_t position_;
};

bool equalsIgnoreCase(const char *left, const char *right)
{
	if (left == nullptr || right == nullptr)
	{
		return false;
	}
	std::size_t index = 0U;
	while (left[index] != '\0' && right[index] != '\0')
	{
		if (std::tolower(static_cast<unsigned char>(left[index])) !=
			std::tolower(static_cast<unsigned char>(right[index])))
		{
			return false;
		}
		++index;
	}
	return left[index] == '\0' && right[index] == '\0';
}

std::FILE *openFile(const char *path, const char *mode)
{
	std::FILE *file = nullptr;
#ifdef _WIN32
	if (fopen_s(&file, path, mode) != 0)
	{
		return nullptr;
	}
#else
	file = std::fopen(path, mode);
#endif
	return file;
}

bool copyName(const Token &token, char *name)
{
	if ((token.type != TokenType::Word && token.type != TokenType::Quoted) || name == nullptr ||
		token.text[0] == '\0' || std::strlen(token.text) > compat::ProfileRecord::kNameCapacity)
	{
		return false;
	}
	const std::size_t length = std::strlen(token.text);
	for (std::size_t index = 0U; index <= length; ++index)
	{
		name[index] = token.text[index];
	}
	return true;
}

bool parseInteger(const Token &token, int minimum, int maximum, int *value)
{
	constexpr int kDecimalRadix = 10;
	if (value == nullptr || (token.type != TokenType::Word && token.type != TokenType::Quoted) || token.text[0] == '\0')
	{
		return false;
	}
	errno = 0;
	char *end = nullptr;
	const long parsed = std::strtol(token.text, &end, kDecimalRadix);
	if (errno != 0 || end == token.text || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX || parsed < minimum ||
		parsed > maximum)
	{
		return false;
	}
	*value = static_cast<int>(parsed);
	return true;
}

int difficultyFromSkill(int skill)
{
	constexpr int kLowestSkillUpperBound = 20;
	constexpr int kLowerSkillUpperBound = 40;
	constexpr int kMiddleSkillUpperBound = 60;
	constexpr int kHigherSkillUpperBound = 80;
	constexpr int kLowestSkillDifficulty = 0;
	constexpr int kLowerSkillDifficulty = 1;
	constexpr int kMiddleSkillDifficulty = 2;
	constexpr int kHigherSkillDifficulty = 3;
	constexpr int kHighestSkillDifficulty = 4;
	if (skill < kLowestSkillUpperBound)
	{
		return kLowestSkillDifficulty;
	}
	if (skill < kLowerSkillUpperBound)
	{
		return kLowerSkillDifficulty;
	}
	if (skill < kMiddleSkillUpperBound)
	{
		return kMiddleSkillDifficulty;
	}
	if (skill < kHigherSkillUpperBound)
	{
		return kHigherSkillDifficulty;
	}
	return kHighestSkillDifficulty;
}

bool parseTeam(const Token &token, compat::ProfileTeam *team)
{
	if (team == nullptr || (token.type != TokenType::Word && token.type != TokenType::Quoted))
	{
		return false;
	}
	if (equalsIgnoreCase(token.text, "any"))
	{
		*team = compat::ProfileTeam::Any;
		return true;
	}
	if (equalsIgnoreCase(token.text, "t") || equalsIgnoreCase(token.text, "terrorist"))
	{
		*team = compat::ProfileTeam::Terrorist;
		return true;
	}
	if (equalsIgnoreCase(token.text, "ct") || equalsIgnoreCase(token.text, "counterterrorist") ||
		equalsIgnoreCase(token.text, "counter-terrorist"))
	{
		*team = compat::ProfileTeam::CounterTerrorist;
		return true;
	}
	return false;
}

bool parseExtendedField(const Token &key, const Token &value, compat::ProfileRecord *profile, unsigned *mask)
{
	constexpr int kMaximumTeamwork = 100;
	constexpr int kMaximumVoicePitch = 255;
	unsigned field = 0;
	if (equalsIgnoreCase(key.text, "ReactionTime"))
		field = kReactionTimeFieldMask;
	else if (equalsIgnoreCase(key.text, "AttackDelay"))
		field = kAttackDelayFieldMask;
	else if (equalsIgnoreCase(key.text, "Teamwork"))
		field = kTeamworkFieldMask;
	else if (equalsIgnoreCase(key.text, "Cost"))
		field = kCostFieldMask;
	else if (equalsIgnoreCase(key.text, "VoicePitch"))
		field = kVoicePitchFieldMask;
	else if (equalsIgnoreCase(key.text, "VoiceBank"))
		field = kVoiceBankFieldMask;
	else if (equalsIgnoreCase(key.text, "WeaponPreference"))
		field = kWeaponPreferenceFieldMask;
	if (field == 0)
		return true;
	if (field == kVoiceBankFieldMask)
	{
		const auto length = std::strlen(value.text);
		if (length == 0 || length >= sizeof(profile->voiceBank))
			return false;
		std::memcpy(profile->voiceBank, value.text, length + 1U);
	}
	else if (field == kReactionTimeFieldMask || field == kAttackDelayFieldMask)
	{
		errno = 0;
		char *end = nullptr;
		const float parsed = std::strtof(value.text, &end);
		if (errno || end == value.text || *end != '\0' || !std::isfinite(parsed) || parsed < 0)
			return false;
		if (field == kReactionTimeFieldMask)
			profile->reactionTimeSeconds = parsed;
		else
			profile->attackDelaySeconds = parsed;
	}
	else if (field == kTeamworkFieldMask || field == kCostFieldMask || field == kVoicePitchFieldMask)
	{
		int parsed = 0;
		if (!parseInteger(value, 0,
				field == kTeamworkFieldMask ? kMaximumTeamwork
											: (field == kVoicePitchFieldMask ? kMaximumVoicePitch : INT_MAX),
				&parsed))
			return false;
		if (field == kTeamworkFieldMask)
			profile->teamwork = parsed;
		else if (field == kCostFieldMask)
			profile->cost = parsed;
		else
			profile->voicePitch = parsed;
	}
	else
	{
		struct Alias
		{
			const char *name;
			int id;
		};
		const Alias aliases[] = {{"p228", 1}, {"scout", 3}, {"hegrenade", 4}, {"xm1014", 5}, {"c4", 6}, {"mac10", 7},
			{"aug", 8}, {"smokegrenade", 9}, {"elite", 10}, {"elites", 10}, {"fiveseven", 11}, {"fn57", 11},
			{"ump45", 12}, {"sg550", 13}, {"galil", 14}, {"famas", 15}, {"usp", 16}, {"glock18", 17}, {"glock", 17},
			{"awp", 18}, {"mp5navy", 19}, {"mp5", 19}, {"m249", 20}, {"m3", 21}, {"m4a1", 22}, {"tmp", 23},
			{"g3sg1", 24}, {"flashbang", 25}, {"deagle", 26}, {"sg552", 27}, {"ak47", 28}, {"knife", 29}, {"p90", 30},
			{"shield", 99}};
		int id = 0;
		for (const auto &alias : aliases)
			if (equalsIgnoreCase(value.text, alias.name))
			{
				id = alias.id;
				break;
			}
		if (id == 0 && !equalsIgnoreCase(value.text, "none"))
			return false;
		if ((*mask & field) == 0 || id == 0)
			profile->weaponPreferenceCount = 0;
		if (id != 0)
		{
			if (profile->weaponPreferenceCount >= compat::ProfileRecord::kWeaponPreferenceCapacity)
				return false;
			profile->weaponPreferences[profile->weaponPreferenceCount++] = id;
		}
	}
	*mask |= field;
	return true;
}

bool parseProfileBlock(Tokenizer *tokenizer, compat::ProfileRecord *profile)
{
	if (tokenizer == nullptr || profile == nullptr)
	{
		return false;
	}
	bool hasDifficulty = false;
	unsigned extendedMask = 0;
	profile->team = compat::ProfileTeam::Any;
	profile->difficulty = kDefaultDifficulty;
	profile->skill = kDefaultSkill;
	profile->aggression = kDefaultAggression;
	while (true)
	{
		const Token key = tokenizer->next();
		if (key.type == TokenType::RightBrace)
		{
			if (!hasDifficulty)
			{
				profile->difficulty = difficultyFromSkill(profile->skill);
			}
			return true;
		}
		if (key.type != TokenType::Word)
		{
			return false;
		}
		Token value = tokenizer->next();
		if (value.type == TokenType::Equals)
		{
			value = tokenizer->next();
		}
		if (value.type != TokenType::Word && value.type != TokenType::Quoted)
		{
			return false;
		}
		if (equalsIgnoreCase(key.text, "Name"))
		{
			if (!copyName(value, profile->name))
			{
				return false;
			}
		}
		else if (equalsIgnoreCase(key.text, "Team"))
		{
			if (!parseTeam(value, &profile->team))
			{
				return false;
			}
		}
		else if (equalsIgnoreCase(key.text, "Difficulty"))
		{
			if (!parseInteger(value, compat::ProfileCatalog::kMinimumDifficulty,
					compat::ProfileCatalog::kMaximumDifficulty, &profile->difficulty))
			{
				return false;
			}
			hasDifficulty = true;
		}
		else if (equalsIgnoreCase(key.text, "Skill"))
		{
			if (!parseInteger(value, compat::ProfileCatalog::kMinimumAttribute,
					compat::ProfileCatalog::kMaximumAttribute, &profile->skill))
			{
				return false;
			}
		}
		else if (equalsIgnoreCase(key.text, "Aggression"))
		{
			if (!parseInteger(value, compat::ProfileCatalog::kMinimumAttribute,
					compat::ProfileCatalog::kMaximumAttribute, &profile->aggression))
			{
				return false;
			}
		}
		else if (!parseExtendedField(key, value, profile, &extendedMask))
			return false;
	}
}

ProfileLoadResult mapAddResult(compat::ProfileAddResult result)
{
	if (result == compat::ProfileAddResult::DuplicateName)
	{
		return ProfileLoadResult::DuplicateProfile;
	}
	if (result == compat::ProfileAddResult::Full)
	{
		return ProfileLoadResult::CatalogFull;
	}
	return ProfileLoadResult::Malformed;
}
} // namespace

struct CsbotValues
{
	int difficulty;
	int skill;
	int aggression;
	compat::ProfileTeam team;
	bool hasDifficulty;
	bool hasSkill;
	bool hasAggression;
	bool hasTeam;
	unsigned difficultyMask;
	compat::ProfileRecord extended;
	unsigned extendedMask;
};

struct CsbotTemplate
{
	char name[ProfileLoader::kMaximumTokenLength + 1U];
	CsbotValues values;
};

class LineReader
{
public:
	LineReader(const char *data, std::size_t size) : data_(data), size_(size), position_(0U), valid_(true)
	{
	}

	bool next(char *line, std::size_t capacity)
	{
		if (line == nullptr || capacity == 0U || position_ >= size_)
		{
			return false;
		}

		std::size_t length = 0U;
		while (position_ < size_ && data_[position_] != '\n')
		{
			if (length + 1U >= capacity)
			{
				valid_ = false;
			}
			else
			{
				line[length++] = data_[position_];
			}
			++position_;
		}
		if (position_ < size_ && data_[position_] == '\n')
		{
			++position_;
		}
		if (!valid_)
		{
			return false;
		}

		line[length] = '\0';
		trim(line);
		return true;
	}

	bool isValid() const
	{
		return valid_;
	}

private:
	static void trim(char *line)
	{
		std::size_t begin = 0U;
		while (line[begin] != '\0' && std::isspace(static_cast<unsigned char>(line[begin])) != 0)
		{
			++begin;
		}
		if (begin != 0U)
		{
			const std::size_t length = std::strlen(line + begin);
			std::memmove(line, line + begin, length + 1U);
		}

		char *comment = std::strstr(line, "//");
		if (comment != nullptr)
		{
			*comment = '\0';
		}

		std::size_t length = std::strlen(line);
		while (length > 0U && std::isspace(static_cast<unsigned char>(line[length - 1U])) != 0)
		{
			--length;
		}
		line[length] = '\0';
	}

	const char *data_;
	std::size_t size_;
	std::size_t position_;
	bool valid_;
};

CsbotValues makeCsbotValues()
{
	return {kDefaultDifficulty, kDefaultSkill, kDefaultAggression, compat::ProfileTeam::Any, false, false, false, false,
		0, {}, 0};
}

bool parseCsbotField(const char *line, Token *key, Token *value)
{
	if (line == nullptr || key == nullptr || value == nullptr)
	{
		return false;
	}
	Tokenizer tokenizer(line, std::strlen(line));
	*key = tokenizer.next();
	if (key->type != TokenType::Word)
	{
		return false;
	}
	*value = tokenizer.next();
	if (value->type == TokenType::Equals)
	{
		*value = tokenizer.next();
	}
	if (value->type != TokenType::Word && value->type != TokenType::Quoted)
	{
		return false;
	}
	return tokenizer.next().type == TokenType::End;
}

bool parseCsbotDifficulty(const Token &token, int *difficulty, unsigned *mask)
{
	constexpr int kEasyProfileDifficulty = 0;
	constexpr int kNormalProfileDifficulty = 2;
	constexpr int kHardProfileDifficulty = 3;
	constexpr int kExpertProfileDifficulty = 4;
	constexpr int kEasyDifficultyBit = 0;
	constexpr int kNormalDifficultyBit = 1;
	constexpr int kHardDifficultyBit = 2;
	constexpr int kExpertDifficultyBit = 3;
	if (difficulty == nullptr || (token.type != TokenType::Word && token.type != TokenType::Quoted))
	{
		return false;
	}
	if (parseInteger(
			token, compat::ProfileCatalog::kMinimumDifficulty, compat::ProfileCatalog::kMaximumDifficulty, difficulty))
	{
		*mask = 1U << *difficulty;
		return true;
	}

	int strongestDifficulty = -1;
	*mask = 0;
	std::size_t begin = 0U;
	const std::size_t length = std::strlen(token.text);
	for (std::size_t index = 0U; index <= length; ++index)
	{
		if (token.text[index] != '+' && token.text[index] != '\0')
		{
			continue;
		}
		if (index == begin || index - begin > ProfileLoader::kMaximumTokenLength)
		{
			return false;
		}
		char part[ProfileLoader::kMaximumTokenLength + 1U] = {};
		std::memcpy(part, token.text + begin, index - begin);
		int partDifficulty = -1;
		if (equalsIgnoreCase(part, "EASY"))
		{
			partDifficulty = kEasyProfileDifficulty;
		}
		else if (equalsIgnoreCase(part, "NORMAL"))
		{
			partDifficulty = kNormalProfileDifficulty;
		}
		else if (equalsIgnoreCase(part, "HARD"))
		{
			partDifficulty = kHardProfileDifficulty;
		}
		else if (equalsIgnoreCase(part, "EXPERT"))
		{
			partDifficulty = kExpertProfileDifficulty;
		}
		if (partDifficulty < 0)
		{
			return false;
		}
		const int actualDifficulty = equalsIgnoreCase(part, "EASY") ? kEasyDifficultyBit
			: equalsIgnoreCase(part, "NORMAL")						? kNormalDifficultyBit
			: equalsIgnoreCase(part, "HARD")						? kHardDifficultyBit
																	: kExpertDifficultyBit;
		*mask |= 1U << actualDifficulty;
		if (partDifficulty > strongestDifficulty)
		{
			strongestDifficulty = partDifficulty;
		}
		begin = index + 1U;
	}
	if (strongestDifficulty < 0)
	{
		return false;
	}
	*difficulty = strongestDifficulty;
	return true;
}

bool applyCsbotField(const Token &key, const Token &value, CsbotValues *values)
{
	if (values == nullptr)
	{
		return false;
	}
	if (equalsIgnoreCase(key.text, "Difficulty"))
	{
		if (!parseCsbotDifficulty(value, &values->difficulty, &values->difficultyMask))
		{
			return false;
		}
		values->hasDifficulty = true;
	}
	else if (equalsIgnoreCase(key.text, "Skill"))
	{
		if (!parseInteger(value, compat::ProfileCatalog::kMinimumAttribute, compat::ProfileCatalog::kMaximumAttribute,
				&values->skill))
		{
			return false;
		}
		values->hasSkill = true;
	}
	else if (equalsIgnoreCase(key.text, "Aggression"))
	{
		if (!parseInteger(value, compat::ProfileCatalog::kMinimumAttribute, compat::ProfileCatalog::kMaximumAttribute,
				&values->aggression))
		{
			return false;
		}
		values->hasAggression = true;
	}
	else if (equalsIgnoreCase(key.text, "Team"))
	{
		if (!parseTeam(value, &values->team))
		{
			return false;
		}
		values->hasTeam = true;
	}
	return true;
}

bool parseCsbotBlock(LineReader *reader, CsbotValues *values)
{
	if (reader == nullptr || values == nullptr)
	{
		return false;
	}
	// Each block starts its own preference list, replacing inherited preferences on first assignment.
	unsigned blockMask = 0;
	char line[ProfileLoader::kMaximumTokenLength * 2U + 1U] = {};
	while (reader->next(line, sizeof(line)))
	{
		if (line[0] == '\0')
		{
			continue;
		}
		if (equalsIgnoreCase(line, "End"))
		{
			return true;
		}
		Token key = {};
		Token value = {};
		if (!parseCsbotField(line, &key, &value) || !applyCsbotField(key, value, values))
		{
			return false;
		}
		if (!parseExtendedField(key, value, &values->extended, &blockMask))
			return false;
		values->extendedMask |= blockMask;
	}
	return false;
}

const CsbotTemplate *findCsbotTemplate(
	const std::array<CsbotTemplate, kMaximumCsbotTemplates> &templates, std::size_t templateCount, const char *name)
{
	if (name == nullptr)
	{
		return nullptr;
	}
	for (std::size_t index = 0U; index < templateCount; ++index)
	{
		if (equalsIgnoreCase(templates[index].name, name))
		{
			return &templates[index];
		}
	}
	return nullptr;
}

void applyCsbotTemplate(const CsbotTemplate &source, CsbotValues *values)
{
	const unsigned mask = source.values.extendedMask;
	if (mask & kReactionTimeFieldMask)
		values->extended.reactionTimeSeconds = source.values.extended.reactionTimeSeconds;
	if (mask & kAttackDelayFieldMask)
		values->extended.attackDelaySeconds = source.values.extended.attackDelaySeconds;
	if (mask & kTeamworkFieldMask)
		values->extended.teamwork = source.values.extended.teamwork;
	if (mask & kCostFieldMask)
		values->extended.cost = source.values.extended.cost;
	if (mask & kVoicePitchFieldMask)
		values->extended.voicePitch = source.values.extended.voicePitch;
	if (mask & kVoiceBankFieldMask)
		std::memcpy(values->extended.voiceBank, source.values.extended.voiceBank, sizeof(values->extended.voiceBank));
	if (mask & kWeaponPreferenceFieldMask)
	{
		values->extended.weaponPreferenceCount = source.values.extended.weaponPreferenceCount;
		std::memcpy(values->extended.weaponPreferences, source.values.extended.weaponPreferences,
			sizeof(values->extended.weaponPreferences));
	}
	values->extendedMask |= mask;
	if (source.values.hasDifficulty)
	{
		values->difficulty = source.values.difficulty;
		values->difficultyMask = source.values.difficultyMask;
		values->hasDifficulty = true;
	}
	if (source.values.hasSkill)
	{
		values->skill = source.values.skill;
		values->hasSkill = true;
	}
	if (source.values.hasAggression)
	{
		values->aggression = source.values.aggression;
		values->hasAggression = true;
	}
	if (source.values.hasTeam)
	{
		values->team = source.values.team;
		values->hasTeam = true;
	}
}

bool splitCsbotTemplateNames(const Token &token,
	std::array<std::array<char, ProfileLoader::kMaximumTokenLength + 1U>, kMaximumCsbotTemplateNames> *names,
	std::size_t *nameCount)
{
	if (names == nullptr || nameCount == nullptr || (token.type != TokenType::Word && token.type != TokenType::Quoted))
	{
		return false;
	}
	std::size_t begin = 0U;
	const std::size_t length = std::strlen(token.text);
	for (std::size_t index = 0U; index <= length; ++index)
	{
		if (token.text[index] != '+' && token.text[index] != '\0')
		{
			continue;
		}
		if (index == begin || *nameCount >= names->size() || index - begin > ProfileLoader::kMaximumTokenLength)
		{
			return false;
		}
		std::memcpy((*names)[*nameCount].data(), token.text + begin, index - begin);
		(*names)[*nameCount][index - begin] = '\0';
		*nameCount += 1U;
		begin = index + 1U;
	}
	return *nameCount > 0U;
}

ProfileLoadResult loadCsbotText(
	const char *data, std::size_t size, std::size_t maximumProfiles, compat::ProfileCatalog *catalog)
{
	constexpr unsigned kMaximumHeaderTokens = 8U;
	constexpr unsigned kDefaultHeaderTokens = 1U;
	constexpr unsigned kTemplateHeaderTokens = 2U;
	constexpr unsigned kMinimumProfileHeaderTokens = 2U;
	if (catalog == nullptr || (data == nullptr && size != 0U))
	{
		return ProfileLoadResult::InvalidArgument;
	}

	compat::ProfileCatalog candidate;
	CsbotValues defaults = makeCsbotValues();
	std::array<CsbotTemplate, kMaximumCsbotTemplates> templates = {};
	std::size_t templateCount = 0U;
	LineReader reader(data, size);
	char line[ProfileLoader::kMaximumTokenLength * 2U + 1U] = {};
	bool foundProfile = false;
	while (reader.next(line, sizeof(line)))
	{
		if (line[0] == '\0')
		{
			continue;
		}
		Tokenizer headerTokenizer(line, std::strlen(line));
		std::array<Token, kMaximumHeaderTokens> headerTokens = {};
		std::size_t headerCount = 0U;
		while (headerCount < headerTokens.size())
		{
			const Token token = headerTokenizer.next();
			if (token.type == TokenType::End)
			{
				break;
			}
			if (token.type != TokenType::Word && token.type != TokenType::Quoted)
			{
				return ProfileLoadResult::Malformed;
			}
			headerTokens[headerCount++] = token;
		}
		if (headerCount == headerTokens.size() || headerCount == 0U)
		{
			return ProfileLoadResult::Malformed;
		}

		if (equalsIgnoreCase(headerTokens[0].text, "Default"))
		{
			if (headerCount != kDefaultHeaderTokens || !parseCsbotBlock(&reader, &defaults))
			{
				return ProfileLoadResult::Malformed;
			}
			continue;
		}
		if (equalsIgnoreCase(headerTokens[0].text, "Template"))
		{
			if (headerCount != kTemplateHeaderTokens || templateCount >= templates.size() ||
				!copyName(headerTokens[1], templates[templateCount].name))
			{
				return templateCount >= templates.size() ? ProfileLoadResult::CatalogFull
														 : ProfileLoadResult::Malformed;
			}
			templates[templateCount].values = makeCsbotValues();
			if (!parseCsbotBlock(&reader, &templates[templateCount].values))
			{
				return ProfileLoadResult::Malformed;
			}
			++templateCount;
			continue;
		}

		if (headerCount < kMinimumProfileHeaderTokens || candidate.size() >= maximumProfiles)
		{
			return headerCount < kMinimumProfileHeaderTokens ? ProfileLoadResult::Malformed
															 : ProfileLoadResult::CatalogFull;
		}
		compat::ProfileRecord profile = {};
		if (!copyName(headerTokens[headerCount - 1U], profile.name))
		{
			return ProfileLoadResult::Malformed;
		}
		CsbotValues values = defaults;
		for (std::size_t index = 0U; index + 1U < headerCount; ++index)
		{
			std::array<std::array<char, ProfileLoader::kMaximumTokenLength + 1U>, kMaximumCsbotTemplateNames> names =
				{};
			std::size_t nameCount = 0U;
			if (!splitCsbotTemplateNames(headerTokens[index], &names, &nameCount))
			{
				return ProfileLoadResult::Malformed;
			}
			for (std::size_t nameIndex = 0U; nameIndex < nameCount; ++nameIndex)
			{
				const CsbotTemplate *source = findCsbotTemplate(templates, templateCount, names[nameIndex].data());
				if (source == nullptr)
				{
					return ProfileLoadResult::Malformed;
				}
				applyCsbotTemplate(*source, &values);
			}
		}
		if (!parseCsbotBlock(&reader, &values))
		{
			return ProfileLoadResult::Malformed;
		}
		profile.team = values.team;
		profile.difficultyMask = values.difficultyMask;
		profile.reactionTimeSeconds = values.extended.reactionTimeSeconds;
		profile.attackDelaySeconds = values.extended.attackDelaySeconds;
		profile.teamwork = values.extended.teamwork;
		profile.cost = values.extended.cost;
		profile.voicePitch = values.extended.voicePitch;
		std::memcpy(profile.voiceBank, values.extended.voiceBank, sizeof(profile.voiceBank));
		profile.weaponPreferenceCount = values.extended.weaponPreferenceCount;
		std::memcpy(profile.weaponPreferences, values.extended.weaponPreferences, sizeof(profile.weaponPreferences));
		profile.difficulty = values.hasDifficulty ? values.difficulty : difficultyFromSkill(values.skill);
		profile.skill = values.skill;
		profile.aggression = values.aggression;
		const compat::ProfileAddResult addResult = candidate.add(profile);
		if (addResult != compat::ProfileAddResult::Added)
		{
			return mapAddResult(addResult);
		}
		foundProfile = true;
	}
	if (!reader.isValid())
	{
		return ProfileLoadResult::TooLarge;
	}
	if (!foundProfile)
	{
		return ProfileLoadResult::Empty;
	}
	*catalog = candidate;
	return ProfileLoadResult::Loaded;
}

ProfileLoader::ProfileLoader() : limits_{kMaximumFileBytes, compat::ProfileCatalog::kMaximumProfiles}

{
}

ProfileLoader::ProfileLoader(const ProfileLoadLimits &limits) : limits_(limits)
{
	if (limits_.maxBytes > kMaximumFileBytes)
	{
		limits_.maxBytes = kMaximumFileBytes;
	}
	if (limits_.maxProfiles > compat::ProfileCatalog::kMaximumProfiles)
	{
		limits_.maxProfiles = compat::ProfileCatalog::kMaximumProfiles;
	}
}

ProfileLoadResult ProfileLoader::loadFile(const char *path, compat::ProfileCatalog *catalog) const
{
	if (path == nullptr || catalog == nullptr)
	{
		return ProfileLoadResult::InvalidArgument;
	}
	std::FILE *file = openFile(path, "rb");
	if (file == nullptr)
	{
		return ProfileLoadResult::MissingFile;
	}
	std::array<char, kMaximumFileBytes + 1U> buffer = {};
	const std::size_t bytesRead = std::fread(buffer.data(), 1U, buffer.size(), file);
	const bool readFailed = std::ferror(file) != 0;
	const int closeResult = std::fclose(file);
	if (readFailed || closeResult != 0)
	{
		return ProfileLoadResult::ReadError;
	}
	if (bytesRead > limits_.maxBytes || bytesRead > kMaximumFileBytes)
	{
		return ProfileLoadResult::TooLarge;
	}
	return loadText(buffer.data(), bytesRead, catalog);
}

ProfileLoadResult ProfileLoader::loadText(const char *data, std::size_t size, compat::ProfileCatalog *catalog) const
{
	if (catalog == nullptr || (data == nullptr && size != 0U))
	{
		return ProfileLoadResult::InvalidArgument;
	}
	if (size > limits_.maxBytes || size > kMaximumFileBytes)
	{
		return ProfileLoadResult::TooLarge;
	}
	if (size != 0U && std::memchr(data, '{', size) == nullptr)
	{
		return loadCsbotText(data, size, limits_.maxProfiles, catalog);
	}

	compat::ProfileCatalog candidate;
	Tokenizer tokenizer(data, size);
	bool foundProfile = false;
	while (true)
	{
		const Token header = tokenizer.next();
		if (header.type == TokenType::End)
		{
			break;
		}
		if (header.type != TokenType::Word && header.type != TokenType::Quoted)
		{
			return ProfileLoadResult::Malformed;
		}
		if (candidate.size() >= limits_.maxProfiles)
		{
			return ProfileLoadResult::CatalogFull;
		}
		if (tokenizer.next().type != TokenType::LeftBrace)
		{
			return ProfileLoadResult::Malformed;
		}
		compat::ProfileRecord profile = {};
		if (!copyName(header, profile.name) || !parseProfileBlock(&tokenizer, &profile))
		{
			return ProfileLoadResult::Malformed;
		}
		const compat::ProfileAddResult addResult = candidate.add(profile);
		if (addResult != compat::ProfileAddResult::Added)
		{
			return mapAddResult(addResult);
		}
		foundProfile = true;
	}
	if (!foundProfile)
	{
		return ProfileLoadResult::Empty;
	}
	*catalog = candidate;
	return ProfileLoadResult::Loaded;
}
} // namespace metamod
} // namespace astrabot
