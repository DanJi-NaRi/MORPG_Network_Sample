# Agent Active Rules

Purpose:
- Keep a compact, low-token rule set for daily task execution.
- Use this file as the default read target before every meaningful task.

## How To Use
- Read this file before implementation.
- Apply all `Keep` and `Add` rules.
- Do not apply rules listed under `Remove`.
- Update this file only when a new rule has clear evidence from recent tasks.

## Current Rules
### Keep
- Always explain chosen approach, alternatives, and final tradeoff.
- Run targeted checks before full build/test when possible.
- Avoid unrelated refactors.
- Report exact changed files and verification results.

### Add
- For protocol tasks, enforce file placement rules:
  - Add packet type in `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Public\Net\PacketType.h`
  - Add `.h` in `Public`, `.cpp` in `Private`
  - Use packet category folders (`C2SPackets`, `S2CPackets`, `ErrorPackets`, or dedicated folder)

### Remove
- None

## Maintenance
- Keep this file short (target under 120 lines).
- Keep only active rules that matter now.
- Move detailed rationale/history to:
  - `C:\Project\MORPG_Network_Sample\docs\agent-improvement-loop.md`
