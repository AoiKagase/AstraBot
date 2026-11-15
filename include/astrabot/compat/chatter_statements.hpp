#ifndef ASTRABOT_COMPAT_CHATTER_STATEMENTS_HPP
#define ASTRABOT_COMPAT_CHATTER_STATEMENTS_HPP
#include "astrabot/compat/chatter_catalog.hpp"
#include <list>
namespace astrabot
{
namespace compat
{
enum class ChatterVerbosity
{
	Normal,
	Minimal,
	Radio,
	Off
};
inline ChatterVerbosity parseChatterVerbosity(const char *text)
{
	if (!text)
		return ChatterVerbosity::Normal;
	switch (*text)
	{
	case 'o':
	case 'O':
		return ChatterVerbosity::Off;
	case 'm':
	case 'M':
		return ChatterVerbosity::Minimal;
	case 'r':
	case 'R':
		return ChatterVerbosity::Radio;
	default:
		return ChatterVerbosity::Normal;
	}
}
enum class ChatterStatementContent
{
	Fixed,
	RemainingEnemies,
	EnemyReport
};
struct ChatterStatementRequest
{
	ChatterScheduleSource source = ChatterScheduleSource::None;
	ChatterStatementTopic topic = ChatterStatementTopic::None;
	ChatterStatementContent content = ChatterStatementContent::Fixed;
	ChatterPhrase phrase = ChatterPhrase::None;
	float created = 0, start = 0, expires = 0, readyAt = 0;
	std::uint16_t place = 0;
	int subject = -1;
	bool requiresCombat = false;
	std::uint64_t navRevision = 0;
	std::uint32_t noiseTick = 0;
};
struct ChatterStatementHandle
{
	std::uint64_t id = 0;
	ChatterScheduleEntry owner{};
};
struct ChatterPendingStatement
{
	ChatterStatementRequest request{};
	ChatterScheduleEntry schedule{};
};
struct ChatterPendingMaintenance
{
	ChatterScheduleAdmission admission{};
	bool inCombat = false;
	ChatterScheduleEntry speaking{};
	ChatterScheduleEntry teammate{};
	bool roundOver = false;
	bool noiseActive = false;
	float noiseCreated = 0;
	std::uint32_t noiseTick = 0;
	std::uint64_t navRevision = 0;
};
inline bool sameChatterOwner(const ChatterScheduleAdmission &a, const ChatterScheduleAdmission &b)
{
	return a.owner.actor == b.owner.actor && a.owner.entityGeneration == b.owner.entityGeneration &&
		a.owner.frame.mapGeneration == b.owner.frame.mapGeneration &&
		a.owner.frame.roundGeneration == b.owner.frame.roundGeneration && a.connection == b.connection &&
		a.ownerRevision == b.ownerRevision;
}
inline bool sameChatterStatement(const ChatterScheduleEntry &a, const ChatterScheduleEntry &b)
{
	return a.order != 0 && a.order == b.order &&
		sameChatterOwner(
			{a.owner, a.connection, a.ownerRevision, nullptr}, {b.owner, b.connection, b.ownerRevision, nullptr});
}
class ChatterPendingQueue
{
public:
	ChatterPendingQueue() = default;
	ChatterPendingQueue(const ChatterPendingQueue &) = delete;
	ChatterPendingQueue &operator=(const ChatterPendingQueue &) = delete;
	bool synchronize(const ChatterScheduleAdmission &admission)
	{
		if (!admission.owner.valid())
		{
			clear();
			owner_ = {};
			return false;
		}
		if (!sameChatterOwner(owner_, admission) || admission.owner.now < owner_.owner.now ||
			admission.owner.frame.tick < owner_.owner.frame.tick)
		{
			clear();
			bump();
		}
		owner_ = admission;
		return true;
	}
	ChatterStatementHandle admit(const ChatterStatementRequest &request, const ChatterScheduleAdmission &admission,
		const ChatterRadioCatalog &catalog, ChatterVerbosity mode, const ChatterScheduleEntry &speaking = {})
	{
		if (!synchronize(admission) || mode == ChatterVerbosity::Off || request.source == ChatterScheduleSource::None ||
			request.topic == ChatterStatementTopic::None || !std::isfinite(request.created) || request.created < 0 ||
			!std::isfinite(request.start) || request.start < 0 || !std::isfinite(request.expires) ||
			request.expires < request.created || !std::isfinite(request.readyAt) || request.readyAt < 0)
			return {};
		const bool fixed = request.content == ChatterStatementContent::Fixed;
		if (fixed && !catalog.defined(request.phrase))
			return {};
		const bool important =
			request.content == ChatterStatementContent::EnemyReport || (fixed && catalog.important(request.phrase));
		if (mode == ChatterVerbosity::Minimal && !important)
			return {};
		ChatterScheduleEntry schedule{admission.owner, admission.connection, admission.ownerRevision, request.source,
			request.topic, request.created, request.start, 0, request.place, request.subject, false};
		if (validChatterScheduleEntry(speaking) && chatterStatementsRedundant(schedule, speaking))
			return {};
		for (const auto &node : nodes_)
			if (chatterStatementsRedundant(schedule, node.schedule))
				return {};
		auto effective = admission;
		if (!effective.clock)
			effective.clock = &localClock_;
		schedule = effective.make(
			request.source, request.topic, request.created, request.start, request.place, request.subject);
		if (!validChatterScheduleEntry(schedule))
			return {};
		auto at = nodes_.begin();
		while (at != nodes_.end() && at->schedule.start <= schedule.start)
			++at;
		nodes_.insert(at, {request, schedule});
		bump();
		return {schedule.order, schedule};
	}
	bool erase(const ChatterStatementHandle &handle)
	{
		for (auto at = nodes_.begin(); at != nodes_.end(); ++at)
		{
			if (handle.id == at->schedule.order && sameChatterStatement(handle.owner, at->schedule))
			{
				nodes_.erase(at);
				bump();
				return true;
			}
		}
		return false;
	}
	bool read(const ChatterStatementHandle &handle, ChatterPendingStatement *out) const
	{
		if (!out)
			return false;
		for (const auto &node : nodes_)
			if (handle.id == node.schedule.order && sameChatterStatement(handle.owner, node.schedule))
			{
				*out = node;
				return true;
			}
		return false;
	}
	ChatterStatementHandle handle(const ChatterScheduleEntry &entry) const
	{
		ChatterPendingStatement node{};
		ChatterStatementHandle candidate{entry.order, entry};
		return read(candidate, &node) ? candidate : ChatterStatementHandle{};
	}
	void clear()
	{
		if (!nodes_.empty())
		{
			nodes_.clear();
			bump();
		}
	}
	bool empty() const
	{
		return nodes_.empty();
	}
	std::uint64_t revision() const
	{
		return revision_;
	}
	template <class Visitor> void visitSchedules(Visitor visitor) const
	{
		for (const auto &node : nodes_)
			if (!visitor(node.schedule))
				break;
	}
	void maintain(const ChatterPendingMaintenance &in)
	{
		if (!synchronize(in.admission))
			return;
		for (auto at = nodes_.begin(); at != nodes_.end();)
		{
			const auto &request = at->request;
			const bool invalid = request.requiresCombat && !in.inCombat;
			const bool speaking = sameChatterStatement(at->schedule, in.speaking) && in.speaking.speaking;
			const bool noiseInvalid = request.phrase == ChatterPhrase::HeardNoise &&
				(!in.noiseActive || request.navRevision != in.navRevision || request.created != in.noiseCreated ||
					request.noiseTick != in.noiseTick);
			const bool obsolete = in.admission.owner.now > request.expires ||
				(in.roundOver && request.topic != ChatterStatementTopic::Emote);
			if (invalid ||
				(!speaking && (noiseInvalid || obsolete || chatterStatementsRedundant(at->schedule, in.teammate))))
			{
				at = nodes_.erase(at);
				bump();
			}
			else
				++at;
		}
	}

private:
	void bump()
	{
		if (++revision_ == 0)
			++revision_;
	}
	std::list<ChatterPendingStatement> nodes_{};
	ChatterScheduleAdmission owner_{};
	ChatterAdmissionClock localClock_{};
	std::uint64_t revision_ = 0;
};
}
}
#endif
