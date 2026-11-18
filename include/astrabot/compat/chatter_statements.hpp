#ifndef ASTRABOT_COMPAT_CHATTER_STATEMENTS_HPP
#define ASTRABOT_COMPAT_CHATTER_STATEMENTS_HPP
#include "astrabot/compat/chatter_catalog.hpp"
#include "astrabot/compat/chatter_playback.hpp"
#include "astrabot/compat/random_source.hpp"
#include "astrabot/nav/nav_model.hpp"
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
enum class ChatterMemeKind : std::uint8_t
{
	None,
	Help,
	Follow,
	DefendHere,
	RequestReport,
	BombsiteClear
};
struct ChatterMeme
{
	ChatterMemeKind kind = ChatterMemeKind::None;
	std::uint16_t place = 0;
	nav::NavVector position{};
	world::EntityKey site{};
};
struct ChatterStatementRequest
{
	ChatterScheduleSource source = ChatterScheduleSource::None;
	ChatterStatementTopic topic = ChatterStatementTopic::None;
	ChatterStatementContent content = ChatterStatementContent::Fixed;
	ChatterPhrase phrase = ChatterPhrase::None;
	float created = 0, start = 0, expires = 0, readyAt = 0;
	std::uint16_t place = 0;
	std::uint32_t criteriaPlace = 0;
	int subject = -1;
	ChatterProgram program{};
	bool requiresCombat = false;
	std::uint64_t navRevision = 0;
	std::uint32_t noiseTick = 0;
	ChatterMeme meme{};
};
struct ChatterStatementHandle
{
	std::uint64_t id = 0;
	ChatterScheduleEntry owner{};
};
struct ChatterPartToken
{
	ChatterStatementHandle statement{};
	std::uint64_t step = 0;
};
struct ChatterStatementCompletion
{
	bool natural = false;
	ChatterMeme meme{};
	ChatterStatementHandle source{};
};
struct ChatterStatementStep
{
	ChatterPlaybackDecision playback{};
	ChatterPartToken token{};
	ChatterScheduleEntry schedule{};
	std::uint16_t explicitPlace = 0;
	std::uint32_t criteriaPlace = 0;
	ChatterStatementCompletion completion{};
};
struct ChatterPendingStatement
{
	ChatterStatementRequest request{};
	ChatterScheduleEntry schedule{};
	ChatterPlaybackState playback{};
	ChatterStatementStep pending{};
};
struct ChatterPendingMaintenance
{
	ICompatibilityRandomSource *random = nullptr;
	RandomTimingContext randomTiming{};
	std::uint32_t agreementPhraseId = 0;
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
		auto prepared = request;
		if (request.program.count > request.program.parts.size())
			return {};
		ChatterProgram source = request.program;
		if (source.count == 0)
		{
			if (request.content == ChatterStatementContent::EnemyReport)
			{
				source.append({ChatterPartKind::AccumulateEnemies, ChatterPhrase::None, 0, 0});
				source.append({ChatterPartKind::CurrentEnemyCount, ChatterPhrase::None, 0, 0});
			}
			else if (request.content == ChatterStatementContent::RemainingEnemies)
				source.append({ChatterPartKind::RemainingEnemyCount, ChatterPhrase::None, 0, 0});
			else
				source.append({ChatterPartKind::Fixed, request.phrase, 0, 0});
		}
		prepared.program = {};
		bool important = false;
		std::uint16_t effectivePlace = request.place;
		for (std::size_t i = 0; i < source.count; ++i)
		{
			const auto &part = source.parts[i];
			if (static_cast<unsigned>(part.kind) > static_cast<unsigned>(ChatterPartKind::AccumulateEnemies))
				return {};
			if (part.kind == ChatterPartKind::Fixed)
			{
				if (!catalog.defined(part.phrase))
					continue;
				important = important || catalog.important(part.phrase);
			}
			else if (part.kind == ChatterPartKind::Place)
			{
				const auto *definition = catalog.place(part.placePhrase);
				if (!definition)
					continue;
				important = important || definition->important;
				if (!effectivePlace)
					effectivePlace = part.place;
			}
			else if (part.kind == ChatterPartKind::CurrentEnemyCount)
				important = true;
			prepared.program.append(part);
		}
		if (!prepared.program.count || (mode == ChatterVerbosity::Minimal && !important))
			return {};
		ChatterScheduleEntry schedule{admission.owner, admission.connection, admission.ownerRevision, request.source,
			request.topic, request.created, request.start, 0, effectivePlace, request.subject, false};
		if (validChatterScheduleEntry(speaking) && chatterStatementsRedundant(schedule, speaking))
			return {};
		for (const auto &node : nodes_)
			if (chatterStatementsRedundant(schedule, node.schedule))
				return {};
		auto effective = admission;
		if (!effective.clock)
			effective.clock = &localClock_;
		schedule = effective.make(
			request.source, request.topic, request.created, request.start, effectivePlace, request.subject);
		if (!validChatterScheduleEntry(schedule))
			return {};
		const auto &first = prepared.program.parts[0];
		schedule.firstPhrase = first.kind == ChatterPartKind::Fixed ? catalog.phraseId(first.phrase)
			: first.kind == ChatterPartKind::Place					? first.placePhrase
																	: 0;
		auto at = nodes_.begin();
		while (at != nodes_.end() && at->schedule.start <= schedule.start)
			++at;
		nodes_.insert(at, {prepared, schedule, {}, {}});
		bump();
		return {schedule.order, schedule};
	}
	ChatterStatementStep advance(const ChatterStatementHandle &handle, const ChatterPlaybackInput &input)
	{
		auto at = findNode(handle);
		if (at == nodes_.end() || !std::isfinite(input.now) || input.now < 0 || input.now < at->request.start)
			return {};
		ChatterStatementStep result{};
		result.schedule = at->schedule;
		result.explicitPlace = at->request.place;
		result.criteriaPlace = at->request.criteriaPlace;
		if (input.friendsRemaining == 0 && !input.emote)
		{
			result.playback.kind = ChatterPlaybackKind::Finished;
			nodes_.erase(at);
			bump();
			return result;
		}
		const auto revision = at->playback.revision;
		result.playback = advanceChatterPlayback(at->request.program, at->playback, input);
		at->schedule.speaking = at->playback.speaking;
		result.schedule = at->schedule;
		result.token = {handle, result.playback.step};
		if (result.playback.kind == ChatterPlaybackKind::Finished)
		{
			// The friends-zero abort returned above; admitted programs are nonempty.
			// Transfer by value before erasing the exact source node.
			result.completion = {true, at->request.meme, handle};
			nodes_.erase(at);
			bump();
			return result;
		}
		if (result.playback.phrase == ChatterPhrase::Help && at->playback.index >= 0 &&
			static_cast<std::size_t>(at->playback.index) < at->request.program.count &&
			at->request.program.parts[static_cast<std::size_t>(at->playback.index)].kind ==
				ChatterPartKind::CurrentEnemyCount)
			at->request.meme = {ChatterMemeKind::Help, 0};
		if (at->playback.inFlight && result.playback.step)
			at->pending = result;
		if (at->playback.revision != revision)
			bump();
		return result;
	}
	bool finishPart(const ChatterPartToken &token, float duration, bool skip = false)
	{
		auto at = findNode(token.statement);
		if (at == nodes_.end() || !finishChatterPlaybackPart(at->playback, token.step, duration, skip))
			return false;
		at->pending = {};
		bump();
		return true;
	}
	bool finishDelay(const ChatterPartToken &token, float duration)
	{
		auto at = findNode(token.statement);
		if (at == nodes_.end() || !finishChatterPlaybackDelay(at->playback, token.step, duration))
			return false;
		at->pending = {};
		bump();
		return true;
	}
	bool cancelPart(const ChatterPartToken &token)
	{
		auto at = findNode(token.statement);
		if (at == nodes_.end() || !cancelChatterPlaybackStep(at->playback, token.step))
			return false;
		at->pending = {};
		bump();
		return true;
	}
	bool currentPart(const ChatterPartToken &token, ChatterStatementStep *out) const
	{
		const auto at = findNode(token.statement);
		if (!out || at == nodes_.end() || !at->playback.inFlight || !token.step || token.step != at->playback.revision)
			return false;
		*out = at->pending;
		return true;
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
		maintain(in,
			[]()
			{
				return true;
			});
	}
	template <class Current> void maintain(const ChatterPendingMaintenance &in, Current current)
	{
		if (!synchronize(in.admission))
			return;
		for (auto at = nodes_.begin(); at != nodes_.end();)
		{
			const auto &request = at->request;
			const bool invalid = request.requiresCombat && !in.inCombat;
			const bool speaking =
				at->playback.speaking || (sameChatterStatement(at->schedule, in.speaking) && in.speaking.speaking);
			const bool noiseInvalid = request.phrase == ChatterPhrase::HeardNoise &&
				(!in.noiseActive || request.navRevision != in.navRevision || request.created != in.noiseCreated ||
					request.noiseTick != in.noiseTick);
			const bool obsolete = in.admission.owner.now > request.expires ||
				(in.roundOver && request.topic != ChatterStatementTopic::Emote);
			if (invalid || (!speaking && (noiseInvalid || obsolete)))
			{
				at = nodes_.erase(at);
				bump();
				continue;
			}
			if (!speaking)
			{
				if (in.random && request.topic == ChatterStatementTopic::MyPlan &&
					in.teammate.topic == ChatterStatementTopic::MyPlan &&
					at->schedule.firstPhrase != in.agreementPhraseId &&
					at->schedule.firstPhrase == in.teammate.firstPhrase)
				{
					const bool samePlace = at->schedule.place == in.teammate.place;
					constexpr float kAgreementMinimumSeconds = .5f;
					constexpr float kAgreementMaximumSeconds = 1;
					constexpr float kDifferentPlaceMinimumSeconds = 3;
					constexpr float kDifferentPlaceMaximumSeconds = 4;
					const auto lower = samePlace ? kAgreementMinimumSeconds : kDifferentPlaceMinimumSeconds;
					const auto upper = samePlace ? kAgreementMaximumSeconds : kDifferentPlaceMaximumSeconds;
					const auto revision = revision_;
					if (!current() || revision_ != revision)
						return;
					const auto draw = in.random->nextFloat(RandomRequest::floatRequest(
						samePlace ? "RNG-CSBOT-AGREE-WITH-PLAN" : "RNG-CSBOT-DIFFERENT-PLAN-PLACE",
						{in.admission.owner.actor.slot, in.admission.owner.actor.generation}, in.randomTiming, lower,
						upper));
					// A callback may destroy this node, replace the queue or change its teammate.
					if (!current() || revision_ != revision)
						return;
					const float start = in.admission.owner.now + draw.value;
					if (draw.status == RandomStatus::Ok && std::isfinite(draw.value) && draw.value >= lower &&
						draw.value <= upper && std::isfinite(start))
					{
						at->request.start = at->schedule.start = start;
						if (samePlace)
						{
							const auto phrase =
								in.agreementPhraseId ? ChatterPhrase::AgreeWithPlan : ChatterPhrase::None;
							at->request.program.parts[0] = {ChatterPartKind::Fixed, phrase, 0, 0};
							at->request.phrase = phrase;
							at->schedule.firstPhrase = in.agreementPhraseId;
						}
						bump();
					}
				}
				if (chatterStatementsRedundant(at->schedule, in.teammate))
				{
					at = nodes_.erase(at);
					bump();
					continue;
				}
			}
			++at;
		}
	}

private:
	using Nodes = std::list<ChatterPendingStatement>;
	Nodes::iterator findNode(const ChatterStatementHandle &handle)
	{
		for (auto at = nodes_.begin(); at != nodes_.end(); ++at)
			if (handle.id == at->schedule.order && sameChatterStatement(handle.owner, at->schedule))
				return at;
		return nodes_.end();
	}
	Nodes::const_iterator findNode(const ChatterStatementHandle &handle) const
	{
		for (auto at = nodes_.begin(); at != nodes_.end(); ++at)
			if (handle.id == at->schedule.order && sameChatterStatement(handle.owner, at->schedule))
				return at;
		return nodes_.end();
	}
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
