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
# The VS2013 web installer is dead (every package link 404s), only the offline ISO still installs.
$vsIsoUrl = 'https://download.microsoft.com/download/A/A/D/AAD1AA11-FF9A-4B3C-8601-054E89260B78/vs2013.5_ce_enu.iso'
$vsIsoSha = '7DE2340D9570895451C5F5D9F2081DB860A4F753E7596E31B2742390A6D0E6BC'
$dxSdkUrl = 'https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe'
$dxSdkSha = '705271DC83BFEE54D9B94E028426E288D5F070784B7446D164F48ECFBB2A02CB'
$dxRedistUrl = 'https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe'
# Depot 6061 manifest whose lvls match the hashes pinned in tools\lvl_build.cpp.
$stockDepot = 6061
$stockManifest = '6184123930358608135'

$results = @()
$downloaded = @()

function Add-Result([string]$name, [string]$status, [string]$detail) {
    $script:results += New-Object PSObject -Property @{ Name = $name; Status = $status; Detail = $detail }
    Write-Host ("[{0}] {1}: {2}" -f $status, $name, $detail)
}

function Find-MSBuild12 {
    $msbuild = Join-Path $pf86 'MSBuild\12.0\Bin\MSBuild.exe'
    if (-not (Test-Path $msbuild)) {
        $key = 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\MSBuild\ToolsVersions\12.0'
        if (Test-Path $key) { $msbuild = Join-Path (Get-ItemProperty $key).MSBuildToolsPath 'MSBuild.exe' }
    }
    if (Test-Path $msbuild) { return $msbuild }
    return $null
}

function Get-VsMissing {
    $missing = @()
    if (-not (Find-MSBuild12)) { $missing += 'MSBuild 12.0' }
    $need = [ordered]@{
        'VC++ 2013 compiler' = 'Microsoft Visual Studio 12.0\VC\bin\cl.exe'
        'v120_xp toolset' = 'MSBuild\Microsoft.Cpp\v4.0\V120\Platforms\Win32\PlatformToolsets\v120_xp'
        'Windows SDK 7.1A' = 'Microsoft SDKs\Windows\v7.1A\Include\Windows.h'
    }
    foreach ($k in $need.Keys) { if (-not (Test-Path (Join-Path $pf86 $need[$k]))) { $missing += $k } }
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

function Install-VS2013 {
    Assert-FreeSpace $CacheDir 15
    $iso = Get-Download $vsIsoUrl $vsIsoSha
    $image = Mount-DiskImage -ImagePath $iso -PassThru
    try {
        $setup = '{0}:\vs_community.exe' -f ($image | Get-Volume).DriveLetter
        Assert-MicrosoftSigned $setup
        $log = Join-Path $CacheDir 'vs2013_install.log'
        Write-Host 'Installing Visual Studio 2013 Update 5 (accept the UAC prompt, takes 20-40 minutes)...'
        $code = Invoke-Elevated $setup @('/Passive', '/NoRestart', '/NoRefresh', '/Log', "`"$log`"")
        if ($code -ne 0 -and $code -ne 3010) { throw "VS2013 setup exited with $code, see $log" }
        if ($code -eq 3010) { Write-Host 'VS2013 setup asks for a reboot before building.' }
    }
    finally { Dismount-DiskImage -ImagePath $iso | Out-Null }
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

function Get-KnownLvlHashes {
    $src = Get-Content -Raw (Join-Path $root 'tools\lvl_build.cpp')
    $known = @{}
    foreach ($m in [regex]::Matches($src, '\{"(shell|common|ingame|mission)",\s*"([0-9a-f]{64})"\}')) {
        $known[$m.Groups[1].Value] = $m.Groups[2].Value.ToUpper()
    }
    if ($known.Count -ne 4) { throw 'Could not read the stock lvl hashes from tools\lvl_build.cpp.' }
    return $known
}

function Get-ModdedLvls([string]$gameData, $known) {
    $bad = @()
    foreach ($lvl in ($known.Keys | Sort-Object)) {
        $file = Join-Path $gameData "data\_lvl_pc\$lvl.lvl"
        if (Test-Path "$file.orig") { $file = "$file.orig" }
        if (-not (Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash -ne $known[$lvl]) { $bad += "$lvl.lvl" }
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
    if ($vsMissing.Count -eq 0) { Add-Result 'Visual Studio 2013' 'OK' 'MSBuild 12, VC++ 2013, v120_xp, Windows SDK 7.1A' }
    elseif ($CheckOnly) { Add-Result 'Visual Studio 2013' 'MISSING' ("missing: {0}; run without -CheckOnly to install from the offline ISO (5.1 GB)" -f ($vsMissing -join ', ')) }
    else {
        Install-VS2013
        $vsMissing = @(Get-VsMissing)
        if ($vsMissing.Count -eq 0) { Add-Result 'Visual Studio 2013' 'FIX' 'installed Visual Studio 2013 Community Update 5' }
        else { Add-Result 'Visual Studio 2013' 'MISSING' ("still missing after install: {0}" -f ($vsMissing -join ', ')) }
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
    if (Test-Path (Join-Path $sys 'msvcr120.dll')) { Add-Result 'VC++ 2013 runtime' 'OK' 'msvcr120.dll' }
    else { Add-Result 'VC++ 2013 runtime' 'MISSING' 'msvcr120.dll not in SysWOW64; install vcredist 2013 x86 (Visual Studio 2013 includes it)' }

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
    Write-Host '    "C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" BattlefrontII.sln /p:Configuration=Release /p:Platform=Win32'
    exit 0
}
catch {
    Write-Host ''
    Write-Host "Setup error: $($_.Exception.Message)"
    exit 2
}
