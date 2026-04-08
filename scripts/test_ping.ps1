param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [int]$Port = 9000,
    [int]$StartupTimeoutSec = 10
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Ensure-Dir {
    param([string]$PathValue)
    if (-not (Test-Path $PathValue)) {
        New-Item -ItemType Directory -Path $PathValue | Out-Null
    }
}

function Read-Exact {
    param(
        [System.IO.Stream]$Stream,
        [byte[]]$Buffer,
        [int]$Offset,
        [int]$Count
    )

    $readTotal = 0
    while ($readTotal -lt $Count) {
        $readNow = $Stream.Read($Buffer, $Offset + $readTotal, $Count - $readTotal)
        if ($readNow -le 0) {
            throw "Socket closed while reading."
        }
        $readTotal += $readNow
    }
}

function Write-U16LE {
    param([byte[]]$Buffer, [int]$Offset, [UInt16]$Value)
    $Buffer[$Offset + 0] = [byte]($Value -band 0xFF)
    $Buffer[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
}

function Write-U32LE {
    param([byte[]]$Buffer, [int]$Offset, [UInt32]$Value)
    $Buffer[$Offset + 0] = [byte]($Value -band 0xFF)
    $Buffer[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
    $Buffer[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
    $Buffer[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

function Read-U32LE {
    param([byte[]]$Buffer, [int]$Offset)
    return [UInt32](
        [UInt32]$Buffer[$Offset + 0] -bor
        ([UInt32]$Buffer[$Offset + 1] -shl 8) -bor
        ([UInt32]$Buffer[$Offset + 2] -shl 16) -bor
        ([UInt32]$Buffer[$Offset + 3] -shl 24)
    )
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$featureName = "ping"

$resultRoot = Join-Path $PSScriptRoot "..\Result"
$logDir = Join-Path $resultRoot "Log"
$reportDir = Join-Path $resultRoot "Report"
Ensure-Dir -PathValue $resultRoot
Ensure-Dir -PathValue $logDir
Ensure-Dir -PathValue $reportDir

$logPath = Join-Path $logDir "${timestamp}_${featureName}.log"
$reportPath = Join-Path $reportDir "${timestamp}_${featureName}_report.md"

function Write-Log {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"), $Message
    Write-Host $line
    Add-Content -Path $logPath -Value $line
}

$result = "FAIL"
$failureRootCause = ""
$scopeImpact = ""
$nextActions = @()
$executedCommands = @()
$keyEvidence = @()

try {
    Write-Log "Ping protocol test started."

    $serverExe = Join-Path $PSScriptRoot "..\Bin\$Platform\$Configuration\Server\YunoServer.exe"
    $serverExe = (Resolve-Path $serverExe).Path
    if (-not (Test-Path $serverExe)) {
        throw "Server executable not found: $serverExe"
    }

    $oldServer = Get-Process YunoServer -ErrorAction SilentlyContinue
    if ($oldServer) {
        Write-Log "Existing YunoServer process detected. Stopping old process."
        $oldServer | Stop-Process -Force
    }

    $executedCommands += "$serverExe $Port"
    Write-Log "Starting server: $serverExe $Port"
    Start-Process -FilePath $serverExe -ArgumentList "$Port" -PassThru | Out-Null

    $started = $false
    $deadline = (Get-Date).AddSeconds($StartupTimeoutSec)
    while ((Get-Date) -lt $deadline) {
        try {
            $probe = New-Object System.Net.Sockets.TcpClient
            $probe.Connect("127.0.0.1", $Port)
            $probe.Close()
            $started = $true
            break
        } catch {
            Start-Sleep -Milliseconds 200
        }
    }

    if (-not $started) {
        throw "Server start timeout. Port $Port not reachable."
    }
    Write-Log "Server is reachable on port $Port."

    $client = New-Object System.Net.Sockets.TcpClient
    $client.ReceiveTimeout = 5000
    $client.SendTimeout = 5000
    $executedCommands += "TcpClient connect 127.0.0.1:$Port"
    $client.Connect("127.0.0.1", $Port)
    $stream = $client.GetStream()

    $reqTime64 = [Int64][DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $reqTime = [UInt32]($reqTime64 % 4294967296)
    $packet = New-Object byte[] 12
    Write-U32LE -Buffer $packet -Offset 0 -Value 4
    $packet[4] = [byte]6
    $packet[5] = [byte]1
    Write-U16LE -Buffer $packet -Offset 6 -Value 0
    Write-U32LE -Buffer $packet -Offset 8 -Value $reqTime

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $executedCommands += "Send C2S_Ping(type=6, reqTime=$reqTime)"
    $stream.Write($packet, 0, $packet.Length)
    $stream.Flush()

    $header = New-Object byte[] 8
    Read-Exact -Stream $stream -Buffer $header -Offset 0 -Count 8
    $bodyLen = Read-U32LE -Buffer $header -Offset 0
    $packetType = [int]$header[4]
    $version = [int]$header[5]
    $reserved = [int]([UInt16]($header[6] -bor ($header[7] -shl 8)))

    if ($bodyLen -ne 4) {
        throw "Unexpected body length: $bodyLen"
    }
    if ($packetType -ne 140) {
        throw "Unexpected packet type. Expected 140(S2C_Pong), actual: $packetType"
    }

    $body = New-Object byte[] $bodyLen
    Read-Exact -Stream $stream -Buffer $body -Offset 0 -Count $bodyLen
    $sw.Stop()

    $echoReqTime = Read-U32LE -Buffer $body -Offset 0
    if ($echoReqTime -ne $reqTime) {
        throw "reqTime mismatch. sent=$reqTime received=$echoReqTime"
    }

    $rttMs = [math]::Round($sw.Elapsed.TotalMilliseconds, 3)
    $result = "PASS"
    $keyEvidence += "Received packetType=$packetType version=$version reserved=$reserved bodyLen=$bodyLen"
    $keyEvidence += "Echo reqTime matched: $echoReqTime"
    $keyEvidence += "Measured RTT (client socket round-trip): ${rttMs}ms"

    Write-Log "PASS: Received S2C_Pong with matching reqTime. RTT=${rttMs}ms"

    $stream.Close()
    $client.Close()
}
catch {
    $result = "FAIL"
    $failureRootCause = $_.Exception.Message
    $scopeImpact = "Ping/Pong protocol runtime verification failed."
    $nextActions += "Check server startup prerequisites and port availability."
    $nextActions += "Re-run scripts/test_ping.ps1 after blocker resolution."
    Write-Log "FAIL: $failureRootCause"
}
finally {
    try {
        $running = Get-Process YunoServer -ErrorAction SilentlyContinue
        if ($running) {
            $running | Stop-Process -Force
            Write-Log "Stopped YunoServer process."
        }
    } catch {
        Write-Log "Warning: failed to stop YunoServer cleanly: $($_.Exception.Message)"
    }
}

if (-not $nextActions.Count) {
    $nextActions += "Keep this script as baseline for future Ping regression checks."
}

$report = @()
$report += "# Protocol Test Report: ping"
$report += ""
$report += "## Objective"
$report += "- Validate C2S_Ping to S2C_Pong immediate response path and round-trip delay."
$report += ""
$report += "## Expected Behavior"
$report += "- Server responds with S2C_Pong immediately after receiving C2S_Ping."
$report += "- Response reqTime must match request reqTime."
$report += ""
$report += "## Executed Commands"
if ($executedCommands.Count -gt 0) {
    foreach ($cmd in $executedCommands) {
        $report += "- $cmd"
    }
} else {
    $report += "- None"
}
$report += ""
$report += "## Result"
$report += "- $result"
$report += ""
$report += "## Key Log Evidence"
if ($keyEvidence.Count -gt 0) {
    foreach ($e in $keyEvidence) {
        $report += "- $e"
    }
} else {
    $report += "- See log file for failure details."
}
$report += ""
$report += "## Failure Analysis (if any)"
if ($result -eq "FAIL") {
    $report += "- Root cause: $failureRootCause"
    $report += "- Scope impact: $scopeImpact"
} else {
    $report += "- Root cause: None"
    $report += "- Scope impact: None"
}
$report += ""
$report += "## Next Actions"
foreach ($action in $nextActions) {
    $report += "- $action"
}
$report += ""
$report += "## Artifacts"
$report += "- Log: $logPath"

Set-Content -Path $reportPath -Value $report -Encoding UTF8
Write-Log "Report generated: $reportPath"
Write-Host "TEST_RESULT=$result"
Write-Host "LOG_PATH=$logPath"
Write-Host "REPORT_PATH=$reportPath"
if ($result -eq "FAIL") {
    exit 1
}
exit 0
