# OpenSWBF2 dedicated server in Docker

Runs the OpenSWBF2 dedicated server headless on Linux. The image is `ghcr.io/jt-marte/openswbf2-server`,
built from each OpenSWBF2 release by `.github/workflows/docker.yml`. It holds only OpenSWBF2's own
files, run under Wine. The game data is yours: you mount your copy of Star Wars Battlefront II (2005).

It runs on any x86-64 machine with Docker: any Linux distribution, a NAS, a cloud server, or Docker
Desktop on Windows or an Intel Mac. ARM machines (Raspberry Pi, Apple Silicon) can't run it, since
Wine runs x86 code. Everything inside is 64-bit, so Docker's default security settings work as they are.

## 1. Copy your game data to the server

On the server, make a folder for it:

```bash
mkdir -p ~/openswbf2/gamedata ~/openswbf2/addons ~/openswbf2/config
sudo chown -R 1000:1000 ~/openswbf2/config   # the container runs as uid 1000
```

From your Windows PC, copy your game's `GameData\data` folder (about 13 GB) into `gamedata`:

```powershell
scp -r "C:\Program Files (x86)\Steam\steamapps\common\Star Wars Battlefront II Classic\GameData\data" user@server:~/openswbf2/gamedata/
```

The four core files (`shell`, `common`, `ingame`, `mission.lvl` in `data\_lvl_pc`) must be the
original Steam or GOG ones. On first start the container rebuilds them for its OpenSWBF2 version,
the same way the OpenBattlefront launcher does. Script mods in there (`user_script_*`,
`custom_gc_*`) are left out unless `SCRIPT_MODS=1`.

For the mod maps, copy the mod folders into `addons`, for example the Battlefront Conversion Pack
and the Xbox DLC that come in the OpenBattlefront zip (`Content\AddOn\BF1` and `Content\AddOn\DLC_XBOX`):

```powershell
scp -r Content\AddOn\BF1 Content\AddOn\DLC_XBOX user@server:~/openswbf2/addons/
```

Players need the same mods to join a mod map. With the OpenBattlefront launcher they get them from
the server itself when they join (see [Mod downloads](#mod-downloads)).

## 2. Start it

Put `compose.yaml` in `~/openswbf2`, set at least `ADMIN_PASSWORD`, then:

```bash
cd ~/openswbf2
docker compose up -d
docker compose logs -f
```

The first start builds the core files (a minute or two, cached in `config/lvl` after that), then
logs `OPENSWBF2 NET mission start ... map=...`. Players join from the OpenBattlefront launcher's
**Play > Join a server**, or in the game from **Multiplayer > Direct Connect**, with `your-address:3658`.

## Settings

| Variable | Default | |
|---|---|---|
| `PORT` | `3658` | UDP port for players and admin. Map the same number in `ports`. |
| `SERVER_NAME` | `OpenSWBF2 Server` | Up to 31 characters. |
| `ADMIN_PASSWORD` | generated | 6-31 letters or digits. Empty: one is generated into `config/admin-password`. |
| `JOIN_PASSWORD` | empty | Up to 15 characters, no spaces. Empty means anyone can join. |
| `MAPS` | 8 stock Conquest maps | Mission names, space-separated (`geo1c_con`, `BS1k_con`, ...). |
| `MAX_PLAYERS` | `32` | 2-64. |
| `TICK_RATE` | `60` | 30, 60 or 120 turns a second. |
| `BOTS_PER_TEAM` | `16` | 0-32. |
| `DIFFICULTY` | `3` | 2 Medium, 3 Hard, 4 Overhaul. |
| `HEROES` / `AI_HEROES` | `1` / `1` | Heroes on; a bot takes the hero nobody takes within 30 s. |
| `HERO_UNLOCK`, `HERO_UNLOCK_VALUE`, `HERO_PLAYER` | `3`, `10`, `1` | Unlock by points (3) or time (5); 1 best player, 7 random. |
| `TEAM_DAMAGE`, `AUTO_ASSIGN_TEAMS`, `SHOW_NAMES`, `AWARDS` | `1` | |
| `CONQUEST_REINFORCEMENTS` | `100` | Percent, 10-500. |
| `CONQUEST_TIME_LIMIT`, `CTF_TIME_LIMIT`, `HUNT_TIME_LIMIT` | `0` | Minutes, 0 = none. |
| `CTF_CAPTURES`, `HUNT_SCORE`, `ASSAULT_SCORE` | `5`, `50`, `180` | |
| `LOBBY` | `0` | 1 = each map waits in a lobby until an admin sends `/start`. |
| `SCRIPT_MODS` | `0` | 1 = also load the `user_script_*` / `custom_gc_*` mods from your data. |
| `MOD_SHARING` | `1` | Players' launchers download the server's mods from it. 0 = off. |
| `MOD_PORT` | `PORT` | TCP port for mod downloads. Map the same number in `ports`, as `/tcp`. |
| `MOD_RATE` | `50` | Download speed cap in Mbit/s, for all downloads together. 0 = none. |
| `MOD_UPLOADS` | `1` | Admins can upload mods from the launcher. 0 = off. |
| `EXTRA_SETTINGS` | | More admin commands for `ServerSettings.cfg`, one per line. |
| `EXTRA_ARGS` | | More game switches. |

XL and Uber missions are single player only: their 320-560 soldiers overload players' connections.

## Admin

From the server, with the `rcon` tool in the image:

```bash
docker exec openswbf2 rcon /status
docker exec openswbf2 rcon "/password secret"     # change the join password now ("/password" alone clears it)
docker exec openswbf2 rcon "/changemap nab2c_con" "/kick 3" "/maplist"
```

In the game, type in chat: `/admin /login <ADMIN_PASSWORD>`, then commands such as
`/admin /password secret`. Changes made this way last until the container restarts; the
environment settings apply again then.

## Mod downloads

Players who join with the OpenBattlefront launcher (**Play > Join a server**) get the server's
mods from the server itself, like Source's FastDL. The launcher shows what they lack and its size,
asks once, downloads it (picking up where it stopped if the connection drops), checks every file's
SHA-256, installs it and starts the game straight into the server. Their routers need nothing.

- **What is shared:** the mod folders the game loads: those in `addons` and the uploaded ones in
  `config/addons`. Programs (`.exe .dll .bat .cmd .com .scr .msi .ps1 .vbs`) and the unofficial 1.3
  patch are never shared. Script mods in `data/_lvl_pc` aren't either.
- **Who can download:** whoever can join. With a `JOIN_PASSWORD`, the launcher asks for it (the
  admin password works too). Changing the join password live with `/password` doesn't change it
  for downloads; `JOIN_PASSWORD` does, from the next start.
- **Speed:** all downloads share `MOD_RATE` Mbit/s (50 by default), which leaves room for the game
  on a home connection. Each file is hashed once; the hashes are kept in `config/mod-hashes.json`.
- **Port:** TCP on the game's port number, next to the game's UDP. Map both in `ports`, as
  compose.yaml does.

### Uploading mods

In the launcher, **Server manager > Connect to server as admin** with the server's address and the
admin password lists the server's mods next to yours. **Upload** sends one the server lacks, or
has another version of. It goes into `config/addons` and loads when the game restarts:

- **Restart now:** players are dropped and can rejoin straight away.
- **Restart when empty:** the server restarts the next time nobody is on.

Only the game restarts; the container and the downloads keep running. Uploaded mods can be removed
the same way. The ones in your `addons` folder can't: change those on the host.

An upload is a mod folder with an `addme.script`, up to 2 GB a file and 8 GB in all, with no
programs, and never the 1.3 patch. A new mod's maps still have to be in the rotation: add them to
`MAPS`, or over rcon with `/appendmap`. `MOD_UPLOADS=0` turns uploads off and `MOD_SHARING=0` the
whole service.

## Updating

```bash
docker compose pull && docker compose up -d
```

A new OpenSWBF2 version rebuilds the core files once. Players need the same OpenSWBF2 version
(the OpenBattlefront launcher updates itself to the latest release).

## Opening it to the internet

Forward `3658` (or your `PORT`) on your router to the server, as **UDP and TCP** (many routers
have a "TCP/UDP" or "Both" choice). UDP carries the game and admin; TCP carries the mod downloads.
Without the TCP part players can still join, but the launcher can't bring them the mods.

Is that safe enough? It is a reasonable setup for a home server, with one caveat:

- **One port number, password-protected admin.** Admin commands need the admin password; logins
  are limited to 5 per 10 seconds per address, and the session key changes as it goes. Use a long
  random admin password, and a join password for private games.
- **Mod downloads are plain HTTP.** As with admin commands, passwords travel unencrypted. The
  service only sends the listed mod files and only writes uploads under `config/addons`; it takes
  32 connections at most (4 per address), and 5 wrong passwords in 10 seconds lock an address out
  for 30 seconds.
- **The caveat: this is a 2005 game engine,** rebuilt from the original. Its network readers were
  hardened and fuzz-tested, but it has not been security-audited like a modern server. So the
  container is the safety net:
  - it runs as an unprivileged user, with no capabilities and `no-new-privileges`;
  - your game data is mounted read-only;
  - memory, CPU and process counts are capped;
  - it uses Docker's normal network (not host networking).
- **Keep it current:** `docker compose pull` picks up fixes.
- **Friends only?** A VPN such as Tailscale lets them join without opening a port at all.

## Troubleshooting

- `no /gamedata/data/_lvl_pc/core.lvl`: the `gamedata` folder must contain the `data` folder itself.
- `building the core game files failed`: the four core lvls aren't the original ones (a patch or
  mod replaced them). Copy them from a clean install.
- `/config is not writable`: `sudo chown -R 1000:1000 ~/openswbf2/config`.
- The launcher says the server doesn't share mods: check the TCP mapping and forward, then
  `curl -H "Authorization: Bearer <JOIN_PASSWORD>" http://your-address:3658/mods.json` from outside.
  The log names any mod folder or file left out, and why.
- Logs: `docker compose logs`, and `config/logs/server.log`.
