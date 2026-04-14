param(
    [int]$WarmupSeconds = 3,
    [int]$RunSeconds = 10,
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [int]$LoginPort = 7000,
    [int]$WorldPort = 9000
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'TestProcessHelpers.ps1')

Write-Host '==> smoke_world_enter.ps1 started'

$loginServer = $null
$worldServer = $null
$probeExitCode = 0

try {
    $loginServer = Start-RequiredProcess -ProcessName 'YunoLoginServer' -Configuration $Configuration -Platform $Platform -ArgumentList @("$LoginPort")
    if (-not (Wait-TcpPortReady -Port $LoginPort -TimeoutSeconds 10)) {
        throw "Login server start timeout. Port $LoginPort not reachable."
    }
    Start-Sleep -Seconds $WarmupSeconds

    $worldServer = Start-RequiredProcess -ProcessName 'YunoServer' -Configuration $Configuration -Platform $Platform -ArgumentList @("$WorldPort")
    if (-not (Wait-TcpPortReady -Port $WorldPort -TimeoutSeconds 10)) {
        throw "World server start timeout. Port $WorldPort not reachable."
    }
    Start-Sleep -Seconds $WarmupSeconds

    $probeScript = Join-Path $PSScriptRoot 'world_enter_probe.ps1'
    if (Test-Path $probeScript) {
        Write-Host "Running WorldEnter probe: $probeScript"
        & powershell.exe -ExecutionPolicy Bypass -File $probeScript -LoginHost '127.0.0.1' -LoginPort $LoginPort -WorldHost '127.0.0.1' -WorldPort $WorldPort
        $probeExitCode = $LASTEXITCODE
        if ($probeExitCode -ne 0) {
            throw "WorldEnter probe failed with exit code: $probeExitCode"
        }
    }
    else {
        Write-Host 'No world_enter_probe.ps1 found. Running process-liveness smoke only.'
        Start-Sleep -Seconds $RunSeconds
    }

    Write-Host '==> smoke_world_enter.ps1 completed successfully.'
}
finally {
    Stop-ProcessSafe -ProcessObj $worldServer
    Stop-ProcessSafe -ProcessObj $loginServer
}
