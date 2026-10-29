# Star Wars: Battlefront II (2005) Decompiled

## Prerequisites

- Visual Studio 2013 (Update 5) with the v120_xp platform toolset
  https://visualstudio.microsoft.com/vs/older-downloads/
- DirectX SDK (June 2010), which sets DXSDK_DIR
  https://www.microsoft.com/en-us/download/details.aspx?id=6812

Or let `tools\setup.ps1` check and install them. The VS2013 web installer and the DirectX SDK installer no longer work on current systems, so the script installs VS2013 from the offline ISO and extracts the SDK headers and libs directly. It also checks that your GameData is unmodded (the 1.3 patch and Remaster replace lvls the build needs) and explains how to get a stock copy if not.

```
powershell -ExecutionPolicy Bypass -File tools\setup.ps1
```

Add `-CheckOnly` to only report what is missing.

## Build

```
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" BattlefrontII.sln /p:Configuration={Config} /p:Platform={Platform}
```

The exe is written to `bin\{Platform}\{Config}\BattlefrontII.exe`.

## Run

The build automatically finds the Steam install's GameData through Steam's registry entry, or set `SWBF2_GAMEDATA` to the desired GameData directory to use another copy.
