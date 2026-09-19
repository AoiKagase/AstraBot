#ifndef ASTRABOT_COMPAT_CVAR_STATE_HPP
#define ASTRABOT_COMPAT_CVAR_STATE_HPP

namespace astrabot
{
namespace compat
{
	enum class JoinTeam
	{
		Any,
		Terrorist,
		CounterTerrorist
	};

	enum class RuntimeMode
	{
		Compatibility,
		Enhanced
	};

	enum class CvarUpdateResult
	{
		Updated,
		NoChange,
		Unknown,
		InvalidValue
	};

	struct CvarSnapshot
	{
		float botEnable;
		float botStop;
		int botDifficulty;
		int botQuota;
		JoinTeam botJoinTeam;
		RuntimeMode mode;
	};

	class CvarState
	{
	public:
		CvarState();

		CvarUpdateResult setFloat(const char *name, float value);
		CvarUpdateResult setString(const char *name, const char *value);
		CvarSnapshot snapshot() const;

	private:
		CvarSnapshot state_;
	};
}
}

#endif
