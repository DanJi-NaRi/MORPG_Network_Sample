# Protocol Test Operations Guide

Purpose:
- Standardize protocol validation execution and artifacts.
- Reduce repeated mistakes and avoid missing evidence after implementation.

## 1) Mandatory Outputs
- Test script path:
  - `C:\Project\MORPG_Network_Sample\scripts\test_<protocol_or_feature>.ps1`
- Execution log path:
  - `C:\Project\MORPG_Network_Sample\Result\Log\<timestamp>_<protocol_or_feature>.log`
- Analysis report path:
  - `C:\Project\MORPG_Network_Sample\Result\Report\<timestamp>_<protocol_or_feature>_report.md`
- Log and report language:
  - English by default
  - Keep generated headings, summaries, and analysis in English

## 2) Directory Policy
- Ensure these directories exist before test execution:
  - `C:\Project\MORPG_Network_Sample\Result\Log`
  - `C:\Project\MORPG_Network_Sample\Result\Report`

## 3) Test Script Baseline
- Script must include:
1. setup section (target binaries, env assumptions)
2. execution section (send protocol request, capture response)
3. validation section (expected vs actual)
4. exit code policy (`0` pass, non-zero fail)
- Script should write concise console output suitable for log analysis.

## 4) Report Template
Use this structure:

```text
# Protocol Test Report: <protocol_or_feature>

## Objective
- 

## Expected Behavior
- 

## Executed Commands
- 

## Result
- PASS or FAIL

## Key Log Evidence
- 

## Failure Analysis (if any)
- Root cause:
- Scope impact:

## Next Actions
- 
```

## 5) Failure/Blocker Policy
- If full test cannot run (missing dependency, server down, credentials unavailable):
1. still generate log with attempted commands
2. still generate report with blocker details and exact prerequisite list
3. do not claim completion as fully verified
