$script:SmokeStandardMission = 'geo1c_con'
$script:SmokeMarker = 'OPENSWBF2 RUN COMPLETE'
$script:WerKeyPath = 'SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\BattlefrontII.exe'
$script:WerBasePath = 'SOFTWARE\Microsoft\Windows\Windows Error Reporting'

function Find-GameDataRoot {
    $candidate = $null
    if ($env:SWBF2_GAMEDATA) {
        $candidate = $env:SWBF2_GAMEDATA
    }
    else {
        foreach ($view in 'Registry64', 'Registry32') {
            $base = $null
            $sub = $null
            try {
                $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', $view)
                $sub = $base.OpenSubKey('SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 6060')
                if ($sub) {
                    $loc = $sub.GetValue('InstallLocation')
                    if ($loc) { $candidate = "$loc\GameData"; break }
                }
            }
            catch {}
            finally {
                if ($sub) { $sub.Close() }
                if ($base) { $base.Close() }
            }
        }
    }
    if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'data\_lvl_pc') -PathType Container)) { return $candidate }
    return $null
}

function Get-SmokePrecheck($gameDataRoot, $exeDir) {
    if (-not $gameDataRoot) { return 'no retail GameData found (set SWBF2_GAMEDATA or install Steam App 6060)' }
    if (-not (Test-Path -LiteralPath (Join-Path $exeDir 'BattlefrontII.exe'))) { return "BattlefrontII.exe not found in $exeDir" }
    foreach ($dll in 'binkw32.dll', 'Galaxy.dll', 'GalaxyPeer.dll', 'steam_api.dll') {
        if (-not (Test-Path -LiteralPath (Join-Path $exeDir $dll))) { return "$dll missing beside the exe in $exeDir" }
    }
    $data = Get-Item -LiteralPath (Join-Path $exeDir 'data') -Force -ErrorAction SilentlyContinue
    if (-not $data -or ($data.Attributes -band [IO.FileAttributes]::ReparsePoint) -or -not (Test-Path -LiteralPath (Join-Path $exeDir 'data\_lvl_pc\shell.lvl'))) { return "staged data missing in $exeDir (build without SWBF2_NOSTAGE)" }
    return $null
}

function Get-SmokeClass($skipped, $timedOut, $code, $logText, $dumps) {
    if ($skipped) { return 'Skipped' }
    if ($timedOut) { return 'TimeOut' }
    if ($code -eq 3221226505) { return 'GsFailure' }
    if ($code -eq 1 -or $code -eq 3 -or $code -eq 4 -or $code -eq 5 -or $logText -match 'FATAL:' -or @($dumps).Count -gt 0) { return 'Fatal' }
    if ($code -eq 0 -and $logText -match $script:SmokeMarker) { return 'Pass' }
    if ($code -eq 0) { return 'InitFailure' }
    return 'Unknown'
}

function Get-SmokeToken($class) {
    switch ($class) {
        'Pass' { return 'PASS' }
        'Skipped' { return 'SKIP' }
        'TimeOut' { return 'TIMEOUT' }
        'Fatal' { return 'CRASH' }
        'GsFailure' { return 'CRASH' }
        default { return 'FAIL' }
    }
}

function Test-WerKey {
    $base = $null
    $sub = $null
    try {
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry64')
        $sub = $base.OpenSubKey($script:WerKeyPath)
        return [bool]$sub
    }
    catch { return $false }
    finally {
        if ($sub) { $sub.Close() }
        if ($base) { $base.Close() }
    }
}

function Test-WerSetup {
    $warnings = @()
    if (-not (Test-WerKey)) {
        $warnings += 'WER LocalDumps key for BattlefrontII.exe is missing; import tools\wer-localdumps.reg as administrator'
    }
    $dontShow = $false
    foreach ($hive in 'LocalMachine', 'CurrentUser') {
        $base = $null
        $sub = $null
        try {
            $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($hive, 'Registry64')
            $sub = $base.OpenSubKey($script:WerBasePath)
            if ($sub -and $sub.GetValue('DontShowUI') -eq 1) { $dontShow = $true }
        }
        catch {}
        finally {
            if ($sub) { $sub.Close() }
            if ($base) { $base.Close() }
        }
    }
    if (-not $dontShow) {
        $warnings += 'WER DontShowUI is not set; a crash can leave a dialog open (see tools\wer-localdumps.reg)'
    }
    return $warnings
}

function Get-WerDumpFolder {
    $base = $null
    $sub = $null
    try {
        $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry64')
        $sub = $base.OpenSubKey($script:WerKeyPath)
        if ($sub) {
            $folder = $sub.GetValue('DumpFolder')
            if ($folder) { return [string]$folder }
        }
    }
    catch {}
    finally {
        if ($sub) { $sub.Close() }
        if ($base) { $base.Close() }
    }
    return (Join-Path $env:LOCALAPPDATA 'OpenSWBF2\crashes')
}

function Invoke-SmokeCase($name, $exe, $workDir, $argLine, $timeoutSec, $caseDir) {
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $scoreLog = Join-Path $workDir 'finalscore.log'
    Remove-Item -LiteralPath $scoreLog -Force -ErrorAction SilentlyContinue
    $appId = Join-Path $workDir 'steam_appid.txt'
    if (-not (Test-Path -LiteralPath $appId)) { Set-Content -Path $appId -Value '6060' -Encoding ASCII }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argLine
    $psi.WorkingDirectory = $workDir
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $start = Get-Date
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $p = [System.Diagnostics.Process]::Start($psi)
    try { $p.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal } catch {}
    $o = $p.StandardOutput.ReadToEndAsync()
    $e = $p.StandardError.ReadToEndAsync()
    $timedOut = -not $p.WaitForExit([int]([int64]$timeoutSec * 1000))
    if ($timedOut) {
        & taskkill /PID $p.Id /T /F 2>&1 | Out-Null
        $p.WaitForExit()
    }
    $sw.Stop()
    $o.Wait(5000) | Out-Null
    $e.Wait(5000) | Out-Null
    $code = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$p.ExitCode), 0)
    $caseIni = Join-Path $caseDir 'display.ini'
    if (Test-Path -LiteralPath $caseIni) { [IO.File]::SetAttributes($caseIni, [IO.FileAttributes]::Normal) }

    $logText = ''
    $runLog = Join-Path $caseDir 'run.log'
    if (Test-Path -LiteralPath $runLog) { $logText = [IO.File]::ReadAllText($runLog) }
    if ($o.IsCompleted) { $logText += "`r`n" + $o.Result }
    if ($e.IsCompleted) { $logText += "`r`n" + $e.Result }
    if (Test-Path -LiteralPath $scoreLog) { Copy-Item -LiteralPath $scoreLog -Destination (Join-Path $caseDir 'finalscore.log') -Force }

    $dumps = @(Get-ChildItem -LiteralPath $caseDir -Filter '*.dmp' -ErrorAction SilentlyContinue)
    $class = Get-SmokeClass $false $timedOut $code $logText $dumps
    if ($class -eq 'GsFailure') {
        $werDir = Get-WerDumpFolder
        if (Test-Path -LiteralPath $werDir) {
            $newest = Get-ChildItem -LiteralPath $werDir -Filter '*.dmp' -ErrorAction SilentlyContinue |
                Where-Object { $_.LastWriteTime -ge $start } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($newest) { Copy-Item -LiteralPath $newest.FullName -Destination $caseDir -Force }
        }
        $dumps = @(Get-ChildItem -LiteralPath $caseDir -Filter '*.dmp' -ErrorAction SilentlyContinue)
    }
    $ms = $sw.ElapsedMilliseconds
    $result = ("class={0}`r`nexit=0x{1:X8}`r`nelapsed_ms={2}`r`ndumps={3}`r`n" -f $class, $code, $ms, $dumps.Count)
    Set-Content -Path (Join-Path $caseDir 'result.txt') -Value $result -Encoding ASCII
    return New-Object PSObject -Property @{ Name = $name; Class = $class; Code = $code; Dumps = $dumps; LogText = $logText; Ms = $ms; CaseDir = $caseDir; WorkDir = $workDir }
}

function New-SmokeWorkerDir($srcDir, $dir) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    foreach ($f in 'BattlefrontII.exe', 'binkw32.dll', 'Galaxy.dll', 'GalaxyPeer.dll', 'steam_api.dll', 'unicows.dll', 'steam_appid.txt') {
        $s = Join-Path $srcDir $f
        if (Test-Path -LiteralPath $s) { Copy-Item -LiteralPath $s -Destination $dir -Force }
    }
    & cmd /c mklink /J "$(Join-Path $dir 'data')" "$(Join-Path $srcDir 'data')" | Out-Null
}

function Remove-SmokeWorkerDir($dir) {
    if (-not (Test-Path -LiteralPath $dir)) { return }
    $link = Join-Path $dir 'data'
    if (Test-Path -LiteralPath $link) { & cmd /c rmdir "$link" | Out-Null }
    if (Test-Path -LiteralPath $link) { throw "worker data junction not removed: $link" }
    Remove-Item -LiteralPath $dir -Recurse -Force
}

function Start-SmokeBatch($items, $workDirs) {
    $src = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'smoke.ps1'))
    $queue = New-Object 'System.Collections.Concurrent.ConcurrentQueue[string]'
    foreach ($d in $workDirs) { $queue.Enqueue([string]$d) }
    $pool = [runspacefactory]::CreateRunspacePool(1, @($workDirs).Count)
    $pool.Open()
    $worker = {
        param($src, $queue, $item)
        . ([scriptblock]::Create($src))
        $dir = $null
        while (-not $queue.TryDequeue([ref]$dir)) { Start-Sleep -Milliseconds 50 }
        try { Invoke-SmokeCase $item.Name (Join-Path $dir $item.ExeName) $dir $item.ArgLine $item.Timeout $item.CaseDir }
        finally { $queue.Enqueue($dir) }
    }
    $handles = @()
    foreach ($item in $items) {
        $ps = [powershell]::Create()
        $ps.RunspacePool = $pool
        $ps.AddScript($worker).AddArgument($src).AddArgument($queue).AddArgument($item) | Out-Null
        $handles += New-Object PSObject -Property @{ Ps = $ps; Async = $ps.BeginInvoke() }
    }
    return New-Object PSObject -Property @{ Pool = $pool; Handles = $handles }
}

function Receive-SmokeCase($batch, $index) {
    $h = $batch.Handles[$index]
    try {
        $out = @($h.Ps.EndInvoke($h.Async))
        if ($h.Ps.Streams.Error.Count -gt 0) { throw ("smoke worker error: " + (($h.Ps.Streams.Error | ForEach-Object { $_.ToString() }) -join '; ')) }
        if ($out.Count -ne 1) { throw "smoke worker returned $($out.Count) results" }
        return $out[0]
    }
    finally { $h.Ps.Dispose() }
}

function Stop-SmokeBatch($batch) {
    foreach ($h in $batch.Handles) { try { $h.Ps.Stop() } catch {}; try { $h.Ps.Dispose() } catch {} }
    $batch.Pool.Close()
    $batch.Pool.Dispose()
}

function Get-SmokeAllowed($caseName) {
    $allowed = @()
    $file = Join-Path $PSScriptRoot 'smoke\expected-errors.txt'
    if (-not (Test-Path -LiteralPath $file)) { return $allowed }
    $section = ''
    foreach ($line in (Get-Content -LiteralPath $file)) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith('#')) { continue }
        if ($t -cmatch '^\[(.+)\]$') { $section = $Matches[1]; continue }
        if ($section -ceq 'global' -or $section -ceq $caseName) { $allowed += $t }
    }
    return $allowed
}

function Get-DigestExpected($caseName) {
    $file = $env:OPENSWBF2_DIGEST_EXPECTED
    if (-not $file) { $file = Join-Path $PSScriptRoot 'smoke\digest-expected.txt' }
    if (-not (Test-Path -LiteralPath $file)) { return $null }
    foreach ($line in [IO.File]::ReadAllLines($file)) {
        $t = $line.Trim()
        if (-not $t -or $t.StartsWith('#')) { continue }
        $i = $t.IndexOf("`t")
        if ($i -lt 0) { continue }
        if ($t.Substring(0, $i) -ceq $caseName) { return $t.Substring($i + 1) }
    }
    return $null
}

function Get-SmokeUnexpected($caseName, $caseDir) {
    $runLog = Join-Path $caseDir 'run.log'
    if (-not (Test-Path -LiteralPath $runLog)) { return @() }
    $allowed = @(Get-SmokeAllowed $caseName)
    $bad = @()
    foreach ($line in [IO.File]::ReadAllLines($runLog)) {
        $hit = ($line -cmatch '^\[t=\d+\.\d{3}\] \[f=\d+\] \w+ (Error|Bug|Fatal) ') -or
            $line.Contains('ensure failed: ') -or ($line -cmatch '^Message Severity: [345]$')
        if (-not $hit) { continue }
        $covered = $false
        foreach ($rx in $allowed) { if ($line -cmatch $rx) { $covered = $true; break } }
        if (-not $covered) { $bad += $line }
    }
    return $bad
}

function Test-SmokeExpect($case, $res) {
    $x = $case.Expect
    if ($res.Class -ne $x.Class) { return $false }
    if ($x.ContainsKey('Code') -and $res.Code -ne [uint32]$x.Code) { return $false }
    $n = @($res.Dumps).Count
    if ($x.ContainsKey('MinDumps') -and $n -lt $x.MinDumps) { return $false }
    if ($x.ContainsKey('MaxDumps') -and $n -gt $x.MaxDumps) { return $false }
    if ($x.ContainsKey('LogRx')) {
        foreach ($rx in @($x.LogRx)) { if ($res.LogText -cnotmatch $rx) { return $false } }
    }
    if ($x.ContainsKey('NoIni') -and (Test-Path -LiteralPath (Join-Path $res.CaseDir 'display.ini'))) { return $false }
    if ($x.ContainsKey('IniRx')) {
        $ini = Join-Path $res.CaseDir 'display.ini'
        if (-not (Test-Path -LiteralPath $ini)) { return $false }
        $iniText = [IO.File]::ReadAllText($ini)
        foreach ($rx in @($x.IniRx)) { if ($iniText -cnotmatch $rx) { return $false } }
    }
    if (-not $script:SmokeDigestConfiguration -or $script:SmokeDigestConfiguration -eq 'Release') {
        $want = Get-DigestExpected $case.Name
        if ($want) {
            $got = Get-DigestKey $res.CaseDir
            if ($want -cne $got) {
                Write-Host "    digest expected: $want"
                Write-Host "    digest actual: $got"
                return $false
            }
        }
    }
    $unexpected = @(Get-SmokeUnexpected $case.Name $res.CaseDir)
    if ($unexpected.Count -gt 0) {
        foreach ($u in $unexpected) { Write-Host "    unexpected log line: $u" }
        return $false
    }
    return $true
}

$script:SmokeAddrSpaceRx = 'OPENSWBF2 ADDRSPACE arena=0x[89A-F][0-9A-F]{7} arena_end=0x[0-9A-F]{8} laa=1 steam=1 steam_api=0x(?!0{8})[0-9A-F]{8} Galaxy=0x(?!0{8})[0-9A-F]{8} GalaxyPeer=0x[0-9A-F]{8} binkw32=0x(?!0{8})[0-9A-F]{8}'

function Get-SmokeCases($which, $missions, $movies, $dedicatedSpec, $hog = $false) {
    $standard = $script:SmokeStandardMission
    $render = '-norender'
    if ($movies) { $render = '' }
    $tail = (("$render -nosound -fixedrate 30 -nowait -logfinalscore -DecompCONMinutes 5 -seed 1 -exitongameover -nofocuspause -log ""{CASEDIR}\run.log""") -replace '^\s+', '')
    $htail = $tail
    $pass = @{ Class = 'Pass' }
    $hrx = @()
    if ($hog) {
        $htail = "$tail -hogaddrspace"
        $hrx = @($script:SmokeAddrSpaceRx)
        $pass = @{ Class = 'Pass'; LogRx = $hrx }
    }
    $cases = @()

    $missionCase = {
        param($m)
        New-Object PSObject -Property @{ Name = $m; Args = "$m $htail"; Expect = $pass; Twice = 0; Timeout = 0 }
    }
    $allCase = {
        param($m, $class)
        $x = $pass
        $to = 0
        if ($class -eq 'TimeOut') { $x = @{ Class = 'TimeOut'; LogRx = (@('OPENSWBF2 MISSION RUNNING') + $hrx) } }
        elseif ($class -eq 'Fatal') {
            $x = @{ Class = 'Fatal'; Code = 3; MinDumps = 1; MaxDumps = 1; LogRx = (@('FATAL: exception 0xC0000005 at 0x[0-9A-F]+ \(d3dx9_43\.dll\+0x1BA553\)') + $hrx) }
        }
        elseif ($class -ne 'Pass') { throw "unknown expected class '$class' for $m in missions file" }
        New-Object PSObject -Property @{ Name = $m; Args = "$m $htail"; Expect = $x; Twice = 0; Timeout = $to }
    }

    if ($which -eq 'Standard') { $cases += & $missionCase $standard }
    if ($which -eq 'Standard') {
        $cases += New-Object PSObject -Property @{ Name = 'display-switches'; Args = "$standard $htail -borderless -adapter 0 -refresh 60 -vsync half -fps 120 -nowaitquery -nodpi"; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 PACING fps=120 vsync=on interval=0x00000000 waitquery=0 timer=none', 'pacing: fps cap 120 is above 80', 'vsync: requested interval not supported here, using on', 'OPENSWBF2 DPI awareness=off') + $hrx) }; Twice = 0; Timeout = 0 }
        $cases += New-Object PSObject -Property @{ Name = 'pacing-default'; Args = "$standard $htail"; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 PACING fps=80 vsync=off interval=0x80000000 waitquery=0 timer=none', 'OPENSWBF2 DPI awareness=(pmv2|pm|system|none) dpi=\d+', 'OPENSWBF2 GAMMA path=ramp reason=norender', '(?s)\A(?!.*OPENSWBF2 GAMMA FAKE)', '(?s)\A(?!.*OPENSWBF2 GAMMA path=pass)', 'OPENSWBF2 CURSOR mode=hw image=skipped') + $hrx) }; Twice = 0; Timeout = 0 }
        $cases += New-Object PSObject -Property @{ Name = 'cursor-sw'; Args = "$standard $htail -swcursor"; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 CURSOR mode=sw image=skipped', '(?s)\A(?!.*OPENSWBF2 CURSOR mode=hw)') + $hrx) }; Twice = 0; Timeout = 0 }
        $swIni = Join-Path $PSScriptRoot 'smoke\display-swcursor.ini'
        $cases += New-Object PSObject -Property @{ Name = 'cursor-sw-ini'; Args = "$standard $htail -displayini `"$swIni`""; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 CURSOR mode=sw image=skipped', '(?s)\A(?!.*OPENSWBF2 CURSOR mode=hw)') + $hrx) }; Twice = 0; Timeout = 0 }
        $cases += New-Object PSObject -Property @{ Name = 'window-borderless-headless'; Args = "$standard $htail -borderless -adapter 5"; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 DPI awareness=(pmv2|pm|system|none) dpi=\d+') + $hrx) }; Twice = 0; Timeout = 0 }
        $cases += New-Object PSObject -Property @{ Name = 'fps-0'; Args = "$standard $htail -fps 0"; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 PACING fps=0 ', 'pacing: fps cap is uncapped') + $hrx) }; Twice = 0; Timeout = 0 }
        $cases += New-Object PSObject -Property @{ Name = 'fps-30'; Args = "$standard $htail -fps 30"; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 PACING fps=30 ') + $hrx) }; Twice = 0; Timeout = 0 }
        $rtTail = $htail -replace ' -fixedrate 30', ''
        $cases += New-Object PSObject -Property @{ Name = 'pacing-realtime'; Args = "nab2g_c $rtTail -fps 30"; Expect = @{ Class = 'TimeOut'; LogRx = (@('OPENSWBF2 MISSION RUNNING', 'OPENSWBF2 PACING fps=30 ') + $hrx) }; Timeout = 30; Twice = 0 }
        $cases += New-Object PSObject -Property @{ Name = 'pacing-realtime-uncapped'; Args = "nab2g_c $rtTail -fps 0"; Expect = @{ Class = 'TimeOut'; LogRx = (@('OPENSWBF2 MISSION RUNNING', 'OPENSWBF2 PACING fps=0 ') + $hrx) }; Timeout = 30; Twice = 0 }
        $badIni = Join-Path $PSScriptRoot 'smoke\display-bad.ini'
        $cases += New-Object PSObject -Property @{ Name = 'display-badini'; Args = "$standard $htail -adapter abc -refresh 99999 -displayini `"$badIni`""; Expect = $pass; Twice = 0; Timeout = 0 }
        $waitIni = Join-Path $PSScriptRoot 'smoke\display-waitquery.ini'
        $cases += New-Object PSObject -Property @{ Name = 'pacing-waitquery-ini'; Args = "$standard $htail -displayini `"$waitIni`""; Expect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 PACING fps=80 vsync=off interval=0x80000000 waitquery=1 timer=none') + $hrx) }; Twice = 0; Timeout = 0 }
    }
    if ($which -eq 'All') {
        foreach ($line in $missions) {
            $tok = @($line.Trim() -split '\s+')
            if ($tok[0]) {
                $class = 'Pass'
                if ($tok.Count -gt 1) { $class = $tok[1] }
                $cases += & $allCase $tok[0] $class
            }
        }
    }
    if ($which -eq 'Dedicated' -or $which -eq 'All') {
        $dm = $standard
        if ($dedicatedSpec) { $dm = $dedicatedSpec }
        $cases += New-Object PSObject -Property @{
            Name = 'dedicated'
            Args = "/autonet dedicated -lan -nopregame -playercount 0 $dm 100 100 $htail"
            Expect = $pass
            Twice = 0; Timeout = 0
        }
    }
    if ($which -eq 'Crash' -or $which -eq 'All') {
        foreach ($kind in 'nullderef', 'divzero', 'stackoverflow', 'debugbreak', 'abort', 'invalidparam') {
            $cases += New-Object PSObject -Property @{
                Name = "crash-$kind"
                Args = "$standard $tail -crashtest $kind"
                Expect = @{ Class = 'Fatal'; Code = 3; MinDumps = 1; MaxDumps = 1; LogRx = 'FATAL:.*\.dmp' }
                Twice = 0; Timeout = 0
            }
        }
        $ff = @{ Class = 'GsFailure'; Code = 3221226505 }
        if (Test-WerKey) { $ff.MinDumps = 1 }
        $cases += New-Object PSObject -Property @{ Name = 'crash-fastfail'; Args = "$standard $tail -crashtest fastfail"; Expect = $ff; Twice = 0; Timeout = 0 }
        $cases += New-Object PSObject -Property @{
            Name = 'fatal-init'
            Args = "no_such_mission $tail"
            Expect = @{ Class = 'Fatal'; Code = 1; MaxDumps = 0; LogRx = 'FATAL: initialization failed' }
            Twice = 0; Timeout = 0
        }
        foreach ($kind in 'arenaoom', 'debugheapoom') {
            $cases += New-Object PSObject -Property @{
                Name = "crash-$kind"
                Args = "$standard $tail -crashtest $kind"
                Expect = @{ Class = 'Fatal'; Code = 1; MaxDumps = 0; LogRx = ('FATAL: out of memory allocating ' + $(if ($kind -eq 'arenaoom') { 'arena' } else { 'debug heap' }) + ' \(\d+ bytes\) load \d+% phys \d+ MB virt \d+ MB') }
                Twice = 0; Timeout = 0
            }
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-heapfail'
            Args = "$standard $tail -crashtest heapfail"
            Expect = @{ Class = 'Pass'; LogRx = 'alloc failure heap \S+ \(\d+\) size \d+ free \d+ load \d+% phys \d+ MB virt \d+ MB' }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-luaoom'
            Args = "$standard $tail -crashtest luaoom"
            Expect = @{ Class = 'Pass'; LogRx = 'Lua out of memory' }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-luapanic'
            Args = "$standard $tail -crashtest luapanic"
            Expect = @{ Class = 'Fatal'; Code = 4; MaxDumps = 0; LogRx = 'FATAL: lua panic' }
            Twice = 0; Timeout = 0
        }
    }
    if ($which -eq 'Crash' -or $which -eq 'All') {
        $cases += New-Object PSObject -Property @{
            Name = 'crash-hang'
            Args = "$standard $tail -crashtest hang -hangtimeout 5"
            Expect = @{ Class = 'Fatal'; Code = 5; MinDumps = 1; MaxDumps = 1; LogRx = 'FATAL: hang' }
            Twice = 0; Timeout = 30
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-check'
            Args = "$standard $tail -crashtest check"
            Expect = @{ Class = 'Fatal'; Code = 3; MinDumps = 1; MaxDumps = 1; LogRx = @('FATAL: check failed', 'ensure summary: 1 failures at 1 sites') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-resetinitlost'
            Args = "$standard $tail -crashtest resetinitlost"
            Expect = @{ Class = 'Pass'; LogRx = 'OPENSWBF2 RESET recovered after 4 attempts' }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-resetrecover'
            Args = "$standard $tail -crashtest resetrecover"
            Expect = @{ Class = 'Pass'; LogRx = 'OPENSWBF2 RESET recovered after 4 attempts' }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-resetflap'
            Args = "$standard $tail -crashtest resetflap"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 RESET recovered after 25 attempts', '(?s)\A(?!.*OPENSWBF2 RESET failed.*OPENSWBF2 RESET failed)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-resetlaterinit'
            Args = "$standard $tail -crashtest resetlaterinit"
            Expect = @{ Class = 'InitFailure'; Code = 0; MaxDumps = 0; LogRx = '(?s)OPENSWBF2 MISSION RUNNING.*OPENSWBF2 CLOSE deferred until loading finishes.*OPENSWBF2 RESET recovered after 2 attempts' }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-resetfatal'
            Args = "$standard $tail -crashtest resetfatal"
            Expect = @{ Class = 'Fatal'; Code = 1; MinDumps = 1; MaxDumps = 1; LogRx = '(?s)\A(?:(?!FATAL:).)*OPENSWBF2 RESET reverting asked=\d+x\d+ windowed=\d used=\d+x\d+ windowed=\d(?:(?!FATAL:).)*FATAL: device reset failed hr=0x8876086C(?:(?!FATAL:).)*\z' }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-applymode'
            Args = "$standard $tail -crashtest applymode"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY applied \d+x\d+ mode=', 'OPENSWBF2 APPLY TARGETS alive=\d+ fresh=(?:[2-9]|[1-9]\d+) stale=0 buffers=3/3 moved=1 ', '(?s)OPENSWBF2 DISPLAY applied (\d+)x(\d+) .*OPENSWBF2 DISPLAY relayout generation=\d+ screen=\1x\2 view=\1x\2 hud=\1x\2 aspect=(\d+\.\d{4}) camera=\3\b', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-applymodesmall'
            Args = "$standard $tail -crashtest applymodesmall"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY applied 320x240 mode=', 'OPENSWBF2 APPLY TARGETS alive=\d+ fresh=(?:[3-9]|[1-9]\d+) stale=0 buffers=3/3 moved=3 ', '(?s)OPENSWBF2 DISPLAY applied (\d+)x(\d+) .*OPENSWBF2 DISPLAY relayout generation=\d+ screen=\1x\2 view=\1x\2 hud=\1x\2 ', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-applyfail'
            Args = "$standard $tail -crashtest applyfail"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY reverted asked=(\d+)x(\d+) mode=\d+ used=(?!\1x\2 )', '(?s)OPENSWBF2 DISPLAY reverted .*OPENSWBF2 RESET recovered after \d+ attempts', '(?s)OPENSWBF2 DISPLAY reverted .*OPENSWBF2 DISPLAY profile restored ', '(?s)\A(?!.*FATAL:)', '(?s)\A(?!.*OPENSWBF2 DISPLAY applied)', '(?s)\A(?!.*OPENSWBF2 DISPLAY relayout)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-applypresentfail'
            Args = "$standard $tail -crashtest applypresentfail"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY reverted asked=(\d+)x(\d+) mode=\d+ used=(?!\1x\2 )', '(?s)\A(?!.*FATAL:)', '(?s)\A(?!.*OPENSWBF2 DISPLAY applied)', '(?s)OPENSWBF2 DISPLAY reverted asked=\d+x\d+ mode=\d+ used=(\d+)x(\d+) .*OPENSWBF2 DISPLAY relayout generation=\d+ screen=\1x\2 view=\1x\2 hud=\1x\2 (?!.*OPENSWBF2 DISPLAY relayout)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-applytargetfail'
            Args = "$standard $tail -crashtest applytargetfail"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY reverted asked=(\d+)x(\d+) mode=\d+ used=(?!\1x\2 )\d+x\d+ mode=\d+ hr=0x8876017C', '(?s)\A(?!.*FATAL:)', '(?s)\A(?!.*OPENSWBF2 DISPLAY applied)', '(?s)OPENSWBF2 DISPLAY reverted asked=\d+x\d+ mode=\d+ used=(\d+)x(\d+) .*OPENSWBF2 DISPLAY relayout generation=\d+ screen=\1x\2 view=\1x\2 hud=\1x\2 (?!.*OPENSWBF2 DISPLAY relayout)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-gammapass'
            Args = "$standard $tail -crashtest gammapass"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 GAMMA path=pass', '(?s)OPENSWBF2 GAMMA FAKE target=(\d+)x(\d+) alive=1 bb=\1x\2 draws=[1-9]\d* ramp_nonid=0.*OPENSWBF2 DISPLAY applied \1x\2 ', '(?s)OPENSWBF2 RESET recovered after \d+ attempts.*OPENSWBF2 GAMMA FAKE target=(\d+)x(\d+) alive=1 bb=\1x\2 draws=[1-9]\d* ramp_nonid=0', '(?s)\A(?!.*ramp_nonid=[1-9])', '(?s)\A(?!.*OPENSWBF2 GAMMA fallback)', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-gammafallback'
            Args = "$standard $tail -crashtest gammafallback"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 GAMMA path=ramp reason=createfail hr=0x8876017C', 'OPENSWBF2 GAMMA FAKE target=\d+x\d+ alive=0 bb=\d+x\d+ draws=0 ramp_nonid=[1-9]', '(?s)\A(?!.*OPENSWBF2 GAMMA path=pass)', '(?s)\A(?!.*draws=[1-9])', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-gammalost'
            Args = "$standard $tail -crashtest gammalost"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 GAMMA path=pass', 'OPENSWBF2 GAMMA FAKE target=(\d+)x(\d+) alive=1 bb=\1x\2 draws=[1-9]\d* ramp_nonid=0', '(?s)\A(?!.*OPENSWBF2 GAMMA path=ramp)', '(?s)\A(?!.*OPENSWBF2 GAMMA fallback)', '(?s)\A(?!.*ramp_nonid=[1-9])', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-shellstretch'
            Args = "$standard $tail -crashtest shellstretch"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY shell stretch linear (\d+)x(\d+)->(\d+)x(\d+)', '(?s)OPENSWBF2 SHELLSTRETCH FAKE chain=\d+x\d+ presents=[1-9]\d* filter=linear.*OPENSWBF2 RESET recovered after \d+ attempts.*OPENSWBF2 SHELLSTRETCH FAKE chain=\d+x\d+ presents=[1-9]\d* filter=linear', '(?s)\A(?!.*shell stretch unavailable)', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-gammadrawfail'
            Args = "$standard $tail -crashtest gammadrawfail"
            Expect = @{ Class = 'Pass'; LogRx = @('(?s)OPENSWBF2 GAMMA path=pass.*OPENSWBF2 GAMMA fallback to ramp reason=drawfail hr=0x8876086C', 'OPENSWBF2 GAMMA FAKE target=\d+x\d+ alive=0 bb=\d+x\d+ draws=0 ramp_nonid=[1-9]', '(?s)\A(?!.*OPENSWBF2 GAMMA fallback.*OPENSWBF2 GAMMA fallback)', '(?s)\A(?!.*draws=[1-9])', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-applyadapter'
            Args = "$standard $tail -crashtest applyadapter -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY adapter=1 invalid', 'OPENSWBF2 DISPLAY adapter=0 pending, running adapter unchanged', '(?s)\A(?!.*OPENSWBF2 DISPLAY adapter=\d+ saved)', '(?s)\A(?!.*FATAL:)'); NoIni = $true }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-adaptercancel'
            Args = "$standard $tail -crashtest adaptercancel -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('(?s)OPENSWBF2 DISPLAY adapter=0 pending.*OPENSWBF2 DISPLAY menu cancel restored.*OPENSWBF2 DISPLAY adaptertest save=0', '(?s)\A(?!.*OPENSWBF2 DISPLAY adapter=\d+ saved)', '(?s)\A(?!.*FATAL:)'); NoIni = $true }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-adapteraccept'
            Args = "$standard $tail -crashtest adapteraccept -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('(?s)OPENSWBF2 DISPLAY adapter=0 pending, running adapter unchanged.*OPENSWBF2 DISPLAY adapter=0 saved, running adapter unchanged.*OPENSWBF2 DISPLAY menutest save=1', 'OPENSWBF2 DISPLAY confirm timeout, reverting', 'OPENSWBF2 DISPLAY menutest done result=2', '(?s)\A(?!.*FATAL:)'); IniRx = @('(?m)^Adapter=0\r?$', '(?s)\A(?!.*\bVsync=)(?!.*\bAdapter=1)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menuconfirm'
            Args = "$standard $tail -crashtest menuconfirm"
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY confirm pending 15s', '(?s)OPENSWBF2 DISPLAY confirm pending 15s.*OPENSWBF2 DISPLAY menutest plain vsync=\d.*OPENSWBF2 DISPLAY confirm timeout, reverting to ', '(?s)OPENSWBF2 DISPLAY menutest base (\d+x\d+).*OPENSWBF2 DISPLAY confirm timeout, reverting to \1 mode=', '(?s)OPENSWBF2 DISPLAY confirm timeout, reverting to (\d+x\d+) .*OPENSWBF2 DISPLAY applied \1 ', '(?s)OPENSWBF2 DISPLAY menutest base \d+x\d+.*?OPENSWBF2 DISPLAY applied \d+x\d+ mode=\w+ refresh=\S+ vsync=(\w+) .*OPENSWBF2 DISPLAY confirm timeout, reverting to .*?OPENSWBF2 DISPLAY applied \d+x\d+ mode=\w+ refresh=\S+ vsync=\1 ', '(?s)OPENSWBF2 DISPLAY menutest plain vsync=\d.*?OPENSWBF2 DISPLAY applied \d+x\d+ mode=\w+ refresh=\S+ vsync=(\w+) .*OPENSWBF2 DISPLAY confirm timeout, reverting to .*?OPENSWBF2 DISPLAY applied \d+x\d+ mode=\w+ refresh=\S+ vsync=(?!\1 )', 'OPENSWBF2 DISPLAY menutest done result=2', '(?s)OPENSWBF2 DISPLAY menutest base \d+x\d+ fps=(\d+) waitquery=(\d)\b.*OPENSWBF2 DISPLAY confirm timeout, reverting to \d+x\d+ mode=\w+ fps=\1 waitquery=\2\b.*OPENSWBF2 DISPLAY menutest done result=2 fps=\1 waitquery=\2\b','(?s)\A(?!.*OPENSWBF2 DISPLAY confirm kept)', '(?s)\A(?!.*FATAL:)', 'OPENSWBF2 DISPLAY menu options built adapters=\d+', '(?s)\A(?!.*OPENSWBF2 LUA error)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menukeep'
            Args = "$standard $tail -crashtest menukeep -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY confirm pending 15s', 'OPENSWBF2 DISPLAY confirm kept', 'OPENSWBF2 DISPLAY menutest done result=1', 'OPENSWBF2 DISPLAY menutest save=1', 'OPENSWBF2 DISPLAY menu save deferred until the confirm ends', '(?s)OPENSWBF2 DISPLAY confirm kept.*OPENSWBF2 DISPLAY menu saved keys=0x', 'menutest done result=1 fps=\d+ waitquery=\d keepsave=0', '(?s)\A(?!.*OPENSWBF2 DISPLAY confirm timeout)', '(?s)\A(?!.*FATAL:)'); IniRx = @('(?m)^Vsync=2\r?$', '(?s)\A(?!.*\bFps=)(?!.*\bRefresh=)(?!.*\bNoWaitQuery=)(?!.*\bBorderless=)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menusavefail'
            Args = "$standard $tail -crashtest menusavefail -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY menutest save=1', '(?s)OPENSWBF2 DISPLAY confirm kept.*OPENSWBF2 DISPLAY menu save failed', 'OPENSWBF2 DISPLAY menutest save=2', 'menutest done result=1 fps=\d+ waitquery=\d keepsave=2', '(?s)\A(?!.*FATAL:)'); IniRx = @('(?s)\A(?!.*\bVsync=)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menuorder'
            Args = "$standard $tail -crashtest menuorder"
            Expect = @{ Class = 'Pass'; LogRx = @('(?s)OPENSWBF2 DISPLAY menutest order=legacy-menu(?:(?!OPENSWBF2 DISPLAY applied ).)*OPENSWBF2 DISPLAY applied (?:(?!OPENSWBF2 DISPLAY applied ).)*OPENSWBF2 DISPLAY confirm pending 15s', 'OPENSWBF2 DISPLAY confirm timeout, reverting to ', 'OPENSWBF2 DISPLAY menutest done result=2', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menuorderrev'
            Args = "$standard $tail -crashtest menuorderrev"
            Expect = @{ Class = 'Pass'; LogRx = @('(?s)OPENSWBF2 DISPLAY menutest order=menu-legacy(?:(?!OPENSWBF2 DISPLAY applied ).)*OPENSWBF2 DISPLAY applied (?:(?!OPENSWBF2 DISPLAY applied ).)*OPENSWBF2 DISPLAY confirm pending 15s', 'OPENSWBF2 DISPLAY confirm timeout, reverting to ', 'OPENSWBF2 DISPLAY menutest done result=2', '(?s)\A(?!.*FATAL:)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menurevertsave'
            Args = "$standard $tail -crashtest menurevertsave -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY menutest save=1', '(?s)OPENSWBF2 DISPLAY confirm pending 15s.*OPENSWBF2 DISPLAY confirm timeout, reverting to .*OPENSWBF2 DISPLAY menu saved keys=0x10\b.*OPENSWBF2 DISPLAY menu cancel restored', '(?s)OPENSWBF2 DISPLAY menutest prior vsync=(\d+) fps=(\d+)\b.*OPENSWBF2 DISPLAY menutest restored vsync=\1 fps=(?!\2\b)\d+', 'OPENSWBF2 DISPLAY menutest done result=2 fps=\d+ waitquery=\d keepsave=0', '(?s)\A(?!.*FATAL:)'); IniRx = @('(?m)^Fps=(?:30|60)\r?$', '(?s)\A(?!.*\bVsync=)(?!.*\bBorderless=)(?!.*\bRefresh=)') }
            Twice = 0; Timeout = 0
        }
        $cases += New-Object PSObject -Property @{
            Name = 'crash-menuapplyfail'
            Args = "$standard $tail -crashtest menuapplyfail -displayini ""{CASEDIR}\display.ini"""
            Expect = @{ Class = 'Pass'; LogRx = @('OPENSWBF2 DISPLAY menutest save=1', '(?s)OPENSWBF2 DISPLAY menutest base \d+x\d+ fps=\d+ waitquery=(\d)\b.*OPENSWBF2 DISPLAY menutest prior vsync=\d+ fps=\d+ accept=(\d+)\b.*OPENSWBF2 DISPLAY reverted asked=.*OPENSWBF2 DISPLAY profile restored \d+x\d+ windowed=\d vsync=\d fps=\2 waitquery=\1\b.*OPENSWBF2 DISPLAY menu saved keys=0x10\b.*OPENSWBF2 DISPLAY menutest done result=0 fps=\2 waitquery=\1 keepsave=0', '(?s)\A(?!.*OPENSWBF2 DISPLAY confirm pending)', '(?s)\A(?!.*OPENSWBF2 DISPLAY applied)', '(?s)\A(?!.*FATAL:)'); IniRx = @('(?m)^Fps=(?:30|60)\r?$', '(?s)\A(?!.*\bVsync=)(?!.*\bBorderless=)(?!.*\bRefresh=)(?!.*\bNoWaitQuery=)') }
            Twice = 0; Timeout = 0
        }
    }
    if ($which -eq 'Twice') {
        foreach ($i in 1, 2) {
            $cases += New-Object PSObject -Property @{ Name = "twice-$i"; Args = "$standard $htail"; Expect = $pass; Twice = $i; Timeout = 0 }
        }
        $digestExpect = @{ Class = 'TimeOut'; LogRx = (@('OPENSWBF2 MISSION RUNNING', 'OPENSWBF2 SIM DIGEST turns=') + $hrx) }
        foreach ($i in 1, 2) {
            $cases += New-Object PSObject -Property @{ Name = "twice-digest-$i"; Args = "nab2g_c $htail"; Expect = $digestExpect; Twice = 2 + $i; Timeout = 30 }
        }
        foreach ($m in 'hot1g_con', 'spa3c_ass') {
            $cases += New-Object PSObject -Property @{ Name = "digest-$m"; Args = "$m $htail"; Expect = $pass; Twice = 0; Timeout = 0 }
        }
        $traceExpect = @{ Class = 'Pass'; LogRx = (@('OPENSWBF2 DIGEST TRACE turn=1500 ') + $hrx) }
        foreach ($i in 1, 2) {
            $ttail = $htail.Replace('-seed 1 ', "-seed $i ")
            $cases += New-Object PSObject -Property @{ Name = "trace-seed-$i"; Args = "hot1g_con $ttail -digesttrace 1600"; Expect = $traceExpect; Twice = 0; Timeout = 0 }
        }
    }
    return $cases
}

function Get-DigestKey($caseDir) {
    $f = Join-Path $caseDir 'run.log'
    if (-not (Test-Path -LiteralPath $f)) { return $null }
    $m = Select-String -LiteralPath $f -Pattern 'OPENSWBF2 SIM DIGEST .*' | Select-Object -First 1
    if (-not $m) { return $null }
    return $m.Matches[0].Value
}

function Get-DigestTrace($runLog) {
    $map = @{}
    if (-not (Test-Path -LiteralPath $runLog)) { return $map }
    foreach ($m in (Select-String -LiteralPath $runLog -Pattern 'OPENSWBF2 DIGEST TRACE turn=(\d+) (.*)' -CaseSensitive)) {
        $map[[int]$m.Matches[0].Groups[1].Value] = $m.Matches[0].Groups[2].Value
    }
    return $map
}

function Compare-DigestTrace($logA, $logB) {
    $a = Get-DigestTrace $logA
    $b = Get-DigestTrace $logB
    if ($a.Count -eq 0 -or $b.Count -eq 0) { return -1 }
    $last = [Math]::Max($a.Count, $b.Count)
    for ($t = 1; $t -le $last; $t++) {
        if (-not $a.ContainsKey($t) -or -not $b.ContainsKey($t)) { return -1 }
        if (($a[$t] -split ' ')[0] -cne ($b[$t] -split ' ')[0]) { return $t }
    }
    return 0
}

function Get-FinalScoreKey($caseDir) {
    $f = Join-Path $caseDir 'finalscore.log'
    if (-not (Test-Path -LiteralPath $f)) { return $null }
    $line = Get-Content -LiteralPath $f -TotalCount 1
    if (-not $line) { return $null }
    $i = $line.IndexOf("`t")
    if ($i -lt 0) { return $line }
    return $line.Substring($i + 1)
}

function Find-Dumpbin {
    $cands = @()
    if ($env:VS120COMNTOOLS) { $cands += (Join-Path $env:VS120COMNTOOLS '..\..\VC\bin\dumpbin.exe') }
    $cands += (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio 12.0\VC\bin\dumpbin.exe')
    foreach ($c in $cands) { if (Test-Path -LiteralPath $c) { return (Resolve-Path -LiteralPath $c).Path } }
    Write-Host 'dumpbin not found (install VS2013 Update 5).'
    exit 2
}

function Find-Editbin {
    $cands = @()
    if ($env:VS120COMNTOOLS) { $cands += (Join-Path $env:VS120COMNTOOLS '..\..\VC\bin\editbin.exe') }
    $cands += (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio 12.0\VC\bin\editbin.exe')
    foreach ($c in $cands) { if (Test-Path -LiteralPath $c) { return (Resolve-Path -LiteralPath $c).Path } }
    Write-Host 'editbin not found (install VS2013 Update 5).'
    exit 2
}

function Compare-LaaRuns($base, $laa, $minRuns = 5) {
    $failures = New-Object System.Collections.ArrayList
    $base = @($base)
    $laa = @($laa)
    if ($base.Count -lt $minRuns) { [void]$failures.Add("only $($base.Count) non-LAA runs, need $minRuns") }
    if ($laa.Count -lt $minRuns) { [void]$failures.Add("only $($laa.Count) LAA runs, need $minRuns") }
    $keys = @(($base + $laa) | ForEach-Object { "$($_.Key) [f=$($_.Frames)]" } | Select-Object -Unique)
    if ($keys.Count -gt 1) { [void]$failures.Add("runs differ: $($keys -join ' | ')") }
    return New-Object PSObject -Property @{ Ok = ($failures.Count -eq 0); Failures = $failures; Keys = $keys }
}

function Get-HardeningChecks($hdr, $cfg, $clTotal, $clGsMinus, $clSdl) {
    $relocSize = 0
    if ($hdr -match '(?m)^\s+[0-9A-F]+ \[\s*([0-9A-F]+)\] RVA \[size\] of Base Relocation Directory') { $relocSize = [Convert]::ToInt32($Matches[1], 16) }
    $seh = 0
    if ($cfg -match '(?m)^\s+([0-9A-F]+) Safe Exception Handler Count') { $seh = [Convert]::ToInt32($Matches[1], 16) }
    $cookie = 0
    if ($cfg -match '(?m)^\s+([0-9A-F]+) Security Cookie') { $cookie = [Convert]::ToInt64($Matches[1], 16) }
    $hasReloc =$hdr -match '(?m)^\s+\.reloc name\s*$'
    $stripped = $hdr -match '(?m)^\s+Relocations stripped\s*$'
    $checks = @()
    $dyn = $hdr -match '(?m)^\s+Dynamic base\s*$'
    $checks += New-Object PSObject -Property @{ Name = 'dynamicbase'; Ok = $dyn; Detail = 'Dynamic base flag missing' }
    $nx = $hdr -match '(?m)^\s+NX compatible\s*$'
    $checks += New-Object PSObject -Property @{ Name = 'nxcompat'; Ok = $nx; Detail = 'NX compatible flag missing' }
    $laa = $hdr -match '(?m)^\s+Application can handle large \(>2GB\) addresses\s*$'
    $checks += New-Object PSObject -Property @{ Name = 'laa'; Ok = $laa; Detail = 'Application can handle large (>2GB) addresses missing from dumpbin /headers' }
    $checks += New-Object PSObject -Property @{
        Name = 'reloc'
        Ok = ($hasReloc -and $relocSize -gt 0 -and -not $stripped)
        Detail = ('.reloc={0} reloc-dir-size={1} stripped={2}' -f $hasReloc, $relocSize, $stripped)
    }
    $checks += New-Object PSObject -Property @{ Name = 'safeseh'; Ok = ($seh -gt 0); Detail = "Safe Exception Handler Count=$seh" }
    $checks += New-Object PSObject -Property @{
        Name = 'gs'
        Ok = ($clTotal -gt 0 -and $clGsMinus -eq 0 -and $cookie -ne 0)
        Detail = ('cl entries={0} /GS-={1} security-cookie=0x{2:X8}' -f $clTotal, $clGsMinus, $cookie)
    }
    $checks += New-Object PSObject -Property @{
        Name = 'sdl'
        Ok = ($clTotal -gt 0 -and $clSdl -eq $clTotal)
        Detail = ('cl entries={0} /sdl={1}' -f $clTotal, $clSdl)
    }
    return $checks
}

function Get-ImportChecks($imp) {
    $rows = @()
    foreach ($n in 'SetProcessDpiAwarenessContext', 'SetProcessDpiAwareness', 'SetProcessDPIAware', 'GetDpiForWindow', 'AdjustWindowRectExForDpi') {
        $rows += New-Object PSObject -Property @{ Name = "import-$n"; Ok = ($imp -cnotmatch "(?m)\s$n\s*$"); Detail = "$n statically imported (breaks XP load)" }
    }
    $rows += New-Object PSObject -Property @{ Name = 'import-shcore'; Ok = ($imp -notmatch '(?mi)^\s+shcore\.dll\s*$'); Detail = 'shcore.dll in import table' }
    return $rows
}

function Get-ClTlogCounts($objDir) {
    $total = 0
    $gsMinus = 0
    $sdl = 0
    foreach ($f in Get-ChildItem -LiteralPath $objDir -Filter 'cl.command*.tlog' -Recurse -ErrorAction SilentlyContinue) {
        foreach ($l in (Get-Content -LiteralPath $f.FullName)) {
            if ($l -and -not $l.StartsWith('^')) {
                $total++
                if ($l -match ' /GS-( |$)') { $gsMinus++ }
                if ($l -match ' /sdl( |$)') { $sdl++ }
            }
        }
    }
    return New-Object PSObject -Property @{ Total = $total; GsMinus = $gsMinus; Sdl = $sdl }
}

function Get-ImageBase($exe, $workDir, $argLine) {
    $p = Start-Process -FilePath $exe -ArgumentList $argLine -WorkingDirectory $workDir -PassThru -WindowStyle Hidden
    try {
        $base = $null
        for ($i = 0; $i -lt 50 -and -not $base; $i++) {
            Start-Sleep -Milliseconds 200
            try { $base = $p.MainModule.BaseAddress.ToInt64() } catch {}
        }
        return $base
    }
    finally {
        try { if (-not $p.HasExited) { & taskkill /PID $p.Id /T /F 2>&1 | Out-Null } } catch {}
        $p.WaitForExit()
    }
}
