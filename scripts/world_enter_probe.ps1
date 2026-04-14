param(
    [string]$LoginHost = '127.0.0.1',
    [int]$LoginPort = 7000,
    [string]$WorldHost = '',
    [int]$WorldPort = 0,
    [int]$ReceiveTimeoutMs = 5000,
    [int]$SnapshotAckTimeoutMs = 3000,
    [string]$UsernamePrefix = 'we'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

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

function Read-U16LE {
    param([byte[]]$Buffer, [int]$Offset)
    return [UInt16]([UInt16]$Buffer[$Offset] -bor ([UInt16]$Buffer[$Offset + 1] -shl 8))
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

function Read-F32LE {
    param([byte[]]$Buffer, [int]$Offset)
    return [BitConverter]::ToSingle($Buffer, $Offset)
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
            throw 'Socket closed while reading.'
        }
        $readTotal += $readNow
    }
}

function New-PacketBodyBuffer {
    return New-Object System.Collections.Generic.List[byte]
}

function Add-U8 {
    param([System.Collections.Generic.List[byte]]$Body, [byte]$Value)
    $Body.Add($Value)
}

function Add-U16LE {
    param([System.Collections.Generic.List[byte]]$Body, [UInt16]$Value)
    $Body.Add([byte]($Value -band 0xFF))
    $Body.Add([byte](($Value -shr 8) -band 0xFF))
}

function Add-U32LE {
    param([System.Collections.Generic.List[byte]]$Body, [UInt32]$Value)
    $Body.Add([byte]($Value -band 0xFF))
    $Body.Add([byte](($Value -shr 8) -band 0xFF))
    $Body.Add([byte](($Value -shr 16) -band 0xFF))
    $Body.Add([byte](($Value -shr 24) -band 0xFF))
}

function Add-StringU16 {
    param([System.Collections.Generic.List[byte]]$Body, [string]$Value)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
    if ($bytes.Length -gt [UInt16]::MaxValue) {
        throw "String too long for U16 encoding: $Value"
    }

    Add-U16LE -Body $Body -Value ([UInt16]$bytes.Length)
    foreach ($b in $bytes) {
        $Body.Add($b)
    }
}

function Build-Packet {
    param(
        [byte]$PacketType,
        [System.Collections.Generic.List[byte]]$Body,
        [byte]$Version = 1,
        [UInt16]$Reserved = 0
    )

    $bodyBytes = if ($null -ne $Body) { $Body.ToArray() } else { [byte[]]@() }
    $packet = New-Object byte[] (8 + $bodyBytes.Length)
    Write-U32LE -Buffer $packet -Offset 0 -Value ([UInt32]$bodyBytes.Length)
    $packet[4] = $PacketType
    $packet[5] = $Version
    Write-U16LE -Buffer $packet -Offset 6 -Value $Reserved
    if ($bodyBytes.Length -gt 0) {
        [Array]::Copy($bodyBytes, 0, $packet, 8, $bodyBytes.Length)
    }
    return $packet
}

function Connect-TcpClient {
    param(
        [string]$HostName,
        [int]$Port,
        [int]$TimeoutMs
    )

    $client = New-Object System.Net.Sockets.TcpClient
    $client.ReceiveTimeout = $TimeoutMs
    $client.SendTimeout = $TimeoutMs
    $client.Connect($HostName, $Port)
    return $client
}

function Read-Packet {
    param([System.Net.Sockets.TcpClient]$Client)

    $stream = $Client.GetStream()
    $header = New-Object byte[] 8
    Read-Exact -Stream $stream -Buffer $header -Offset 0 -Count 8

    $bodyLength = [int](Read-U32LE -Buffer $header -Offset 0)
    $body = New-Object byte[] $bodyLength
    if ($bodyLength -gt 0) {
        Read-Exact -Stream $stream -Buffer $body -Offset 0 -Count $bodyLength
    }

    return [PSCustomObject]@{
        BodyLength = $bodyLength
        Type = [int]$header[4]
        Version = [int]$header[5]
        Reserved = [int](Read-U16LE -Buffer $header -Offset 6)
        Body = $body
    }
}

function Read-StringU16 {
    param([byte[]]$Buffer, [ref]$Offset)

    $length = [int](Read-U16LE -Buffer $Buffer -Offset $Offset.Value)
    $Offset.Value += 2
    if ($Offset.Value + $length -gt $Buffer.Length) {
        throw 'Invalid string length.'
    }

    $value = [System.Text.Encoding]::UTF8.GetString($Buffer, $Offset.Value, $length)
    $Offset.Value += $length
    return $value
}

function Parse-AuthResult {
    param([byte[]]$Body)

    $offset = 0
    $success = [int]$Body[$offset]
    $offset += 1
    $code = [int](Read-U16LE -Buffer $Body -Offset $offset)
    $offset += 2
    $message = Read-StringU16 -Buffer $Body -Offset ([ref]$offset)
    $gameHost = Read-StringU16 -Buffer $Body -Offset ([ref]$offset)
    $gamePort = [int](Read-U16LE -Buffer $Body -Offset $offset)
    $offset += 2
    $loginToken = Read-StringU16 -Buffer $Body -Offset ([ref]$offset)

    return [PSCustomObject]@{
        Success = $success
        Code = $code
        Message = $message
        GameHost = $gameHost
        GamePort = $gamePort
        LoginToken = $loginToken
    }
}

function Parse-SpawnEntity {
    param([byte[]]$Body)

    return [PSCustomObject]@{
        EntityId = [uint32](Read-U32LE -Buffer $Body -Offset 0)
        ArchetypeId = [uint32](Read-U32LE -Buffer $Body -Offset 4)
        X = Read-F32LE -Buffer $Body -Offset 8
        Y = Read-F32LE -Buffer $Body -Offset 12
        Z = Read-F32LE -Buffer $Body -Offset 16
    }
}

function Parse-WorldSnapshot {
    param([byte[]]$Body)

    $offset = 0
    $serverTick = [uint32](Read-U32LE -Buffer $Body -Offset $offset)
    $offset += 4
    $snapshotId = [uint32](Read-U32LE -Buffer $Body -Offset $offset)
    $offset += 4
    $baseSnapshotId = [uint32](Read-U32LE -Buffer $Body -Offset $offset)
    $offset += 4
    $count = [int](Read-U16LE -Buffer $Body -Offset $offset)
    $offset += 2

    $entities = @()
    for ($i = 0; $i -lt $count; $i++) {
        $entity = [PSCustomObject]@{
            EntityId = [uint32](Read-U32LE -Buffer $Body -Offset $offset)
            StateFlags = [uint32](Read-U32LE -Buffer $Body -Offset ($offset + 4))
            LastProcessedInputSequence = [uint32](Read-U32LE -Buffer $Body -Offset ($offset + 8))
            X = Read-F32LE -Buffer $Body -Offset ($offset + 12)
            Y = Read-F32LE -Buffer $Body -Offset ($offset + 16)
            Z = Read-F32LE -Buffer $Body -Offset ($offset + 20)
            Yaw = Read-F32LE -Buffer $Body -Offset ($offset + 24)
            VX = Read-F32LE -Buffer $Body -Offset ($offset + 28)
            VY = Read-F32LE -Buffer $Body -Offset ($offset + 32)
            VZ = Read-F32LE -Buffer $Body -Offset ($offset + 36)
        }
        $entities += $entity
        $offset += 40
    }

    return [PSCustomObject]@{
        ServerTick = $serverTick
        SnapshotId = $snapshotId
        BaseSnapshotId = $baseSnapshotId
        Entities = $entities
    }
}

function Send-Packet {
    param([System.Net.Sockets.TcpClient]$Client, [byte[]]$Packet)
    $stream = $Client.GetStream()
    $stream.Write($Packet, 0, $Packet.Length)
    $stream.Flush()
}

function Invoke-AuthRequest {
    param(
        [string]$HostName,
        [int]$Port,
        [byte]$PacketType,
        [System.Collections.Generic.List[byte]]$Body,
        [int]$TimeoutMs
    )

    $client = $null
    try {
        $client = Connect-TcpClient -HostName $HostName -Port $Port -TimeoutMs $TimeoutMs
        $packet = Build-Packet -PacketType $PacketType -Body $Body
        Send-Packet -Client $client -Packet $packet
        $response = Read-Packet -Client $client
        if ($response.Type -ne 141) {
            throw "Unexpected auth response packet type: $($response.Type)"
        }
        return (Parse-AuthResult -Body $response.Body)
    }
    finally {
        if ($null -ne $client) {
            $client.Dispose()
        }
    }
}

$timestampSuffix = Get-Date -Format 'MMddHHmmssfff'
$rawUsername = '{0}_{1}' -f $UsernamePrefix, $timestampSuffix
$username = if ($rawUsername.Length -le 30) { $rawUsername } else { $rawUsername.Substring(0, 30) }
$password = 'Pw!Probe12345'
$token = ''
$spawnEntity = $null
$firstSnapshot = $null
$ackedSnapshotObserved = $false
$worldClient = $null

try {
    Write-Host "[Probe] register start login=${LoginHost}:$LoginPort username=$username"
    $registerBody = New-PacketBodyBuffer
    Add-StringU16 -Body $registerBody -Value $username
    Add-StringU16 -Body $registerBody -Value $password
    $registerResult = Invoke-AuthRequest -HostName $LoginHost -Port $LoginPort -PacketType 19 -Body $registerBody -TimeoutMs $ReceiveTimeoutMs
    Write-Host "[Probe] register result success=$($registerResult.Success) code=$($registerResult.Code) message=$($registerResult.Message)"
    if ($registerResult.Success -eq 0 -and $registerResult.Code -ne 6) {
        throw "Register failed unexpectedly: code=$($registerResult.Code) message=$($registerResult.Message)"
    }

    $loginBody = New-PacketBodyBuffer
    Add-StringU16 -Body $loginBody -Value $username
    Add-StringU16 -Body $loginBody -Value $password
    $loginResult = Invoke-AuthRequest -HostName $LoginHost -Port $LoginPort -PacketType 12 -Body $loginBody -TimeoutMs $ReceiveTimeoutMs
    Write-Host "[Probe] login result success=$($loginResult.Success) code=$($loginResult.Code) message=$($loginResult.Message) game=$($loginResult.GameHost):$($loginResult.GamePort)"
    if ($loginResult.Success -eq 0) {
        throw "Login failed: code=$($loginResult.Code) message=$($loginResult.Message)"
    }
    if ([string]::IsNullOrWhiteSpace($loginResult.LoginToken)) {
        throw 'Login succeeded but loginToken was empty.'
    }

    $token = $loginResult.LoginToken
    if ([string]::IsNullOrWhiteSpace($WorldHost)) {
        $WorldHost = $loginResult.GameHost
    }
    if ($WorldPort -le 0) {
        $WorldPort = $loginResult.GamePort
    }

    $worldClient = Connect-TcpClient -HostName $WorldHost -Port $WorldPort -TimeoutMs $ReceiveTimeoutMs
    Write-Host "[Probe] world connect ok target=${WorldHost}:$WorldPort"

    $enterBody = New-PacketBodyBuffer
    Add-U32LE -Body $enterBody -Value 1001
    Add-U32LE -Body $enterBody -Value 0
    Add-StringU16 -Body $enterBody -Value $token
    $enterPacket = Build-Packet -PacketType 13 -Body $enterBody
    Send-Packet -Client $worldClient -Packet $enterPacket
    Write-Host '[Probe] enter-world request sent'

    $deadline = (Get-Date).AddMilliseconds($ReceiveTimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $packet = Read-Packet -Client $worldClient
        switch ($packet.Type) {
            143 {
                $spawnEntity = Parse-SpawnEntity -Body $packet.Body
                Write-Host "[Probe] spawn entityId=$($spawnEntity.EntityId) archetype=$($spawnEntity.ArchetypeId) pos=($($spawnEntity.X),$($spawnEntity.Y),$($spawnEntity.Z))"
            }
            145 {
                $snapshot = Parse-WorldSnapshot -Body $packet.Body
                Write-Host "[Probe] snapshot id=$($snapshot.SnapshotId) base=$($snapshot.BaseSnapshotId) entityCount=$($snapshot.Entities.Count)"
                if ($null -eq $firstSnapshot) {
                    $firstSnapshot = $snapshot
                    $ackBody = New-PacketBodyBuffer
                    Add-U32LE -Body $ackBody -Value $snapshot.SnapshotId
                    $ackPacket = Build-Packet -PacketType 18 -Body $ackBody
                    Send-Packet -Client $worldClient -Packet $ackPacket
                    Write-Host "[Probe] ack snapshot sent id=$($snapshot.SnapshotId)"
                }
                elseif ($snapshot.BaseSnapshotId -eq $firstSnapshot.SnapshotId) {
                    $ackedSnapshotObserved = $true
                }
            }
            default {
                Write-Host "[Probe] ignored packet type=$($packet.Type)"
            }
        }

        if ($spawnEntity -and $firstSnapshot -and $ackedSnapshotObserved) {
            break
        }
    }

    if (-not $spawnEntity) {
        throw 'Did not receive S2C_SpawnEntity.'
    }
    if (-not $firstSnapshot) {
        throw 'Did not receive first S2C_WorldSnapshot.'
    }

    $entitySeenInSnapshot = $false
    foreach ($entity in $firstSnapshot.Entities) {
        if ($entity.EntityId -eq $spawnEntity.EntityId) {
            $entitySeenInSnapshot = $true
            break
        }
    }

    if (-not $entitySeenInSnapshot) {
        throw "Spawned entityId $($spawnEntity.EntityId) not found in first world snapshot."
    }
    if (-not $ackedSnapshotObserved) {
        throw "Did not observe snapshot acknowledging baseSnapshotId=$($firstSnapshot.SnapshotId)."
    }

    Write-Host "[Probe] PASS spawnEntity=$($spawnEntity.EntityId) firstSnapshot=$($firstSnapshot.SnapshotId) ackObserved=$ackedSnapshotObserved"
    exit 0
}
catch {
    Write-Host "[Probe] FAIL $($_.Exception.Message)"
    exit 1
}
finally {
    if ($null -ne $worldClient) {
        try { $worldClient.Dispose() } catch {}
    }
}
