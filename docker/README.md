# OpenSWBF2 dedicated server in Docker

Runs the OpenSWBF2 dedicated server headless on Linux. The image is `ghcr.io/jt-marte/openswbf2-server`,
built from each OpenSWBF2 release by `.github/workflows/docker.yml`. It holds only OpenSWBF2's own
files, run under Wine. The game data is yours: you mount your copy of Star Wars Battlefront II (2005).

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

Players need the same mods to join a mod map.

## 2. Start it

Put `compose.yaml` in `~/openswbf2`, set at least `ADMIN_PASSWORD`, then:

```bash
cd ~/openswbf2
docker compose up -d
docker compose logs -f
```

The first start builds the core files (a minute or two, cached in `config/lvl` after that), then
logs `OPENSWBF2 NET mission start ... map=...`. Players join from **Multiplayer > Direct Connect**
with `your-address:3658`.

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

## Updating

```bash
docker compose pull && docker compose up -d
```

A new OpenSWBF2 version rebuilds the core files once. Players need the same OpenSWBF2 version
(the OpenBattlefront launcher updates itself to the latest release).

## Opening it to the internet

Forward **UDP** `3658` (or your `PORT`) on your router to the server. Nothing else is needed: game
traffic and admin share that one port.

Is that safe enough? It is a reasonable setup for a home server, with one caveat:

- **One port, password-protected admin.** Admin commands need the admin password; logins are
  limited to 5 per 10 seconds per address, and the session key changes as it goes. Use a long
  random admin password, and a join password for private games.
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
- Logs: `docker compose logs`, and `config/logs/server.log`.
