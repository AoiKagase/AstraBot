# P12 console debug diagnostics

## Operator command

The Metamod adapter registers a server-console command through the bootstrap
engine table:

```text
astrabot_debug
astrabot_debug 1
astrabot_debug 2
astrabot_debug 0
```

The command is disabled by default. With no argument it reports the current
state. `1` enables the production lifecycle, FakeClient, Join, and Removal
Trace sinks; `2` additionally enables NAV diagnostic sinks. This includes NAV
load, route, movement, Jump/Drop, ladder, and status lines emitted by the
adapter. `0` disables all diagnostics. Invalid arguments leave the current
state unchanged.

Every emitted diagnostic starts with a stable searchable prefix:

```text
[ASTRABOT][DEBUG][LIFECYCLE]
[ASTRABOT][DEBUG][FAKECLIENT]
[ASTRABOT][DEBUG][JOIN]
[ASTRABOT][DEBUG][REMOVAL]
[ASTRABOT][DEBUG][COMMAND]
```

`FrameStarted` is intentionally suppressed to avoid one log line per server
frame. Trace fields are value-only key/value data; SDK pointers and internal
addresses are never written to the server log.

## Join-failure diagnosis

To capture the automatic primary FakeClient join from a fresh map:

```text
astrabot_debug 1
changelevel de_dust
```

The relevant failure sequence is expected to contain a named `JoinError`, for
example:

```text
[ASTRABOT][DEBUG][JOIN] phase=Failed error=MenuOptionUnavailable ...
[ASTRABOT][DEBUG][REMOVAL] outcome=KickQueued error=None ...
```

This distinguishes an AstraBot join failure and cleanup kick from an
independent HLDS rejection. Use `astrabot_debug 0` after capturing the
diagnostic.

## Live HLDS diagnosis

The local RealBot clone confirms the compatible FakeClient sequence: initialize
the player info keys, set `_vgui_menus` to `0`, call `ClientConnect`, then call
`ClientPutInServer`. AstraBot now follows that initialization, including the
standard rate/model keys. Because a FakeClient has no network channel, the
fallback `menuselect` path also treats successful team/class command dispatch
as `Joined` without waiting for a `TeamInfo` packet that cannot arrive.

The updated x86 Release DLL was installed at:

```text
D:\SteamCMD\cstrike_rehlds\cstrike\addons\astrabot\dlls\astrabot_mm.dll
```

The live check used `astrabot_debug 1` followed by `astrabot_addbot 3`. The
command response reported `requested=3 created=3`, and the subsequent HLDS
`status` reported four active BOTs: the automatic primary BOT plus the three
requested BOTs. This is the required multi-client join/retention result; the
previous `Timeout`/`KickQueued` sequence and zero active players were not
reproduced.

## Verification

Focused adapter verification covers command registration, state transitions,
Prefix formatting, debug suppression, and detach cleanup. The canonical full
gate remains the P12 completion gate and is not repeated for commit, merge, or
DLL installation alone.
