# OpenSWBF2 dedicated server in Docker

Runs the OpenSWBF2 dedicated server headless on Linux. The image is `ghcr.io/jt-marte/openswbf2-server`,
built from each OpenSWBF2 release by `.github/workflows/docker.yml`. It holds only OpenSWBF2's own
files, run under Wine. The game itself is yours: on its first start the server downloads it from
your own Steam account, or you give it your own copy.

It runs on any x86-64 machine with Docker: any Linux distribution, a NAS, a cloud server, or Docker
Desktop on Windows or an Intel Mac. ARM machines (Raspberry Pi, Apple Silicon) can't run it, since
Wine runs x86 code. Everything inside is 64-bit, so Docker's default security settings work as they are.

## 1. Start it

On the server, in an empty folder, get `compose.yaml`, set `ADMIN_PASSWORD` in it (or leave it
empty to have one generated), and start it:

```bash
mkdir -p ~/openswbf2 && cd ~/openswbf2
curl -fsSLO https://raw.githubusercontent.com/jt-marte/OpenSWBF2/main/docker/compose.yaml
docker compose up -d
```

## 2. Log in to Steam, once

The server needs STAR WARS Battlefront II (Classic, 2005) from a Steam account that owns it. Log
in once:

```bash
docker compose exec openswbf2 steam-login
```

It shows a QR code: in the Steam app on your phone, open **Steam Guard** (the shield) and choose
**Scan a QR code**. Your password never touches the server; only a login token is kept, in the
`config` folder next to `compose.yaml`. (`steam-login --password` asks for the account name, password and Steam Guard
code instead.)

The server then downloads the game's data (about 13 GB, once), checks the core files are the
originals, builds OpenSWBF2's versions of them and starts:

```bash
docker compose logs -f
```

Once it logs `OPENSWBF2 NET mission start ... map=...`, players join from the OpenBattlefront
launcher's **Multiplayer > Direct Connect**, or in the game from **Multiplayer > Direct Connect**,
with `your-address:3658`. Restarts and updates (`docker compose pull && docker compose up -d`)
start straight away: the download and the built files are kept.

The download uses [DepotDownloader](https://github.com/SteamRE/DepotDownloader) (GPL-2.0), a
64-bit Steam client: Valve's own `steamcmd` is 32-bit, and Docker's default security settings keep
32-bit programs offline.

### Or use your own copy

Instead of Steam, you can give the server the `data` folder of an install you have (Steam or GOG;
the four core files `shell`, `common`, `ingame` and `mission.lvl` must be the originals). Uncomment
the `./gamedata` line in `compose.yaml`, then from your Windows PC:

```powershell
scp -r "C:\Program Files (x86)\Steam\steamapps\common\Star Wars Battlefront II Classic\GameData\data" user@server:~/openswbf2/gamedata/
```

The server finds `data/_lvl_pc` even a folder or two deeper than that (a whole `GameData` copied
in, say) and says where it found it. Script mods in it (`user_script_*`, `custom_gc_*`) are left
out unless `SCRIPT_MODS=1`.

### Mods

For the mod maps, copy the mod folders into `addons` next to `compose.yaml`, for example the
Battlefront Conversion Pack and the Xbox DLC that come in the OpenBattlefront zip
(`Content\AddOn\BF1` and `Content\AddOn\DLC_XBOX`):

```powershell
scp -r Content\AddOn\BF1 Content\AddOn\DLC_XBOX user@server:~/openswbf2/addons/
```

Players need the same mods to join a mod map. With the OpenBattlefront launcher they get them from
the server itself when they join (see [Mod downloads](#mod-downloads)).

## Settings

compose.yaml holds only what the server needs to start. Everything you can change from the launcher
is the server's own (see [Admin](#admin)).

| Variable | Default | |
|---|---|---|
| `PORT` | `3658` | UDP port for players and admin. Map the same number in `ports`. |
| `ADMIN_PASSWORD` | generated | 6-31 letters or digits. Empty: one is generated into `config/admin-password`. |
| `MOD_SHARING` | `1` | Players' launchers download the server's mods from it. 0 = off. |
| `MOD_PORT` | `PORT` | TCP port for mod downloads. Map the same number in `ports`, as `/tcp`. |
| `MOD_RATE` | `50` | Download speed cap in Mbit/s, for all downloads together. 0 = none. |
| `MOD_UPLOADS` | `1` | Admins can upload mods from the launcher. 0 = off. |
| `SCRIPT_MODS` | `0` | 1 = also load the `user_script_*` / `custom_gc_*` mods from your data. |
| `EXTRA_SETTINGS` | | More admin commands for `ServerSettings.cfg`, one per line. They come last, so they win over the game options. |
| `EXTRA_ARGS` | | More game switches. |

A new server starts with:
- the name "OpenSWBF2 Server", no join password, up to 32 players and 60 turns a second;
- the 8 stock Conquest maps (Geonosis, Kashyyyk, Naboo, Hoth, Tatooine, Coruscant, Mygeeto, Utapau);
- 16 bots a team, Hard AI, heroes on, and the game's usual limits;
- no lobby: each map starts at once.

Change any of it from the launcher's admin screen.

**Upgrading.** Up to image 0.2.4, compose.yaml set these with variables such as `MAPS`, `BOTS_PER_TEAM` and
`SERVER_NAME`. At its first start, the new image takes those values into the server's settings,
once. After that they're only named in the log as no longer used, and they can go from compose.yaml.

XL and Uber missions are single player only: their 320-560 soldiers overload players' connections.

## Admin

### From the launcher

**Server manager > Connect to server as admin**, with the server's address and admin password, opens
the same screen as **Host a server**:
- the playlist, built from the server's own map list (mods included, even ones you don't have);
- Start match, End match, Next map, Restart map, Back to lobby, and ▶ on a map to play it now;
- the players, to message, move to the other team or kick;
- chat, and the game options;
- the server's name, join password, player limit, tick rate and lobby;
- the server's mods.

Admin commands travel over UDP on the game port, like the game. The mod service, on TCP at the same port
number, keeps the settings and lists the maps.

What you apply is kept, through restarts and image updates.
- The playlist and the game options take effect at once.
- The name, join password, player limit, tick rate and lobby take a restart of the game:
  **Restart now** or **Restart when empty**.
- **Back to the defaults** on the admin screen forgets it all.

The settings are in `config/settings/admin.json`. The server's map list is `config/settings/missions.json`.
The game writes that list at the first start, and again whenever the mods change. It takes a few seconds.

### From the command line

With the `rcon` tool in the image:

```bash
docker exec openswbf2 rcon /status
docker exec openswbf2 rcon "/password secret"     # a join password until the game restarts
docker exec openswbf2 rcon "/changemap nab2c_con" "/kick 3" "/maplist"
```

In the game, type in chat: `/admin /login <ADMIN_PASSWORD>`, then commands such as
`/admin /password secret`. Changes made this way last until the game restarts. Only the launcher's
admin screen keeps its changes.

## Mod downloads

Players who join with the OpenBattlefront launcher (**Multiplayer > Direct Connect**) get the
server's mods from the server itself, like Source's FastDL. The launcher shows what they lack and its size,
asks once, downloads it (picking up where it stopped if the connection drops), checks every file's
SHA-256, installs it and starts the game straight into the server. Their routers need nothing.

- **What is shared:** the mod folders the game loads: those in `addons` and the uploaded ones in
  `/config/addons`. Programs (`.exe .dll .bat .cmd .com .scr .msi .ps1 .vbs`) and the unofficial 1.3
  patch are never shared. Script mods in `data/_lvl_pc` aren't either.
- **Who can download:** whoever can join. With a join password, the launcher asks for it (the
  admin password works too). Downloads take the join password the game runs with.
- **Speed:** all downloads share `MOD_RATE` Mbit/s (50 by default), which leaves room for the game
  on a home connection. Each file is hashed once; the hashes are kept in `/config/mod-hashes.json`.
- **Port:** TCP on the game's port number, next to the game's UDP. Map both in `ports`, as
  compose.yaml does.

### Uploading mods

In the launcher, **Server manager > Connect to server as admin** with the server's address and the
admin password lists the server's mods next to yours. **Upload** sends one the server lacks, or
has another version of. It goes into `/config/addons` and loads when the game restarts:

- **Restart now:** players are dropped and can rejoin straight away.
- **Restart when empty:** the server restarts the next time nobody is on.

Only the game restarts; the container and the downloads keep running. Uploaded mods can be removed
the same way. The ones in your `addons` folder can't: change those on the host.

An upload is a mod folder with an `addme.script`, up to 2 GB a file and 8 GB in all, with no
programs, and never the 1.3 patch. A new mod's maps still have to be in the rotation. After the
restart, they show up in the admin screen's map list, to drag into the playlist. `MOD_UPLOADS=0`
turns uploads off. `MOD_SHARING=0` turns off the whole service. The admin screen then still controls
the game, but it can't save anything or read the server's map list.

### The launcher's lobby

With the lobby on (the admin screen's Options), players who connect from the launcher wait in its lobby with their game closed,
after their mods are in place. They see the map and everyone else waiting or still downloading.
**Start match** on the admin screen (or `/admin /start` in the game, or `docker exec openswbf2 rcon
/start`) opens every waiting player's game into the server's held lobby. The match begins once they
are all in, or after 60 seconds at most, with a 5-second countdown. Anyone still downloading joins
when their download finishes, and players who connect mid-match go straight in. **Back to lobby**
holds the next map again. The service reads the game's state over rcon on the loopback, with the
admin password.

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
  service only sends the listed mod files and only writes uploads under `/config/addons`; it takes
  32 connections at most (4 per address), and 5 wrong passwords in 10 seconds lock an address out
  for 30 seconds.
- **The caveat: this is a 2005 game engine,** rebuilt from the original. Its network readers were
  hardened and fuzz-tested, but it has not been security-audited like a modern server. So the
  container is the safety net:
  - it starts as root only to give `/config` to the server's user, then runs as that unprivileged
    user with no capabilities and `no-new-privileges`;
  - your own game copy, if you mount one, is read-only; the Steam download and settings sit in `config`;
  - memory, CPU and process counts are capped;
  - it uses Docker's normal network (not host networking).
- **Keep it current:** `docker compose pull` picks up fixes.
- **Friends only?** A VPN such as Tailscale lets them join without opening a port at all.

## Troubleshooting

- `No game data yet`: the server is waiting for `docker compose exec openswbf2 steam-login`, or for
  your own copy in `gamedata`. It carries on by itself once either is there.
- `this Steam account doesn't own ...`: log in with the account that owns the 2005 game
  (`steam-login` again).
- `Steam no longer takes the saved login`: Steam ended the session (a password change, say); run
  `steam-login` again. A download that stopped part-way carries on at the next start.
- `building the core game files failed`: the four core lvls aren't the original ones (a patch or
  mod replaced them). Use the Steam download, or copy them from a clean install.
- `/config is not writable`: the server fixes a host folder's ownership itself unless
  `compose.yaml` sets `user:` (older examples did). Remove that line, or `sudo chown -R 1000:1000`
  the folder.
- The launcher says the server doesn't share mods: check the TCP mapping and forward, then
  `curl -H "Authorization: Bearer <join password>" http://your-address:3658/mods.json` from outside.
  The log names any mod folder or file left out, and why.
- Logs: `docker compose logs`, and `config/logs/server.log` and `config/logs/steam-download.log`
  next to `compose.yaml`.
