param(
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [int]$LoginPort = 7000,
    [int]$WorldPort = 9000,
    [int]$WarmupSeconds = 10,
    [string]$OutputPath = 'C:\Project\MORPG_Network_Sample\Image_runtime.png'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'TestProcessHelpers.ps1')

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$loginServer = $null
$worldServer = $null
$gameProc = $null

try {
    $loginServer = Start-RequiredProcess -ProcessName 'YunoLoginServer' -Configuration $Configuration -Platform $Platform -ArgumentList @("$LoginPort")
    if (-not (Wait-TcpPortReady -Port $LoginPort -TimeoutSeconds 10)) {
        throw "Login server start timeout. Port $LoginPort not reachable."
    }

    $worldServer = Start-RequiredProcess -ProcessName 'YunoServer' -Configuration $Configuration -Platform $Platform -ArgumentList @("$WorldPort")
    if (-not (Wait-TcpPortReady -Port $WorldPort -TimeoutSeconds 10)) {
        throw "World server start timeout. Port $WorldPort not reachable."
    }

    $gameExe = Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..')).Path 'Bin\x64\Debug\Game\YunoGame.exe'
    if (-not (Test-Path $gameExe)) {
        throw "Game executable not found: $gameExe"
    }

    $stamp = Get-Date -Format 'MMddHHmmss'
    $loginId = "cap_$stamp"
    $loginPw = "pw_$stamp"

    $inputLines = @(
        '2',
        $loginId,
        $loginPw,
        '1',
        $loginId,
        $loginPw
    ) -join [Environment]::NewLine

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = 'cmd.exe'
    $psi.Arguments = "/c `"$gameExe`""
    $psi.UseShellExecute = $false
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.WorkingDirectory = Split-Path $gameExe -Parent

    $gameProc = New-Object System.Diagnostics.Process
    $gameProc.StartInfo = $psi
    [void]$gameProc.Start()
    $gameProc.StandardInput.WriteLine($inputLines)
    $gameProc.StandardInput.Close()

    Start-Sleep -Seconds $WarmupSeconds

    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()

    Write-Output $OutputPath
}
finally {
    if ($gameProc -and -not $gameProc.HasExited) {
        $gameProc.Kill()
        $gameProc.WaitForExit()
    }
    Stop-ProcessSafe -ProcessObj $worldServer
    Stop-ProcessSafe -ProcessObj $loginServer
}
