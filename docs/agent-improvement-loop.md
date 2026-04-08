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
