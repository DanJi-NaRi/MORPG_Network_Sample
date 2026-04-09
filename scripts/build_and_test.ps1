param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-MSBuildPath {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) {
            return $path
        }
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($installPath) {
            $resolved = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $resolved) {
                return $resolved
            }
        }
    }

    throw "MSBuild.exe not found."
}

function Invoke-MSBuildTarget {
    param(
        [string]$MSBuildPath,
        [string]$Target
    )

    Write-Host ""
    Write-Host "==> Building target: $Target ($Configuration|$Platform)"
    & $MSBuildPath ".\YunoEngine.sln" "/t:$Target" "/p:Configuration=$Configuration" "/p:Platform=$Platform" "/m:1" "/nologo" "/v:minimal" "/clp:ErrorsOnly"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed for target: $Target"
    }
}

Write-Host "==> build_and_test.ps1 started"
Write-Host "Configuration: $Configuration"
Write-Host "Platform: $Platform"

if (-not $SkipBuild) {
    $msbuild = Resolve-MSBuildPath
    Write-Host "MSBuild: $msbuild"

    $targets = @(
        "YunoNetProtocol",
        "YunoGameProtocol",
        "YunoLoginServer",
        "YunoServer",
        "YunoGame"
    )

    foreach ($target in $targets) {
        Invoke-MSBuildTarget -MSBuildPath $msbuild -Target $target
    }
}
else {
    Write-Host "Build step skipped."
}

# Hook point: run project-specific tests when available.
$testScript = ".\scripts\run_tests.ps1"
if (Test-Path $testScript) {
    Write-Host ""
    Write-Host "==> Running test script: $testScript"
    & powershell -ExecutionPolicy Bypass -File $testScript
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed: $testScript"
    }
}
else {
    Write-Host ""
    Write-Host "==> No run_tests.ps1 found. Skipping explicit test phase."
}

Write-Host ""
Write-Host "==> build_and_test.ps1 completed successfully."
