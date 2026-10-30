$script:VsTools = $null
$T1Runtimes = 'd3dx9_43.dll', 'vcruntime140.dll', 'msvcp140.dll'
$GameRuntimes = $T1Runtimes + 'msvcr120.dll', 'msvcp120.dll'

function Get-VsTools {
    if ($script:VsTools) { return $script:VsTools }
    $vs = $null
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $vs = & $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
    }
    $tools = $null
    if ($vs) {
        $verFile = Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'
        if (Test-Path -LiteralPath $verFile) {
            $ver = (Get-Content -LiteralPath $verFile | Select-Object -First 1).Trim()
            $tools = New-Object PSObject -Property @{
                Install = $vs
                ToolsVersion = $ver
                Bin = Join-Path $vs "VC\Tools\MSVC\$ver\bin\HostX86\x86"
                MSBuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
            }
        }
    }
    if (-not $tools -or -not (Test-Path -LiteralPath $tools.Bin) -or -not (Test-Path -LiteralPath $tools.MSBuild)) {
        Write-Host 'Visual Studio 2022 with the C++ x86/x64 build tools not found.'
        exit 2
    }
    $script:VsTools = $tools
    return $tools
}

function Find-MSBuild { return (Get-VsTools).MSBuild }

function Find-VcBin { return (Get-VsTools).Bin }

function Find-Dumpbin { return (Join-Path (Find-VcBin) 'dumpbin.exe') }

function Find-Editbin { return (Join-Path (Find-VcBin) 'editbin.exe') }

function Get-MissingRuntimes([string]$dir, [string[]]$dlls) {
    return @($dlls | Where-Object { -not (Test-Path -LiteralPath (Join-Path $dir $_)) })
}

# $dir: SysWOW64 for the Win32 build, System32 for x64 (Get-RuntimeDllDir in smoke.ps1)
function Assert-Runtimes([string[]]$dlls, [string]$dir = (Join-Path $env:windir 'SysWOW64')) {
    $missing = @(Get-MissingRuntimes $dir $dlls)
    if ($missing.Count -gt 0) {
        Write-Host "Preflight failed: missing from $(Split-Path $dir -Leaf): $($missing -join ', '). Install the DirectX June 2010 redist, the VC++ 2015-2022 redistributable for this platform and (Win32) the VS2013 x86 runtime."
        exit 2
    }
}

function Write-ToolBanner {
    $t = Get-VsTools
    $cl = Join-Path $t.Bin 'cl.exe'
    $banner = @(& cmd /c "`"$cl`" 2>&1" | Where-Object { $_ -and $_.Trim() }) | Select-Object -First 1
    Write-Host "toolchain: $banner (MSVC $($t.ToolsVersion))"
    Write-Host "ucrtbase: $((Get-Item -LiteralPath (Join-Path $env:windir 'SysWOW64\ucrtbase.dll')).VersionInfo.FileVersion)"
    $nt = Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
    Write-Host "windows: $($nt.CurrentBuild).$($nt.UBR)"
}
