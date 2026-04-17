param()

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

function Test-Contains {
    param(
        [string]$Path,
        [string]$Pattern
    )

    if (-not (Test-Path $Path)) {
        return $false
    }

    return Select-String -Path $Path -Pattern $Pattern -SimpleMatch -Quiet
}

$checks = @(
    @{
        Name = 'Widget anchor API present';
        Passed = (Test-Contains (Join-Path $repoRoot 'YunoEngine\Public\UI\Widget.h') 'SetAnchor(UIDirection dir)');
        Details = 'Widget exposes anchor configuration for child placement.'
    },
    @{
        Name = 'Widget anchor offset resolver present';
        Passed = (Test-Contains (Join-Path $repoRoot 'YunoEngine\Private\UI\Widget.cpp') 'ResolveParentAnchorOffset() const');
        Details = 'Widget base computes parent anchor offset before local translation composition.'
    },
    @{
        Name = 'Rotated/root rect fallback restored';
        Passed = (Test-Contains (Join-Path $repoRoot 'YunoEngine\Private\UI\Widget.cpp') 'UpdateRectWorld();');
        Details = 'Widget rect update falls back to world-space rect calculation when fast path does not apply.'
    },
    @{
        Name = 'UIScope helper added';
        Passed = (Test-Path (Join-Path $repoRoot 'YunoEngine\Public\UI\UITool\UIScope.h'));
        Details = 'Code-first UI helper surface exists.'
    },
    @{
        Name = 'TestScene migrated to UIScope';
        Passed = (Test-Contains (Join-Path $repoRoot 'YunoGame\Scenes\TestScene.cpp') 'UIScope ui(*this);');
        Details = 'TestScene HUD uses the new helper entry point.'
    },
    @{
        Name = 'TestScene uses labeled button helper';
        Passed = (Test-Contains (Join-Path $repoRoot 'YunoGame\Scenes\TestScene.cpp') 'CreateLabeledButton(');
        Details = 'TestScene creates action and party-row buttons through the new helper path.'
    }
)

$failed = @($checks | Where-Object { -not $_.Passed })

foreach ($check in $checks) {
    $status = if ($check.Passed) { 'PASS' } else { 'FAIL' }
    Write-Host ("[{0}] {1} - {2}" -f $status, $check.Name, $check.Details)
}

if ($failed.Count -gt 0) {
    Write-Error ("UI base refactor checks failed: {0}" -f (($failed | ForEach-Object { $_.Name }) -join ', '))
}

Write-Host 'All UI base refactor checks passed.'
