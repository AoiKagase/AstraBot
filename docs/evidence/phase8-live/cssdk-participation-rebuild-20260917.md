# CSSDK participation rebuild live evidence

Status: `blocked` / `not-run`

Date: 2026-09-17 (Asia/Tokyo)

## Implementation identity

- Source HEAD: `433235f02383d601406d27ccbbfed2de98465693` with working-tree
  changes present.
- Windows x86 target: `astrabot_mm` from `build-metamod-x86-test`.
- PE verification: passed, x86, seven exact exports.
- Built DLL SHA-256:
  `50198f42ffc0fbd6c091801453e407d7ce4943dd9310b007001326f30ccc14b2`
- Deployed DLL SHA-256: same as the built DLL.
- Previous deployed DLL backup:
  `D:/SteamCMD/cstrike_rehlds/cstrike/addons/astrabot/dlls/astrabot_mm.dll.pre-cssdk-20260917-163149.bak`

## Server preflight

- Configured Windows server root exists at `D:/SteamCMD/cstrike_rehlds`.
- The inspected `qconsole.log` ends with `Server shutdown` at 15:56:25.
- No `hlds` or `rehlds` process was present during the post-deployment
  preflight.
- No server restart or process stop was issued by this run.

## Acceptance matrix

| Scenario | Result | Evidence |
|---|---|---|
| Plugin load beside unmodified ReGameDLL-CS | not-run | Server was not running after deployment |
| `bot_add`, `bot_add_t`, `bot_add_ct` | not-run | No live command session |
| Legacy/VGUI team and class menu confirmation | not-run | No live message trace |
| Leave spectator/intro camera | not-run | No live entity trace |
| `sv_restart 1` recovery | not-run | No live round transition |
| One/two-team and multi-Bot stability | not-run | No live run |
| Nav locomotion and grounded movement | not-run | No live physics trace |

This report does not close `PAR-06`, `TEST-03`, or `TEST-04`. Offline build
and export evidence does not prove participation or movement on HLDS/ReHLDS.

## Resume condition

Start the pinned unmodified Windows x86 server with the deployed DLL, capture
the server/plugin identity, then run `bot_enable 1`, `bot_quota 20`, each add
command, menu/team/class state, camera state, and `sv_restart 1`. Record the
new plugin diagnostics and server log separately from the older
`windows-x86.md` report. Debian Linux x86 remains a separate required gate.
