<div align="center">

# ⚔️ OpenSWBF2

### *Star Wars: Battlefront II (2005), decompiled, rebuilt and turned up to eleven.*

![Platforms](https://img.shields.io/badge/platform-Win32%20%7C%20x64-blue)
![Renderer](https://img.shields.io/badge/renderer-Vulkan%20(DXVK)%20%7C%20DX9-red)
![Toolchain](https://img.shields.io/badge/toolchain-VS2022%20v143-purple)
![Tick rate](https://img.shields.io/badge/server-60%20ticks%2Fs-brightgreen)
![AI](https://img.shields.io/badge/AI-Overhaul-orange)

A fork of [OpenSWBF2/OpenSWBF2](https://github.com/OpenSWBF2/OpenSWBF2) that builds a brand-new `BattlefrontII.exe` for the game data you already own.<br>
Sharper graphics, real controller support, bots that actually fight back, rebuilt online play and a native 64-bit build.

</div>

---

## ✨ At a glance

| | |
|---|---|
| 🖥️ **Looks great at 4K** | Crisp text, scaling menus, HUD scale and FOV sliders |
| 🌋 **Vulkan by default** | Through DXVK, with an automatic DirectX 9 fallback |
| 🎮 **Plug in a pad and play** | Native Xbox controllers with a Modern layout |
| 🤖 **Overhaul difficulty** | Smarter, fairer bots on both teams |
| 🌐 **Rebuilt multiplayer** | Direct Connect, dedicated servers, 60 ticks a second |
| 🚀 **64-bit build** | Plays with the 32-bit one in the same games |

---

## 🖥️ Graphics

- **Sharp at 1440p and 4K**: crisp text, sharp front-end menus, in-game menus that scale with the screen, and a correct reticle, HUD icons and scope lens at high resolutions.
- **HUD scale** (50–150%) and **field of view** (55–100°) in Options → Video.
- **Vulkan renderer** by default, through [DXVK](https://github.com/doitsujin/dxvk) 3.1.1, shipped in a `dxvk` folder beside the exe with its zlib licence (`dxvk\LICENSE.txt`).
  - Options → Video → Renderer switches to DirectX 9 from the next start.
  - `-vulkan` and `-d3d9` pick one for a single run.
  - When Vulkan can't start, the game quietly falls back to DirectX 9.
- New profiles start on the **highest video settings**.

## 🎮 Controller

- **Native Xbox (XInput) controllers**, no binding needed. Choose the **Modern** or **Legacy** layout in Options → Game; the Controller page lists every button.
- The **Modern** layout:

  | Do this | Press |
  |---|---|
  | Aim | LT |
  | Throw | RB |
  | Swap weapons | Y |
  | Reload / use | tap X / hold X |
  | Roll | double-tap B |
  | Lock on (or step the scope zoom) | click RS |

- Xbox button prompts in hints, a sensitivity slider, and pad navigation in the menus.

## 🤖 AI: the Overhaul difficulty

> *Normal, Elite… and then there's Overhaul.*

- A third choice after Normal and Elite. **Both teams** play one step above Elite with no handicaps, so your AI allies finally pull their weight.
- Bots pick targets and aim the same way at humans and bots, on foot, in vehicles and on turrets, instead of ganging up on you.
- Smarter tactics:
  - turning on whoever shoots them;
  - chasing enemies who duck out of sight;
  - varying their attack style;
  - falling back when badly hurt;
  - grenading enemies in cover;
  - spreading over command posts;
  - jinking in starfighters.
- It's host-side: stock players need nothing and see the server listed as Elite.

## 🌐 Multiplayer

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

## 🚀 The 64-bit build

- A **64-bit `BattlefrontII.exe`** beside the 32-bit one, for the same game data. 64-bit and 32-bit players join and host the same games, in any mix.
- Profiles work in both builds.
- **Movies play through FFmpeg**, since Bink has no 64-bit DLL (see [Third-party code](#-third-party-code)).
- For now it's **Modern multiplayer only**: Galaxy and Steam ship only 32-bit DLLs, so the Legacy server list isn't there. Voice chat is off in the 64-bit build until its codec is ported.

## 🧰 Tooling

- `tools\setup.ps1` installs the build tools and dependencies on a current PC.
- `tests\run.ps1` runs the unit, regression and smoke tests; `-Platform x64` runs them against the 64-bit build.
- `tests\netpair.ps1` pits a server and clients against each other with simulated lag, jitter and loss; `-ClientPlatform` mixes 32-bit and 64-bit peers.

---

## 🛠️ Setup

You need the **Visual Studio 2022 Build Tools** (or any VS2022 edition) with the C++ build tools for x86 and x64 (v143) and the **Windows SDK 10.0.26100.0**, plus the **DirectX SDK (June 2010)** for d3dx9. One script installs whatever is missing:

```
powershell -ExecutionPolicy Bypass -File tools\setup.ps1
```

It installs the VS2022 Build Tools with those components (accept the UAC prompt), extracts the DirectX SDK headers and libs, and fetches DXVK into `third_party\dxvk` and FFmpeg (for the 64-bit build) into `third_party\ffmpeg`. It also checks that your GameData is unmodded: the 1.3 patch and the Remaster replace files the build needs. Add `-CheckOnly` to only report what's missing.

The game needs **Windows 10 or later** and these runtimes, which aren't shipped beside the exe:

- Microsoft Visual C++ 2015-2022 Redistributable: x86 for the 32-bit build, x64 for the 64-bit build
- Visual C++ 2013 Redistributable (x86), for the 32-bit build's Galaxy DLLs
- DirectX End-User Runtime (June 2010), for `d3dx9_43.dll`

## 🏗️ Build and run

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" BattlefrontII.sln /p:Configuration=Release /p:Platform=Win32
```

| Build | Flag | Output |
|---|---|---|
| 32-bit | `/p:Platform=Win32` | `bin\Win32\Release\BattlefrontII.exe` |
| 64-bit | `/p:Platform=x64` | `bin\x64\Release\BattlefrontII.exe` |

- Use the MSBuild of whichever VS2022 edition is installed; `tests\vstools.ps1` finds it through `vswhere`.
- The build finds the Steam install's GameData through the registry; set `SWBF2_GAMEDATA` to use another copy.
- Each build keeps its own settings and `SaveGames` folder next to its exe.
- The toolchain moved from VS2013 to VS2022 without changing the 32-bit simulation. The few places where the newer compiler rounds floats differently are pinned, so matches and the test digests stay bit-identical (see `tests\baseline\v143\CAUSES.md`).

---

## 🛰️ Hosting

**In game:** Multiplayer → Create with the Modern source. Friends join from the Direct Connect tab with your address (port 3658). Players outside your network need UDP 3658 forwarded to you, or a VPN such as Tailscale.

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

---

## 📜 Third-party code

The x64 build plays the game's Bink movies with FFmpeg, since binkw32 only exists as a 32-bit DLL. It ships FFmpeg's LGPL DLLs (`avcodec`, `avformat`, `avutil`, `swscale`, `swresample`) unmodified next to `BattlefrontII.exe` and links them dynamically, so they can be swapped for another build of the same versions. The Win32 build still uses the game's `binkw32.dll`.

- Build: [BtbN FFmpeg-Builds](https://github.com/BtbN/FFmpeg-Builds) `ffmpeg-n8.1.3-27-g8dca1586f4-win64-lgpl-shared-8.1.zip` (release `autobuild-2026-10-30-13-01`), fetched by `tools\setup.ps1` into `third_party\ffmpeg`
- SHA-256: 21b96a5b382b031e89c1c91d7af8efc3db989c4c11bff27422a4c4ea24cf45ed
- Source: FFmpeg tag [n8.1](https://github.com/FFmpeg/FFmpeg/tree/n8.1); this build is commit 8dca1586f4 on its `release/8.1` branch
- License: LGPL version 3 or later (`third_party\ffmpeg\LICENSE.txt`)

<div align="center">

*May the frame rate be with you.* 🌌

</div>
