param(
    [int]$Runs = 5,
    [string]$Configuration = 'Release',
    [switch]$NoBuild,
    [string]$OutDir,
    [ValidateRange(1,64)][int]$Jobs = 8,
    [int]$Timeout = 120,
    [ValidateSet('Win32','x64')][string]$Platform = 'Win32'
)
$ErrorActionPreference = 'Stop'
$testsDir = $PSScriptRoot
$root = Split-Path $testsDir -Parent
. (Join-Path $testsDir 'smoke.ps1')

# A 64-bit image is always large address aware and its arena sits above 2 GB, so there is nothing to compare.
if ($Platform -eq 'x64') { Write-Host '[SKIP] laa-compare - large-address-aware comparison does not apply to x64'; exit 0 }

if ($Runs -lt 5) { Write-Host "INFO: -Runs raised from $Runs to 5 (minimum)"; $Runs = 5 }

if (-not $NoBuild) {
    $msbuild = Join-Path ${env:ProgramFiles(x86)} 'MSBuild\12.0\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) { Write-Host 'MSBuild 12.0 not found (install VS2013 Update 5).'; exit 2 }
    & $msbuild (Join-Path $root 'BattlefrontII.vcxproj') "/p:Configuration=$Configuration" '/p:Platform=Win32' /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { Write-Host 'Build failed.'; exit 2 }
}

$exeDir = Join-Path $root "bin\Win32\$Configuration"
$gameExe = Join-Path $exeDir 'BattlefrontII.exe'
$reason = Get-SmokePrecheck (Find-GameDataRoot) $exeDir
if ($reason) { Write-Host "[SKIP] laa-compare - $reason"; exit 0 }

if (-not $OutDir) { $OutDir = Join-Path $root ("artifacts\laa-compare\" + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$dumpbin = Find-Dumpbin
$editbin = Find-Editbin
$laaText = 'Application can handle large \(>2GB\) addresses'

function Test-ExeLaa($exe) {
    $hdr = (& $dumpbin /headers $exe | Out-String)
    return ($hdr -match $laaText)
}

if (-not (Test-ExeLaa $gameExe)) { Write-Host "[FAIL] laa-compare - built exe lacks the large address aware flag: $gameExe"; exit 1 }

$nolaaName = 'BattlefrontII-nolaa.exe'
$nolaaExe = Join-Path $OutDir $nolaaName
$workDirs = @()
$batch = $null
$failed = $false
try {
    Copy-Item -LiteralPath $gameExe -Destination $nolaaExe -Force
    & $editbin /LARGEADDRESSAWARE:NO $nolaaExe | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "editbin exit $LASTEXITCODE" }
    if (Test-ExeLaa $nolaaExe) { throw 'patched copy still has the large address aware flag' }

    $baseCase = (Get-SmokeCases 'Standard' @() $false $null $false)[0]
    $laaCase = (Get-SmokeCases 'Standard' @() $false $null $true)[0]

    $items = @()
    for ($i = 1; $i -le $Runs; $i++) {
        $items += New-Object PSObject -Property @{ Name = "base-$i"; ExeName = $nolaaName; ArgLine = $baseCase.Args.Replace('{CASEDIR}', (Join-Path $OutDir "base-$i")); Timeout = $Timeout; CaseDir = (Join-Path $OutDir "base-$i") }
        $items += New-Object PSObject -Property @{ Name = "laa-$i"; ExeName = 'BattlefrontII.exe'; ArgLine = $laaCase.Args.Replace('{CASEDIR}', (Join-Path $OutDir "laa-$i")); Timeout = $Timeout; CaseDir = (Join-Path $OutDir "laa-$i") }
    }
    $workerCount = [Math]::Min($Jobs, $items.Count)
    Write-Host "INFO: laa-compare runs=$Runs per side jobs=$workerCount out=$OutDir"
    for ($w = 1; $w -le $workerCount; $w++) {
        $d = Join-Path $OutDir "workers\w$w"
        $workDirs += $d
        New-SmokeWorkerDir $exeDir $d
        Copy-Item -LiteralPath $nolaaExe -Destination $d -Force
    }
    $batch = Start-SmokeBatch $items $workDirs

    $base = @(); $laa = @()
    for ($i = 0; $i -lt $items.Count; $i++) {
        $res = Receive-SmokeCase $batch $i
        $name = $items[$i].Name
        $isLaa = $name.StartsWith('laa-')
        $key = Get-FinalScoreKey $res.CaseDir
        $frames = 0
        if ($res.LogText -match 'OPENSWBF2 RUN COMPLETE') {
            $fm = [regex]::Match($res.LogText, '\[f=(\d+)\][^\r\n]*OPENSWBF2 RUN COMPLETE')
            if ($fm.Success) { $frames = [int]$fm.Groups[1].Value }
        }
        $arena = $null
        $am = [regex]::Match($res.LogText, 'OPENSWBF2 ADDRSPACE arena=0x([0-9A-F]{8})')
        if ($am.Success) { $arena = [Convert]::ToInt64($am.Groups[1].Value, 16) }
        $problem = $null
        if ($res.Class -ne 'Pass') { $problem = "class=$($res.Class)" }
        elseif (-not $key) { $problem = 'no finalscore.log' }
        elseif ($frames -eq 0) { $problem = 'no frame count' }
        elseif ($arena -eq $null) { $problem = 'no ADDRSPACE line' }
        elseif ($isLaa -and $arena -lt 2147483648) { $problem = ('arena 0x{0:X8} below 2 GB' -f $arena) }
        elseif (-not $isLaa -and $arena -ge 2147483648) { $problem = ('arena 0x{0:X8} at or above 2 GB' -f $arena) }
        $arenaText = if ($arena -ne $null) { '0x{0:X8}' -f $arena } else { 'none' }
        Write-Host ("{0,-8} arena={1} frames={2} key={3}{4}" -f $name, $arenaText, $frames, $key, $(if ($problem) { " PROBLEM: $problem" } else { '' }))
        if ($problem) { $failed = $true }
        $rec = New-Object PSObject -Property @{ Key = $key; Frames = $frames }
        if ($isLaa) { $laa += $rec } else { $base += $rec }
    }

    if (-not $failed) {
        $cmp = Compare-LaaRuns $base $laa 5
        if ($cmp.Keys.Count -eq 1) { Write-Host "  all $($base.Count + $laa.Count) runs identical: $($cmp.Keys[0])" }
        foreach ($m in $cmp.Failures) { Write-Host "  $m" }
        if (-not $cmp.Ok) { $failed = $true }
    }
}
catch {
    Write-Host "[FAIL] laa-compare - $($_.Exception.Message)"
    $failed = $true
}
finally {
    if ($batch) { Stop-SmokeBatch $batch }
    foreach ($d in $workDirs) { Remove-SmokeWorkerDir $d }
    Remove-Item -LiteralPath $nolaaExe -Force -ErrorAction SilentlyContinue
}

if (-not (Test-ExeLaa $gameExe)) { Write-Host "[FAIL] laa-compare - built exe lost the large address aware flag"; $failed = $true }

if ($failed) { Write-Host '[FAIL] laa-compare'; exit 1 }
Write-Host "[PASS] laa-compare ($Runs runs per side)"
exit 0
