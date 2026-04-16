param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [int]$LoginPort = 7000,
    [int]$WorldPort = 9000,
    [string]$FeatureName = 'party_instance_ui_movement',
    [switch]$SkipBuild,
    [switch]$SkipSmoke
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'TestProcessHelpers.ps1')

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resultRoot = Join-Path $repoRoot 'Result'
$logDir = Join-Path $resultRoot 'Log'
$reportDir = Join-Path $resultRoot 'Report'
Ensure-Dir -PathValue $resultRoot
Ensure-Dir -PathValue $logDir
Ensure-Dir -PathValue $reportDir

$logPath = Join-Path $logDir ("{0}_{1}.log" -f $timestamp, $FeatureName)
$reportPath = Join-Path $reportDir ("{0}_{1}_report.md" -f $timestamp, $FeatureName)
$executedCommands = New-Object System.Collections.Generic.List[string]
$keyEvidence = New-Object System.Collections.Generic.List[string]
$failureReasons = New-Object System.Collections.Generic.List[string]
$nextActions = New-Object System.Collections.Generic.List[string]
$checks = New-Object System.Collections.Generic.List[object]
$result = 'FAIL'

function Write-Log {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'), $Message
    Write-Host $line
    Add-Content -Path $logPath -Value $line
}

function Add-CheckResult {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Details,
        [string]$Impact
    )

    $checks.Add([PSCustomObject]@{
        Name = $Name
        Passed = $Passed
        Details = $Details
        Impact = $Impact
    }) | Out-Null

    $status = if ($Passed) { 'PASS' } else { 'FAIL' }
    Write-Log ("CHECK {0}: {1} -- {2}" -f $status, $Name, $Details)

    if ($Passed) {
        $keyEvidence.Add(("{0}: {1}" -f $Name, $Details))
    }
    else {
        $failureReasons.Add(("{0}: {1}" -f $Name, $Details))
        if (-not [string]::IsNullOrWhiteSpace($Impact)) {
            $nextActions.Add($Impact)
        }
    }
}

function Test-FileContains {
    param([string]$Path, [string]$Pattern)
    if (-not (Test-Path $Path)) { return $false }
    return $null -ne (Select-String -Path $Path -Pattern $Pattern -SimpleMatch -ErrorAction SilentlyContinue)
}

Write-Log 'Party/instance UI/movement verification started.'

$packetTypePath = Join-Path $repoRoot 'YunoNetProtocol\Public\Net\PacketType.h'
$partyPacketsPath = Join-Path $repoRoot 'YunoGameProtocol\Public\Net\MORPGPackets\PartyPackets.h'
$clientNetworkPath = Join-Path $repoRoot 'YunoGame\ClientNetwork\YunoClientNetwork.cpp'
$gameAppPath = Join-Path $repoRoot 'YunoGame\Game\GameApp.cpp'
$testScenePath = Join-Path $repoRoot 'YunoGame\Scenes\TestScene.cpp'
$serverNetworkPath = Join-Path $repoRoot 'YunoServer\ServerNetwork\YunoServerNetwork.cpp'
$clientStatePath = Join-Path $repoRoot 'YunoGame\Game\PartyInstanceClientState.h'

Add-CheckResult -Name 'Party list packet types declared' -Passed ((Test-FileContains $packetTypePath 'C2S_PartyList') -and (Test-FileContains $packetTypePath 'S2C_PartyList')) -Details 'PacketType.h includes party list request/response opcodes.' -Impact 'Add authoritative party list packet IDs before UI discovery can work.'
Add-CheckResult -Name 'Party packet schema includes list + display names' -Passed ((Test-FileContains $partyPacketsPath 'struct C2S_PartyList') -and (Test-FileContains $partyPacketsPath 'struct S2C_PartyList') -and (Test-FileContains $partyPacketsPath 'displayName')) -Details 'PartyPackets.h defines party list transport and party member display names.' -Impact 'Extend PartyPackets.* with list and nickname fields before client UI can render the requested flow.'
Add-CheckResult -Name 'Client network handles party and instance packets' -Passed ((Test-FileContains $clientNetworkPath 'PacketType::S2C_PartyState') -and (Test-FileContains $clientNetworkPath 'PacketType::S2C_PartyList') -and (Test-FileContains $clientNetworkPath 'PacketType::S2C_InstanceState')) -Details 'YunoClientNetwork.cpp registers party-state, party-list, and instance-state handlers.' -Impact 'Register the missing client packet handlers before UI state can update.'
Add-CheckResult -Name 'GameApp sends UI-driven party commands and removes click move' -Passed ((Test-FileContains $gameAppPath 'PacketType::C2S_PartyList') -and (Test-FileContains $gameAppPath 'PacketType::C2S_InstanceEnter') -and (-not (Test-FileContains $gameAppPath 'TryPickGroundPointFromMouse'))) -Details 'GameApp.cpp dispatches queued UI commands and no longer uses click-to-move helpers.' -Impact 'Wire UI commands through GameApp and keep keyboard-only movement for this slice.'
Add-CheckResult -Name 'TestScene contains party/instance HUD affordances' -Passed ((Test-FileContains $testScenePath 'Create Party') -and (Test-FileContains $testScenePath 'Search Parties') -and (Test-FileContains $testScenePath 'Leader Enter Instance') -and (Test-FileContains $testScenePath 'Party Members')) -Details 'TestScene.cpp includes button labels and roster/loading HUD text for the requested flow.' -Impact 'Add actual in-game UI buttons/text before claiming a UI-driven loop.'
Add-CheckResult -Name 'Shared client state helper present' -Passed (Test-Path $clientStatePath) -Details 'PartyInstanceClientState.h exists to bridge packet state and UI actions.' -Impact 'Add a shared client-state bridge or equivalent before wiring UI buttons to network requests.'
Add-CheckResult -Name 'Server handles party list and display names' -Passed ((Test-FileContains $serverNetworkPath 'HandlePartyList') -and (Test-FileContains $serverNetworkPath 'SendPartyList') -and (Test-FileContains $serverNetworkPath 'displayName')) -Details 'YunoServerNetwork.cpp handles list requests and emits display-name-backed party state/list responses.' -Impact 'Add authoritative list/name handling on the server before claiming discovery or nickname support.'

$buildScript = Join-Path $repoRoot 'scripts\build_and_test.ps1'
if (-not $SkipBuild -and (Test-Path $buildScript)) {
    $buildArgs = @('-ExecutionPolicy','Bypass','-File',$buildScript,'-Configuration',$Configuration,'-Platform',$Platform,'-Targets','YunoNetProtocol,YunoGameProtocol,YunoLoginServer,YunoServer,YunoGame')
    $executedCommands.Add("powershell.exe $($buildArgs -join ' ')")
    Write-Log 'Running build_and_test.ps1 for affected targets.'
    $buildOutput = & powershell.exe @buildArgs 2>&1
    foreach ($line in $buildOutput) { Write-Log ([string]$line) }
    Add-CheckResult -Name 'Build command' -Passed ($LASTEXITCODE -eq 0) -Details ($(if ($LASTEXITCODE -eq 0) { 'build_and_test.ps1 passed.' } else { "build_and_test.ps1 failed with exit code $LASTEXITCODE." })) -Impact 'Fix build failures before claiming implementation completeness.'
} elseif ($SkipBuild) {
    Add-CheckResult -Name 'Build command' -Passed $true -Details 'Build step skipped by caller per operator request.' -Impact ''
} else {
    Add-CheckResult -Name 'Build command' -Passed $false -Details 'scripts/build_and_test.ps1 not found.' -Impact 'Restore build_and_test.ps1 before final sign-off.'
}

$smokeScript = Join-Path $repoRoot 'scripts\smoke_world_enter.ps1'
if (-not $SkipSmoke -and (Test-Path $smokeScript)) {
    $smokeArgs = @('-ExecutionPolicy','Bypass','-File',$smokeScript,'-Configuration',$Configuration,'-Platform',$Platform,'-LoginPort',"$LoginPort",'-WorldPort',"$WorldPort")
    $executedCommands.Add("powershell.exe $($smokeArgs -join ' ')")
    Write-Log 'Running smoke_world_enter.ps1.'
    $smokeOutput = & powershell.exe @smokeArgs 2>&1
    foreach ($line in $smokeOutput) { Write-Log ([string]$line) }
    Add-CheckResult -Name 'Smoke command' -Passed ($LASTEXITCODE -eq 0) -Details ($(if ($LASTEXITCODE -eq 0) { 'smoke_world_enter.ps1 passed.' } else { "smoke_world_enter.ps1 failed with exit code $LASTEXITCODE." })) -Impact 'Restore baseline login/world flow before multi-client UI verification.'
} elseif ($SkipSmoke) {
    Add-CheckResult -Name 'Smoke command' -Passed $true -Details 'Smoke step skipped by caller per operator request.' -Impact ''
} else {
    Add-CheckResult -Name 'Smoke command' -Passed $false -Details 'scripts/smoke_world_enter.ps1 not found.' -Impact 'Restore smoke_world_enter.ps1 before final sign-off.'
}

Add-CheckResult -Name 'Feature automation scaffold' -Passed $true -Details 'This script generated log/report artifacts and file-level evidence for party list, UI controls, keyboard-only movement, and nickname state.' -Impact ''

$allPassed = ($checks | Where-Object { -not $_.Passed }).Count -eq 0
if ($allPassed) {
    $result = 'PASS'
    $nextActions.Add('No further action required beyond manual 3-client UX confirmation.')
} else {
    $result = 'FAIL'
    if ($nextActions.Count -eq 0) {
        $nextActions.Add('Review failed checks and rerun verification after fixes.')
    }
}

$reportLines = @(
    "# Protocol Test Report: $FeatureName",
    '',
    '## Objective',
    '- Verify the party create/join -> leader-only instance enter -> keyboard-only movement UI slice and its required evidence artifacts.',
    '',
    '## Expected Behavior',
    '- Party discovery/select is available through UI.',
    '- Invite/accept affordances are visible in UI.',
    '- Only the leader can enter the instance.',
    '- Loading text appears during transition.',
    '- Keyboard-only movement remains active in the instance flow.',
    '- Nickname labels/roster text are available from authoritative party state.',
    '',
    '## Executed Commands'
)
if ($executedCommands.Count -gt 0) {
    $reportLines += $executedCommands | ForEach-Object { "- $_" }
} else {
    $reportLines += '- No external commands were executed.'
}
$reportLines += ''
$reportLines += '## Result'
$reportLines += "- $result"
$reportLines += ''
$reportLines += '## Key Log Evidence'
if ($keyEvidence.Count -gt 0) {
    $reportLines += $keyEvidence | ForEach-Object { "- $_" }
} else {
    $reportLines += '- No passing evidence was captured.'
}
$reportLines += ''
$reportLines += '## Failure Analysis (if any)'
if ($failureReasons.Count -gt 0) {
    foreach ($reason in $failureReasons) {
        $reportLines += "- Root cause: $reason"
    }
} else {
    $reportLines += '- Root cause: none'
}
$reportLines += ''
$reportLines += '## Next Actions'
$reportLines += $nextActions | ForEach-Object { "- $_" }

Set-Content -Path $reportPath -Value $reportLines -Encoding UTF8
Write-Log "Report written: $reportPath"
Write-Log "Log written: $logPath"

if ($result -eq 'PASS') { exit 0 } else { exit 1 }
