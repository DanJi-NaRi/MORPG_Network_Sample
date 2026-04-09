param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Write-Host "==> test_transport_hardening.ps1 started"
Write-Host "Configuration: $Configuration"
Write-Host "Platform: $Platform"

& powershell -ExecutionPolicy Bypass -File ".\scripts\build_and_test.ps1" -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "build_and_test.ps1 failed with exit code: $LASTEXITCODE"
}

& powershell -ExecutionPolicy Bypass -File ".\scripts\smoke_world_enter.ps1" -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "smoke_world_enter.ps1 failed with exit code: $LASTEXITCODE"
}

Write-Host "==> test_transport_hardening.ps1 completed successfully"
