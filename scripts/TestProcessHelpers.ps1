Set-StrictMode -Version Latest

function Ensure-Dir {
    param([string]$PathValue)

    if (-not (Test-Path $PathValue)) {
        New-Item -ItemType Directory -Path $PathValue | Out-Null
    }
}

function Resolve-BinaryPath {
    param(
        [string]$Name,
        [string]$Configuration = 'Debug',
        [string]$Platform = 'x64'
    )

    $candidates = @(
        ".\\Bin\\$Platform\\$Configuration\\LoginServer\\$Name.exe",
        ".\\Bin\\$Platform\\$Configuration\\Server\\$Name.exe",
        ".\\Bin\\$Platform\\$Configuration\\Game\\$Name.exe",
        ".\\Bin\\$Platform\\$Configuration\\$Name.exe",
        ".\\Bin\\$Configuration\\$Name.exe",
        ".\\Bin\\$Name.exe"
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
        [string]$ProcessName,
        [string]$Configuration = 'Debug',
        [string]$Platform = 'x64',
        [string[]]$ArgumentList = @()
    )

    $exePath = Resolve-BinaryPath -Name $ProcessName -Configuration $Configuration -Platform $Platform
    if (-not $exePath) {
        throw "Required binary not found: $ProcessName.exe"
    }

    Write-Host "Starting $ProcessName => $exePath $($ArgumentList -join ' ')"
    return Start-Process -FilePath $exePath -ArgumentList $ArgumentList -PassThru
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

function Wait-TcpPortReady {
    param(
        [string]$HostName = '127.0.0.1',
        [int]$Port,
        [int]$TimeoutSeconds = 10,
        [int]$RetryDelayMs = 200
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $probe = $null
        try {
            $probe = New-Object System.Net.Sockets.TcpClient
            $probe.Connect($HostName, $Port)
            $probe.Close()
            return $true
        }
        catch {
            Start-Sleep -Milliseconds $RetryDelayMs
        }
        finally {
            if ($null -ne $probe) {
                $probe.Dispose()
            }
        }
    }

    return $false
}
