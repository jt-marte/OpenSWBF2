<#
Builds the x64 release package that the OpenBattlefront launcher installs:
dist\OpenSWBF2-<version>-x64.zip, plus a .sha256 beside it.

The package holds only our own files and their licences: the x64 game, FFmpeg, DXVK, LuaCompiler,
the Lua scripts and manifest.json (every file's SHA-256 and the stock lvl hashes the launcher
accepts). It never holds a retail file. The launcher rebuilds the four core lvls on the player's PC
from their own copy, with this package's LuaCompiler.

Publishing is a separate step: the script prints the gh command and runs nothing.
#>
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version,
    [string]$OutDir,
    [string]$MinLauncherVersion = '0.1.0',
    [switch]$NoBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tests\vstools.ps1')
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
if (-not $OutDir) { $OutDir = Join-Path $root 'dist' }

$name = "OpenSWBF2-$Version-x64"
$bin = Join-Path $root 'bin\x64\Release'
# The x64 LuaCompiler ships (the Docker server runs it under 64-bit-only Wine); the Win32 one is
# built too, and both must compile every script to the same bytes
$luac = Join-Path $bin 'LuaCompiler.exe'
$luac32 = Join-Path $root 'bin\Win32\Release\LuaCompiler.exe'

# Every path in the package must match one of these. Retail files can never match.
$allowed = @(
    '^BattlefrontII\.exe$',
    '^LuaCompiler\.exe$',
    '^(avcodec|avformat|avutil|swresample|swscale)-\d+\.dll$',
    '^FFmpeg-LICENSE\.txt$',
    '^dxvk/(d3d9\.dll|LICENSE\.txt)$',
    '^data/scripts/[A-Za-z0-9_./-]+\.(lua|json)$',
    '^README\.md$'
)
$retail = '\.(lvl|mvs|bik|bnk|str|sfx|tex|msh|zaa|zaf|ini|orig|profile)$|(^|/)(binkw32|galaxy|galaxypeer|steam_api|unicows)\.dll$'

function Get-StockLvlHashes {
    $src = Get-Content -Raw (Join-Path $root 'tools\lvl_build.cpp')
    $known = [ordered]@{}
    # Steam's and GOG's copies, from lvl_build.cpp's known_stock table
    foreach ($m in [regex]::Matches($src, '\{"(shell|common|ingame|mission)",\s*"(steam|gog)",\s*"([0-9a-f]{64})"')) {
        $lvl = $m.Groups[1].Value
        if (-not $known.Contains($lvl)) { $known[$lvl] = @() }
        $known[$lvl] += $m.Groups[3].Value
    }
    if ($known.Count -ne 4) { throw 'Could not read the stock lvl hashes from tools\lvl_build.cpp.' }
    return $known
}

function Get-Imports([string]$file) {
    $text = & (Find-Dumpbin) /nologo /dependents $file
    return @($text | Where-Object { $_ -match '^\s+(\S+\.dll)\s*$' } | ForEach-Object { $Matches[1].ToLower() })
}

function Get-PeMachine([string]$file) {
    $bytes = [IO.File]::ReadAllBytes($file)
    $pe = [BitConverter]::ToInt32($bytes, 0x3C)
    return [BitConverter]::ToUInt16($bytes, $pe + 4)
}

if (-not $NoBuild) {
    $msbuild = Find-MSBuild
    foreach ($platform in 'x64', 'Win32') {
        & $msbuild (Join-Path $root 'LuaCompiler.vcxproj') /p:Configuration=Release /p:Platform=$platform /m /v:minimal /nologo
        if ($LASTEXITCODE) { throw "The $platform LuaCompiler build failed." }
    }
    & $msbuild (Join-Path $root 'BattlefrontII.vcxproj') /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
    if ($LASTEXITCODE) { throw 'The x64 game build failed.' }
}

# package path -> source file
$files = [ordered]@{}
$files['BattlefrontII.exe'] = Join-Path $bin 'BattlefrontII.exe'
foreach ($dll in Get-ChildItem $bin -Filter '*.dll' | Sort-Object Name) {
    if ($dll.Name -match '^(avcodec|avformat|avutil|swresample|swscale)-\d+\.dll$') { $files[$dll.Name] = $dll.FullName }
}
$files['FFmpeg-LICENSE.txt'] = Join-Path $bin 'FFmpeg-LICENSE.txt'
$files['dxvk/d3d9.dll'] = Join-Path $bin 'dxvk\d3d9.dll'
$files['dxvk/LICENSE.txt'] = Join-Path $bin 'dxvk\LICENSE.txt'
$files['LuaCompiler.exe'] = $luac
foreach ($script in git -C $root ls-files -- 'data/scripts') {
    $files[$script] = Join-Path $root ($script -replace '/', '\')
}
$files['README.md'] = Join-Path $root 'README.md'

foreach ($path in $files.Keys) {
    if (-not (Test-Path -LiteralPath $files[$path] -PathType Leaf)) { throw "Missing $path ($($files[$path]))." }
    if ($path -match $retail) { throw "Refusing to package a retail file: $path" }
    if (-not ($allowed | Where-Object { $path -match $_ })) { throw "Not on the package allowlist: $path" }
}
if (@($files.Keys | Where-Object { $_ -match '^(avcodec|avformat|avutil|swresample|swscale)-' }).Count -ne 5) {
    throw "Expected the five FFmpeg DLLs in $bin."
}
if (-not $files.Contains('data/scripts/index.json')) { throw 'data/scripts/index.json is not tracked.' }

# The game and the lvl builder must be x64, and the lvl builder must run without the VC++ runtime.
if ((Get-PeMachine $files['BattlefrontII.exe']) -ne 0x8664) { throw 'BattlefrontII.exe is not an x64 build.' }
if ((Get-PeMachine $luac) -ne 0x8664) { throw 'LuaCompiler.exe is not an x64 build.' }
$crt = @(Get-Imports $luac | Where-Object { $_ -match '^(vcruntime|msvcp)\d+|^api-ms-win-crt-' })
if ($crt.Count) { throw "LuaCompiler.exe links the VC++ runtime dynamically ($($crt -join ', ')); rebuild it." }

# The x64 LuaCompiler writes the 32-bit tools' bytecode: check it against the Win32 build
if (-not (Test-Path -LiteralPath $luac32)) { throw "Missing the Win32 LuaCompiler ($luac32) to check the x64 one against." }
$check = Join-Path ([IO.Path]::GetTempPath()) "luac-check-$PID"
New-Item -ItemType Directory -Force $check | Out-Null
try {
    foreach ($script in $files.Keys | Where-Object { $_ -match '\.lua$' }) {
        foreach ($strip in $false, $true) {
            $outputs = foreach ($compiler in $luac, $luac32) {
                $out = Join-Path $check ([IO.Path]::GetRandomFileName())
                $arguments = @(if ($strip) { '-s' }) + @('-o', $out, $files[$script])
                & $compiler @arguments 2>$null
                if ($LASTEXITCODE) { throw "LuaCompiler could not compile $script." }
                (Get-FileHash -LiteralPath $out).Hash
            }
            if ($outputs[0] -ne $outputs[1]) { throw "The x64 and Win32 LuaCompilers compile $script differently." }
        }
    }
} finally {
    Remove-Item -LiteralPath $check -Recurse -Force -ErrorAction SilentlyContinue
}

$stage = Join-Path $OutDir $name
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }

$entries = @()
foreach ($path in $files.Keys) {
    $to = Join-Path $stage ($path -replace '/', '\')
    New-Item -ItemType Directory -Force (Split-Path $to -Parent) | Out-Null
    Copy-Item -LiteralPath $files[$path] -Destination $to
    $entries += [ordered]@{
        path = $path
        size = (Get-Item -LiteralPath $to).Length
        sha256 = (Get-FileHash -LiteralPath $to -Algorithm SHA256).Hash.ToLower()
    }
}

$commit = (git -C $root rev-parse HEAD).Trim()
$dirty = [bool](git -C $root status --porcelain --untracked-files=no)
$manifest = [ordered]@{
    schema = 1
    name = 'OpenSWBF2'
    version = $Version
    platform = 'x64'
    commit = $commit
    dirty = $dirty
    built = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    minLauncherVersion = $MinLauncherVersion
    exe = 'BattlefrontII.exe'
    lvlBuilder = 'LuaCompiler.exe'
    index = 'data/scripts/index.json'
    stockLvls = Get-StockLvlHashes
    files = $entries
}
$json = $manifest | ConvertTo-Json -Depth 6
[IO.File]::WriteAllText((Join-Path $stage 'manifest.json'), $json, (New-Object Text.UTF8Encoding $false))

# Forward-slash entry names, whatever the .NET Framework version does by default.
$archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    foreach ($path in @($files.Keys) + 'manifest.json') {
        $src = Join-Path $stage ($path -replace '/', '\')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $src, $path, 'Optimal') | Out-Null
    }
}
finally { $archive.Dispose() }

$sha = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
[IO.File]::WriteAllText("$zip.sha256", "$sha  $name.zip`n", (New-Object Text.ASCIIEncoding))

Write-Host ''
Write-Host ("{0}  {1:N1} MB  {2} files" -f $zip, ((Get-Item -LiteralPath $zip).Length / 1MB), ($files.Count + 1))
Write-Host "SHA-256 $sha"
if ($dirty) { Write-Host 'Warning: the working tree has uncommitted changes; the manifest records dirty = true.' }
Write-Host ''
Write-Host 'To publish (only when it has been tested):'
Write-Host "  gh release create v$Version `"$zip`" `"$zip.sha256`" --repo jt-marte/OpenSWBF2 --title `"OpenSWBF2 $Version`""
