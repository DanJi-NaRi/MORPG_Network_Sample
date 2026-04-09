param(
    [int]$WarmupSeconds = 3,
    [int]$RunSeconds = 10,
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-BinaryPath {
    param(
        [string]$Name
    )

    $candidates = @(
        ".\Bin\$Platform\$Configuration\LoginServer\$Name.exe",
        ".\Bin\$Platform\$Configuration\Server\$Name.exe",
        ".\Bin\$Platform\$Configuration\$Name.exe",
        ".\Bin\$Configuration\$Name.exe",
        ".\Bin\$Name.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return (Resolve-Path $path).Path
        }
    }

    return $null
}

function Start-RequiredProcess {
    param(
        [string]$ProcessName
    )

    $exePath = Resolve-BinaryPath -Name $ProcessName
    if (-not $exePath) {
        throw "Required binary not found: $ProcessName.exe"
    }

    Write-Host "Starting $ProcessName => $exePath"
    return Start-Process -FilePath $exePath -PassThru
}

function Stop-ProcessSafe {
    param(
        [System.Diagnostics.Process]$ProcessObj
    )

    if ($null -eq $ProcessObj) {
        return
    }

    try {
        if (-not $ProcessObj.HasExited) {
            Stop-Process -Id $ProcessObj.Id -Force
        }
    }
    catch {
        Write-Warning "Failed to stop process id=$($ProcessObj.Id): $($_.Exception.Message)"
    }
}

Write-Host "==> smoke_world_enter.ps1 started"

$loginServer = $null
$worldServer = $null
$probeExitCode = 0

try {
    $loginServer = Start-RequiredProcess -ProcessName "YunoLoginServer"
    Start-Sleep -Seconds $WarmupSeconds

    $worldServer = Start-RequiredProcess -ProcessName "YunoServer"
    Start-Sleep -Seconds $WarmupSeconds

    # Optional functional probe:
    # Implement scripts\world_enter_probe.ps1 to validate real WorldEnter protocol flow.
    $probeScript = ".\scripts\world_enter_probe.ps1"
    if (Test-Path $probeScript) {
        Write-Host "Running WorldEnter probe: $probeScript"
        & powershell -ExecutionPolicy Bypass -File $probeScript
        $probeExitCode = $LASTEXITCODE
        if ($probeExitCode -ne 0) {
            throw "WorldEnter probe failed with exit code: $probeExitCode"
        }
    }
    else {
        Write-Host "No world_enter_probe.ps1 found. Running process-liveness smoke only."
        Start-Sleep -Seconds $RunSeconds
    }

    Write-Host "==> smoke_world_enter.ps1 completed successfully."
}
finally {
    Stop-ProcessSafe -ProcessObj $worldServer
    Stop-ProcessSafe -ProcessObj $loginServer
}
