param(
    [string]$Configuration = 'Release',
    [string]$Map = 'geo1c_con',
    [ValidateRange(1, 8)][int]$Clients = 1,
    [ValidateRange(10, 3600)][int]$Seconds = 45,
    [ValidateSet('clean', 'lag100', 'lag200', 'jitter', 'all')][string]$Profile = 'clean',
    [ValidateRange(30, 600)][int]$JoinTimeout = 120,
    [switch]$Direct,
    [string]$ServerArgs = '',
    [string]$ClientArgs = '',
    [string]$OutDir,
    [ValidateSet('Win32','x64')][string]$Platform = 'Win32',
    [ValidateSet('Win32','x64')][string]$ClientPlatform
)
# Paired network run: a headless dedicated LAN server plus headless autonet clients on this PC,
# each in its own worker folder (-Direct: clients join 127.0.0.1 by address instead of LAN
# discovery, and every socket listens on loopback only, so Windows Firewall never prompts).
# -ClientPlatform runs the clients on the other build (mixed Win32/x64 games).
# Reads the OPENSWBF2 NET markers and -netstats lines from every
# run.log and prints one [PASS]/[FAIL] line per profile. See tests\README.md, "Network pair runs".
$ErrorActionPreference = 'Stop'
$testsDir = $PSScriptRoot
$root = Split-Path $testsDir -Parent
. (Join-Path $testsDir 'smoke.ps1')
Set-SmokePlatform $Platform
$Platform = $script:SmokePlatform

$exeDir = Get-GameExeDir $root $Configuration
$gameData = Find-GameDataRoot
$skip = Get-SmokePrecheck $gameData $exeDir
if (-not $ClientPlatform) { $ClientPlatform = $Platform }
Set-SmokePlatform $ClientPlatform
$ClientPlatform = $script:SmokePlatform
$clientExeDir = Get-GameExeDir $root $Configuration
if (-not $skip) { $skip = Get-SmokePrecheck $gameData $clientExeDir }
Set-SmokePlatform $Platform
if (-not $skip) {
    $private = @([Net.Dns]::GetHostAddresses([Net.Dns]::GetHostName()) | Where-Object {
            $b = $_.GetAddressBytes()
            $_.AddressFamily -eq 'InterNetwork' -and ($b[0] -eq 10 -or ($b[0] -eq 172 -and $b[1] -ge 16 -and $b[1] -le 31) -or ($b[0] -eq 192 -and $b[1] -eq 168))
        })
    if ($private.Count -eq 0) { $skip = 'no private LAN address (LAN mode needs one)' }
}
if ($skip) {
    Write-Host "[SKIP] netpair: $skip"
    exit 0
}

$profiles = @{
    clean  = @{ Drag = ''; MinUpdates = 8.0 }
    lag100 = @{ Drag = '-latency 100 -loss 1'; MinUpdates = 3.0 }
    lag200 = @{ Drag = '-latency 200 -loss 2'; MinUpdates = 2.0 }
    # 60 ms each way, +-30 ms per packet (-jitter is a percentage of -latency), 1% loss
    jitter = @{ Drag = '-latency 60 -jitter 50 -loss 1'; MinUpdates = 3.0 }
}
$runProfiles = if ($Profile -eq 'all') { @('clean', 'lag100', 'lag200', 'jitter') } else { @($Profile) }
if (-not $OutDir) { $OutDir = Join-Path $root ("artifacts\netpair\" + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Desync, crash and disconnect lines that fail a run outright.
$badRx = 'OPENSWBF2 NET (align mismatch|bad (soldier|walker) state|drop |host lost|join lost|join refused|join rejected)|FATAL:'

function Read-Stats($log, $rx) {
    $out = @()
    foreach ($line in [IO.File]::ReadAllLines($log)) {
        $m = [regex]::Match($line, $rx)
        if ($m.Success) { $out += $m }
    }
    return $out
}

function Invoke-NetPair($name, $drag) {
    $caseDir = Join-Path $OutDir $name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $gameName = "NETPAIR$PID$name"
    $common = '-norender -nosound -nowait -nofocuspause -netstats -logcmds "all Warning, LogNet Inform"'
    if ($Direct) { $common += ' -netloopback' }
    $procs = @()
    $dirs = @()
    try {
        $names = @('server') + @(1..$Clients | ForEach-Object { "client$_" })
        foreach ($n in $names) {
            $d = Join-Path $caseDir "w-$n"
            if ($n -eq 'server') { New-SmokeWorkerDir $exeDir $d }
            else {
                Set-SmokePlatform $ClientPlatform
                New-SmokeWorkerDir $clientExeDir $d
                Set-SmokePlatform $Platform
            }
            Set-Content -Path (Join-Path $d 'steam_appid.txt') -Value '6060' -Encoding ASCII
            $dirs += $d
        }
        $srvLog = Join-Path $caseDir 'server.log'
        # -Direct also covers the -dedicated shorthand (headless Direct server, no legal screens).
        $srvArgs = if ($Direct) { "-dedicated $Map 100 100 /gamename $gameName $common $drag $ServerArgs -log `"$srvLog`"" }
                   else { "/autonet dedicated -lan -nopregame -playercount 0 /gamename $gameName $Map 100 100 $common $drag $ServerArgs -log `"$srvLog`"" }
        $procs += Start-Process -FilePath (Join-Path $dirs[0] 'BattlefrontII.exe') -ArgumentList $srvArgs -WorkingDirectory $dirs[0] -PassThru
        $deadline = (Get-Date).AddSeconds($JoinTimeout)
        while ((Get-Date) -lt $deadline -and -not ((Test-Path $srvLog) -and (Select-String -Path $srvLog -Pattern 'OPENSWBF2 NET mission start' -Quiet))) {
            if ($procs[0].HasExited) { break }
            Start-Sleep -Milliseconds 500
        }
        $cliLogs = @()
        for ($i = 1; $i -le $Clients; ++$i) {
            $cliLog = Join-Path $caseDir "client$i.log"
            $cliLogs += $cliLog
            $join = if ($Direct) { '/connect 127.0.0.1' } else { '' }
            $cliArgs = "/autonet client -lan /gamename $gameName $join $common $ClientArgs -log `"$cliLog`""
            $procs += Start-Process -FilePath (Join-Path $dirs[$i] 'BattlefrontII.exe') -ArgumentList $cliArgs -WorkingDirectory $dirs[$i] -PassThru
        }
        $joined = $false
        while ((Get-Date) -lt $deadline) {
            $joined = @($cliLogs | Where-Object { (Test-Path $_) -and (Select-String -Path $_ -Pattern 'OPENSWBF2 NET mission start' -Quiet) }).Count -eq $Clients
            if ($joined -or @($procs | Where-Object { $_.HasExited }).Count) { break }
            Start-Sleep -Milliseconds 500
        }
        if ($joined) {
            $end = (Get-Date).AddSeconds($Seconds)
            while ((Get-Date) -lt $end -and -not @($procs | Where-Object { $_.HasExited }).Count) { Start-Sleep -Milliseconds 500 }
        }
        $exited = @($procs | Where-Object { $_.HasExited } | ForEach-Object { '0x{0:X8}' -f $_.ExitCode })
    }
    finally {
        foreach ($p in $procs) { if (-not $p.HasExited) { & taskkill /PID $p.Id /T /F 2>&1 | Out-Null } }
        foreach ($p in $procs) { $p.WaitForExit(10000) | Out-Null }
        foreach ($d in $dirs) {
            Get-ChildItem -LiteralPath $d -Filter '*.dmp' -ErrorAction SilentlyContinue | Copy-Item -Destination $caseDir
            Remove-SmokeWorkerDir $d
        }
    }

    $problems = @()
    if (-not $joined) { $problems += 'clients did not reach the mission' }
    if ($exited.Count) { $problems += "process exited early ($($exited -join ','))" }
    $updates = @()
    $pings = @()
    $loss = @()
    $corrAvg = @()
    $corrMax = @()
    $fills = @()
    $fixed = @()
    $lost = @()
    foreach ($log in @($srvLog) + $cliLogs) {
        if (-not (Test-Path $log)) { $problems += "missing $(Split-Path $log -Leaf)"; continue }
        foreach ($m in (Read-Stats $log $badRx)) { $problems += "$(Split-Path $log -Leaf): $($m.Value)" }
    }
    # Soldier state names characters by index, so every peer must build the same table.
    $tables = @()
    foreach ($log in @($srvLog) + $cliLogs) {
        if (Test-Path $log) { $tables += @(Read-Stats $log 'OPENSWBF2 NET characters (total=\d+ players=\d+ bots=\d+)' | Select-Object -First 1 | ForEach-Object { $_.Groups[1].Value }) }
    }
    if (@($tables | Select-Object -Unique).Count -gt 1) { $problems += "character tables differ: $($tables -join ' / ')" }
    foreach ($log in $cliLogs) {
        if (-not (Test-Path $log)) { continue }
        # Skip the first two stats lines: the first second after load is still catching up.
        $stats = @(Read-Stats $log 'OPENSWBF2 NET STATS client player=\d+ ping=(\d+) updates=(\d+) in=\d+ out=\d+ loss=(\d+).*?(?: corr=([\d.]+)/([\d.]+))?$') | Select-Object -Skip 2
        foreach ($m in $stats) {
            $pings += [int]$m.Groups[1].Value
            $updates += [int]$m.Groups[2].Value
            $loss += [int]$m.Groups[3].Value
            if ($m.Groups[4].Success) {
                $corrAvg += [double]$m.Groups[4].Value
                $corrMax += [double]$m.Groups[5].Value
            }
        }
    }
    # Host side, per remote player and second: turns filled with a guess, late moves replayed,
    # and filled turns whose real move never came in time.
    if (Test-Path $srvLog) {
        foreach ($m in (@(Read-Stats $srvLog 'OPENSWBF2 NET STATS player=\d+ .* fills_s=(\d+) same_s=(\d+) fixed_s=(\d+) lost_s=(\d+)') | Select-Object -Skip 2)) {
            $fills += [int]$m.Groups[1].Value
            $fixed += [int]$m.Groups[3].Value
            $lost += [int]$m.Groups[4].Value
        }
    }
    $avg = { param($a) if ($a.Count) { [Math]::Round(($a | Measure-Object -Average).Average, 1) } else { 0 } }
    $u = & $avg $updates
    $p = & $avg $pings
    $l = & $avg $loss
    $ca = if ($corrAvg.Count) { [Math]::Round(($corrAvg | Measure-Object -Average).Average, 2) } else { 0 }
    $cm = if ($corrMax.Count) { [Math]::Round(($corrMax | Measure-Object -Maximum).Maximum, 2) } else { 0 }
    if ($joined -and $updates.Count -lt 5) { $problems += "only $($updates.Count) stats seconds" }
    if ($updates.Count -and $u -lt $profiles[$name].MinUpdates) { $problems += "updates/s $u below $($profiles[$name].MinUpdates)" }
    $mode = if ($Direct) { 'direct' } else { 'lan' }
    $summary = "netpair:$name join=$mode clients=$Clients updates=$u ping=$p loss=$l seconds=$($updates.Count)" +
        " fills/s=$(& $avg $fills) replayed/s=$(& $avg $fixed) lost/s=$(& $avg $lost) corr=$ca/$cm"
    $problems = @($problems | Select-Object -Unique)
    $token = if ($problems.Count) { 'FAIL' } else { 'PASS' }
    $text = "[$token] $summary"
    if ($problems.Count) { $text += "`r`n" + (($problems | Select-Object -First 10 | ForEach-Object { "    $_" }) -join "`r`n") }
    Set-Content -Path (Join-Path $caseDir 'result.txt') -Value $text -Encoding ASCII
    return $text
}

$failed = $false
foreach ($name in $runProfiles) {
    $line = Invoke-NetPair $name $profiles[$name].Drag
    Write-Host $line
    if ($line.StartsWith('[FAIL]')) { $failed = $true }
}
Write-Host "Logs: $OutDir"
if ($failed) { exit 1 }
exit 0
