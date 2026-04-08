# Protocol Style Guide (EN)

Use this guide when adding or modifying packet protocols so output matches existing project conventions.

## 1) Scope
- Applies to all client-server packet definitions and related handler wiring.
- Use together with:
  - `C:\Project\MORPG_Network_Sample\docs\protocol-template.md`

## 2) Decision Rule
- If there is conflict between this guide and current codebase patterns:
1. Follow existing codebase pattern first
2. Document mismatch in PR/report
3. Propose guide update

## 3) Naming Conventions
- Request packet: `C2S_<Feature><Action>Req`
- Response packet: `S2C_<Feature><Action>Res`
- Event/notify packet: `S2C_<Feature><Action>Ntf`
- Handler method: `Handle<Feature><Action>`
- Keep feature/action names PascalCase.

## 4) File Placement Rule
- Packet type registration is mandatory:
  - Add new packet type to `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Public\Net\PacketType.h`
- Domain-based root module selection:
1. Game-related packets:
   - `C:\Project\MORPG_Network_Sample\YunoGameProtocol\Public\...`
   - `C:\Project\MORPG_Network_Sample\YunoGameProtocol\Private\...`
2. System-related packets:
   - `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Public\...`
   - `C:\Project\MORPG_Network_Sample\YunoNetProtocol\Private\...`
- Public/Private file pair rule:
  - Always add `.h` in `Public`
  - Always add `.cpp` in `Private`
- Packet category folder rule (applies to both Game and System modules):
1. C2S packets -> `C2SPackets`
2. S2C packets -> `S2CPackets`
3. Error packets -> `ErrorPackets`
4. Other packet categories -> create and use a dedicated folder
- Keep routing/opcode mapping and handler wiring in the same style/location as existing packets.

## 5) Packet Field Rule
- Order fields by protocol responsibility:
1. Identity/session context
2. Target/resource identifiers
3. Action parameters
4. Meta/control fields
- New optional fields must be append-only when compatibility is required.
- Avoid reusing one field for two meanings.

## 6) Validation Rule
- Validate at protocol boundary before business logic:
1. Auth/session validity
2. State precondition
3. Range/enum checks
4. Duplicate or replay check
- Return explicit error codes, not generic failure.

## 7) Error Code Rule
- Naming: `<FEATURE>_<REASON>` in uppercase snake case.
- Keep one reason per code.
- Document retryability (`retryable` true/false).

## 8) Compatibility Rule
- For additive changes, keep backward compatibility by default.
- For breaking changes:
1. Gate by version check or feature flag
2. Provide migration window and fallback behavior
3. Document rollout and rollback steps

## 9) Logging and Metrics Rule
- Log one line for request entry and one for failure with reason code.
- Add metrics at minimum:
  - request count
  - success count/rate
  - latency (p95 or p99)

## 10) Required Output In Agent Report
- Changed files list
- Protocol summary:
  - new/changed packets
  - field changes
  - opcode or route changes
  - error code changes
- Alternative options considered and why final choice was selected
- Build/test/smoke command results

## 11) Minimal Prompt Snippet
```text
Read:
- C:\Project\MORPG_Network_Sample\docs\protocol-template.md
- C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.en.md

Requirement:
- Add protocol using existing naming, routing, and field order conventions.
- Apply file placement rules exactly (PacketType.h + Public/Private + packet category folders).
- Keep compatibility unless explicitly blocked.
- Report alternatives and decision tradeoffs.
```
