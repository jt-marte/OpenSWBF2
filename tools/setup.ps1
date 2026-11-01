param(
    [switch]$CheckOnly,
    [string]$CacheDir = (Join-Path $env:LOCALAPPDATA 'OpenSWBF2\setup'),
    [string]$DxSdkDir = (Join-Path $env:LOCALAPPDATA 'OpenSWBF2\DXSDK'),
    [switch]$KeepDownloads
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tests\smoke.ps1')

$pf86 = ${env:ProgramFiles(x86)}
# The VS2022 Build Tools bootstrapper (it installs the current 17.x release; it is checked for
# Microsoft's signature instead of a pinned hash), with the components the projects need: the v143
# C++ tools for x86 and x64 and the Windows SDK they pin.
$vsBootstrapperUrl = 'https://aka.ms/vs/17/release/vs_BuildTools.exe'
$vsComponents = @('Microsoft.VisualStudio.Workload.VCTools', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.Windows11SDK.26100')
$dxSdkUrl = 'https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe'
$dxSdkSha = '705271DC83BFEE54D9B94E028426E288D5F070784B7446D164F48ECFBB2A02CB'
$dxRedistUrl = 'https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe'
# There is no 64-bit binkw32, so the x64 build decodes the movies with FFmpeg: BtbN's LGPL shared
# build of FFmpeg n8.1.3, pinned. It lands in third_party\ffmpeg (include, lib, bin).
$ffmpegUrl = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-10-30-13-01/ffmpeg-n8.1.3-27-g8dca1586f4-win64-lgpl-shared-8.1.zip'
$ffmpegSha = '21B96A5B382B031E89C1C91D7AF8EFC3DB989C4C11BFF27422A4C4EA24CF45ED'
$ffmpegDir = Join-Path $root 'third_party\ffmpeg'
# DXVK, the Vulkan renderer: the build copies x32\d3d9.dll (Win32) or x64\d3d9.dll (x64) to
# <exe dir>\dxvk\d3d9.dll.
$dxvkVersion = '3.1.1'
$dxvkUrl = "https://github.com/doitsujin/dxvk/releases/download/v$dxvkVersion/dxvk-$dxvkVersion.tar.gz"
$dxvkSha = '40565B4A724AADC4433FA4E010B4B23916D9B1F1BAEEE64E17186DB94F54E608'
$dxvkX32Sha = '265888C31CA78DFFA290C39CB7E50BFB02762590E41927906E46FB32F01497FA'
# Depot 6061 manifest whose lvls match the hashes pinned in tools\lvl_build.cpp.
$stockDepot = 6061
$stockManifest = '6184123930358608135'

$results = @()
$downloaded = @()

function Add-Result([string]$name, [string]$status, [string]$detail) {
    $script:results += New-Object PSObject -Property @{ Name = $name; Status = $status; Detail = $detail }
    Write-Host ("[{0}] {1}: {2}" -f $status, $name, $detail)
}

function Find-VS2022 {
    $vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    return (& $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}

function Get-VsMissing {
    $missing = @()
    if (-not (Find-VS2022)) { $missing += 'VS2022 C++ build tools (v143, x86 and x64)' }
    if (-not (Test-Path (Join-Path $pf86 'Windows Kits\10\Include\10.0.26100.0\um\Windows.h'))) { $missing += 'Windows SDK 10.0.26100.0' }
    return $missing
}

function Get-DxSdkDir {
    foreach ($dir in $env:DXSDK_DIR, [Environment]::GetEnvironmentVariable('DXSDK_DIR', 'User'), [Environment]::GetEnvironmentVariable('DXSDK_DIR', 'Machine')) {
        if ($dir -and (Test-Path (Join-Path $dir 'Include\d3dx9.h')) -and (Test-Path (Join-Path $dir 'Lib\x86\d3dx9.lib'))) { return $dir }
    }
    return $null
}

function Assert-FreeSpace([string]$path, [int]$gb) {
    New-Item -ItemType Directory -Force -Path $path | Out-Null
    $drive = (Get-Item $path).PSDrive
    $free = [int]((Get-PSDrive $drive.Name).Free / 1GB)
    if ($free -lt $gb) { throw "Need about $gb GB free on $($drive.Name): (have $free GB)." }
}

function Get-Download([string]$url, [string]$sha) {
    New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
    $file = Join-Path $CacheDir ([IO.Path]::GetFileName($url))
    if ((Test-Path $file) -and $sha -and (Get-FileHash $file -Algorithm SHA256).Hash -eq $sha) { return $file }
    if (-not $sha -and (Test-Path $file)) { Remove-Item $file -Force }
    Write-Host "Downloading $url"
    & curl.exe -L --fail --retry 5 -C - -o $file $url
    if ($LASTEXITCODE -ne 0 -and (Test-Path $file)) {
        Remove-Item $file -Force
        & curl.exe -L --fail --retry 5 -o $file $url
    }
    if ($LASTEXITCODE -ne 0) { throw "Download failed (curl exit $LASTEXITCODE): $url" }
    if ($sha -and (Get-FileHash $file -Algorithm SHA256).Hash -ne $sha) {
        Remove-Item $file -Force
        throw "SHA-256 mismatch for $file (deleted, run setup again)."
    }
    $script:downloaded += $file
    return $file
}

function Assert-MicrosoftSigned([string]$file) {
    $sig = Get-AuthenticodeSignature $file
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw "$file is not validly signed by Microsoft ($($sig.Status))."
    }
}

function Invoke-Elevated([string]$exe, [string[]]$argList) {
    try { $p = Start-Process $exe -ArgumentList $argList -Verb RunAs -PassThru -Wait }
    catch { throw "Could not start $exe elevated (UAC declined?)." }
    return $p.ExitCode
}

function Install-VsBuildTools {
    Assert-FreeSpace $CacheDir 8
    $exe = Get-Download $vsBootstrapperUrl $null
    Assert-MicrosoftSigned $exe
    $vsArgs = @('--passive', '--wait', '--norestart', '--nocache')
    foreach ($c in $vsComponents) { $vsArgs += @('--add', $c) }
    Write-Host 'Installing the Visual Studio 2022 Build Tools (accept the UAC prompt, takes 10-30 minutes)...'
    $code = Invoke-Elevated $exe $vsArgs
    if ($code -ne 0 -and $code -ne 3010) { throw "VS2022 Build Tools setup exited with $code" }
    if ($code -eq 3010) { Write-Host 'The Build Tools setup asks for a reboot before building.' }
}

# The SDK installer fails with S1023 when a newer VC++ 2010 redist is present, and the build
# only needs Include and Lib, so take them straight out of the self-extractor with tar.
function Install-DxSdk {
    $tar = Join-Path $env:windir 'System32\tar.exe'
    if (-not (Test-Path $tar)) { throw 'tar.exe not found (needs Windows 10 1803 or newer).' }
    if (Test-Path $DxSdkDir) { throw "$DxSdkDir already exists but is incomplete, remove it and run setup again." }
    $exe = Get-Download $dxSdkUrl $dxSdkSha
    $tmp = Join-Path $CacheDir 'dxsdk_extract'
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    New-Item -ItemType Directory -Path $tmp | Out-Null
    # tar always exits 1 here: after the last Lib file it reads on into the redist CAB, whose LZX
    # block it cannot decode. Everything needed comes out first, so check the files instead.
    & { $ErrorActionPreference = 'Continue'; & $tar -xf $exe -C $tmp 'DXSDK/Include' 'DXSDK/Lib' 2>$null }
    foreach ($f in 'Include\d3dx9.h', 'Include\dinput.h', 'Lib\x86\d3dx9.lib', 'Lib\x86\dxguid.lib', 'Lib\x86\XInput.lib') {
        if (-not (Test-Path (Join-Path $tmp "DXSDK\$f"))) { throw "tar did not extract DXSDK\$f from $exe." }
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $DxSdkDir -Parent) | Out-Null
    Move-Item (Join-Path $tmp 'DXSDK') $DxSdkDir
    Remove-Item -Recurse -Force $tmp
    $dir = $DxSdkDir.TrimEnd('\') + '\'
    [Environment]::SetEnvironmentVariable('DXSDK_DIR', $dir, 'User')
    $env:DXSDK_DIR = $dir
}

function Install-DxRedist {
    $exe = Get-Download $dxRedistUrl $null
    Assert-MicrosoftSigned $exe
    $tmp = Join-Path $CacheDir 'dxredist'
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    $p = Start-Process $exe -ArgumentList '/Q', "/T:`"$tmp`"" -PassThru -Wait
    if ($p.ExitCode -ne 0) { throw "Could not extract $exe (exit $($p.ExitCode))." }
    Write-Host 'Installing the DirectX June 2010 runtime (accept the UAC prompt)...'
    $code = Invoke-Elevated (Join-Path $tmp 'DXSETUP.exe') @('/silent')
    Remove-Item -Recurse -Force $tmp
    if ($code -ne 0) { throw "DXSETUP exited with $code." }
}

function Test-FFmpeg {
    foreach ($f in 'include\libavcodec\avcodec.h', 'lib\avcodec.lib', 'lib\avformat.lib', 'lib\avutil.lib', 'lib\swscale.lib', 'lib\swresample.lib',
                   'bin\avcodec-62.dll', 'bin\avformat-62.dll', 'bin\avutil-60.dll', 'bin\swscale-9.dll', 'bin\swresample-6.dll', 'LICENSE.txt') {
        if (-not (Test-Path (Join-Path $ffmpegDir $f))) { return $false }
    }
    return $true
}

function Install-FFmpeg {
    if (Test-Path $ffmpegDir) { throw "$ffmpegDir already exists but is incomplete, remove it and run setup again." }
    $zip = Get-Download $ffmpegUrl $ffmpegSha
    $tmp = Join-Path $CacheDir 'ffmpeg_extract'
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    Expand-Archive -LiteralPath $zip -DestinationPath $tmp
    # Everything sits under one folder named after the build
    $top = @(Get-ChildItem -LiteralPath $tmp -Directory)
    if ($top.Count -ne 1) { throw "Unexpected layout in $zip." }
    New-Item -ItemType Directory -Force -Path (Split-Path $ffmpegDir -Parent) | Out-Null
    Move-Item $top[0].FullName $ffmpegDir
    Remove-Item -Recurse -Force $tmp
}

# 'ok' (the pinned build), 'other' (both DLLs there, a different build) or 'missing'.
function Get-DxvkState([string]$dir) {
    $x32 = Join-Path $dir 'x32\d3d9.dll'
    if (-not (Test-Path $x32) -or -not (Test-Path (Join-Path $dir 'x64\d3d9.dll'))) { return 'missing' }
    if ((Get-FileHash $x32 -Algorithm SHA256).Hash -ne $dxvkX32Sha) { return 'other' }
    return 'ok'
}

# Unpacks the release tarball's x32 and x64 folders into $dir (beside it first, so a failed
# unpack never leaves a half-filled $dir behind).
function Install-Dxvk([string]$dir) {
    $tar = Join-Path $env:windir 'System32\tar.exe'
    if (-not (Test-Path $tar)) { throw 'tar.exe not found (needs Windows 10 1803 or newer).' }
    if (Test-Path $dir) { throw "$dir already exists but is incomplete, remove it and run setup again." }
    $file = Get-Download $dxvkUrl $dxvkSha
    $tmp = "$dir.part"
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    & $tar -xzf $file -C $tmp --strip-components 1
    if ($LASTEXITCODE -ne 0) { throw "tar could not unpack $file (exit $LASTEXITCODE)." }
    foreach ($f in 'x32\d3d9.dll', 'x64\d3d9.dll') {
        if (-not (Test-Path (Join-Path $tmp $f))) { throw "$file has no $f." }
    }
    Rename-Item $tmp (Split-Path $dir -Leaf)
}

function Get-KnownLvlHashes {
    $src = Get-Content -Raw (Join-Path $root 'tools\lvl_build.cpp')
    $known = @{}
    # Steam's and GOG's copies, from lvl_build.cpp's known_stock table
    foreach ($m in [regex]::Matches($src, '\{"(shell|common|ingame|mission)",\s*"(steam|gog)",\s*"([0-9a-f]{64})"')) {
        $lvl = $m.Groups[1].Value
        if (-not $known.ContainsKey($lvl)) { $known[$lvl] = @() }
        $known[$lvl] += $m.Groups[3].Value.ToUpper()
    }
    if ($known.Count -ne 4) { throw 'Could not read the stock lvl hashes from tools\lvl_build.cpp.' }
    return $known
}

function Get-ModdedLvls([string]$gameData, $known) {
    $bad = @()
    foreach ($lvl in ($known.Keys | Sort-Object)) {
        $file = Join-Path $gameData "data\_lvl_pc\$lvl.lvl"
        if (Test-Path "$file.orig") { $file = "$file.orig" }
        if (-not (Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash -notin $known[$lvl]) { $bad += "$lvl.lvl" }
    }
    return $bad
}

function Get-SteamPath {
    try { return (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath -replace '/', '\' }
    catch { return $null }
}

try {
    Write-Host 'OpenSWBF2 setup'
    if ($CheckOnly) { Write-Host '(check only, nothing will be installed or changed)' }
    Write-Host ''

    $vsMissing = @(Get-VsMissing)
    if ($vsMissing.Count -eq 0) { Add-Result 'Visual Studio 2022 Build Tools' 'OK' ("v143 x86/x64, Windows SDK 10.0.26100.0 in {0}" -f (Find-VS2022)) }
    elseif ($CheckOnly) { Add-Result 'Visual Studio 2022 Build Tools' 'MISSING' ("missing: {0}; run without -CheckOnly to install them (about 4 GB)" -f ($vsMissing -join ', ')) }
    else {
        Install-VsBuildTools
        $vsMissing = @(Get-VsMissing)
        if ($vsMissing.Count -eq 0) { Add-Result 'Visual Studio 2022 Build Tools' 'FIX' 'installed the v143 C++ tools and Windows SDK 10.0.26100.0' }
        else { Add-Result 'Visual Studio 2022 Build Tools' 'MISSING' ("still missing after install: {0}" -f ($vsMissing -join ', ')) }
    }

    $dx = Get-DxSdkDir
    if ($dx) { Add-Result 'DirectX SDK (June 2010)' 'OK' "DXSDK_DIR=$dx" }
    elseif ($CheckOnly) { Add-Result 'DirectX SDK (June 2010)' 'MISSING' "run without -CheckOnly to extract headers and libs to $DxSdkDir" }
    else {
        Install-DxSdk
        Add-Result 'DirectX SDK (June 2010)' 'FIX' "extracted to $DxSdkDir and set DXSDK_DIR (new terminals pick it up)"
    }

    $sys = Join-Path $env:windir 'SysWOW64'
    if (-not (Test-Path (Join-Path $sys 'd3dx9_43.dll'))) {
        if ($CheckOnly) { Add-Result 'DirectX runtime' 'MISSING' 'd3dx9_43.dll not in SysWOW64; run without -CheckOnly to install the June 2010 runtime' }
        else {
            Install-DxRedist
            if (Test-Path (Join-Path $sys 'd3dx9_43.dll')) { Add-Result 'DirectX runtime' 'FIX' 'installed the DirectX June 2010 runtime' }
            else { Add-Result 'DirectX runtime' 'MISSING' 'd3dx9_43.dll still missing after DXSETUP' }
        }
    }
    else { Add-Result 'DirectX runtime' 'OK' 'd3dx9_43.dll' }
    if (Test-Path (Join-Path $sys 'msvcr120.dll')) { Add-Result 'VC++ 2013 runtime' 'OK' 'msvcr120.dll (the Win32 Galaxy DLLs need it)' }
    else { Add-Result 'VC++ 2013 runtime' 'MISSING' 'msvcr120.dll not in SysWOW64; install vcredist 2013 x86 (the Win32 Galaxy DLLs need it)' }
    $sys64 = Join-Path $env:windir 'System32'
    foreach ($rt in @(@{ Name = 'VC++ 2015-2022 runtime (x86)'; Dir = $sys }, @{ Name = 'VC++ 2015-2022 runtime (x64)'; Dir = $sys64 })) {
        if (Test-Path (Join-Path $rt.Dir 'vcruntime140.dll')) { Add-Result $rt.Name 'OK' 'vcruntime140.dll' }
        else { Add-Result $rt.Name 'MISSING' ("vcruntime140.dll not in {0}; install the VC++ 2015-2022 redistributable (https://aka.ms/vs/17/release/vc_redist.{1}.exe)" -f (Split-Path $rt.Dir -Leaf), $(if ($rt.Dir -eq $sys) { 'x86' } else { 'x64' })) }
    }

    if (Test-FFmpeg) { Add-Result 'FFmpeg (x64 movies)' 'OK' $ffmpegDir }
    elseif ($CheckOnly) { Add-Result 'FFmpeg (x64 movies)' 'MISSING' "run without -CheckOnly to fetch the pinned LGPL build (80 MB) into $ffmpegDir" }
    else {
        Install-FFmpeg
        if (Test-FFmpeg) { Add-Result 'FFmpeg (x64 movies)' 'FIX' "fetched FFmpeg n8.1.3 (LGPL, shared) into $ffmpegDir" }
        else { Add-Result 'FFmpeg (x64 movies)' 'MISSING' "$ffmpegDir is still incomplete after extracting the zip" }
    }

    $dxvkDir = Join-Path $root 'third_party\dxvk'
    $dxvkState = Get-DxvkState $dxvkDir
    if ($dxvkState -eq 'ok') { Add-Result 'DXVK' 'OK' "$dxvkVersion in $dxvkDir" }
    elseif ($dxvkState -eq 'other') { Add-Result 'DXVK' 'OK' "$dxvkDir holds a build other than $dxvkVersion (delete it and run setup again to get $dxvkVersion)" }
    elseif ($CheckOnly) { Add-Result 'DXVK' 'MISSING' "run without -CheckOnly to fetch DXVK $dxvkVersion into $dxvkDir (without it the game runs on DirectX 9 only)" }
    else {
        Install-Dxvk $dxvkDir
        if ((Get-DxvkState $dxvkDir) -eq 'ok') { Add-Result 'DXVK' 'FIX' "fetched DXVK $dxvkVersion into $dxvkDir" }
        else { Add-Result 'DXVK' 'MISSING' "$dxvkDir does not hold the DXVK $dxvkVersion DLLs after unpacking" }
    }

    $known = Get-KnownLvlHashes
    $steam = Get-SteamPath
    $depotGameData = $null
    if ($steam) { $depotGameData = Join-Path $steam "steamapps\content\app_6060\depot_$stockDepot\GameData" }
    $gameData = Find-GameDataRoot
    if (-not $gameData) {
        Add-Result 'Game data' 'MISSING' 'no GameData found; install the Steam version (app 6060) or set SWBF2_GAMEDATA to a GameData folder'
    }
    else {
        $bad = @(Get-ModdedLvls $gameData $known)
        if ($bad.Count -eq 0) { Add-Result 'Game data' 'OK' $gameData }
        elseif ($depotGameData -and (Test-Path $depotGameData) -and @(Get-ModdedLvls $depotGameData $known).Count -eq 0) {
            if ($CheckOnly) { Add-Result 'Game data' 'MISSING' "$gameData is modded, but a stock copy is at $depotGameData; run without -CheckOnly to point SWBF2_GAMEDATA at it" }
            else {
                [Environment]::SetEnvironmentVariable('SWBF2_GAMEDATA', $depotGameData, 'User')
                $env:SWBF2_GAMEDATA = $depotGameData
                Add-Result 'Game data' 'FIX' "set SWBF2_GAMEDATA=$depotGameData (stock depot copy; new terminals pick it up)"
            }
        }
        else {
            Add-Result 'Game data' 'MISSING' ("{0} in {1} differ from the stock Steam files (1.3 patch, Remaster or other mods replace them)" -f ($bad -join ', '), $gameData)
            Write-Host ''
            Write-Host '  The build packs its scripts into the stock lvls, so it needs an unmodded copy. To get one'
            Write-Host '  without touching your modded install, open the Steam console (paste steam://open/console'
            Write-Host '  into Win+R) and run:'
            Write-Host ''
            Write-Host "      download_depot 6060 $stockDepot $stockManifest"
            Write-Host ''
            if ($depotGameData) { Write-Host "  When it finishes, run this script again and it will point SWBF2_GAMEDATA at`n  $depotGameData" }
            else { Write-Host '  When it finishes, set SWBF2_GAMEDATA to the downloaded GameData folder.' }
            Write-Host '  (Steam "Verify integrity" also works, but it reverts your mods.)'
        }
    }

    foreach ($f in $downloaded) { if (-not $KeepDownloads) { Remove-Item -Force $f -ErrorAction SilentlyContinue } }

    Write-Host ''
    $missing = @($results | Where-Object { $_.Status -eq 'MISSING' })
    if ($missing.Count -gt 0) {
        Write-Host ("{0} item(s) still need attention: {1}" -f $missing.Count, (($missing | ForEach-Object { $_.Name }) -join ', '))
        exit 1
    }
    Write-Host 'Ready to build. In a new terminal, from the repo root:'
    Write-Host ('    "{0}" BattlefrontII.sln /p:Configuration=Release /p:Platform=Win32' -f (Join-Path (Find-VS2022) 'MSBuild\Current\Bin\MSBuild.exe'))
    exit 0
}
catch {
    Write-Host ''
    Write-Host "Setup error: $($_.Exception.Message)"
    exit 2
}
