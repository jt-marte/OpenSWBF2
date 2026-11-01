<div align="center">

# ⚔️ OpenSWBF2

*Star Wars: Battlefront II (2005), decompiled and rebuilt.*

![Platforms](https://img.shields.io/badge/platform-Win32%20%7C%20x64-blue)
![Renderer](https://img.shields.io/badge/renderer-Vulkan%20(DXVK)%20%7C%20DX9-red)
![Toolchain](https://img.shields.io/badge/toolchain-VS2022%20v143-purple)

A fork of [OpenSWBF2/OpenSWBF2](https://github.com/OpenSWBF2/OpenSWBF2): a new `BattlefrontII.exe`
for the Steam or GOG copy you already own.

</div>

## ✨ Features

- **Graphics:** sharp text and menus at 1440p and 4K, HUD scale and FOV sliders, resolution and
  display mode in the in-match options. No frame cap by default, as in the retail game (Options →
  Video → Frame cap sets one). Vulkan through [DXVK](https://github.com/doitsujin/dxvk) by
  default, falling back to DirectX 9 (Options → Video, or `-vulkan` / `-d3d9`).
- **Controllers:** Xbox (XInput) and PS4/PS5 pads with no binding. A Modern layout (LT aim, RB
  throw, click RS to lock on) or the Legacy one. In both, tap Back (Create on PlayStation) for the
  player list, or hold it for the map.
- **Overhaul AI:** a difficulty above Elite, for both teams. Bots aim at and pick targets among
  humans and bots alike, chase, grenade enemies in cover, fall back when hurt and spread over
  command posts. It's host-side, so stock players need nothing.
- **Multiplayer:** Direct Connect to any address, headless dedicated servers, 30, 60 or 120 ticks a
  second (stock: 20) with retuned netcode, rcon admin, and AI heroes when no player takes the hero.
- **Mods:** built-in 1.3-style support (every era and mode, user scripts) without the 1.3 patch.
- **64-bit build:** plays with the 32-bit one in the same games; movies play through FFmpeg. Modern
  multiplayer only, and no voice chat yet.

## 💿 Your game copy

Steam or GOG v1.1, with the stock `shell`, `common`, `ingame` and `mission.lvl` (the 1.3 patch and
the Remaster replace them). OpenSWBF2 rebuilds those four with its own scripts and changes nothing
else. GOG's copy lacks two of the scripts, which are added from source; GOG and Steam players play
together. The 32-bit build also needs Steam's `Galaxy.dll` and `steam_api.dll`, so a GOG copy runs
the 64-bit build.

## 🛠️ Build

You need the VS2022 Build Tools (v143, x86 and x64), the Windows SDK 10.0.26100.0 and the DirectX SDK
(June 2010). `tools\setup.ps1` installs them, fetches DXVK and FFmpeg into `third_party`, and checks
your game copy (`-CheckOnly` only reports):

```
powershell -ExecutionPolicy Bypass -File tools\setup.ps1
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" BattlefrontII.sln /p:Configuration=Release /p:Platform=x64
```

- The output is `bin\x64\Release`, or `bin\Win32\Release` with `/p:Platform=Win32`. Each build keeps
  its own settings and `SaveGames` there.
- The build finds Steam's GameData through the registry. Set `SWBF2_GAMEDATA` for another copy, such
  as GOG's `C:\GOG Games\Star Wars - Battlefront 2\GameData`.
- To run it you need Windows 10 or later, the VC++ 2015-2022 Redistributable (x64 or x86), the VC++
  2013 Redistributable (x86, for the 32-bit Galaxy DLLs) and the DirectX End-User Runtime (June 2010).
- Tests: `tests\run.ps1 [-Platform x64]`. `tests\netpair.ps1` pairs a server and clients under
  simulated lag; `-ClientPlatform` and `-ClientExeDir` mix builds and game copies.
- The move from VS2013 to VS2022 kept the 32-bit simulation bit-identical
  (`tests\baseline\v143\CAUSES.md`).

## 🛰️ Hosting

- **In game:** Multiplayer → Create, with the Modern source. Friends join from Direct Connect with
  your address. Players outside your network need UDP 3658 forwarded, or a VPN such as Tailscale.
- **Dedicated:** `BattlefrontII.exe -dedicated <map> [<map> ...]`, such as `geo1c_con`, optionally
  followed by both teams' reinforcements (`geo1c_con 150 150`). Ctrl+C stops it.
- **Docker:** `ghcr.io/jt-marte/openswbf2-server` on any Linux box; see
  [docker/README.md](docker/README.md).

| Switch | Meaning |
|--------|---------|
| `-port N` | game port, UDP (default 3658) |
| `/gamename "Name"` | server name |
| `/playerlimit N` | player limit, 1 to 64 |
| `/password text` | join password |
| `/adminpw text` | rcon admin password |
| `/bots N` | AI units per team |
| `/tps N` | ticks a second: 30, 60 (default) or 120 |
| `/difficulty N` | AI difficulty: 2 Normal, 3 Elite, 4 Overhaul |
| `/configdir dir` | folder with `ServerSettings.cfg` and `ServerRotation.cfg`, one admin command per line |
| `-lobby` | open each map in a lobby until an admin starts it |
| `-console` | open a console window for the log |
| `-netstats` | log ping, updates and bandwidth once a second |
| `-netloopback` | listen on 127.0.0.1 only |
| `-log file` | also write the log to a file |

The command line overrides the config files.

## 📜 Third-party code

- **DXVK** 3.1.1, in the `dxvk` folder beside the exe, under its zlib licence (`dxvk\LICENSE.txt`).
- **FFmpeg**, for the 64-bit build's movies, since Bink only exists as a 32-bit DLL. Its LGPL DLLs
  (`avcodec`, `avformat`, `avutil`, `swscale`, `swresample`) ship unmodified beside the exe and are
  linked dynamically, so they can be swapped for another build of the same versions.
  - Build: [BtbN FFmpeg-Builds](https://github.com/BtbN/FFmpeg-Builds)
    `ffmpeg-n8.1.3-27-g8dca1586f4-win64-lgpl-shared-8.1.zip` (release `autobuild-2026-10-30-13-01`),
    SHA-256 21b96a5b382b031e89c1c91d7af8efc3db989c4c11bff27422a4c4ea24cf45ed
  - Source: FFmpeg tag [n8.1](https://github.com/FFmpeg/FFmpeg/tree/n8.1); this build is commit
    8dca1586f4 on its `release/8.1` branch
  - License: LGPL version 3 or later (`third_party\ffmpeg\LICENSE.txt`)
