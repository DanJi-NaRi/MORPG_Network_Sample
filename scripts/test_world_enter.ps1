param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [int]$LoginPort = 7000,
    [int]$WorldPort = 9000,
    [int]$WarmupSeconds = 3,
    [int]$StartupTimeoutSec = 10,
    [string]$FeatureName = 'world_enter'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'TestProcessHelpers.ps1')

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$resultRoot = Join-Path $PSScriptRoot '..\Result'
$logDir = Join-Path $resultRoot 'Log'
$reportDir = Join-Path $resultRoot 'Report'
Ensure-Dir -PathValue $resultRoot
Ensure-Dir -PathValue $logDir
Ensure-Dir -PathValue $reportDir

$logPath = Join-Path $logDir ("{0}_{1}.log" -f $timestamp, $FeatureName)
$reportPath = Join-Path $reportDir ("{0}_{1}_report.md" -f $timestamp, $FeatureName)
$probeScript = Join-Path $PSScriptRoot 'world_enter_probe.ps1'

function Write-Log {
    param([string]$Message)

    $line = "[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'), $Message
    Write-Host $line
    Add-Content -Path $logPath -Value $line
}

$loginServer = $null
$worldServer = $null
$result = 'FAIL'
$failureRootCause = ''
$scopeImpact = ''
$nextActions = New-Object System.Collections.Generic.List[string]
$keyEvidence = New-Object System.Collections.Generic.List[string]
$executedCommands = New-Object System.Collections.Generic.List[string]

try {
    Write-Log 'World-enter protocol test started.'

    $executedCommands.Add("Start YunoLoginServer port=$LoginPort")
    $loginServer = Start-RequiredProcess -ProcessName 'YunoLoginServer' -Configuration $Configuration -Platform $Platform -ArgumentList @("$LoginPort")
    if (-not (Wait-TcpPortReady -Port $LoginPort -TimeoutSeconds $StartupTimeoutSec)) {
        throw "Login server startup timeout on port $LoginPort"
    }
    Write-Log "Login server ready on port $LoginPort"

    $executedCommands.Add("Start YunoServer port=$WorldPort")
    $worldServer = Start-RequiredProcess -ProcessName 'YunoServer' -Configuration $Configuration -Platform $Platform -ArgumentList @("$WorldPort")
    if (-not (Wait-TcpPortReady -Port $WorldPort -TimeoutSeconds $StartupTimeoutSec)) {
        throw "World server startup timeout on port $WorldPort"
    }
    Write-Log "World server ready on port $WorldPort"

    Start-Sleep -Seconds $WarmupSeconds

    $probeArgs = @(
        '-ExecutionPolicy', 'Bypass',
        '-File', $probeScript,
        '-LoginHost', '127.0.0.1',
        '-LoginPort', "$LoginPort",
        '-WorldHost', '127.0.0.1',
        '-WorldPort', "$WorldPort"
    )
    $executedCommands.Add("powershell.exe $($probeArgs -join ' ')")
    Write-Log 'Running world_enter_probe.ps1'

    $probeOutput = & powershell.exe @probeArgs 2>&1
    foreach ($line in $probeOutput) {
        Write-Log ([string]$line)
        if ($line -like '*PASS*') {
            $keyEvidence.Add([string]$line)
        }
    }

    if ($LASTEXITCODE -ne 0) {
        throw "world_enter_probe.ps1 failed with exit code $LASTEXITCODE"
    }

    $result = 'PASS'
    if ($keyEvidence.Count -eq 0) {
        $keyEvidence.Add('Probe completed successfully.')
    }
    $nextActions.Add('Maintain this probe as the regression baseline for login -> world enter -> snapshot ack flow.')
    Write-Log 'World-enter protocol test passed.'
}
catch {
    $result = 'FAIL'
    $failureRootCause = $_.Exception.Message
    $scopeImpact = 'The login-to-world-enter E2E verification failed or one of the prerequisite servers was not ready.'
    $nextActions.Add('Check login/world server startup prerequisites and DB environment variables.')
    $nextActions.Add('Use the world_enter_probe.ps1 log to isolate the failing stage: register, login, enter-world, or snapshot ACK.')
    Write-Log "FAIL: $failureRootCause"
}
finally {
    Stop-ProcessSafe -ProcessObj $worldServer
    Stop-ProcessSafe -ProcessObj $loginServer
}

if ($nextActions.Count -eq 0) {
    $nextActions.Add('추가 조치 없음.')
}

$report = @()
$report += "# Protocol Test Report: $FeatureName"
$report += ''
$report += '## Objective'
$report += '- Verify the real runtime login -> world-enter -> spawn -> world-snapshot -> snapshot-ack flow.'
$report += ''
$report += '## Expected Behavior'
$report += '- Registration or existing-account login succeeds.'
$report += '- The login response returns a valid world host/port and login token.'
$report += '- The world server sends SpawnEntity and WorldSnapshot packets.'
$report += '- After the client ACKs the first snapshot, a later snapshot reflects the ACK via baseSnapshotId.'
$report += ''
$report += '## Executed Commands'
foreach ($cmd in $executedCommands) {
    $report += "- $cmd"
}
$report += ''
$report += '## Result'
$report += "- $result"
$report += ''
$report += '## Key Log Evidence'
if ($keyEvidence.Count -gt 0) {
    foreach ($e in $keyEvidence) {
        $report += "- $e"
    }
}
else {
    $report += '- See the log file for details.'
}
$report += ''
$report += '## Failure Analysis (if any)'
if ($result -eq 'FAIL') {
    $report += "- Root cause: $failureRootCause"
    $report += "- Scope impact: $scopeImpact"
}
else {
    $report += '- Root cause: None'
    $report += '- Scope impact: The login-to-world-enter E2E path was verified successfully.'
}
$report += ''
$report += '## Next Actions'
foreach ($action in $nextActions) {
    $report += "- $action"
}

$report | Set-Content -Path $reportPath -Encoding UTF8
Write-Host "Log saved to: $logPath"
Write-Host "Report saved to: $reportPath"

if ($result -eq 'PASS') { exit 0 }
exit 1
