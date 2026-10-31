#!/usr/bin/env bash
# OpenSWBF2 dedicated server in a container. Builds the core lvls from the player's own game data
# (once per OpenSWBF2 version), assembles a game folder of links, writes the server settings from
# the environment and runs BattlefrontII.exe -dedicated under Wine. See README.md.
set -euo pipefail

APP=/opt/openswbf2
GAME=/srv/game
GAMEDATA=/gamedata
ADDONS=/addons
CONFIG=/config
VERSION=$(python3 -c 'import json; print(json.load(open("/opt/openswbf2/manifest.json"))["version"])')

log() { echo "[openswbf2] $*"; }
die() { echo "[openswbf2] error: $*" >&2; exit 1; }

# An integer setting: the variable clamped to [min, max], or its default
int_env() {
    local name=$1 default=$2 min=$3 max=$4
    local value="${!name:-}"
    value=${value:-$default}
    if [[ ! $value =~ ^-?[0-9]+$ ]]; then
        log "$name=$value is not a number; using $default"
        value=$default
    fi
    ((value < min)) && value=$min
    ((value > max)) && value=$max
    echo "$value"
}

# An on/off setting as 1 or 0
bool_env() {
    local name=$1 default=$2
    local value="${!name:-}"
    value=${value:-$default}
    case ${value,,} in
        1 | true | yes | on) echo 1 ;;
        *) echo 0 ;;
    esac
}

# The shipped tools under Wine, as a check that the image works without any game data
if [[ ${1:-} == selftest ]]; then
    [[ -f $APP/BattlefrontII.exe && -f $APP/LuaCompiler.exe && -f $APP/data/scripts/index.json ]] ||
        die "the OpenSWBF2 package is incomplete"
    out=$(cd "$APP" && wine LuaCompiler.exe 2>&1 || true)
    grep -q 'usage: luac' <<<"$out" || die "LuaCompiler (32-bit) did not run under Wine: $out"
    out=$(wine cmd /c echo wine64-ok 2>&1 || true)
    grep -q 'wine64-ok' <<<"$out" || die "64-bit Wine did not run: $out"
    log "selftest ok: OpenSWBF2 $VERSION, $(wine --version)"
    exit 0
fi

# --- The player's game data --------------------------------------------------------------------
for lvl in core shell common ingame mission; do
    [[ -f $GAMEDATA/data/_lvl_pc/$lvl.lvl ]] ||
        die "no $GAMEDATA/data/_lvl_pc/$lvl.lvl: mount your Star Wars Battlefront II GameData folder at $GAMEDATA (see README.md)"
done
touch "$CONFIG/.write-test" 2>/dev/null && rm -f "$CONFIG/.write-test" ||
    die "$CONFIG is not writable by uid $(id -u): run 'sudo chown -R $(id -u):$(id -g)' on the host folder you mounted there"
mkdir -p "$CONFIG/settings" "$CONFIG/logs" "$CONFIG/lvl"

# --- Settings ----------------------------------------------------------------------------------
PORT=$(int_env PORT 3658 1024 65534)
SERVER_NAME=${SERVER_NAME:-OpenSWBF2 Server}
SERVER_NAME=${SERVER_NAME//\"/}
SERVER_NAME=${SERVER_NAME:0:31}
JOIN_PASSWORD=${JOIN_PASSWORD:-}
[[ $JOIN_PASSWORD =~ ^[^[:space:]\"]{0,15}$ ]] || die "JOIN_PASSWORD is at most 15 characters, without spaces or quotes"

ADMIN_PASSWORD=${ADMIN_PASSWORD:-}
if [[ -z $ADMIN_PASSWORD ]]; then
    if [[ -s $CONFIG/admin-password ]]; then
        ADMIN_PASSWORD=$(<"$CONFIG/admin-password")
    else
        ADMIN_PASSWORD=$(python3 -c 'import secrets, string; a = string.ascii_letters + string.digits; print("".join(secrets.choice(a) for _ in range(16)))')
        (umask 077 && echo "$ADMIN_PASSWORD" >"$CONFIG/admin-password")
        log "No ADMIN_PASSWORD set: generated one in /config/admin-password (docker exec openswbf2 cat /config/admin-password)"
    fi
fi
[[ $ADMIN_PASSWORD =~ ^[A-Za-z0-9]{6,31}$ ]] || die "ADMIN_PASSWORD must be 6 to 31 letters or digits"

MAPS=${MAPS:-geo1c_con kas2g_con nab2c_con hot1g_con tat2g_con cor1c_con myg1g_con uta1c_con}
read -r -a MAP_LIST <<<"$MAPS"
((${#MAP_LIST[@]} > 0)) || die "MAPS is empty"
for map in "${MAP_LIST[@]}"; do
    [[ $map =~ ^[A-Za-z0-9_]{1,31}$ ]] || die "MAPS: '$map' is not a mission name (such as geo1c_con)"
done
((${#MAPS} <= 1200)) || die "MAPS is too long for the game's command line (1200 characters at most)"

MAX_PLAYERS=$(int_env MAX_PLAYERS 32 2 64)
TICK_RATE=$(int_env TICK_RATE 60 30 120)
case $TICK_RATE in 30 | 60 | 120) ;; *) log "TICK_RATE is 30, 60 or 120; using 60"; TICK_RATE=60 ;; esac
LOBBY=$(bool_env LOBBY 0)
SCRIPT_MODS=$(bool_env SCRIPT_MODS 0)

BOTS=$(int_env BOTS_PER_TEAM 16 0 32)
HERO_UNLOCK=$(int_env HERO_UNLOCK 3 3 5)
[[ $HERO_UNLOCK == 5 ]] || HERO_UNLOCK=3
if [[ $HERO_UNLOCK == 5 ]]; then
    HERO_UNLOCK_VALUE=$(int_env HERO_UNLOCK_VALUE 10 0 120)
else
    HERO_UNLOCK_VALUE=$(int_env HERO_UNLOCK_VALUE 10 1 50)
fi
HERO_PLAYER=$(int_env HERO_PLAYER 1 1 7)
[[ $HERO_PLAYER == 7 ]] || HERO_PLAYER=1

# The game options as admin commands, one per line, as the launcher's Server manager writes them
# (ServerProfile.SettingsLines); the game reads ServerSettings.cfg at start
{
    printf '/difficulty %s\r\n' "$(int_env DIFFICULTY 3 2 4)"
    printf '/conaiperteam %s\r\n/ctfaiperteam %s\r\n/assaiperteam %s\r\n/eliaiperteam %s\r\n' "$BOTS" "$BOTS" "$BOTS" "$BOTS"
    printf '/teamdamage %s\r\n' "$(bool_env TEAM_DAMAGE 1)"
    printf '/shownames %s\r\n' "$(bool_env SHOW_NAMES 1)"
    printf '/autoassignteams %s\r\n' "$(bool_env AUTO_ASSIGN_TEAMS 1)"
    printf '/awards %s\r\n' "$(bool_env AWARDS 1)"
    printf '/heroes %s\r\n' "$(bool_env HEROES 1)"
    printf '/hrunlock %s\r\n/hrunlockvalue %s\r\n/hrplayer %s\r\n' "$HERO_UNLOCK" "$HERO_UNLOCK_VALUE" "$HERO_PLAYER"
    printf '/aiheroes %s\r\n' "$(bool_env AI_HEROES 1)"
    printf '/conreinforcements %s\r\n' "$(int_env CONQUEST_REINFORCEMENTS 100 10 500)"
    printf '/contimelimit %s\r\n' "$(int_env CONQUEST_TIME_LIMIT 0 0 60)"
    printf '/ctfscorelimit %s\r\n' "$(int_env CTF_CAPTURES 5 1 15)"
    printf '/ctftimelimit %s\r\n' "$(int_env CTF_TIME_LIMIT 0 0 60)"
    printf '/huntimelimit %s\r\n' "$(int_env HUNT_TIME_LIMIT 0 0 60)"
    printf '/huntscorelimit %s\r\n' "$(int_env HUNT_SCORE 50 50 150)"
    printf '/assscorelimit %s\r\n' "$(int_env ASSAULT_SCORE 180 150 300)"
    if [[ -n ${EXTRA_SETTINGS:-} ]]; then
        while IFS= read -r line; do
            [[ -n $line ]] && printf '%s\r\n' "$line"
        done <<<"$EXTRA_SETTINGS"
    fi
} >"$CONFIG/settings/ServerSettings.cfg"
# The maps go on the command line, where their tickets stick
: >"$CONFIG/settings/ServerRotation.cfg"

# --- Core lvls: rebuilt from the stock ones for this OpenSWBF2 version, once ------------------
LVL=$CONFIG/lvl/$VERSION
mkdir -p "$LVL"
log "Core game files for OpenSWBF2 $VERSION (built once, then cached in /config/lvl)"
(cd "$APP" && wine LuaCompiler.exe lvl --index 'data\scripts\index.json' --gamedata 'Z:\gamedata' --out-dir "Z:\\config\\lvl\\$VERSION") ||
    die "building the core game files failed: are $GAMEDATA/data/_lvl_pc's shell, common, ingame and mission.lvl the original Steam/GOG ones?"

# --- The game folder: links to the package, the player's data and the rebuilt lvls ------------
rm -rf "${GAME:?}"/*
mkdir -p "$GAME/data/_lvl_pc" "$GAME/SaveGames"
for file in "$APP"/*.exe "$APP"/*.dll; do
    ln -s "$file" "$GAME/"
done
for entry in "$GAMEDATA"/data/*; do
    name=${entry##*/}
    [[ ${name,,} == _lvl_pc ]] || ln -s "$entry" "$GAME/data/$name"
done
skipped=0
for entry in "$GAMEDATA"/data/_lvl_pc/*; do
    name=${entry##*/}
    case ${name,,} in
        shell.lvl | common.lvl | ingame.lvl | mission.lvl | *.orig | *.cache) continue ;;
        user_script_* | custom_gc_* | v1.3patch*)
            if [[ $SCRIPT_MODS == 0 ]]; then
                skipped=$((skipped + 1))
                continue
            fi
            ;;
    esac
    ln -s "$entry" "$GAME/data/_lvl_pc/$name"
done
for lvl in shell common ingame mission; do
    ln -s "$LVL/$lvl.lvl" "$GAME/data/_lvl_pc/$lvl.lvl"
done
((skipped > 0)) && log "Left out $skipped script mod files (SCRIPT_MODS=1 loads them)"
if [[ -d $ADDONS ]] && [[ -n $(ls -A "$ADDONS" 2>/dev/null) ]]; then
    ln -s "$ADDONS" "$GAME/AddOn"
    log "Mods: $(ls "$ADDONS" | tr '\n' ' ')"
fi

# --- Run ---------------------------------------------------------------------------------------
args=(-dedicated "${MAP_LIST[@]}")
((LOBBY == 1)) && args+=(-lobby)
args+=(/configdir 'Z:\config\settings' /gamename "$SERVER_NAME")
[[ -n $JOIN_PASSWORD ]] && args+=(/password "$JOIN_PASSWORD")
args+=(/adminpw "$ADMIN_PASSWORD" /playerlimit "$MAX_PLAYERS" /tps "$TICK_RATE" -port "$PORT")
args+=(-logcmds 'all Warning, LogNet Inform, LogCore Inform' -log 'Z:\config\logs\server.log')
if [[ -n ${EXTRA_ARGS:-} ]]; then
    read -r -a extra <<<"$EXTRA_ARGS"
    args+=("${extra[@]}")
fi

log "Starting \"$SERVER_NAME\" on UDP $PORT: ${#MAP_LIST[@]} maps, up to $MAX_PLAYERS players, $TICK_RATE turns a second"
cd "$GAME"
xvfb-run -a -s '-screen 0 640x480x24' wine BattlefrontII.exe "${args[@]}" &
child=$!

# docker stop: WM_CLOSE first, the server's own clean stop (as the launcher's ServerHost does)
stop() {
    log "Stopping the server"
    wine taskkill /IM BattlefrontII.exe >/dev/null 2>&1 || true
    for _ in $(seq 15); do
        kill -0 "$child" 2>/dev/null || exit 0
        sleep 1
    done
    wineserver -k >/dev/null 2>&1 || true
    kill "$child" 2>/dev/null || true
    exit 0
}
trap stop TERM INT

set +e
wait "$child"
status=$?
wineserver -k >/dev/null 2>&1
log "The server exited ($status)"
exit "$status"
