param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [int]$LoginPort = 7000,
    [int]$WorldPort = 9000,
    [switch]$SkipBuild,
    [switch]$SkipSmoke,
    [switch]$BuildOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Write-Host '[party-popup-social-flow] starting verification workflow'

$buildScript = Join-Path $PSScriptRoot 'build_and_test.ps1'
$smokeScript = Join-Path $PSScriptRoot 'smoke_world_enter.ps1'

if (-not $SkipBuild) {
    if (-not (Test-Path $buildScript)) {
        throw '[party-popup-social-flow] scripts/build_and_test.ps1 not found.'
    }

    Write-Host '[party-popup-social-flow] running build_and_test.ps1'
    & powershell.exe -ExecutionPolicy Bypass -File $buildScript -Configuration $Configuration -Platform $Platform
    if ($LASTEXITCODE -ne 0) {
        throw "[party-popup-social-flow] build_and_test.ps1 failed with exit code $LASTEXITCODE."
    }
}
else {
    Write-Host '[party-popup-social-flow] build step skipped by caller'
}

if ($BuildOnly) {
    Write-Host '[party-popup-social-flow] build-only mode complete'
    exit 0
}

if (-not $SkipSmoke) {
    if (-not (Test-Path $smokeScript)) {
        throw '[party-popup-social-flow] scripts/smoke_world_enter.ps1 not found.'
    }

    Write-Host '[party-popup-social-flow] running smoke_world_enter.ps1'
    & powershell.exe -ExecutionPolicy Bypass -File $smokeScript -Configuration $Configuration -Platform $Platform -LoginPort $LoginPort -WorldPort $WorldPort
    if ($LASTEXITCODE -ne 0) {
        throw "[party-popup-social-flow] smoke_world_enter.ps1 failed with exit code $LASTEXITCODE."
    }
}
else {
    Write-Host '[party-popup-social-flow] smoke step skipped by caller'
}

Write-Host '[party-popup-social-flow] baseline automation passed'
Write-Host '[party-popup-social-flow] Manual validation checklist:'
Write-Host '  1. Launch three clients and connect them to the server.'
Write-Host '  2. Client A opens Party Menu and creates a party from the popup.'
Write-Host '  3. Client B searches parties, selects A''s party, and sends a join request.'
Write-Host '  4. Client A opens Approve Requests and accepts B.'
Write-Host '  5. Client A opens Invite Players, selects Client C, and sends an invite.'
Write-Host '  6. Client C opens Received Invites and accepts or rejects.'
Write-Host '  7. Confirm roster placement, status text, stale-row cleanup, and leader-only instance button behavior.'
