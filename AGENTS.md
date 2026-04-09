# AGENTS.md

## Goal
- Use Codex Agent consistently in this project for learning and development.
- Do not return only results. Always explain why a specific approach was chosen.

## Core Response Rules
- For every technical decision, include:
1. Chosen approach
2. One or two realistic alternatives
3. Tradeoff-based reason for the final choice
- When the user is learning, prioritize decision process over final output.
- If the request is ambiguous, state assumptions before implementation.

## Workflow
- Work in small units:
1. Confirm requirement
2. Explain design choice and why
3. Implement
4. Verify (build/test)
5. Report result and next improvement options
- Prefer incremental changes over one large risky change.

## Code Change Principles
- Preserve existing project style and structure unless change is justified.
- Avoid unnecessary refactors.
- For bug fixes, include reproduction condition and verification method when possible.

## Review Priorities
- When asked to review code, prioritize in this order:
1. Logic/functionality bugs
2. Crash/runtime risk
3. Network/concurrency risk
4. Missing tests and regression risk
5. Style/readability

## Server-Focused Checks
- For server/network code, always check:
1. Connection lifecycle (connect/disconnect/reconnect)
2. Packet boundary and validation (length/type/range)
3. Thread safety (lock scope/race conditions)
4. Failure handling (timeout/retry/exception logs)
- For performance suggestions, prefer measured metrics (TPS, latency, CPU) over guesses.

## Basic Prompt Template (Learning)
```text
Goal:
- (What to build)

Constraints:
- (Tech stack, forbidden options, perf/security constraints)

Expected output:
1) Chosen approach + reason
2) Alternative comparison
3) Implementation steps
4) Verification method
```

## Required References For Protocol Tasks
- Before protocol-related work, read:
  - `C:\Project\MORPG_Network_Sample\docs\protocol-template.md`
  - `C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.en.md`
  - `C:\Project\MORPG_Network_Sample\docs\protocol-style-guide.ko.md`
  - `C:\Project\MORPG_Network_Sample\docs\agent-active-rules.md`
  - `C:\Project\MORPG_Network_Sample\docs\protocol-test-ops.md`
- For protocol feature implementation, run:
  - `powershell -ExecutionPolicy Bypass -File .\scripts\build_and_test.ps1`
  - `powershell -ExecutionPolicy Bypass -File .\scripts\smoke_world_enter.ps1`
- If protocol document and implementation differ, update both and report mismatch explicitly.
- Do not declare protocol feature complete unless build/test/smoke steps have been executed and results are reported.

## Protocol Test Automation Rule
- When a protocol feature is requested, the Agent must do these steps without requiring a separate user prompt:
1. Create or update a protocol-specific test script under `C:\Project\MORPG_Network_Sample\scripts\` using this naming:
   - `test_<protocol_or_feature>.ps1`
2. Ensure result directories exist:
   - `C:\Project\MORPG_Network_Sample\Result\Log`
   - `C:\Project\MORPG_Network_Sample\Result\Report`
3. Execute the protocol test script after implementation.
4. Save execution logs to:
   - `C:\Project\MORPG_Network_Sample\Result\Log\<timestamp>_<protocol_or_feature>.log`
5. Generate an analysis report in Markdown:
   - `C:\Project\MORPG_Network_Sample\Result\Report\<timestamp>_<protocol_or_feature>_report.md`
   - Report language policy: Korean by default
6. Report must include:
   - objective and expected behavior
   - executed commands
   - pass/fail result
   - key log evidence
   - root cause for failures
   - next actions
- If runtime environment prevents execution, still generate:
1. attempted command log
2. blocker analysis report with concrete missing prerequisites

## Agent Learning Loop Rule
- For every meaningful task, the Agent must:
1. Read `C:\Project\MORPG_Network_Sample\docs\agent-active-rules.md` before implementation.
2. Apply active prevention rules from `agent-active-rules.md`.
3. Append one new entry to `C:\Project\MORPG_Network_Sample\docs\agent-improvement-loop.md` after task completion (append-only).
4. Read history file only when needed (for repeated failures, unclear root cause, or policy conflict).

## Do Not
- Do not make claims without evidence.
- Do not provide a single solution without alternatives.
- Do not declare completion without verification.
