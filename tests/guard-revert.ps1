param(
    [string[]]$Row,
    [switch]$Keep,
    [string]$OutDir,
    [string]$Scratch,
    [string]$RowsFile,
    [int]$CaseTimeout = 30,
    [ValidateSet('Win32','x64')][string]$Platform = 'Win32'
)
$ErrorActionPreference = 'Stop'
if ($Row) { $Row = @($Row | ForEach-Object { $_ -split ',' }) }
$testsDir = $PSScriptRoot
$root = Split-Path $testsDir -Parent
if (-not $RowsFile) { $RowsFile = Join-Path $testsDir 'guardrevert\rows.ps1' }
if (-not $Scratch) { $Scratch = Join-Path $root 'artifacts\guard-revert\wt' }
$Scratch = [IO.Path]::GetFullPath($Scratch)
if ($env:TEMP -and $Scratch.StartsWith([IO.Path]::GetFullPath($env:TEMP), [StringComparison]::OrdinalIgnoreCase)) { Write-Host 'Scratch must not be under TEMP: MSBuild incremental builds miss header changes there.'; exit 2 }
. (Join-Path $testsDir 'smoke.ps1')
Set-SmokePlatform $Platform
$Platform = $script:SmokePlatform
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutDir) { $OutDir = Join-Path $root "artifacts\guard-revert\$stamp" }

function Find-MSBuild {
    $msbuild = Join-Path ${env:ProgramFiles(x86)} 'MSBuild\12.0\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) {
        $key = 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\MSBuild\ToolsVersions\12.0'
        if (Test-Path $key) { $msbuild = Join-Path (Get-ItemProperty $key).MSBuildToolsPath 'MSBuild.exe' }
    }
    if (-not (Test-Path $msbuild)) { Write-Host 'MSBuild 12.0 not found (install VS2013 Update 5).'; exit 2 }
    return $msbuild
}

function Invoke-Git([string]$dir, [string[]]$gitArgs) {
    $ErrorActionPreference = 'Continue'
    $out = & git -C $dir @gitArgs 2>&1
    if ($LASTEXITCODE -ne 0) { throw "git $($gitArgs -join ' ') failed: $out" }
    return $out
}

function Build-Tests([string]$wt, [string]$logPath) {
    $ErrorActionPreference = 'Continue'
    $out = & $script:msbuild (Join-Path $wt 'tests\OpenSWBF2Tests.vcxproj') '/p:Configuration=Debug' "/p:Platform=$Platform" /m /v:minimal /nologo 2>&1
    $code = $LASTEXITCODE
    $out | Set-Content -Path $logPath -Encoding ASCII
    return ($code -eq 0)
}

function Build-Game([string]$wt, [string]$logPath) {
    $ErrorActionPreference = 'Continue'
    $env:SWBF2_NOSTAGE = 'true'
    $out = & $script:msbuild (Join-Path $wt 'BattlefrontII.vcxproj') '/p:Configuration=Release' "/p:Platform=$Platform" '/p:SWBF2_NOSTAGE=true' /m /v:minimal /nologo 2>&1
    $code = $LASTEXITCODE
    $out | Set-Content -Path $logPath -Encoding ASCII
    return ($code -eq 0)
}

function Run-Smoke([string]$wt, [string]$tag, $case) {
    $runDir = Join-Path $OutDir "run-$tag"
    $caseDir = Join-Path $runDir 'case'
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $srcBin = Get-GameExeDir $root 'Release'
    $dllNames = @('binkw32.dll', 'Galaxy.dll', 'GalaxyPeer.dll', 'steam_api.dll', 'unicows.dll')
    if ($Platform -eq 'x64') { $dllNames = @(Get-ChildItem -LiteralPath $srcBin -Filter '*.dll' -File -ErrorAction SilentlyContinue | ForEach-Object { $_.Name }) }
    foreach ($f in $dllNames) {
        if (Test-Path (Join-Path $srcBin $f)) { Copy-Item (Join-Path $srcBin $f) $runDir -Force }
    }
    Copy-Item (Join-Path (Get-GameExeDir $wt 'Release') 'BattlefrontII.exe') $runDir -Force
    $dataLink = Join-Path $runDir 'data'
    & cmd /c mklink /J "$dataLink" "$(Join-Path $srcBin 'data')" | Out-Null
    try {
        $argLine = $case.Args.Replace('{CASEDIR}', $caseDir)
        $res = Invoke-SmokeCase $case.Name (Join-Path $runDir 'BattlefrontII.exe') $runDir $argLine 120 $caseDir
    }
    finally {
        & cmd /c rmdir "$dataLink" | Out-Null
    }
    $ok = Test-SmokeExpect $case $res
    return New-Object PSObject -Property @{ Ok = $ok; Class = $res.Class; Code = $res.Code; Dumps = @($res.Dumps).Count; LogText = $res.LogText }
}

function Run-Case([string]$exe, [string]$wt, [string]$name) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $isProbe = $name -like 'strictness probe*'
    $psi.Arguments = '"-tc=' + ($name -replace '"', '\"') + '" --timeout 10' + $(if ($isProbe) { ' -ns' } else { '' })
    $psi.WorkingDirectory = Join-Path $wt 'tests'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $o = $p.StandardOutput.ReadToEndAsync()
    $e = $p.StandardError.ReadToEndAsync()
    if (-not $p.WaitForExit($CaseTimeout * 1000)) {
        try { $p.Kill() } catch {}
        $p.WaitForExit()
        return New-Object PSObject -Property @{ State = 'RED'; Detail = 'timeout'; Code = 124; Text = $o.Result + $e.Result }
    }
    $code = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$p.ExitCode), 0)
    $text = $o.Result + $e.Result
    $state = 'RED'
    $detail = ''
    if ($isProbe) {
        if ($code -eq 1 -and $text -match '\[UNEXPECTED-LOG\]') { $state = 'GREEN' }
        else { $state = 'RED' }
    }
    elseif ($code -eq 0) {
        if ($text -match 'test cases:\s+1 \|\s+1 passed') { $state = 'GREEN' }
        else { $state = 'NO-MATCH' }
    }
    elseif ($code -eq 77) { $state = 'SKIP' }
    elseif ($code -eq 2) { $state = 'NO-MATCH' }
    elseif ($code -eq 1) { $state = 'RED' }
    else { $detail = ('crash 0x{0:X8}' -f $code) }
    return New-Object PSObject -Property @{ State = $state; Detail = $detail; Code = $code; Text = $text }
}

function Set-Anchor([string]$path, [string]$anchor, [string]$replacement) {
    $latin = [Text.Encoding]::GetEncoding(28591)
    $bytes = [IO.File]::ReadAllBytes($path)
    $text = $latin.GetString($bytes)
    if ($text.Contains("`r`n")) {
        if (-not $anchor.Contains("`r")) { $anchor = $anchor.Replace("`n", "`r`n") }
        if (-not $replacement.Contains("`r")) { $replacement = $replacement.Replace("`n", "`r`n") }
    }
    $count = 0
    $at = 0
    while (($at = $text.IndexOf($anchor, $at, [StringComparison]::Ordinal)) -ge 0) { $count++; $at += $anchor.Length }
    if ($count -ne 1) { throw "anchor matched $count times in $path" }
    $idx = $text.IndexOf($anchor, [StringComparison]::Ordinal)
    $new = $text.Substring(0, $idx) + $replacement + $text.Substring($idx + $anchor.Length)
    [IO.File]::WriteAllBytes($path, $latin.GetBytes($new))
    return $bytes
}

. $RowsFile
$rows = @($GuardRevertRows)
if ($Row) { $rows = @($rows | Where-Object { $Row -contains $_.Id }) }
if ($Platform -eq 'x64') {
    # LAA-* rows guard the 32-bit large-address-aware work (unsigned pointer order, the LAA link flag); they mean nothing on x64.
    $laaRows = @($rows | Where-Object { $_.Id -like 'LAA-*' })
    foreach ($r in $laaRows) { Write-Host "[SKIP] $($r.Id) - large-address-aware row, not meaningful on x64" }
    $rows = @($rows | Where-Object { $_.Id -notlike 'LAA-*' })
    if ($rows.Count -eq 0 -and $laaRows.Count -gt 0) { exit 0 }
}
if ($rows.Count -eq 0) { Write-Host 'No rows selected.'; exit 2 }
$smokeRows = @($rows | Where-Object { $_.Kind -eq 'Smoke' })
$hardRows = @($rows | Where-Object { $_.Kind -eq 'Hardening' })
$rows = @($rows | Where-Object { $_.Kind -ne 'Smoke' -and $_.Kind -ne 'Hardening' })
$gameData = Find-GameDataRoot
$smokeCases = @{}
foreach ($w in 'Crash', 'Standard') {
    foreach ($c in (Get-SmokeCases $w @() $false $null)) {
        if ($smokeCases.ContainsKey($c.Name)) { throw "smoke case '$($c.Name)' is defined more than once" }
        $smokeCases[$c.Name] = $c
    }
}

foreach ($dll in 'd3dx9_43.dll', 'msvcr120.dll') {
    if (-not (Test-Path (Join-Path (Get-RuntimeDllDir) $dll))) {
        Write-Host "Preflight failed: $dll missing from $(if ($Platform -eq 'x64') { 'System32' } else { 'SysWOW64' })."
        exit 2
    }
}

$msbuild = Find-MSBuild
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$wt = Join-Path $Scratch "gr-$stamp"
$before = (Invoke-Git $root @('status', '--porcelain')) -join "`n"
$table = @()
$failed = $false
$added = $false

try {
    Invoke-Git $root @('worktree', 'add', '--detach', $wt, 'HEAD') | Out-Null
    $added = $true
    $patch = Join-Path $OutDir 'overlay.patch'
    Invoke-Git $root @('diff', 'HEAD', '--binary', "--output=$patch") | Out-Null
    if ((Get-Item $patch).Length -gt 0) {
        Invoke-Git $wt @('apply', '--whitespace=nowarn', $patch) | Out-Null
    }
    foreach ($rel in (Invoke-Git $root @('ls-files', '--others', '--exclude-standard'))) {
        $rel = "$rel".Trim()
        if (-not $rel) { continue }
        $dst = Join-Path $wt $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $dst -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $root $rel) -Destination $dst -Force
    }

    if ($rows.Count -gt 0) {
        $exe = Get-TestExePath $wt 'Debug'
        $controlOk = Build-Tests $wt (Join-Path $OutDir 'build-control.log')
        if (-not $controlOk) { throw "control build failed, see $(Join-Path $OutDir 'build-control.log')" }

        $control = @{}
        foreach ($name in ($rows | ForEach-Object { $_.Tests } | Select-Object -Unique)) {
            $control[$name] = Run-Case $exe $wt $name
            Set-Content -Path (Join-Path $OutDir (('control-' + ($name -replace '[^A-Za-z0-9_.-]', '_')) + '.txt')) -Value $control[$name].Text -Encoding ASCII
        }

        foreach ($r in $rows) {
            $path = Join-Path $wt $r.File
            $orig = $null
            $result = 'BUILD-FAIL'
            $detail = ''
            $code = ''
            $ctl = 'GREEN'
            foreach ($name in $r.Tests) { if ($control[$name].State -ne 'GREEN') { $ctl = $control[$name].State } }
            try {
                $orig = Set-Anchor $path $r.Anchor $r.Replacement
                $ok = Build-Tests $wt (Join-Path $OutDir ("build-$($r.Id).log"))
                if ($ok) {
                    $states = @()
                    $codes = @()
                    $details = @()
                    foreach ($name in $r.Tests) {
                        $c = Run-Case $exe $wt $name
                        Set-Content -Path (Join-Path $OutDir (("$($r.Id)-" + ($name -replace '[^A-Za-z0-9_.-]', '_')) + '.txt')) -Value $c.Text -Encoding ASCII
                        $states += $c.State
                        $codes += $c.Code
                        if ($c.Detail) { $details += $c.Detail }
                    }
                    $code = ($codes -join ',')
                    $detail = ($details -join ',')
                    if ($states -contains 'NO-MATCH') { $result = 'NO-MATCH' }
                    elseif ($states -contains 'RED') { $result = 'RED' }
                    elseif ($states -contains 'SKIP') { $result = 'SKIP' }
                    else { $result = 'STILL-GREEN' }
                }
            }
            catch {
                $result = 'ERROR'
                $detail = $_.Exception.Message
            }
            finally {
                if ($orig) { [IO.File]::WriteAllBytes($path, $orig) }
            }
            $table += New-Object PSObject -Property @{ Id = $r.Id; Tests = ($r.Tests -join ' | '); Control = $ctl; Reverted = $result; Exit = $code; Detail = $detail }
            if ($result -ne 'RED' -or $ctl -ne 'GREEN') { $failed = $true }
            Write-Host ('{0,-12} control={1,-8} reverted={2,-11} exit={3} {4}' -f $r.Id, $ctl, $result, $code, $detail)
        }
    }

    if ($hardRows.Count -gt 0) {
        $wtExe = Join-Path (Get-GameExeDir $wt 'Release') 'BattlefrontII.exe'
        $getCheck = {
            param($name)
            $db = Find-Dumpbin
            $hdr = (& $db /headers $wtExe) -join "`n"
            $imp = (& $db /imports $wtExe) -join "`n"
            return ((@(Get-HardeningChecks $hdr '' 0 0 0) + @(Get-ImportChecks $imp)) | Where-Object { $_.Name -eq $name })
        }
        if (-not (Build-Game $wt (Join-Path $OutDir 'build-hardening-control.log'))) { throw "hardening control build failed, see $(Join-Path $OutDir 'build-hardening-control.log')" }
        $hardControl = @{}
        foreach ($name in ($hardRows | ForEach-Object { $_.Check } | Select-Object -Unique)) {
            $hardControl[$name] = [bool](& $getCheck $name).Ok
        }
        foreach ($r in $hardRows) {
            $path = Join-Path $wt $r.File
            $orig = $null
            $result = 'BUILD-FAIL'
            $detail = ''
            $ctl = 'GREEN'
            if (-not $hardControl[$r.Check]) { $ctl = 'FAIL' }
            try {
                $orig = Set-Anchor $path $r.Anchor $r.Replacement
                if (Build-Game $wt (Join-Path $OutDir "build-$($r.Id).log")) {
                    $chk = & $getCheck $r.Check
                    $detail = "check=$($r.Check) ok=$($chk.Ok)"
                    if ($chk.Ok) { $result = 'STILL-GREEN' } else { $result = 'RED' }
                }
            }
            catch {
                $result = 'ERROR'
                $detail = $_.Exception.Message
            }
            finally {
                if ($orig) { [IO.File]::WriteAllBytes($path, $orig) }
            }
            $table += New-Object PSObject -Property @{ Id = $r.Id; Tests = "hardening:$($r.Check)"; Control = $ctl; Reverted = $result; Exit = ''; Detail = $detail }
            if ($result -ne 'RED' -or $ctl -ne 'GREEN') { $failed = $true }
            Write-Host ('{0,-12} control={1,-8} reverted={2,-11} {3}' -f $r.Id, $ctl, $result, $detail)
        }
    }

    if ($smokeRows.Count -gt 0) {
        $pre = Get-SmokePrecheck $gameData (Get-GameExeDir $root 'Release')
        if ($pre) { throw "smoke rows need a prior game build and data: $pre" }
        if (-not (Build-Game $wt (Join-Path $OutDir 'build-smoke-control.log'))) { throw "smoke control build failed, see $(Join-Path $OutDir 'build-smoke-control.log')" }
        $smokeControl = @{}
        foreach ($name in ($smokeRows | ForEach-Object { $_.Case } | Select-Object -Unique)) {
            $smokeControl[$name] = Run-Smoke $wt "control-$name" $smokeCases[$name]
        }
        foreach ($r in $smokeRows) {
            $path = Join-Path $wt $r.File
            $orig = $null
            $result = 'BUILD-FAIL'
            $detail = ''
            $code = ''
            $ctlRes = $smokeControl[$r.Case]
            $ctl = 'GREEN'
            if (-not $ctlRes.Ok) { $ctl = 'FAIL-' + $ctlRes.Class }
            try {
                $orig = Set-Anchor $path $r.Anchor $r.Replacement
                if (Build-Game $wt (Join-Path $OutDir "build-$($r.Id).log")) {
                    $s = Run-Smoke $wt $r.Id $smokeCases[$r.Case]
                    $code = ('0x{0:X8}' -f $s.Code)
                    $detail = "class=$($s.Class) dumps=$($s.Dumps)"
                    if ($s.Ok) { $result = 'STILL-GREEN' }
                    elseif ($r.RedRx -and $s.LogText -cnotmatch $r.RedRx) { $result = 'WRONG-RED' }
                    else { $result = 'RED' }
                }
            }
            catch {
                $result = 'ERROR'
                $detail = $_.Exception.Message
            }
            finally {
                if ($orig) { [IO.File]::WriteAllBytes($path, $orig) }
            }
            $table += New-Object PSObject -Property @{ Id = $r.Id; Tests = "smoke:$($r.Case)"; Control = $ctl; Reverted = $result; Exit = $code; Detail = $detail }
            if ($result -ne 'RED' -or $ctl -ne 'GREEN') { $failed = $true }
            Write-Host ('{0,-12} control={1,-8} reverted={2,-11} exit={3} {4}' -f $r.Id, $ctl, $result, $code, $detail)
        }
    }
}
finally {
    $ErrorActionPreference = 'Continue'
    if ($added -and -not $Keep) {
        & git -C $root worktree remove --force $wt 2>&1 | Out-Null
        if (Test-Path $wt) { Remove-Item -Recurse -Force $wt -ErrorAction SilentlyContinue }
    }
    & git -C $root worktree prune 2>&1 | Out-Null
}

$lines = @()
$lines += ('{0,-12} {1,-8} {2,-11} {3,-10} {4}' -f 'Id', 'Control', 'Reverted', 'Exit', 'Tests')
foreach ($t in $table) {
    $lines += ('{0,-12} {1,-8} {2,-11} {3,-10} {4} {5}' -f $t.Id, $t.Control, $t.Reverted, $t.Exit, $t.Tests, $t.Detail)
}
Write-Host ''
$lines | ForEach-Object { Write-Host $_ }
$lines | Set-Content -Path (Join-Path $OutDir 'summary.txt') -Encoding ASCII
Write-Host "Artifacts: $OutDir"

$after = (Invoke-Git $root @('status', '--porcelain')) -join "`n"
if ($before -ne $after) { Write-Host 'WARNING: main tree status changed during the run.'; exit 1 }
if ($failed) { exit 1 }
exit 0
