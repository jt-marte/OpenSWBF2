$ErrorActionPreference = 'Stop'
$run = Join-Path $PSScriptRoot 'run.ps1'
$tmp = Join-Path $env:TEMP ("openswbf2-selftest-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null

$list = 'echo %* | findstr /C:"list-test-cases" >nul && (echo ==========& echo fake_case& echo ==========& exit /b 0)'
$oneFail = 'echo [doctest] test cases: 1 ^| 0 passed ^| 1 failed'

$scenarios = @(
    @('listing exit non-zero', 'unit', 'exit /b 3', $null, 2, 'Listing failed', $null),
    @('listing zero cases', 'unit', 'exit /b 0', $null, 2, 'listed 0 cases', $null),
    @('listing timeout', 'unit', "ping -n 3 127.0.0.1 >nul`r`nexit /b 0", '1000', 2, 'timed out', $null),
    @('known_bug did not run once', 'known_bug', "$list`r`nexit /b 0", $null, 1, '\[FAIL\] fake_case', '\[KNOWN\]'),
    @('known_bug genuine failure', 'known_bug', "$list`r`n$oneFail`r`nexit /b 1", $null, 0, '\[KNOWN\] fake_case', $null),
    @('known_bug exit 1 without 1-case output', 'known_bug', "$list`r`nexit /b 1", $null, 1, '\[FAIL\] fake_case', '\[KNOWN\]'),
    @('known_bug crash', 'known_bug', "$list`r`nexit /b 3221225477", $null, 1, '\[CRASH\] fake_case', '\[KNOWN\]')
)

$failed = 0
try {
    $n = 0
    foreach ($sc in $scenarios) {
        $n++
        $stub = Join-Path $tmp "stub$n.cmd"
        Set-Content -Path $stub -Value ("@echo off`r`n" + $sc[2]) -Encoding ASCII
        $env:OPENSWBF2_TEST_EXE = $stub
        if ($sc[3]) { $env:OPENSWBF2_LIST_TIMEOUT_MS = $sc[3] }
        try {
            $out = (& powershell -NoProfile -File $run -NoBuild -Smoke None -Suite $sc[1] 2>&1 | Out-String)
            $code = $LASTEXITCODE
        }
        finally {
            Remove-Item Env:\OPENSWBF2_LIST_TIMEOUT_MS -ErrorAction SilentlyContinue
        }
        $why = $null
        if ($code -ne $sc[4]) { $why = "exit $code, expected $($sc[4])" }
        elseif ($out -notmatch $sc[5]) { $why = "output missing /$($sc[5])/" }
        elseif ($sc[6] -and $out -match $sc[6]) { $why = "output unexpectedly matches /$($sc[6])/" }
        if ($why) { $failed++; Write-Host "FAILED ${n}: $($sc[0]): $why" }
        else { Write-Host "OK $n $($sc[0])" }
    }
}
finally {
    Remove-Item Env:\OPENSWBF2_TEST_EXE -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
. (Join-Path $PSScriptRoot 'smoke.ps1')
$smokeTmp = Join-Path $env:TEMP ("openswbf2-smoke-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $smokeTmp | Out-Null
$smokeScenarios = @(
    @('smoke exit 0 without marker', 'exit /b 0', 5, 'InitFailure'),
    @('smoke fast-fail code', 'exit /b 3221226505', 5, 'GsFailure'),
    @('smoke exit 3', 'exit /b 3', 5, 'Fatal'),
    @('smoke exit 4', 'exit /b 4', 5, 'Fatal'),
    @('smoke marker in log', 'echo OPENSWBF2 RUN COMPLETE>%2', 5, 'Pass'),
    @('smoke timeout', "ping -n 31 127.0.0.1 >nul`r`nexit /b 0", 2, 'TimeOut')
)
try {
    foreach ($sc in $smokeScenarios) {
        $n++
        $stub = Join-Path $smokeTmp "smoke$n.cmd"
        Set-Content -Path $stub -Value ("@echo off`r`n" + $sc[1]) -Encoding ASCII
        $caseDir = Join-Path $smokeTmp "case$n"
        New-Item -ItemType Directory -Path $caseDir | Out-Null
        $r = Invoke-SmokeCase "t$n" $stub $smokeTmp ('-log "' + $caseDir + '\run.log"') $sc[2] $caseDir
        $why = $null
        if ($r.Class -ne $sc[3]) { $why = "class $($r.Class), expected $($sc[3])" }
        elseif ($sc[3] -eq 'TimeOut' -and @(Get-CimInstance Win32_Process -Filter "Name='PING.EXE'" | Where-Object { $_.CommandLine -like '*-n 31 127.0.0.1*' }).Count -gt 0) { $why = 'child ping survived the kill' }
        if ($why) { $failed++; Write-Host "FAILED ${n}: $($sc[0]): $why" }
        else { Write-Host "OK $n $($sc[0])" }
    }
    $n++
    if ((Get-SmokeClass $false $false 0 'x OPENSWBF2 RUN COMPLETE y' @('a.dmp')) -ne 'Fatal') { $failed++; Write-Host "FAILED ${n}: smoke marker plus dump must be Fatal" }
    else { Write-Host "OK $n smoke marker plus dump is Fatal" }
    $n++
    $pre = Get-SmokePrecheck $null $smokeTmp
    if ($pre -notmatch 'GameData') { $failed++; Write-Host "FAILED ${n}: smoke precheck without GameData" }
    else { Write-Host "OK $n smoke precheck without GameData" }
    $n++
    $preDir = Join-Path $smokeTmp 'precheck'
    New-Item -ItemType Directory -Force -Path (Join-Path $preDir 'retail\_lvl_pc') | Out-Null
    Set-Content -Path (Join-Path $preDir 'retail\_lvl_pc\shell.lvl') -Value 'x'
    foreach ($f in 'BattlefrontII.exe', 'binkw32.dll', 'Galaxy.dll', 'GalaxyPeer.dll', 'steam_api.dll') { Set-Content -Path (Join-Path $preDir $f) -Value 'x' }
    & cmd /c mklink /J "$(Join-Path $preDir 'data')" "$(Join-Path $preDir 'retail')" | Out-Null
    $pre = Get-SmokePrecheck $smokeTmp $preDir
    & cmd /c rmdir "$(Join-Path $preDir 'data')" | Out-Null
    if ($pre -notmatch 'staged data') { $failed++; Write-Host "FAILED ${n}: smoke precheck rejects a retail data junction" }
    else { Write-Host "OK $n smoke precheck rejects a retail data junction" }
}
finally {
    Remove-Item -Recurse -Force $smokeTmp -ErrorAction SilentlyContinue
}
$goodHdr = "    Characteristics`n         102 characteristics`n    DLL characteristics`n            8140 DLL characteristics`n                   Dynamic base`n                   NX compatible`n                   Application can handle large (>2GB) addresses`n   1C13000 [   38D38] RVA [size] of Base Relocation Directory`n  .reloc name`n"
$cookieCfg = "            0085C930 Security Cookie`n"
$goodCfg = "$cookieCfg            008327F0 Safe Exception Handler Table`n                 7CF Safe Exception Handler Count`n"
$oldHdr = "    Characteristics`n         103 characteristics`n                   Relocations stripped`n                   NX compatible`n         0 [       0] RVA [size] of Base Relocation Directory`n"
$hardScenarios = @(
    @('hardening good text passes all', $goodHdr, $goodCfg, 736, 0, 736, @('laa', 'dynamicbase', 'nxcompat', 'reloc', 'safeseh', 'gs', 'sdl'), @()),
    @('hardening old exe fails dynamicbase and reloc', $oldHdr, $goodCfg, 736, 0, 736, @('nxcompat', 'safeseh', 'gs', 'sdl'), @('dynamicbase', 'reloc', 'laa')),
    @('hardening safeseh count 0 fails', $goodHdr, "$cookieCfg                 0 Safe Exception Handler Count`n", 736, 0, 736, @('laa', 'dynamicbase', 'nxcompat', 'reloc', 'gs', 'sdl'), @('safeseh')),
    @('hardening missing security cookie fails gs', $goodHdr, "                 7CF Safe Exception Handler Count`n", 736, 0, 736, @('laa', 'dynamicbase', 'nxcompat', 'reloc', 'safeseh', 'sdl'), @('gs')),
    @('hardening /GS- entries fail gs', $goodHdr, $goodCfg, 736, 3, 736, @('laa', 'dynamicbase', 'nxcompat', 'reloc', 'safeseh', 'sdl'), @('gs')),
    @('hardening empty tlog fails gs and sdl', $goodHdr, $goodCfg, 0, 0, 0, @('laa', 'reloc', 'safeseh'), @('gs', 'sdl')),
    @('hardening missing /sdl entry fails sdl', $goodHdr, $goodCfg, 736, 0, 735, @('laa', 'dynamicbase', 'nxcompat', 'reloc', 'safeseh', 'gs'), @('sdl')),
    @('hardening non-LAA header fails laa', $goodHdr.Replace('Application can handle large (>2GB) addresses', 'Something else'), $goodCfg, 736, 0, 736, @('dynamicbase', 'nxcompat', 'reloc', 'safeseh', 'gs', 'sdl'), @('laa'))
)
foreach ($sc in $hardScenarios) {
    $n++
    $got = Get-HardeningChecks $sc[1] $sc[2] $sc[3] $sc[4] $sc[5]
    $why = $null
    foreach ($name in $sc[6]) { if (-not ($got | Where-Object { $_.Name -eq $name }).Ok) { $why = "$name should pass" } }
    foreach ($name in $sc[7]) { if (($got | Where-Object { $_.Name -eq $name }).Ok) { $why = "$name should fail" } }
    if ($why) { $failed++; Write-Host "FAILED ${n}: $($sc[0]): $why" }
    else { Write-Host "OK $n $($sc[0])" }
}
$n++
$allCases = @(Get-SmokeCases 'All' @('foo1c_ctf TimeOut', 'tat3c_con Fatal', 'bar1c_con', 'quux1c_con Pass') $false $null)
$byName = @{}
foreach ($c in $allCases) { $byName[$c.Name] = $c }
$rejected = 0
foreach ($badLine in 'baz1c_con Bogus', 'qux1c_con GsFailure') {
    try { Get-SmokeCases 'All' @($badLine) $false $null | Out-Null } catch { $rejected++ }
}
$why = $null
if ($byName['foo1c_ctf'].Expect.Class -ne 'TimeOut' -or $byName['foo1c_ctf'].Timeout -ne 0 -or $byName['foo1c_ctf'].Expect.LogRx -ne 'OPENSWBF2 MISSION RUNNING') { $why = 'TimeOut line' }
elseif ($byName['tat3c_con'].Expect.Class -ne 'Fatal' -or $byName['tat3c_con'].Timeout -ne 0) { $why = 'Fatal line' }
elseif (-not $byName['tat3c_con'].Expect.LogRx -or $byName['tat3c_con'].Expect.Code -ne 3 -or $byName['tat3c_con'].Expect.MaxDumps -ne 1) { $why = 'Fatal line must pin the d3dx9 signature' }
elseif ('FATAL: exception 0xC0000005 at 0x658FA553 (d3dx9_43.dll+0x1BA553) build 6ADE5B7C1C4C000' -notmatch $byName['tat3c_con'].Expect.LogRx) { $why = 'Fatal LogRx does not match the recorded tat3 crash' }
elseif ($byName['bar1c_con'].Expect.Class -ne 'Pass' -or $byName['quux1c_con'].Expect.Class -ne 'Pass') { $why = 'missing or Pass class must be Pass' }
elseif ($rejected -ne 2) { $why = 'unknown and GsFailure classes must be rejected' }
elseif (@($allCases | Where-Object { $_.Name -like 'crash-*' }).Count -ne 39 -or -not $byName.ContainsKey('fatal-init') -or $byName['dedicated'].Timeout -ne 0) { $why = 'dedicated or crash cases changed' }
if ($why) { $failed++; Write-Host "FAILED ${n}: all-missions parsing: $why" }
else { Write-Host "OK $n all-missions parsing" }
$n++
$hrx = $script:SmokeAddrSpaceRx
$okLine = 'OPENSWBF2 ADDRSPACE arena=0x8000F020 arena_end=0x8400F020 laa=1 steam=1 steam_api=0x65740000 Galaxy=0x652E0000 GalaxyPeer=0x00000000 binkw32=0x18000000'
$badLines = @($okLine.Replace('arena=0x8000F020', 'arena=0x00EA7020'), $okLine.Replace('laa=1', 'laa=0'), $okLine.Replace('steam=1', 'steam=0'), $okLine.Replace('Galaxy=0x652E0000', 'Galaxy=0x00000000'), $okLine.Replace('binkw32=0x18000000', 'binkw32=0x00000000'), $okLine.Replace('steam_api=0x65740000', 'steam_api=0x00000000'))
$hogCases = @(Get-SmokeCases 'All' @('foo1c_ctf TimeOut', 'tat3c_con Fatal', 'bar1c_con') $false $null $true)
$hogByName = @{}
foreach ($c in $hogCases) { $hogByName[$c.Name] = $c }
$why = $null
if ($okLine -cnotmatch $hrx) { $why = 'positive line rejected' }
foreach ($b in $badLines) { if (-not $why -and $b -cmatch $hrx) { $why = "bad line accepted: $b" } }
if (-not $why) {
    foreach ($name in 'foo1c_ctf', 'tat3c_con', 'bar1c_con', 'dedicated') {
        $c = $hogByName[$name]
        if (-not $c.Args.Contains('-hogaddrspace')) { $why = "$name lacks hog" }
        elseif (@($c.Expect.LogRx) -notcontains $hrx) { $why = "$name lacks ADDRSPACE rx" }
    }
}
if (-not $why) {
    foreach ($c in $hogCases) {
        if ($c.Name -like 'crash-*' -or $c.Name -eq 'fatal-init') {
            if ($c.Args.Contains('-hogaddrspace') -or @($c.Expect.LogRx) -contains $hrx) { $why = "$($c.Name) must not be hogged" }
        }
    }
}
if (-not $why) {
    $plain = @(Get-SmokeCases 'All' @('bar1c_con') $false $null)
    foreach ($c in $plain) { if ($c.Args.Contains('-hogaddrspace') -or @($c.Expect.LogRx) -contains $hrx) { $why = "$($c.Name) hogged by default" } }
}
if ($why) { $failed++; Write-Host "FAILED ${n}: hog smoke cases: $why" }
else { Write-Host "OK $n hog smoke cases" }
$n++
$psrc = Join-Path $env:TEMP ("openswbf2-parallel-" + [guid]::NewGuid().ToString('N'))
$why = $null
$pbatch = $null
$pdirs = @()
try {
    New-Item -ItemType Directory -Path (Join-Path $psrc 'data') | Out-Null
    Copy-Item -LiteralPath $env:ComSpec -Destination (Join-Path $psrc 'BattlefrontII.exe')
    Set-Content -Path (Join-Path $psrc 'binkw32.dll') -Value 'x'
    Set-Content -Path (Join-Path $psrc 'data\marker.txt') -Value 'x'
    foreach ($w in 1, 2) {
        $d = Join-Path $psrc "workers\w$w"
        New-SmokeWorkerDir $psrc $d
        $pdirs += $d
        $link = Get-Item -LiteralPath (Join-Path $d 'data') -Force
        if (-not (Test-Path -LiteralPath (Join-Path $d 'BattlefrontII.exe')) -or -not (Test-Path -LiteralPath (Join-Path $d 'binkw32.dll'))) { $why = "worker $w lacks exe or dll" }
        elseif (-not ($link.Attributes -band [IO.FileAttributes]::ReparsePoint) -or -not (Test-Path -LiteralPath (Join-Path $d 'data\marker.txt'))) { $why = "worker $w data is not a junction to the source data" }
    }
    if (-not $why) {
        $items = @(
            (New-Object PSObject -Property @{ Name = 'slow'; ExeName = 'BattlefrontII.exe'; ArgLine = '/c ping -n 4 127.0.0.1 >nul'; Timeout = 60; CaseDir = (Join-Path $psrc 'case-slow') }),
            (New-Object PSObject -Property @{ Name = 'fast'; ExeName = 'BattlefrontII.exe'; ArgLine = '/c exit 0'; Timeout = 60; CaseDir = (Join-Path $psrc 'case-fast') })
        )
        $pbatch = Start-SmokeBatch $items $pdirs
        $slow = Receive-SmokeCase $pbatch 0
        $fast = Receive-SmokeCase $pbatch 1
        if ($slow.Name -ne 'slow' -or $fast.Name -ne 'fast') { $why = 'results not in case order' }
        elseif ($slow.WorkDir -eq $fast.WorkDir) { $why = 'concurrent cases shared a worker folder' }
        elseif ($fast.Ms -ge $slow.Ms) { $why = "fast case ($($fast.Ms) ms) did not finish before slow ($($slow.Ms) ms)" }
    }
}
catch { $why = $_.Exception.Message }
finally {
    if ($pbatch) { Stop-SmokeBatch $pbatch }
    foreach ($d in $pdirs) { Remove-SmokeWorkerDir $d }
    Remove-Item -Recurse -Force $psrc -ErrorAction SilentlyContinue
}
if ($why) { $failed++; Write-Host "FAILED ${n}: parallel smoke: $why" }
else { Write-Host "OK $n parallel smoke" }
function New-LaaRec($l, $lr, $wr, $frames = 9274, $dur = 309) {
    return New-Object PSObject -Property @{ Key = "geo1c_con`t ${dur}s`tL=Rep`t28u@$l`P`t ${lr}r`t 0p`tW=CIS`t28u@10 `t ${wr}r`t 0p"; Frames = $frames }
}
$laaBase = 1..5 | ForEach-Object { New-LaaRec 15 36 97 }
$laaCases = @(
    @('laa compare identical passes', { (Compare-LaaRuns $laaBase $laaBase).Ok }, $true),
    @('laa compare frame count differs fails', { $r = Compare-LaaRuns $laaBase (@($laaBase[0..3]) + (New-LaaRec 15 36 97 9275)); (-not $r.Ok) -and (($r.Failures -join ';') -match 'runs differ') }, $true),
    @('laa compare reinforcement differs fails', { (Compare-LaaRuns $laaBase (@($laaBase[0..3]) + (New-LaaRec 15 35 97))).Ok }, $false),
    @('laa compare four runs fails', { (Compare-LaaRuns $laaBase @($laaBase[0..3])).Ok }, $false)
)
foreach ($lc in $laaCases) {
    $n++
    $got = [bool](& $lc[1])
    if ($got -ne $lc[2]) { $failed++; Write-Host "FAILED ${n}: $($lc[0])" }
    else { Write-Host "OK $n $($lc[0])" }
}
$digTmp = Join-Path $env:TEMP ("openswbf2-digest-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $digTmp | Out-Null
$savedExpected = $env:OPENSWBF2_DIGEST_EXPECTED
try {
    $n++
    $digLine = 'OPENSWBF2 SIM DIGEST turns=1500 time=50.000 rand=1/2 snd=49.923 state=0x0000ABCD'
    [IO.File]::WriteAllText((Join-Path $digTmp 'run.log'), "$digLine`r`n")
    $goodExp = Join-Path $digTmp 'good.txt'
    $flipExp = Join-Path $digTmp 'flip.txt'
    [IO.File]::WriteAllText($goodExp, "# header`r`nselftest-case`t$digLine`r`n")
    [IO.File]::WriteAllText($flipExp, "selftest-case`t$($digLine.Replace('state=0x0000ABCD', 'state=0x0000ABCC'))`r`n")
    $digCase = New-Object PSObject -Property @{ Name = 'selftest-case'; Expect = @{ Class = 'Pass' } }
    $digRes = New-Object PSObject -Property @{ Class = 'Pass'; CaseDir = $digTmp; LogText = $digLine; Dumps = @() }
    $env:OPENSWBF2_DIGEST_EXPECTED = $goodExp
    $okGood = Test-SmokeExpect $digCase $digRes
    $env:OPENSWBF2_DIGEST_EXPECTED = $flipExp
    $okFlip = Test-SmokeExpect $digCase $digRes
    if (-not $okGood -or $okFlip) { $failed++; Write-Host "FAILED ${n}: digest compare: matching=$okGood flipped=$okFlip" }
    else { Write-Host "OK $n digest compare fails on a one-bit state flip" }
    $n++
    $trA = Join-Path $digTmp 'trace-a.log'
    $trB = Join-Path $digTmp 'trace-b.log'
    $linesA = 1..9 | ForEach-Object { 'OPENSWBF2 DIGEST TRACE turn={0} state=0x{0:X8} rand=1/2' -f $_ }
    $linesB = 1..9 | ForEach-Object { if ($_ -ge 7) { 'OPENSWBF2 DIGEST TRACE turn={0} state=0x{1:X8} rand=1/2' -f $_, ($_ + 100) } else { 'OPENSWBF2 DIGEST TRACE turn={0} state=0x{0:X8} rand=1/2' -f $_ } }
    [IO.File]::WriteAllLines($trA, [string[]]$linesA)
    [IO.File]::WriteAllLines($trB, [string[]]$linesB)
    $trC = Join-Path $digTmp 'trace-c.log'
    $linesC = 1..9 | ForEach-Object { 'OPENSWBF2 DIGEST TRACE turn={0} state=0x{0:X8} rand=3/4' -f $_ }
    [IO.File]::WriteAllLines($trC, [string[]]$linesC)
    $trE = Join-Path $digTmp 'trace-e.log'
    [IO.File]::WriteAllLines($trE, [string[]]@('no trace here'))
    $split = Compare-DigestTrace $trA $trB
    $same = Compare-DigestTrace $trA $trA
    $randOnly = Compare-DigestTrace $trA $trC
    $empty = Compare-DigestTrace $trA $trE
    if ($split -ne 7 -or $same -ne 0 -or $randOnly -ne 0 -or $empty -ne -1) { $failed++; Write-Host "FAILED ${n}: digest trace split: split=$split same=$same randOnly=$randOnly empty=$empty" }
    else { Write-Host "OK $n digest trace split names turn 7, ignores rand, fails an empty trace" }
}
finally {
    $env:OPENSWBF2_DIGEST_EXPECTED = $savedExpected
    Remove-Item -Recurse -Force $digTmp -ErrorAction SilentlyContinue
}
$n++
$allLines = @(Get-Content (Join-Path $PSScriptRoot 'smoke\missions-all.txt') | Where-Object { $_.Trim() })
$parAll = @{}
foreach ($c in @(Get-SmokeCases 'All' $allLines $false $null)) { $parAll[$c.Name] = $c }
$parTw = @{}
foreach ($c in @(Get-SmokeCases 'Twice' @() $false $null)) { $parTw[$c.Name] = $c }
$parStd = @{}
foreach ($c in @(Get-SmokeCases 'Standard' @() $false $null)) { $parStd[$c.Name] = $c }
$why = $null
foreach ($p in @(@('nab2g_c', $parTw, 'twice-digest-1'), @('hot1g_con', $parTw, 'digest-hot1g_con'), @('spa3c_ass', $parTw, 'digest-spa3c_ass'), @('geo1c_con', $parStd, 'geo1c_con'))) {
    if (-not $parAll.ContainsKey($p[0])) { $why = "All case $($p[0]) missing"; break }
    if (-not $p[1].ContainsKey($p[2])) { $why = "case $($p[2]) missing"; break }
    if ($parAll[$p[0]].Args -cne $p[1][$p[2]].Args) { $why = "All $($p[0]) args differ from $($p[2])"; break }
}
if (-not $why -and $parAll['nab2g_c'].Timeout -ne 0) { $why = 'All nab2g_c must use the default timeout' }
if ($why) { $failed++; Write-Host "FAILED ${n}: all/twice digest parity: $why" }
else { Write-Host "OK $n all/twice digest parity" }
$testExe = Join-Path (Split-Path $PSScriptRoot -Parent) 'bin\Tests\Debug\OpenSWBF2Tests.exe'
foreach ($probe in 'strictness probe emits an undeclared error', 'strictness probe unmatched expectation') {
    $n++
    if (-not (Test-Path $testExe)) { $failed++; Write-Host "FAILED ${n}: ${probe}: test exe not built"; continue }
    $out = (& $testExe "-tc=$probe" -ns 2>&1 | Out-String)
    $code = $LASTEXITCODE
    if ($code -ne 1 -or $out -notmatch '\[UNEXPECTED-LOG\]') { $failed++; Write-Host "FAILED ${n}: ${probe}: exit $code, expected 1 with [UNEXPECTED-LOG]" }
    else { Write-Host "OK $n $probe" }
}
if ($failed -gt 0) { exit 1 }
exit 0
