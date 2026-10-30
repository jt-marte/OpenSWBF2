# OpenSWBF2

**Star Wars: Battlefront II (2005), decompiled and modernized.** This fork of [OpenSWBF2/OpenSWBF2](https://github.com/OpenSWBF2/OpenSWBF2) builds a new `BattlefrontII.exe` for the stock game data, with sharper graphics, native controller support, smarter AI and reworked online play.

## What's new in this fork

### Graphics
- **Sharp at 1440p and 4K**: crisp text, sharp front-end menus, in-game menus that scale with the screen, and a correct reticle, HUD icons and scope lens at high resolutions.
- **HUD scale** (50–150%) and **field of view** (55–100°) in Options → Video.
- **Vulkan renderer** by default, through [DXVK](https://github.com/doitsujin/dxvk) 3.1.1, shipped in a `dxvk` folder beside the exe with its zlib licence (`dxvk\LICENSE.txt`). Options → Video → Renderer switches to DirectX 9 from the next start, `-vulkan` and `-d3d9` pick one for a single run, and the game falls back to DirectX 9 by itself when Vulkan can't start.
- New profiles start on the **highest video settings**.

### Controller
- **Native Xbox (XInput) controllers**, no binding needed. Choose the **Modern** or **Legacy** layout in Options → Game; the Controller page lists every button.
- **Modern** layout:
  - aim on LT, throw on RB, swap weapons on Y;
  - tap X to reload, hold X to use;
  - double-tap B to roll;
  - click RS to lock on, or step the zoom while scoped.
- Xbox button prompts in hints, a sensitivity slider, and pad navigation in the menus.

### AI: the Overhaul difficulty
- A third choice after Normal and Elite. Both teams play one step above Elite with no handicaps, so your AI allies match the enemy.
- Bots pick targets and aim the same way at humans and bots, on foot, in vehicles and on turrets, instead of ganging up on the player.
- Smarter tactics:
  - turning on whoever shoots them;
  - chasing enemies who duck out of sight;
  - varying their attack style;
  - falling back when badly hurt;
  - grenading enemies in cover;
  - spreading over command posts;
  - jinking in starfighters.
- Host-side: stock players need nothing and see the server listed as Elite.

### Multiplayer
- **Modern and Legacy** server sources on Join and Create. Modern is OpenSWBF2's own path (LAN and direct joins); Legacy is the Galaxy list shared with the stock game.
- **Direct Connect** tab: join any IP or host name, with a password box and your last eight servers one click away.
- **Dedicated server**: `-dedicated` runs headless, with no window, sound or Steam sign-in.
- **60 ticks a second by default** (30, 60 or 120; the stock game ran 20), with host bandwidth options past 6 Mbit.
- **Netcode retuned for those tick rates**:
  - a 150 ms window for late inputs;
  - an input margin that adapts to jitter;
  - shots no longer dropped from full updates;
  - updates kept to one packet.
- The Galaxy account, Friends and Leaderboard tabs are gone.

### 64-bit build
- A **64-bit `BattlefrontII.exe`** beside the 32-bit one, for the same game data. It joins and hosts the same games as the 32-bit build, in either direction.
- **Modern multiplayer only** for now: Galaxy and Steam ship only 32-bit DLLs, so the Legacy server list is not there. Voice chat is off in the 64-bit build until its codec is ported.
- **Movies play through FFmpeg**, since Bink has no 64-bit DLL (see Third-party code).

### Tooling
- `tools\setup.ps1` installs the 2013-era toolchain on a current PC.
- `tests\run.ps1` runs the unit, regression and smoke tests; `-Platform x64` runs them against the 64-bit build.
- `tests\netpair.ps1` plays a server and clients against each other with simulated lag, jitter and loss; `-ClientPlatform` mixes 32-bit and 64-bit peers.

## Setup

You need Visual Studio 2013 (Update 5) with the v120_xp toolset and the DirectX SDK (June 2010). Their old installers no longer run on current systems, so let the script handle it:

```
powershell -ExecutionPolicy Bypass -File tools\setup.ps1
```

It installs VS2013 from the offline ISO, extracts the SDK headers and libs, and fetches DXVK into `third_party\dxvk` and FFmpeg (for the 64-bit build) into `third_party\ffmpeg`. It also checks that your GameData is unmodded: the 1.3 patch and the Remaster replace files the build needs. Add `-CheckOnly` to only report what is missing.

## Build and run

```
"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild.exe" BattlefrontII.sln /p:Configuration=Release /p:Platform=Win32
```

The game is built to `bin\Win32\Release\BattlefrontII.exe`. The build finds the Steam install's GameData through the registry; set `SWBF2_GAMEDATA` to use another copy.

For the 64-bit build use `/p:Platform=x64`; it goes to `bin\x64\Release\BattlefrontII.exe`. Each build keeps its own settings and `SaveGames` folder next to its exe.

## Hosting

**In game:** Multiplayer → Create with the Modern source. Others join from the Direct Connect tab with your address (port 3658). Players outside your network need UDP 3658 forwarded to you, or a VPN such as Tailscale.

**Dedicated server:** `BattlefrontII.exe -dedicated <map> [<map> ...]`, with mission names such as `geo1c_con`, optionally followed by both teams' reinforcements (`geo1c_con 150 150`). It logs to the console; Ctrl+C stops it cleanly.

| Switch | Meaning |
|--------|---------|
| `-port N` | game port, UDP (default 3658) |
| `/gamename "Name"` | server name |
| `/playerlimit N` | player limit, 1 to 64 |
| `/password text` | join password |
| `/bots N` | AI units per team |
| `/tps N` | ticks a second: 30, 60 (default) or 120 |
| `/difficulty N` | AI difficulty: 2 Normal, 3 Elite, 4 Overhaul |
| `/configdir dir` | folder with `ServerSettings.cfg` and `ServerRotation.cfg`: one admin command per line, such as `/addmap geo1c_con` |
| `-console` | open a console window for the log |
| `-netstats` | log ping, updates and bandwidth once a second |
| `-netloopback` | listen on 127.0.0.1 only: just this PC can join, by Direct Connect to 127.0.0.1 |
| `-log file` | also write the log to a file |

The command line overrides the config files.

## Third-party code

The x64 build plays the game's Bink movies with FFmpeg, since binkw32 only exists as a 32-bit DLL. It ships FFmpeg's LGPL DLLs (`avcodec`, `avformat`, `avutil`, `swscale`, `swresample`) unmodified next to `BattlefrontII.exe` and links them dynamically, so they can be swapped for another build of the same versions. The Win32 build still uses the game's `binkw32.dll`.

- Build: [BtbN FFmpeg-Builds](https://github.com/BtbN/FFmpeg-Builds) `ffmpeg-n8.1.3-27-g8dca1586f4-win64-lgpl-shared-8.1.zip` (release `autobuild-2026-10-30-13-01`), fetched by `tools\setup.ps1` into `third_party\ffmpeg`
- SHA-256: 21b96a5b382b031e89c1c91d7af8efc3db989c4c11bff27422a4c4ea24cf45ed
- Source: FFmpeg tag [n8.1](https://github.com/FFmpeg/FFmpeg/tree/n8.1); this build is commit 8dca1586f4 on its `release/8.1` branch
- License: LGPL version 3 or later (`third_party\ffmpeg\LICENSE.txt`)
