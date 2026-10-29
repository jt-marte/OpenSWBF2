param(
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$NoBuild,
    [string]$OutDir,
    [string]$ArchiveDir,
    [switch]$Map,
    [switch]$Layouts,
    [switch]$Collect,
    [switch]$Tables,
    [switch]$Archive,
    [switch]$Verify,
    [switch]$All,
    [switch]$DebugDigests,
    [string]$SmokeAllDir,
    [string]$HardeningDir,
    [string]$GuardRevertDir,
    [string]$DebugSmokeDir,
    [int]$CaseTimeout = 60
)
$ErrorActionPreference = 'Stop'
$archiveDirGiven = $PSBoundParameters.ContainsKey('ArchiveDir')
$testsDir = Split-Path $PSScriptRoot -Parent
$root = Split-Path $testsDir -Parent
. (Join-Path $testsDir 'smoke.ps1')

if ($All) { $Map = $true; $Layouts = $true; $Collect = $true; $Archive = $true; $Verify = $true }
if (-not ($Map -or $Layouts -or $Collect -or $Tables -or $Archive -or $Verify -or $DebugDigests)) {
    Write-Host 'No step selected: pass -Map, -Layouts, -Collect, -Tables, -DebugDigests, -Archive, -Verify or -All.'
    exit 2
}

function Resolve-FullPath($p) { return $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($p) }

if (-not $OutDir) { $OutDir = Join-Path $root 'tests\baseline\vs2013' }
if (-not $ArchiveDir) {
    if ($env:OPENSWBF2_BASELINE_DIR) { $ArchiveDir = $env:OPENSWBF2_BASELINE_DIR }
    else { $ArchiveDir = Join-Path $env:USERPROFILE 'OpenSWBF2-baseline\vs2013' }
}
$OutDir = Resolve-FullPath $OutDir
$ArchiveDir = (Resolve-FullPath $ArchiveDir).TrimEnd('\')

$objDir = Join-Path $root "obj\Win32\$Configuration"
$exeDir = Join-Path $root "bin\Win32\$Configuration"
$mapFile = Join-Path $objDir 'BattlefrontII.map'
$tlogFile = Join-Path $objDir 'BattlefrontII.tlog\cl.command.1.tlog'
$script:failed = $false
$script:skipped = $false
$manifestName = 'MANIFEST.md'

if (-not ('OpenSwbf2FinalPath' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
public static class OpenSwbf2FinalPath
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr sa, uint disposition, uint flags, IntPtr template);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern uint GetFinalPathNameByHandleW(SafeFileHandle handle, StringBuilder buffer, uint length, uint flags);
    public static string Get(string path)
    {
        using (SafeFileHandle h = CreateFileW(path, 0x80, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero))
        {
            if (h.IsInvalid) { return null; }
            StringBuilder sb = new StringBuilder(32768);
            uint n = GetFinalPathNameByHandleW(h, sb, (uint)sb.Capacity, 0);
            if (n == 0 || n >= sb.Capacity) { return null; }
            string s = sb.ToString();
            if (s.StartsWith(@"\\?\UNC\")) { return @"\\" + s.Substring(8); }
            if (s.StartsWith(@"\\?\")) { return s.Substring(4); }
            return s;
        }
    }
}
'@
}

function Resolve-CanonicalPath($p) {
    $full = Resolve-FullPath $p
    $d = New-Object IO.DirectoryInfo $full
    $tail = New-Object System.Collections.Generic.List[string]
    while ($d -and -not $d.Exists) { $tail.Insert(0, $d.Name); $d = $d.Parent }
    if (-not $d) { return $full.TrimEnd('\') }
    $real = [OpenSwbf2FinalPath]::Get($d.FullName)
    if (-not $real) { throw "cannot resolve the final path of $($d.FullName)" }
    foreach ($t in $tail) { $real = Join-Path $real $t }
    return $real.TrimEnd('\')
}

function Test-InsideRepo($p) {
    $full = Resolve-CanonicalPath $p
    $repo = Resolve-CanonicalPath $root
    return ($full.Equals($repo, [StringComparison]::OrdinalIgnoreCase) -or $full.StartsWith($repo + '\', [StringComparison]::OrdinalIgnoreCase))
}

function Get-SourceStatus {
    $argLine = 'status --porcelain'
    $out = (Resolve-FullPath $OutDir).TrimEnd('\')
    if ($out.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
        $argLine += ' -- . ":(exclude)' + $out.Substring($root.Length + 1).Replace('\', '/') + '"'
    }
    return Get-GitText $argLine
}

function ConvertTo-Scrubbed([string]$text) {
    $subs = New-Object System.Collections.ArrayList
    [void]$subs.Add(@($root, '.'))
    if ($env:USERPROFILE) { [void]$subs.Add(@($env:USERPROFILE.TrimEnd('\'), '%USERPROFILE%')) }
    foreach ($s in @($subs | Sort-Object { $_[0].Length } -Descending)) {
        foreach ($form in @($s[0], $s[0].Replace('\', '/'))) {
            $text = [regex]::Replace($text, [regex]::Escape($form), $s[1].Replace('$', '$$'), [Text.RegularExpressions.RegexOptions]::IgnoreCase)
        }
    }
    return $text
}

function Write-BaselineText($path, $lines) {
    $arr = @($lines)
    $text = ''
    if ($arr.Count -gt 0) { $text = ($arr -join "`r`n") + "`r`n" }
    $text = ConvertTo-Scrubbed $text
    if ($env:USERNAME -and $text.IndexOf($env:USERNAME, [StringComparison]::OrdinalIgnoreCase) -ge 0) { throw "user name left in $(Split-Path $path -Leaf) after scrubbing" }
    New-Item -ItemType Directory -Force -Path (Split-Path $path -Parent) | Out-Null
    [IO.File]::WriteAllText($path, $text, [Text.Encoding]::ASCII)
}

function Invoke-Tool($exe, $argLine, $cwd) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argLine
    $psi.WorkingDirectory = $cwd
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $o = $p.StandardOutput.ReadToEndAsync()
    $e = $p.StandardError.ReadToEndAsync()
    $p.WaitForExit()
    return New-Object PSObject -Property @{ Out = $o.Result; Err = $e.Result; Code = $p.ExitCode }
}

function Get-GitText($argList) {
    $r = Invoke-Tool 'git' $argList $root
    if ($r.Code -ne 0) { throw "git $argList failed: $($r.Err.Trim())" }
    return $r.Out
}

function Invoke-Step($name, [scriptblock]$body) {
    $script:skipped = $false
    try {
        & $body
        if (-not $script:skipped) { Write-Host "[PASS] capture:$name" }
    }
    catch {
        Write-Host "[FAIL] capture:$name - $($_.Exception.Message)"
        $script:failed = $true
    }
}

function Get-VcBin { return (Split-Path (Find-Dumpbin) -Parent) }

function Get-DemangledNames($names) {
    $undname = Join-Path (Get-VcBin) 'undname.exe'
    if (-not (Test-Path -LiteralPath $undname)) { Write-Host 'undname.exe not found (install VS2013 Update 5).'; exit 2 }
    $set = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    $list = New-Object System.Collections.Generic.List[string]
    foreach ($n in $names) { if ($set.Add($n)) { $list.Add($n) } }
    $map = New-Object 'System.Collections.Generic.Dictionary[string,string]' ([StringComparer]::Ordinal)
    for ($s = 0; $s -lt $list.Count; $s += 100) {
        $chunk = $list.GetRange($s, [Math]::Min(100, $list.Count - $s))
        $argLine = (@($chunk) | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $r = Invoke-Tool $undname $argLine $root
        foreach ($m in [regex]::Matches($r.Out, 'Undecoration of :- "(.*)"\r?\nis :- "(.*)"')) { $map[$m.Groups[1].Value] = $m.Groups[2].Value }
    }
    return $map
}

function Format-MapRows($rows, $dem) {
    $out = New-Object System.Collections.Generic.List[string]
    $i = 0
    foreach ($r in $rows) {
        $d = $r.Symbol
        if ($dem.ContainsKey($r.Symbol)) { $d = $dem[$r.Symbol] }
        $out.Add(("{0}`t{1}`t{2}`t{3}`t{4}" -f $i, $r.OffText, $r.Symbol, $d, $r.Obj))
        $i++
    }
    return $out
}

function Step-Map {
    if (-not (Test-Path -LiteralPath $mapFile)) { throw "map not found: obj\Win32\$Configuration\BattlefrontII.map" }
    $lines = [IO.File]::ReadAllLines($mapFile)
    $sec = @{}
    $start = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match 'Publics by Value') { $start = $i + 1; break }
        $m = [regex]::Match($lines[$i], '^\s*0002:([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})H\s+\.CRT\$(XCA|XCU|XCZ)\s')
        if ($m.Success) { $sec[$m.Groups[3].Value] = @([Convert]::ToInt64($m.Groups[1].Value, 16), [Convert]::ToInt64($m.Groups[2].Value, 16)) }
    }
    if ($start -lt 0) { throw 'no Publics by Value table in the map' }
    foreach ($n in 'XCA', 'XCU', 'XCZ') { if (-not $sec.ContainsKey($n)) { throw ".CRT`$$n missing from the map section table" } }
    $lo = $sec['XCA'][0]
    $hi = $sec['XCZ'][0] + $sec['XCZ'][1]
    $rx = New-Object regex '^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+[0-9a-fA-F]{8}\s+(?:f\s+)?(?:i\s+)?(\S.*?)\s*$'
    $inRange = New-Object System.Collections.Generic.List[object]
    $discarded = New-Object System.Collections.Generic.List[object]
    $di = New-Object System.Collections.Generic.List[object]
    for ($i = $start; $i -lt $lines.Length; $i++) {
        $m = $rx.Match($lines[$i])
        if (-not $m.Success) { continue }
        $secNo = $m.Groups[1].Value
        $sym = $m.Groups[3].Value
        $isDi = $sym.StartsWith('??__E')
        $isInit = $sym.Contains('$initializer$')
        if ($secNo -ne '0002' -and -not $isDi) { continue }
        $off = [Convert]::ToInt64($m.Groups[2].Value, 16)
        $in = ($secNo -eq '0002' -and $off -ge $lo -and $off -lt $hi)
        if (-not ($in -or $isDi -or $isInit)) { continue }
        $row = New-Object PSObject -Property @{ Sec = [Convert]::ToInt32($secNo, 16); Offset = $off; OffText = "$($secNo):$($m.Groups[2].Value)"; Symbol = $sym; Obj = $m.Groups[4].Value; Line = $i }
        if ($in) { $inRange.Add($row) }
        elseif ($isInit -and $secNo -eq '0002') { $discarded.Add($row) }
        if ($isDi) { $di.Add($row) }
    }
    $inRange = @($inRange | Sort-Object Offset, Line)
    $discarded = @($discarded | Sort-Object Offset, Line)
    $di = @($di | Sort-Object Sec, Offset, Line)
    if ($inRange.Count -eq 0) { throw 'no symbols inside the .CRT$XCA..XCZ range' }
    $dem = Get-DemangledNames (@($inRange) + @($discarded) + @($di) | ForEach-Object { $_.Symbol })
    Write-BaselineText (Join-Path $OutDir 'initializers.txt') (Format-MapRows $inRange $dem)
    Write-BaselineText (Join-Path $OutDir 'initializers-discarded.txt') (Format-MapRows $discarded $dem)
    Write-BaselineText (Join-Path $OutDir 'initializers-di.txt') (Format-MapRows $di $dem)
    $xcu = [int]($sec['XCU'][1] / 4)
    Write-Host "INFO: map XCU entries=$xcu initializers=$($inRange.Count) discarded=$($discarded.Count) di=$($di.Count) demangled=$($dem.Count)"
}

$script:LayoutTus = [ordered]@{
    'Weapon' = @('src\Battlefront2\Source\WeaponCannon.cpp')
    'EntityFlyer' = @('src\Battlefront2\Source\EntityCarrier.cpp')
    'AmmoCounter' = @('src\Battlefront2\Source\NetState.cpp')
    'EntityWater' = @('src\Battlefront2\Source\EntityWater.cpp')
    'EntityGeometry' = @('src\Battlefront2\Source\EntityGeometry.cpp')
    'GameObject' = @('src\Battlefront2\Source\GameObject.cpp')
    'Ordnance' = @('src\Battlefront2\Source\Ordnance.cpp')
    'Aimer' = @('src\Battlefront2\Source\Aimer.cpp')
    'CommandFlyer' = @('src\Battlefront2\Source\CommandFlyer.cpp')
    'CommandHover' = @('src\Battlefront2\Source\CommandHover.cpp')
    'CommandWalker' = @('src\Battlefront2\Source\CommandWalker.cpp')
    'EntityCarrier' = @('src\Battlefront2\Source\EntityCarrier.cpp')
    'EntityBuildingAnimatedArmed' = @('src\Battlefront2\Source\EntityBuildingAnimatedArmed.cpp')
    'WeaponGrenade' = @('src\Battlefront2\Source\WeaponGrenade.cpp')
    'WeaponRepair' = @('src\Battlefront2\Source\WeaponRepair.cpp')
    'FLInputManager' = @('src\Battlefront2\Source\FLInputManager.cpp')
    'RedBorderElement' = @('src\RedEngineFL\Interface\RedBorderElement.cpp')
    'PAGameObjectTeam' = @('src\Battlefront2\Source\ProceduralAnimation.cpp')
    'EntityFlyerRuntimeLayout' = @('src\Battlefront2\Source\EntityCarrier.cpp')
    'EntityCarrierRuntimeLayout' = @('src\Battlefront2\Source\EntityCarrier.cpp')
    'EntityHover' = @('src\Battlefront2\Source\EntityHover.cpp')
    'EntityWalker' = @('src\Battlefront2\Source\EntityWalker.cpp')
}

function Read-ClTlog {
    if (-not (Test-Path -LiteralPath $tlogFile)) { throw "tlog not found: obj\Win32\$Configuration\BattlefrontII.tlog\cl.command.1.tlog" }
    $flags = @{}
    $keys = @()
    foreach ($l in [IO.File]::ReadAllLines($tlogFile, [Text.Encoding]::Unicode)) {
        $l = $l.TrimStart([char]0xFEFF)
        if ($l.StartsWith('^')) { $keys = @($l.Substring(1).Split('|') | ForEach-Object { $_.ToUpperInvariant() }) }
        elseif ($l -and $keys.Count) { foreach ($k in $keys) { $flags[$k] = $l }; $keys = @() }
    }
    return $flags
}

function Get-LayoutArgs($flagLine, $tuFull, $cls) {
    $f = $flagLine.Trim()
    if ($f.EndsWith($tuFull, [StringComparison]::OrdinalIgnoreCase)) { $f = $f.Substring(0, $f.Length - $tuFull.Length) }
    $f = [regex]::Replace($f, '(^|\s)/F[od]("[^"]*"|\S*)', ' ')
    $f = [regex]::Replace($f, '(^|\s)/(c|Z7|Zi)(?=\s|$)', ' ')
    $f = [regex]::Replace($f, '\s+', ' ').Trim()
    return "$f /Zs /d1reportSingleClassLayout$cls `"$tuFull`""
}

function Get-LayoutBlock($text, $cls) {
    $out = New-Object System.Collections.Generic.List[string]
    $on = $false
    foreach ($l in ($text -split "\r?\n")) {
        if (-not $on) {
            if ($l.StartsWith("class $cls`tsize(", [StringComparison]::Ordinal)) { $on = $true; $out.Add($l) }
            continue
        }
        if ($l.Trim() -eq '') { break }
        $out.Add($l)
    }
    return $out
}

function Step-Layouts {
    $before = Get-SourceStatus
    $vcBin = Get-VcBin
    $cl = Join-Path $vcBin 'cl.exe'
    if (-not $env:DXSDK_DIR) { Write-Host 'DXSDK_DIR not set (install the DirectX SDK June 2010).'; exit 2 }
    $flags = Read-ClTlog
    $pairs = New-Object System.Collections.ArrayList
    $seen = @{}
    $cpps = @(Get-ChildItem -LiteralPath (Join-Path $root 'src') -Recurse -Filter '*.cpp' -File)
    foreach ($cls in $script:LayoutTus.Keys) {
        $tus = @($script:LayoutTus[$cls] | ForEach-Object { Join-Path $root $_ })
        $rx = '^(class|struct)\s+' + [regex]::Escape($cls) + '\b[^;]*$'
        $tus += @($cpps | Select-String -Pattern $rx -CaseSensitive -List | ForEach-Object { $_.Path })
        foreach ($t in $tus) {
            $k = "$cls|$($t.ToUpperInvariant())"
            if ($seen.ContainsKey($k)) { continue }
            $seen[$k] = $true
            [void]$pairs.Add(@($cls, $t))
        }
    }
    $layoutDir = Join-Path $OutDir 'layouts'
    New-Item -ItemType Directory -Force -Path $layoutDir | Out-Null
    $index = New-Object System.Collections.Generic.List[string]
    $problems = New-Object System.Collections.Generic.List[string]
    $oldInclude = $env:INCLUDE
    $env:INCLUDE = (Join-Path (Split-Path $vcBin -Parent) 'include') + ';' + (Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs\Windows\v7.1A\Include') + ';' + $env:DXSDK_DIR.TrimEnd('\') + '\Include'
    $written = 0
    try {
        foreach ($pair in $pairs) {
            $cls = $pair[0]
            $tu = (Resolve-FullPath $pair[1])
            $rel = $tu.Substring($root.Length + 1)
            $fl = $flags[$tu.ToUpperInvariant()]
            if (-not $fl) { Write-Host "NOTE: $rel not in the tlog, skipped for $cls"; continue }
            $r = Invoke-Tool $cl (Get-LayoutArgs $fl $tu $cls) $root
            if ($r.Code -ne 0) {
                $msg = ($r.Out -split "\r?\n" | Where-Object { $_ -match 'error' } | Select-Object -First 1)
                [void]$problems.Add("$cls@$rel cl exit $($r.Code) $msg")
                continue
            }
            $block = Get-LayoutBlock $r.Out $cls
            $base = [IO.Path]::GetFileNameWithoutExtension($tu)
            $sizeLine = 'no layout reported'
            if ($block.Count -gt 0) { $sizeLine = $block[0].Replace("`t", ' ') }
            else { $block.Add('no layout reported') }
            Write-BaselineText (Join-Path $layoutDir "$cls@$base.txt") $block
            $index.Add("$cls`t$rel`t$sizeLine")
            $written++
        }
    }
    finally {
        $env:INCLUDE = $oldInclude
    }
    Write-BaselineText (Join-Path $layoutDir 'INDEX.txt') $index
    Write-Host "INFO: layouts pairs=$($pairs.Count) files=$written"
    $after = Get-SourceStatus
    if ($before -ne $after) { throw 'tree changed' }
    if ($problems.Count) { throw ($problems -join '; ') }
}

function Get-LogLines($caseDir, $pattern) {
    $f = Join-Path $caseDir 'run.log'
    if (-not (Test-Path -LiteralPath $f)) { return @() }
    $rx = New-Object regex $pattern
    $out = New-Object System.Collections.Generic.List[string]
    foreach ($l in [IO.File]::ReadAllLines($f)) {
        $m = $rx.Match($l)
        if ($m.Success) { $out.Add($m.Value.TrimEnd()) }
    }
    return $out
}

function Step-Collect {
    $reason = Get-SmokePrecheck (Find-GameDataRoot) $exeDir
    if ($reason) { Write-Host "[SKIP] capture:collect - $reason"; $script:skipped = $true; return }
    $gameExe = Join-Path $exeDir 'BattlefrontII.exe'
    $artRoot = Join-Path $root 'artifacts'
    New-Item -ItemType Directory -Force -Path $artRoot | Out-Null
    $ign = Join-Path $artRoot '.gitignore'
    if (-not (Test-Path -LiteralPath $ign)) { Set-Content -Path $ign -Value '*' -Encoding ASCII }
    $runDir = Join-Path $artRoot ('baseline\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    $std = (Get-SmokeCases 'Standard' @() $false $null $false)[0].Args
    $hog = (Get-SmokeCases 'Standard' @() $false $null $true)[0].Args
    $mission = $script:SmokeStandardMission
    $digests = New-Object System.Collections.Generic.List[string]
    $hogDigests = New-Object System.Collections.Generic.List[string]
    $problems = New-Object System.Collections.Generic.List[string]
    $hogDiff = New-Object System.Collections.Generic.List[string]
    foreach ($m in 'geo1c_con', 'nab2g_c', 'hot1g_con', 'spa3c_ass') {
        $caseDir = Join-Path $runDir $m
        $argLine = ($m + $std.Substring($mission.Length)).Replace('{CASEDIR}', $caseDir) + ' -factorylog -digesttrace 1600'
        $res = Invoke-SmokeCase $m $gameExe $exeDir $argLine $CaseTimeout $caseDir
        $digest = Get-DigestKey $caseDir
        $trace = Get-LogLines $caseDir 'OPENSWBF2 DIGEST TRACE turn=.*'
        $fact = Get-LogLines $caseDir 'OPENSWBF2 FACTORY .*'
        Write-Host "INFO: collect $m class=$($res.Class) trace=$($trace.Count) factory=$($fact.Count) $digest"
        if ($res.Class -ne 'Pass' -and $res.Class -ne 'TimeOut') { [void]$problems.Add("$m class=$($res.Class)") }
        if (-not $digest) { [void]$problems.Add("$m has no SIM DIGEST line") }
        if ($trace.Count -ne 1600) { [void]$problems.Add("$m has $($trace.Count) DIGEST TRACE lines, expected 1600") }
        if ($fact.Count -lt 1) { [void]$problems.Add("$m has no FACTORY lines") }
        Write-BaselineText (Join-Path $OutDir "factory-$m.txt") $fact
        Write-BaselineText (Join-Path $OutDir "digest-trace-$m.txt") $trace
        $digests.Add("$m`t$digest")

        $hogDir = Join-Path $runDir "hog-$m"
        $hogLine = ($m + $hog.Substring($mission.Length)).Replace('{CASEDIR}', $hogDir) + ' -factorylog -digesttrace 1600'
        $hres = Invoke-SmokeCase "hog-$m" $gameExe $exeDir $hogLine $CaseTimeout $hogDir
        $hdigest = Get-DigestKey $hogDir
        Write-Host "INFO: collect hog-$m class=$($hres.Class) $hdigest"
        if ($hres.Class -ne 'Pass' -and $hres.Class -ne 'TimeOut') { [void]$problems.Add("hog-$m class=$($hres.Class)") }
        if (-not $hdigest) { [void]$problems.Add("hog-$m has no SIM DIGEST line") }
        $hogDigests.Add("$m`t$hdigest")
        $s1 = [regex]::Match([string]$digest, 'state=(0x[0-9A-F]{8})')
        $s2 = [regex]::Match([string]$hdigest, 'state=(0x[0-9A-F]{8})')
        if (-not $s1.Success -or -not $s2.Success -or $s1.Groups[1].Value -ne $s2.Groups[1].Value) { $hogDiff.Add("$m base='$digest' hog='$hdigest'") }
    }
    Write-BaselineText (Join-Path $OutDir 'digests.txt') $digests
    Write-BaselineText (Join-Path $OutDir 'digests-hog.txt') $hogDigests
    if ($hogDiff.Count -eq 0) { Write-Host '[PASS] capture:hog-digest same' }
    else {
        foreach ($d in $hogDiff) { Write-Host "[FAIL] capture:hog-digest $d" }
        $script:failed = $true
    }
    Write-Host "INFO: collect runs in $runDir"
    if ($problems.Count) { throw ($problems -join '; ') }
}

function Step-Tables {
    $any = $false
    if ($SmokeAllDir) {
        $any = $true
        $res = Join-Path $SmokeAllDir 'results.txt'
        if (-not (Test-Path -LiteralPath $res)) { throw "results.txt not found in -SmokeAllDir" }
        $lines = @(Get-Content -LiteralPath $res | ForEach-Object { $_ -replace '\s+\(\d+ ms\)\s*$', '' })
        Write-BaselineText (Join-Path $OutDir 'smoke-all.txt') $lines
        $rows = New-Object System.Collections.Generic.List[string]
        $smokeDir = Join-Path $SmokeAllDir 'smoke'
        foreach ($d in @(Get-ChildItem -LiteralPath $smokeDir -Directory -ErrorAction SilentlyContinue | Sort-Object Name)) {
            $dk = Get-DigestKey $d.FullName
            $fk = Get-FinalScoreKey $d.FullName
            if (-not $dk) { $dk = 'none' }
            if (-not $fk) { $fk = 'none' }
            $rows.Add("$($d.Name)`t$dk`t$fk")
        }
        if ($rows.Count -eq 0) { throw 'no case dirs under -SmokeAllDir\smoke' }
        Write-BaselineText (Join-Path $OutDir 'smoke-all-digests.txt') $rows
    }
    else { Write-Host '[SKIP] capture:tables smoke-all - no -SmokeAllDir' }
    if ($HardeningDir) {
        $any = $true
        $res = Join-Path $HardeningDir 'results.txt'
        if (-not (Test-Path -LiteralPath $res)) { throw "results.txt not found in -HardeningDir" }
        $lines = @(Get-Content -LiteralPath $res | Where-Object { $_ -match 'hardening:' })
        if ($lines.Count -eq 0) { throw 'no hardening: lines in -HardeningDir\results.txt' }
        Write-BaselineText (Join-Path $OutDir 'hardening.txt') $lines
    }
    else { Write-Host '[SKIP] capture:tables hardening - no -HardeningDir' }
    if ($GuardRevertDir) {
        $any = $true
        $sum = Join-Path $GuardRevertDir 'summary.txt'
        if (-not (Test-Path -LiteralPath $sum)) { throw "summary.txt not found in -GuardRevertDir" }
        Write-BaselineText (Join-Path $OutDir 'guard-revert.txt') @(Get-Content -LiteralPath $sum)
    }
    else { Write-Host '[SKIP] capture:tables guard-revert - no -GuardRevertDir' }
    if (-not $any) { throw 'none of -SmokeAllDir, -HardeningDir, -GuardRevertDir given' }
}

function Step-DebugDigests {
    if (-not $DebugSmokeDir) { throw 'no -DebugSmokeDir' }
    $base = $DebugSmokeDir
    if (Test-Path -LiteralPath (Join-Path $base 'smoke') -PathType Container) { $base = Join-Path $base 'smoke' }
    $rows = New-Object System.Collections.Generic.List[string]
    $missing = New-Object System.Collections.Generic.List[string]
    foreach ($c in 'twice-digest-1', 'digest-hot1g_con', 'digest-spa3c_ass') {
        $dk = Get-DigestKey (Join-Path $base $c)
        if (-not $dk) { $missing.Add($c); continue }
        $rows.Add("$c`t$dk")
    }
    if ($missing.Count) { throw "no digest line for $($missing -join ', ')" }
    Write-BaselineText (Join-Path $OutDir 'digests-debug.txt') $rows
}

function Get-Producer($rel) {
    $n = $rel.ToLowerInvariant()
    if ($n.StartsWith('initializers')) { return '`capture-vs2013.ps1 -Map`' }
    if ($n.StartsWith('layouts\')) { return '`capture-vs2013.ps1 -Layouts`' }
    if ($n.StartsWith('factory-') -or $n.StartsWith('digest-trace-') -or $n -eq 'digests.txt' -or $n -eq 'digests-hog.txt') { return '`capture-vs2013.ps1 -Collect`' }
    if ($n -eq 'digests-debug.txt') { return '`capture-vs2013.ps1 -DebugDigests` from `run.ps1 -Smoke Twice -SmokeConfiguration Debug`' }
    if ($n.StartsWith('smoke-all')) { return '`capture-vs2013.ps1 -Tables` from `run.ps1 -Smoke All`' }
    if ($n -eq 'hardening.txt') { return '`capture-vs2013.ps1 -Tables` from `run.ps1 -Hardening`' }
    if ($n -eq 'guard-revert.txt') { return '`capture-vs2013.ps1 -Tables` from `guard-revert.ps1`' }
    return 'unknown'
}

function Get-TextArtifacts {
    if (-not (Test-Path -LiteralPath $OutDir)) { return @() }
    return @(Get-ChildItem -LiteralPath $OutDir -Recurse -File | Where-Object { $_.FullName -ne (Join-Path $OutDir $manifestName) } |
        ForEach-Object { $_.FullName.Substring($OutDir.TrimEnd('\').Length + 1) } | Sort-Object)
}

function Get-ShaHex($path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $fs = [IO.File]::OpenRead((Resolve-FullPath $path))
    try { return ([BitConverter]::ToString($sha.ComputeHash($fs))).Replace('-', '') }
    finally { $fs.Dispose(); $sha.Dispose() }
}

function Step-Archive {
    if (Test-InsideRepo $ArchiveDir) { throw 'archive dir is inside the repo' }
    $vcBin = Get-VcBin
    $members = @(
        (Join-Path $exeDir 'BattlefrontII.exe'),
        (Join-Path $exeDir 'LuaCompiler.exe'),
        (Join-Path $objDir 'BattlefrontII.pdb'),
        $mapFile,
        $tlogFile
    )
    $files = @()
    foreach ($f in $members) {
        if (Test-Path -LiteralPath $f) { $files += $f }
        elseif (-not $f.EndsWith('LuaCompiler.exe')) { throw "missing $(Split-Path $f -Leaf) for configuration $Configuration" }
    }
    $short = (Get-GitText 'rev-parse --short HEAD').Trim()
    $head = (Get-GitText 'rev-parse HEAD').Trim()
    $clean = ((Get-GitText 'status --porcelain --untracked-files=no').Trim() -eq '')
    $zipName = "vs2013-$($Configuration.ToLowerInvariant())-$short.zip"
    New-Item -ItemType Directory -Force -Path $ArchiveDir | Out-Null
    $zip = Join-Path $ArchiveDir $zipName
    Compress-Archive -LiteralPath $files -DestinationPath $zip -Force
    $clr = Invoke-Tool (Join-Path $vcBin 'cl.exe') '' $root
    $clm = [regex]::Match($clr.Err + $clr.Out, 'Version (\S+)')
    $clVer = 'unknown'
    if ($clm.Success) { $clVer = $clm.Groups[1].Value }
    $crtVer = 'not found'
    $crt = Join-Path $env:WINDIR 'SysWOW64\msvcr120.dll'
    if (Test-Path -LiteralPath $crt) { $crtVer = (Get-Item -LiteralPath $crt).VersionInfo.ProductVersion }
    $md = New-Object System.Collections.Generic.List[string]
    $md.Add('# VS2013 baseline manifest')
    $md.Add('')
    $md.Add('## Build')
    $md.Add('')
    $md.Add('| Key | Value |')
    $md.Add('|-----|-------|')
    $md.Add('| toolset | v120_xp |')
    $md.Add("| cl | $clVer |")
    $md.Add("| msvcr120.dll | $crtVer |")
    $md.Add("| configuration | $Configuration |")
    $md.Add("| commit | $head |")
    $md.Add("| tracked tree clean | $(if ($clean) { 'yes' } else { 'no' }) |")
    $md.Add('')
    $md.Add('## Archive')
    $md.Add('')
    $md.Add("Archive: ``$zip`` SHA-256 ``$(Get-ShaHex $zip)``")
    $md.Add('')
    $md.Add('| Archived file | SHA-256 |')
    $md.Add('|---------------|---------|')
    foreach ($f in $files) { $md.Add("| ``$(Split-Path $f -Leaf)`` | $(Get-ShaHex $f) |") }
    $md.Add('')
    $md.Add('## Text artifacts')
    $md.Add('')
    $md.Add('| File | SHA-256 | Produced by |')
    $md.Add('|------|---------|-------------|')
    foreach ($rel in (Get-TextArtifacts)) { $md.Add("| ``$rel`` | $(Get-ShaHex (Join-Path $OutDir $rel)) | $(Get-Producer $rel) |") }
    $md.Add('')
    $md.Add('## Notes')
    $md.Add('')
    $md.Add('- Phase 20 should compare the `turns`, `rand` and `state` fields of each digest line, not the `%.3f` `time=` and `snd=` text, because CRT printf rounding may differ.')
    $md.Add('- Release is the reference configuration for every artifact. The binaries are not in the repo; only their SHA-256 sums are.')
    Write-BaselineText (Join-Path $OutDir $manifestName) $md
    Write-Host "INFO: archive $zipName ($($files.Count) files)"
}

function Step-Verify {
    $problems = New-Object System.Collections.Generic.List[string]
    $man = Join-Path $OutDir $manifestName
    if (-not (Test-Path -LiteralPath $man)) { throw "$manifestName not found in the output dir" }
    $text = [IO.File]::ReadAllLines($man)
    $section = ''
    $listed = @{}
    $members = @{}
    $zipName = $null
    $zipHash = $null
    $zipDir = $null
    foreach ($l in $text) {
        if ($l.StartsWith('## ')) { $section = $l.Substring(3).Trim(); continue }
        $zm = [regex]::Match($l, '^Archive: `([^`]+)\\([^`\\]+)` SHA-256 `([0-9A-F]{64})`')
        if ($zm.Success) { $zipDir = $zm.Groups[1].Value; $zipName = $zm.Groups[2].Value; $zipHash = $zm.Groups[3].Value; continue }
        $rm = [regex]::Match($l, '^\| `([^`]+)` \| ([0-9A-F]{64}) \|')
        if (-not $rm.Success) { continue }
        if ($section -eq 'Archive') { $members[$rm.Groups[1].Value] = $rm.Groups[2].Value }
        elseif ($section -eq 'Text artifacts') { $listed[$rm.Groups[1].Value] = $rm.Groups[2].Value }
    }
    if ($listed.Count -eq 0) { $problems.Add('no text artifacts listed') }
    foreach ($rel in $listed.Keys) {
        $p = Join-Path $OutDir $rel
        if (-not (Test-Path -LiteralPath $p)) { $problems.Add("missing $rel"); continue }
        if ((Get-ShaHex $p) -ne $listed[$rel]) { $problems.Add("hash mismatch $rel") }
    }
    foreach ($rel in (Get-TextArtifacts)) { if (-not $listed.ContainsKey($rel)) { $problems.Add("not in manifest $rel") } }
    $verifyDir = $ArchiveDir
    if ($zipDir -and -not $archiveDirGiven) { $verifyDir = [Environment]::ExpandEnvironmentVariables($zipDir) }
    if (-not $zipName) { $problems.Add('no archive line') }
    elseif (Test-InsideRepo $verifyDir) { $problems.Add('archive dir is inside the repo') }
    else {
        $zip = Join-Path $verifyDir $zipName
        if (-not (Test-Path -LiteralPath $zip)) { $problems.Add("archive $zipName not found in the archive dir") }
        else {
            if ((Get-ShaHex $zip) -ne $zipHash) { $problems.Add("hash mismatch $zipName") }
            $tmp = Join-Path $env:TEMP ('openswbf2-verify-' + [guid]::NewGuid().ToString('N'))
            try {
                Expand-Archive -LiteralPath $zip -DestinationPath $tmp -Force
                $got = @(Get-ChildItem -LiteralPath $tmp -Recurse -File)
                foreach ($k in $members.Keys) {
                    $p = Join-Path $tmp $k
                    if (-not (Test-Path -LiteralPath $p)) { $problems.Add("archive member missing $k"); continue }
                    if ((Get-ShaHex $p) -ne $members[$k]) { $problems.Add("hash mismatch archive member $k") }
                }
                if ($got.Count -ne $members.Count) { $problems.Add("archive holds $($got.Count) files, manifest lists $($members.Count)") }
            }
            finally {
                if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue }
            }
        }
    }
    if ($env:USERNAME) {
        foreach ($f in @(Get-ChildItem -LiteralPath $OutDir -Recurse -File)) {
            if ([IO.File]::ReadAllText($f.FullName).IndexOf($env:USERNAME, [StringComparison]::OrdinalIgnoreCase) -ge 0) { $problems.Add("user name in $($f.Name)") }
        }
    }
    $tracked = @((Get-GitText 'ls-files -- bin obj') -split "\r?\n" | Where-Object { $_ -match '\.(exe|pdb|map)$' })
    foreach ($t in $tracked) { $problems.Add("tracked binary $t") }
    if ($problems.Count) {
        foreach ($p in $problems) { Write-Host "[FAIL] capture:verify - $p" }
        $script:failed = $true
        $script:skipped = $true
    }
}

if (-not $NoBuild -and ($Map -or $Layouts -or $Collect -or $Archive)) {
    $msbuild = Join-Path ${env:ProgramFiles(x86)} 'MSBuild\12.0\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) { Write-Host 'MSBuild 12.0 not found (install VS2013 Update 5).'; exit 2 }
    & $msbuild (Join-Path $root 'BattlefrontII.vcxproj') "/p:Configuration=$Configuration" '/p:Platform=Win32' /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0) { Write-Host 'Build failed.'; exit 2 }
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if ($Map) { Invoke-Step 'map' { Step-Map } }
if ($Layouts) { Invoke-Step 'layouts' { Step-Layouts } }
if ($Collect) { Invoke-Step 'collect' { Step-Collect } }
if ($Tables) { Invoke-Step 'tables' { Step-Tables } }
if ($DebugDigests) { Invoke-Step 'debugdigests' { Step-DebugDigests } }
if ($Archive) { Invoke-Step 'archive' { Step-Archive } }
if ($Verify) { Invoke-Step 'verify' { Step-Verify } }

if ($script:failed) { exit 1 }
exit 0
