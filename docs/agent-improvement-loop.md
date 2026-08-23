# Agent Improvement Loop

Purpose:
- Reduce repeated mistakes and unnecessary cost during Codex Agent tasks.
- Keep a persistent learning record that the user reads and the Agent updates.

## Ownership Model
- User permission model:
  - User is read-only for this file during normal operation.
  - Codex Agent is the writer and must update this file after each meaningful task.

## Read-Then-Write Rule
Before starting a new task, Agent must:
1. Read `C:\Project\MORPG_Network_Sample\docs\agent-active-rules.md` first.
2. Read this history file only when needed:
   - repeated failure
   - unclear root cause
   - conflict between current rules and observed behavior
3. If history is needed, read only the latest 3 entries.

After finishing a task, Agent must:
1. Append one new entry (do not rewrite history).
2. Record mistakes, cost signals, and prevention updates.
3. Update `agent-active-rules.md` only if a rule should be added/removed/changed.

## Non-Negotiable Constraints
- Never delete old entries.
- Never edit past entries except typo fixes.
- Prefer append-only updates.
- Keep entries short and specific.
- Include evidence (file path, command, or symptom) for each mistake claim.

## Cost Control Heuristics
- Minimize avoidable retries:
  - Avoid running full builds before local scope checks.
  - Prefer targeted checks, then full verification.
- Minimize broad edits:
  - Change only files required by current task scope.
- Minimize duplicate analysis:
  - Reuse findings from recent entries when still relevant.

## Mistake Taxonomy
Use one or more labels per issue:
- `REQ_MISS`: requirement misunderstood or omitted.
- `FORMAT_MISS`: output/file format mismatch.
- `PATH_MISS`: wrong module/path placement.
- `COMPAT_MISS`: compatibility impact missed.
- `TEST_MISS`: missing or weak verification.
- `COST_WASTE`: unnecessary command/build/test cycles.
- `COMM_MISS`: missing explanation of alternatives/tradeoffs.

## Entry Template (Append One Block Per Task)
```text
## Entry: <YYYY-MM-DD HH:mm KST> | Task: <short title>
### Summary
- Goal:
- Outcome:

### What Went Well
- 

### Mistakes
- Label: <REQ_MISS|FORMAT_MISS|PATH_MISS|COMPAT_MISS|TEST_MISS|COST_WASTE|COMM_MISS>
  Evidence:
  Root cause:
  Fix applied:

### Cost Signals
- Commands run:
- Build runs:
- Test runs:
- Avoidable retries:

### Prevention Rules (Next Tasks)
- Keep:
- Add:
- Remove:

### Confidence
- Delivery confidence (0-100):
- Verification depth: <low|medium|high>
```

## Bootstrap Entry
## Entry: 2026-04-08 00:00 KST | Task: Bootstrap Improvement Loop
### Summary
- Goal: Create persistent self-improvement process for Codex Agent.
- Outcome: Document created and integrated for future append-only learning.

### What Went Well
- Defined a repeatable template with mistake taxonomy and cost controls.

### Mistakes
- Label: COMM_MISS
  Evidence: Earlier tasks had inconsistent detail in rationale sections.
  Root cause: No single enforced reflection format.
  Fix applied: Added mandatory entry template and read-then-write rule.

### Cost Signals
- Commands run: 0 (bootstrap template only)
- Build runs: 0
- Test runs: 0
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: Always include alternatives and tradeoff justification in final reports.
- Add: Read latest 3 entries before task execution.
- Remove: None

### Confidence
- Delivery confidence (0-100): 92
- Verification depth: medium

## Entry: 2026-04-08 13:00 KST | Task: Add Ping RTT Echo Handling
### Summary
- Goal: Implement Ping request/response path to measure client-server delay.
- Outcome: Reused existing C2S_Ping/S2C_Pong protocol, added server immediate echo and client RTT log.

### What Went Well
- Reused existing packet IDs/files to avoid unnecessary protocol expansion cost.
- Verified impacted targets with focused builds (`YunoNetProtocol`, `YunoServer`, `YunoGame`).

### Mistakes
- Label: COST_WASTE
  Evidence: Initial `rg` command failed due execution permission in this environment.
  Root cause: Assumed `rg` availability from prior runs.
  Fix applied: Switched immediately to PowerShell-native file search.

### Cost Signals
- Commands run: 14
- Build runs: 3 (targeted)
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: Prefer targeted build scope first.
- Add: If `rg` fails in sandbox, switch to `Get-ChildItem` + `Select-String` without retry loops.
- Remove: None

### Confidence
- Delivery confidence (0-100): 90
- Verification depth: medium

## Entry: 2026-04-08 13:25 KST | Task: Add Protocol Test Automation Policy
### Summary
- Goal: Enforce automatic protocol test script creation, execution log output, and analysis report output.
- Outcome: Added policy links in AGENTS.md, created protocol test operations guide, updated user template, and initialized Result directories.

### What Went Well
- Converted policy into explicit file paths and naming conventions to reduce ambiguity.
- Added fallback policy for blocked runtime environments.

### Mistakes
- Label: COST_WASTE
  Evidence: Initial parallel directory/file creation attempted before parent path was fully available.
  Root cause: Race condition in parallel setup commands.
  Fix applied: Re-ran creation in one sequential command.

### Cost Signals
- Commands run: 10
- Build runs: 0
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For path setup, use sequential command when parent/child directory dependency exists.
- Add: When policy requires artifacts, pre-create `Result\Log` and `Result\Report` before test execution.
- Remove: None

### Confidence
- Delivery confidence (0-100): 93
- Verification depth: medium

## Entry: 2026-04-08 17:46 KST | Task: Execute Ping Protocol Automated Test
### Summary
- Goal: Run Ping/Pong runtime verification and generate required log/report artifacts.
- Outcome: Added `scripts/test_ping.ps1`, executed test, and produced PASS artifacts under `Result/Log` and `Result/Report`.

### What Went Well
- Runtime packet validation confirmed expected `S2C_Pong` type and `reqTime` echo match.
- RTT metric was captured directly from socket round-trip.

### Mistakes
- Label: TEST_MISS
  Evidence: First execution failed due same-path stdout/stderr redirection in `Start-Process`.
  Root cause: Platform-specific PowerShell process redirection constraint.
  Fix applied: Removed conflicting redirection and simplified process start.
- Label: TEST_MISS
  Evidence: Second execution failed due UInt32 conversion overflow for timestamp.
  Root cause: Unsafe cast from Unix millisecond value to UInt32.
  Fix applied: Applied modulo conversion before UInt32 cast.

### Cost Signals
- Commands run: 8
- Build runs: 0
- Test runs: 4
- Avoidable retries: 2

### Prevention Rules (Next Tasks)
- Keep: Validate process launch options in script before first run.
- Add: For 32-bit protocol timestamps, normalize with modulo before cast.
- Remove: None

### Confidence
- Delivery confidence (0-100): 94
- Verification depth: high

## Entry: 2026-04-08 17:52 KST | Task: Set Report Language Policy
### Summary
- Goal: Keep runtime logs unchanged and enforce Korean as default report language policy.
- Outcome: Updated AGENTS.md and protocol test operations policy to require Korean reports by default while preserving raw logs.

### What Went Well
- Policy was added at both global rule level and protocol test operations level.
- `scripts/test_ping.ps1` was stabilized after encoding-related parse issues.

### Mistakes
- Label: FORMAT_MISS
  Evidence: Temporary script parse error due corrupted multi-byte text block.
  Root cause: Unsafe inline edits under mixed encoding display.
  Fix applied: Rewrote script with stable ASCII-safe structure.

### Cost Signals
- Commands run: 8
- Build runs: 0
- Test runs: 1
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For script stability, apply full-file rewrite if encoding corruption appears.
- Add: Keep policy text in docs; keep runtime scripts parse-safe first, then localize output with controlled encoding.
- Remove: None

### Confidence
- Delivery confidence (0-100): 91
- Verification depth: medium

## Entry: 2026-04-08 17:55 KST | Task: Run Ping Test And Write Korean Report
### Summary
- Goal: Execute Ping/Pong runtime test and produce a Korean report while keeping raw log format.
- Outcome: Test passed and report file was rewritten in Korean without changing log content.

### What Went Well
- End-to-end test evidence captured with packet type, reqTime echo, and RTT value.
- Report language policy (Korean) applied directly to the generated artifact.

### Mistakes
- Label: FORMAT_MISS
  Evidence: Auto-generated report from script remained English by default.
  Root cause: Script/report template localization was not yet enforced end-to-end.
  Fix applied: Rewrote the generated report file in Korean for this run.

### Cost Signals
- Commands run: 4
- Build runs: 0
- Test runs: 1
- Avoidable retries: 0

### Prevention Rules (Next Tasks)
- Keep: Preserve raw logs as-is.
- Add: Ensure final report artifact language is Korean before closing protocol test task.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: high

## Entry: 2026-04-09 20:49 KST | Task: Transport Hardening and Server Packet Routing Stabilization
- Goal: Improve connection lifecycle robustness, packet flood defense, and server packet handler maintainability.
- Outcome: Added inbound packet-rate budgeting in TcpSession, added TcpServer session cap/options and targeted session disconnect API, and migrated YunoServerNetwork packet routing to PacketDispatcher with per-session packet budget enforcement.
- Verification: Created and ran `scripts\test_transport_hardening.ps1`; PASS with log `Result\Log\20260409_204805_transport_hardening.log` and report `Result\Report\20260409_204805_transport_hardening_report.md`.
- Learning: Explicit transport/server boundary rate-limits plus dispatcher-based routing reduced ad-hoc branching and made future protocol extension safer.

## Entry: 2026-08-23 KST | Task: Reorganize Portfolio README
### Summary
- Goal: Explain the repository's relationship to YunoEngine and make the C++ network, asynchronous programming, and MySQL evidence easy to review.
- Outcome: Replaced the placeholder README with architecture, module responsibilities, runtime flows, code-review entry points, build/test instructions, design tradeoffs, and known limitations.

### What Went Well
- Every relative source/document link in the README was checked against the repository and resolved successfully.
- Technical claims were tied to concrete source paths and existing test reports.
- The document clearly distinguishes this technical sample from a separate completed game.

### Mistakes
- Label: FORMAT_MISS
  Evidence: The first full-file patch used delete and add operations for the same path and was rejected.
  Root cause: The patch format did not support two operations targeting one file in a single request.
  Fix applied: Replaced the existing content with one update operation and re-ran validation.

### Cost Signals
- Build runs: 0
- Test runs: 0
- Avoidable retries: 1

### Prevention Rules (Next Tasks)
- Keep: For a full rewrite of an existing short document, use one update operation instead of delete plus add.
- Add: Validate every relative link in portfolio documentation before delivery.
- Remove: None

### Confidence
- Delivery confidence (0-100): 95
- Verification depth: medium (documentation-only change; link and diff validation)
