param(
    [string]$GameExe = 'C:\Project\MORPG_Network_Sample\Bin\x64\Debug\Game\YunoGame.exe',
    [string]$OutputPath = 'C:\Project\MORPG_Network_Sample\Image_auto.png',
    [int]$WarmupSeconds = 8
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path $GameExe)) {
    throw "Game executable not found: $GameExe"
}

$proc = $null
try {
    $proc = Start-Process -FilePath $GameExe -PassThru
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
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
}
