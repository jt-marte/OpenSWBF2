<#
A minimal rcon client for testing a server's remote admin (NetAdmin.cpp) over UDP, the way the
launcher's server manager talks to it.

  .\rcon.ps1 -Port 3700 -Lines '/admin /login pw /state', '/admin /maplist'

Each request is one datagram: type 28, group (u16), index 0, count 1, then the command byte, a 0
and the session key; 'J' lines add a length byte and up to 63 ASCII characters. Replies are type
29: the key, then [1, length, text] per line, then 0, possibly over several datagrams of one group.
Prints each reply line; exits 1 when the server doesn't answer.

A server keeps 3 admin sessions, one per sender address, for 60 s after it last heard from each
(a map change counts as hearing from it). Scripts that call this often pass the same -LocalPort
every time, so they keep reusing one session instead of filling the slots.
#>
param(
    [string]$Server = '127.0.0.1',
    [int]$Port = 3658,
    [Parameter(Mandatory = $true)][string[]]$Lines,
    [int]$TimeoutMs = 3000,
    [int]$LocalPort = 0
)
$ErrorActionPreference = 'Stop'

$udp = New-Object System.Net.Sockets.UdpClient $LocalPort
$udp.Client.ReceiveTimeout = $TimeoutMs
$udp.Connect($Server, $Port)
$script:group = Get-Random -Minimum 1 -Maximum 60000

function Send-Rcon([char]$Command, [byte]$Key, [byte[]]$Payload) {
    $g = $script:group -band 0xffff
    $script:group++
    $bytes = [byte[]](@(28, ($g -band 0xff), ($g -shr 8), 0, 1, [byte]$Command, 0, $Key) + $Payload)
    [void]$udp.Send($bytes, $bytes.Length)
}

# One reply group's payload (the bytes after each datagram's 5-byte header, in order)
function Receive-Rcon {
    $remote = New-Object System.Net.IPEndPoint ([System.Net.IPAddress]::Any, 0)
    $parts = @{}
    $count = 1
    $wantGroup = $null
    while ($true) {
        $data = $udp.Receive([ref]$remote)
        if ($data.Length -lt 5 -or $data[0] -ne 29) { continue }
        $g = $data[1] -bor ($data[2] -shl 8)
        if ($null -eq $wantGroup) { $wantGroup = $g }
        if ($g -ne $wantGroup) { continue }
        $parts[[int]$data[3]] = $data[5..($data.Length - 1)]
        $count = [Math]::Max(1, $data[4] -band 0x7f)
        if ($parts.Count -ge $count) { break }
    }
    $out = New-Object System.Collections.Generic.List[byte]
    for ($i = 0; $i -lt $count; $i++) { if ($parts.ContainsKey($i)) { $out.AddRange([byte[]]$parts[$i]) } }
    return , $out.ToArray()
}

function Read-Lines([byte[]]$Payload) {
    $lines = @()
    $i = 1 # after the key
    while ($i -lt $Payload.Length -and $Payload[$i] -eq 1) {
        $len = $Payload[$i + 1]
        $lines += [System.Text.Encoding]::ASCII.GetString($Payload, $i + 2, $len)
        $i += 2 + $len
    }
    return $lines
}

try {
    Send-Rcon 's' 0 @()
    $key = (Receive-Rcon)[0]
    Send-Rcon ')' $key @(0)
    [void](Receive-Rcon)
    foreach ($line in $Lines) {
        if ($line.Length -gt 63) { throw "Line over 63 characters: $line" }
        $text = [System.Text.Encoding]::ASCII.GetBytes($line)
        Send-Rcon 'J' $key ([byte[]](@([byte]$text.Length) + $text))
        Read-Lines (Receive-Rcon) | ForEach-Object { Write-Output $_ }
    }
} catch [System.Net.Sockets.SocketException] {
    Write-Output "no answer from ${Server}:$Port"
    exit 1
} finally {
    $udp.Close()
}
