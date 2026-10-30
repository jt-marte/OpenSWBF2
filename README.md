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

## Multiplayer

**Direct connect.** In Multiplayer > Join, set the source to LAN: the list shows servers found on the local network, and **Join IP** joins any address (`1.2.3.4`, `1.2.3.4:3700` or a host name; the port defaults to 3658). Private, VPN (Tailscale, Hamachi), public and loopback addresses all work. Hosting from Create with the LAN source makes a server others can join the same way; players outside your network need UDP 3658 forwarded to you. `BattlefrontII.exe /autonet client -lan /connect <address>` joins headlessly. The Internet source is the Galaxy list, shared with the stock Steam game. Hosts pick 30, 60 (default) or 120 turns per second in Options > Online; the stock game ran 20, and how it copes with a faster host is untested.

**Dedicated server.** `BattlefrontII.exe -dedicated <map> [<map> ...]` starts a headless Direct server: no window, renderer or sound, no Steam sign-in needed, logs to stdout (and to the parent console when started from one). Maps use mission names such as `geo1c_con`, optionally followed by both teams' reinforcements (`geo1c_con 150 150`).

| Switch | Meaning |
|--------|---------|
| `-port N` | game port, UDP (default 3658; LAN discovery stays on 3656) |
| `/gamename "Name"` | server name (default `OpenSWBF2 Server`) |
| `/playerlimit N` | player limit, 1 to 64 |
| `/password text` | join password |
| `/bots N` | AI units per team |
| `/tps N` | simulation turns per second: 30, 60 (default) or 120; clients get as many updates a second as the turn rate, so 120 suits LAN and high-upload servers |
| `/configdir dir` | folder holding `ServerSettings.cfg` and `ServerRotation.cfg` (default `settings`); each line is an admin command such as `/gamename My Server` or `/addmap geo1c_con` |
| `-console` | open a console window for the log when there is no stdout |
| `-netstats` | log ping, updates per second and bandwidth once a second |
| `-log file` | also write the log to a file |

Ctrl+C, Ctrl+Break or closing the console stops the server cleanly. The command line overrides the config files.
