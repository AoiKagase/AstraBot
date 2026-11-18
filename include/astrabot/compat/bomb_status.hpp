#ifndef ASTRABOT_COMPAT_BOMB_STATUS_HPP
#define ASTRABOT_COMPAT_BOMB_STATUS_HPP

#include "astrabot/compat/chatter_statements.hpp"

namespace astrabot
{
namespace compat
{
struct BombStatusMemory
{
	enum class Status { Unknown, Moving, Loose };
	Status status = Status::Unknown;
	nav::NavVector position{};
	float nextLooseReport = -1.0f;

	bool isNewCarrierPosition(nav::NavVector observed) const
	{
		constexpr float kCloseRange = 1000.0f;
		const float dx = observed.x - position.x, dy = observed.y - position.y, dz = observed.z - position.z;
		return status != Status::Moving || dx * dx + dy * dy + dz * dz >= kCloseRange * kCloseRange;
	}

	void rememberCarrier(nav::NavVector observed)
	{
		status = Status::Moving;
		position = observed;
	}

	void rememberLoose(nav::NavVector observed)
	{
		status = Status::Loose;
		position = observed;
	}

	bool spotLooseBomb(nav::NavVector observed, float now)
	{
		if (status == Status::Loose)
			return false;
		rememberLoose(observed);
		if (now <= nextLooseReport)
			return false;
		nextLooseReport = now + 10.0f;
		return true;
	}
};

inline ChatterStatementRequest bombStatusReport(ChatterPhrase phrase, float now, nav::NavVector position,
	std::uint16_t place = 0, std::uint32_t placePhrase = 0, int subject = -1)
{
	ChatterStatementRequest report{};
	report.source = ChatterScheduleSource::Generic;
	report.topic = ChatterStatementTopic::Information;
	report.phrase = phrase;
	report.created = report.start = now;
	report.expires = now + 10.0f;
	report.subject = subject;
	report.program.append({ChatterPartKind::Place, ChatterPhrase::None, place, placePhrase});
	report.program.append({ChatterPartKind::Fixed, report.phrase, 0, 0});
	report.meme = {phrase == ChatterPhrase::SpottedLooseBomb ? ChatterMemeKind::BombLoose : ChatterMemeKind::BombMoving,
		0, position};
	return report;
}
}
}
#endif
