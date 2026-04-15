param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [int]$LoginPort = 7000,
    [int]$WorldPort = 9000,
    [int]$WarmupSeconds = 3,
    [int]$StartupTimeoutSec = 10,
    [string]$FeatureName = 'party_instance_combat_inventory',
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
    param(
        [string]$Path,
        [string]$Pattern
    )

    if (-not (Test-Path $Path)) {
        return $false
    }

    return $null -ne (Select-String -Path $Path -Pattern $Pattern -SimpleMatch -ErrorAction SilentlyContinue)
}

function Find-MatchingFiles {
    param(
        [string[]]$Roots,
        [string[]]$Terms
    )

    $all = @()
    foreach ($root in $Roots) {
        if (-not (Test-Path $root)) {
            continue
        }

        $all += Get-ChildItem -Path $root -Recurse -File | Where-Object {
            $candidate = $_.FullName
            foreach ($term in $Terms) {
                if ($candidate -match $term) {
                    return $true
                }
            }
            return $false
        }
    }

    return @($all | Sort-Object -Property FullName -Unique)
}

Write-Log 'Party/instance/combat/inventory verification started.'

$packetTypePath = Join-Path $repoRoot 'YunoNetProtocol\Public\Net\PacketType.h'
$serverNetworkPath = Join-Path $repoRoot 'YunoServer\ServerNetwork\YunoServerNetwork.cpp'
$sqlInitPath = Join-Path $repoRoot 'YunoLoginServer\Sql\init_yuno_auth.sql'
$gameplayRepositoryHeaderPath = Join-Path $repoRoot 'YunoServer\Gameplay\MySqlGameplayRepository.h'
$gameplayRepositoryCppPath = Join-Path $repoRoot 'YunoServer\Gameplay\MySqlGameplayRepository.cpp'
$protocolRoots = @(
    (Join-Path $repoRoot 'YunoNetProtocol'),
    (Join-Path $repoRoot 'YunoGameProtocol')
)
$serverRoots = @(
    (Join-Path $repoRoot 'YunoServer'),
    (Join-Path $repoRoot 'YunoLoginServer')
)

$executedCommands.Add('Phase 0 ownership scan against PacketType.h, YunoGameProtocol, and YunoServerNetwork.cpp')

Add-CheckResult `
    -Name 'Phase 0 ownership files present' `
    -Passed ((Test-Path $packetTypePath) -and (Test-Path $serverNetworkPath)) `
    -Details 'Packet IDs, packet serde, and runtime registration files were located for manual ownership mapping.' `
    -Impact 'Restore missing protocol/server files before any parallel lane can verify packet ownership.'

$phase0OwnershipDetails = @(
    'Packet IDs: YunoNetProtocol/Public/Net/PacketType.h',
    'MORPG packet structs + serde: YunoGameProtocol/Public|Private/Net/MORPGPackets',
    'Server packet registration/orchestration: YunoServer/ServerNetwork/YunoServerNetwork.cpp'
) -join '; '
Write-Log "Phase 0 ownership map => $phase0OwnershipDetails"
$keyEvidence.Add("Phase 0 ownership map => $phase0OwnershipDetails")

$requiredPacketMarkers = @(
    'C2S_EnterWorld',
    'C2S_MoveInput',
    'C2S_AckSnapshot',
    'C2S_SkillCast',
    'S2C_SpawnEntity',
    'S2C_WorldSnapshot'
)
$missingPacketMarkers = @()
foreach ($marker in $requiredPacketMarkers) {
    if (-not (Test-FileContains -Path $packetTypePath -Pattern $marker)) {
        $missingPacketMarkers += $marker
    }
}
Add-CheckResult `
    -Name 'Packet type markers declared' `
    -Passed ($missingPacketMarkers.Count -eq 0) `
    -Details ($(if ($missingPacketMarkers.Count -eq 0) { 'Current world/combat packet IDs are declared in PacketType.h.' } else { 'Missing packet markers: ' + ($missingPacketMarkers -join ', ') })) `
    -Impact 'Add missing packet IDs before runtime or verification scripts can agree on protocol contracts.'

$skillCastRegistered = Test-FileContains -Path $serverNetworkPath -Pattern 'PacketType::C2S_SkillCast'
Add-CheckResult `
    -Name 'Skill cast server registration present' `
    -Passed $skillCastRegistered `
    -Details ($(if ($skillCastRegistered) { 'YunoServerNetwork.cpp references PacketType::C2S_SkillCast.' } else { 'No C2S_SkillCast registration/handling found in YunoServerNetwork.cpp.' })) `
    -Impact 'Wire the first combat path through YunoServerNetwork orchestration or an extracted manager before combat verification can pass.'

$partyProtocolFiles = @(Find-MatchingFiles -Roots $protocolRoots -Terms @('Party'))
Add-CheckResult `
    -Name 'Party protocol artifacts present' `
    -Passed ($partyProtocolFiles.Count -gt 0) `
    -Details ($(if ($partyProtocolFiles.Count -gt 0) { ($partyProtocolFiles | ForEach-Object { $_.FullName.Replace($repoRoot + '\', '') }) -join '; ' } else { 'No party protocol packet/header files found under YunoNetProtocol or YunoGameProtocol.' })) `
    -Impact 'Define party packet contracts before integration Scenario B can be automated.'

$instanceProtocolFiles = @(Find-MatchingFiles -Roots $protocolRoots -Terms @('Instance', 'Dungeon'))
Add-CheckResult `
    -Name 'Instance protocol artifacts present' `
    -Passed ($instanceProtocolFiles.Count -gt 0) `
    -Details ($(if ($instanceProtocolFiles.Count -gt 0) { ($instanceProtocolFiles | ForEach-Object { $_.FullName.Replace($repoRoot + '\', '') }) -join '; ' } else { 'No instance/dungeon protocol packet/header files found.' })) `
    -Impact 'Add instance enter/result/state packet contracts before dungeon transfer verification can run.'

$runtimeFiles = @(Find-MatchingFiles -Roots ($protocolRoots + $serverRoots) -Terms @('Party', 'Instance', 'Dungeon', 'Combat', 'Inventory', 'Repository'))
$runtimeEvidence = @($runtimeFiles | ForEach-Object { $_.FullName.Replace($repoRoot + '\', '') })
$partyRuntimePresent = @($runtimeEvidence | Where-Object { $_ -match 'Party' })
$instanceRuntimePresent = @($runtimeEvidence | Where-Object { $_ -match 'Instance|Dungeon' })
$combatRuntimePresent = @($runtimeEvidence | Where-Object { $_ -match 'Combat|SkillCast' })
$inventoryRuntimePresent = @($runtimeEvidence | Where-Object { $_ -match 'Inventory|Repository' })
$gameplayRepositoryPaths = @(
    'YunoServer\Gameplay\MySqlGameplayRepository.h',
    'YunoServer\Gameplay\MySqlGameplayRepository.cpp'
)
$gameplayRepositoryPresent = @($gameplayRepositoryPaths | Where-Object { Test-Path (Join-Path $repoRoot $_) })
$gameplayRepositoryMethodMarkers = @(
    'EnsureCharacterForUser',
    'LoadInventory',
    'GrantDemoDungeonReward'
)
$missingGameplayRepositoryMarkers = @()
foreach ($marker in $gameplayRepositoryMethodMarkers) {
    if ((-not (Test-FileContains -Path $gameplayRepositoryHeaderPath -Pattern $marker)) -and (-not (Test-FileContains -Path $gameplayRepositoryCppPath -Pattern $marker))) {
        $missingGameplayRepositoryMarkers += $marker
    }
}

Add-CheckResult `
    -Name 'Party runtime seam present' `
    -Passed ($partyRuntimePresent.Count -gt 0) `
    -Details ($(if ($partyRuntimePresent.Count -gt 0) { $partyRuntimePresent -join '; ' } else { 'No party runtime files found under YunoServer or protocol projects.' })) `
    -Impact 'Extract or add a bounded party runtime seam before three-player party verification can pass.'

Add-CheckResult `
    -Name 'Instance runtime seam present' `
    -Passed ($instanceRuntimePresent.Count -gt 0) `
    -Details ($(if ($instanceRuntimePresent.Count -gt 0) { $instanceRuntimePresent -join '; ' } else { 'No instance/dungeon runtime files found under YunoServer or protocol projects.' })) `
    -Impact 'Add an instance manager/runtime before world-to-dungeon transfer can be tested.'

Add-CheckResult `
    -Name 'Combat runtime seam present' `
    -Passed (($combatRuntimePresent.Count -gt 0) -and $skillCastRegistered) `
    -Details ($(if (($combatRuntimePresent.Count -gt 0) -and $skillCastRegistered) { $combatRuntimePresent -join '; ' } else { 'No validated combat runtime seam with skill-cast handling found.' })) `
    -Impact 'Implement the first combat path and keep YunoServerNetwork as orchestration glue before combat verification can pass.'

$inventorySchemaPresent = Test-FileContains -Path $sqlInitPath -Pattern 'inventory_items'
Add-CheckResult `
    -Name 'Inventory schema baseline present' `
    -Passed $inventorySchemaPresent `
    -Details ($(if ($inventorySchemaPresent) { 'init_yuno_auth.sql already defines inventory_items for persistence verification.' } else { 'inventory_items table definition not found in init_yuno_auth.sql.' })) `
    -Impact 'Restore the inventory schema baseline before DB-backed persistence checks can run.'

Add-CheckResult `
    -Name 'Gameplay persistence repository present' `
    -Passed ($gameplayRepositoryPresent.Count -eq $gameplayRepositoryPaths.Count) `
    -Details ($(if ($gameplayRepositoryPresent.Count -eq $gameplayRepositoryPaths.Count) { $gameplayRepositoryPresent -join '; ' } else { 'Missing gameplay persistence repository file(s): ' + (($gameplayRepositoryPaths | Where-Object { -not (Test-Path (Join-Path $repoRoot $_)) }) -join ', ') })) `
    -Impact 'Restore YunoServer gameplay persistence files before inventory/reward acceptance can be validated.'

Add-CheckResult `
    -Name 'Gameplay persistence methods declared' `
    -Passed ($missingGameplayRepositoryMarkers.Count -eq 0) `
    -Details ($(if ($missingGameplayRepositoryMarkers.Count -eq 0) { 'EnsureCharacterForUser, LoadInventory, and GrantDemoDungeonReward are declared in the gameplay repository seam.' } else { 'Missing gameplay repository markers: ' + ($missingGameplayRepositoryMarkers -join ', ') })) `
    -Impact 'Add the character/inventory/reward repository contract before Scenario D persistence verification can pass.'

Add-CheckResult `
    -Name 'Inventory/persistence runtime seam present' `
    -Passed (($inventoryRuntimePresent.Count -gt 0) -and ($gameplayRepositoryPresent.Count -eq $gameplayRepositoryPaths.Count) -and ($missingGameplayRepositoryMarkers.Count -eq 0)) `
    -Details ($(if (($inventoryRuntimePresent.Count -gt 0) -and ($gameplayRepositoryPresent.Count -eq $gameplayRepositoryPaths.Count) -and ($missingGameplayRepositoryMarkers.Count -eq 0)) { $inventoryRuntimePresent -join '; ' } else { 'Inventory protocol files may exist, but the gameplay persistence seam is incomplete or missing required repository methods.' })) `
    -Impact 'Add repository/service code for gameplay persistence before Scenario D can pass.'

$requiredScripts = @(
    'scripts\build_and_test.ps1',
    'scripts\smoke_world_enter.ps1',
    'scripts\test_party_instance_combat_inventory.ps1'
)
$missingScripts = @()
foreach ($relativePath in $requiredScripts) {
    if (-not (Test-Path (Join-Path $repoRoot $relativePath))) {
        $missingScripts += $relativePath
    }
}
Add-CheckResult `
    -Name 'Required verification scripts present' `
    -Passed ($missingScripts.Count -eq 0) `
    -Details ($(if ($missingScripts.Count -eq 0) { 'build_and_test, smoke_world_enter, and party-instance-combat-inventory scripts are present.' } else { 'Missing scripts: ' + ($missingScripts -join ', ') })) `
    -Impact 'Restore required script entry points before acceptance evidence can be generated.'

$buildScriptPath = Join-Path $repoRoot 'scripts\build_and_test.ps1'
$serverBuildTargets = @('YunoNetProtocol', 'YunoGameProtocol', 'YunoLoginServer', 'YunoServer')
if (Test-Path $buildScriptPath) {
    $buildArgs = @(
        '-ExecutionPolicy', 'Bypass',
        '-File', $buildScriptPath,
        '-Configuration', $Configuration,
        '-Platform', $Platform,
        '-Targets', ($serverBuildTargets -join ',')
    )
    $executedCommands.Add("powershell.exe $($buildArgs -join ' ')")
    Write-Log 'Running scoped server-side build_and_test.ps1 for MORPG runtime targets'
    $buildOutput = & powershell.exe @buildArgs 2>&1
    foreach ($line in $buildOutput) {
        Write-Log ([string]$line)
    }

    Add-CheckResult `
        -Name 'Scoped server-side build' `
        -Passed ($LASTEXITCODE -eq 0) `
        -Details ($(if ($LASTEXITCODE -eq 0) { 'build_and_test.ps1 passed for YunoNetProtocol, YunoGameProtocol, YunoLoginServer, and YunoServer.' } else { "Scoped build_and_test.ps1 failed with exit code $LASTEXITCODE." })) `
        -Impact 'Fix scoped server/runtime build failures before relying on smoke or integration evidence for the MORPG slice.'
}
else {
    Add-CheckResult `
        -Name 'Scoped server-side build' `
        -Passed $false `
        -Details 'build_and_test.ps1 was not found, so the scoped MORPG server build could not be executed.' `
        -Impact 'Restore scripts/build_and_test.ps1 before using this acceptance gate.'
}

if (-not $SkipSmoke) {
    $loginBinary = Resolve-BinaryPath -Name 'YunoLoginServer' -Configuration $Configuration -Platform $Platform
    $worldBinary = Resolve-BinaryPath -Name 'YunoServer' -Configuration $Configuration -Platform $Platform

    if ($loginBinary -and $worldBinary) {
        $smokeArgs = @(
            '-ExecutionPolicy', 'Bypass',
            '-File', (Join-Path $repoRoot 'scripts\smoke_world_enter.ps1'),
            '-Configuration', $Configuration,
            '-Platform', $Platform,
            '-LoginPort', "$LoginPort",
            '-WorldPort', "$WorldPort",
            '-WarmupSeconds', "$WarmupSeconds"
        )
        $executedCommands.Add("powershell.exe $($smokeArgs -join ' ')")
        Write-Log 'Running prerequisite smoke_world_enter.ps1'
        $smokeOutput = & powershell.exe @smokeArgs 2>&1
        foreach ($line in $smokeOutput) {
            Write-Log ([string]$line)
        }

        Add-CheckResult `
            -Name 'Prerequisite login-to-world smoke' `
            -Passed ($LASTEXITCODE -eq 0) `
            -Details ($(if ($LASTEXITCODE -eq 0) { 'smoke_world_enter.ps1 passed against built binaries.' } else { "smoke_world_enter.ps1 failed with exit code $LASTEXITCODE." })) `
            -Impact 'Fix baseline login/world flow before layering party-instance verification on top.'
    }
    else {
        Add-CheckResult `
            -Name 'Prerequisite login-to-world smoke' `
            -Passed $false `
            -Details 'Required binaries were not present under Bin/, so smoke_world_enter.ps1 could not be executed from this workspace.' `
            -Impact 'Build YunoLoginServer and YunoServer first, then rerun the smoke and integration scripts.'
    }
}
else {
    Add-CheckResult `
        -Name 'Prerequisite login-to-world smoke' `
        -Passed $true `
        -Details 'Smoke execution was skipped explicitly.' `
        -Impact ''

    $nextActions.Add('Run smoke_world_enter.ps1 with YUNO_DB_* configured to validate the login -> town prerequisite end-to-end.')
}

if ($failureReasons.Count -eq 0) {
    $result = 'PASS'
    $nextActions.Add('Maintain this script as the acceptance gate for party, instance, combat, and persistence.')
}
else {
    $result = 'FAIL'
    if ($nextActions.Count -eq 0) {
        $nextActions.Add('Address the failed checks above and rerun this script to regenerate evidence.')
    }
}

$report = New-Object System.Collections.Generic.List[string]
$report.Add("# Protocol Test Report: $FeatureName") | Out-Null
$report.Add('') | Out-Null
$report.Add('## Objective') | Out-Null
$report.Add('- Verify the phased MORPG demo path for party formation, dungeon instance entry, first combat path, and persistence prerequisites.') | Out-Null
$report.Add('') | Out-Null
$report.Add('## Expected Behavior') | Out-Null
$report.Add('- Phase 0 ownership is unambiguous across packet IDs, packet serde, and server registration.') | Out-Null
$report.Add('- Packet and runtime seams exist for party, instance, combat, and inventory persistence.') | Out-Null
$report.Add('- Baseline login -> town world flow is available before party/instance scenarios are attempted.') | Out-Null
$report.Add('- The script emits timestamped log/report artifacts under Result/Log and Result/Report.') | Out-Null
$report.Add('') | Out-Null
$report.Add('## Executed Commands') | Out-Null
foreach ($cmd in $executedCommands) {
    $report.Add("- $cmd") | Out-Null
}
$report.Add('') | Out-Null
$report.Add('## Result') | Out-Null
$report.Add("- $result") | Out-Null
$report.Add('') | Out-Null
$report.Add('## Key Log Evidence') | Out-Null
foreach ($evidence in $keyEvidence) {
    $report.Add("- $evidence") | Out-Null
}
if ($keyEvidence.Count -eq 0) {
    $report.Add('- See the log file for details.') | Out-Null
}
$report.Add('') | Out-Null
$report.Add('## Failure Analysis (if any)') | Out-Null
if ($failureReasons.Count -eq 0) {
    $report.Add('- Root cause: None') | Out-Null
    $report.Add('- Scope impact: The verification gate found the expected protocol/runtime seams and prerequisite smoke path.') | Out-Null
}
else {
    $report.Add('- Root cause: One or more required seams or prerequisite binaries are missing.') | Out-Null
    $report.Add("- Scope impact: $($failureReasons -join ' | ')") | Out-Null
}
$report.Add('') | Out-Null
$report.Add('## Next Actions') | Out-Null
foreach ($action in ($nextActions | Select-Object -Unique)) {
    $report.Add("- $action") | Out-Null
}

$report | Set-Content -Path $reportPath -Encoding UTF8
Write-Host "Log saved to: $logPath"
Write-Host "Report saved to: $reportPath"

if ($result -eq 'PASS') { exit 0 }
exit 1
