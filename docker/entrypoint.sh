#!/usr/bin/env bash
# OpenSWBF2 dedicated server in a container. Finds the player's game data (a copy mounted at
# /gamedata, or a download from their own Steam account, made once), builds the core lvls from it
# (once per OpenSWBF2 version), assembles a game folder of links, writes the server's settings
# (kept in /config/settings, set from the launcher's admin screen) and runs BattlefrontII.exe
# -dedicated under Wine, next to the mod service (modserver.py). "steam-login" is the one-time
# Steam login. See README.md.
set -euo pipefail

APP=/opt/openswbf2
GAME=/srv/game
MOUNTED=/gamedata
ADDONS=/addons
CONFIG=/config
UPLOADS=/config/addons

# The game from Steam: STAR WARS Battlefront II (Classic, 2005) and the depot with its files.
# DepotDownloader keeps the login token from the QR code in .NET isolated storage, which follows
# XDG_DATA_HOME, so it lives in STEAM_DIR; the password is never stored.
STEAM_APP=6060
STEAM_DEPOT=6061
STEAM_DIR=$CONFIG/steam
STEAM_GAME=$CONFIG/steam-game
DEPOT=/opt/depotdownloader/DepotDownloader
VERSION=$(python3 -c 'import json; print(json.load(open("/opt/openswbf2/manifest.json"))["version"])')

log() { echo "[openswbf2] $*"; }
die() { echo "[openswbf2] error: $*" >&2; exit 1; }

# Started as root (the image's default): make /config the server's own, since Docker creates a
# missing host folder root-owned, then run everything as swbf2 (uid 1000). Leaving root this way
# drops every capability, and it happens before tini starts, so nothing stays root. As PID 1 the
# script hands over to tini, which reaps and forwards signals, and tini runs it again.
if (($(id -u) == 0)); then
    if [[ ${1:-} != selftest && -d $CONFIG ]]; then
        find "$CONFIG" -xdev \( ! -user 1000 -o ! -group 1000 \) -exec chown -h 1000:1000 {} + 2>/dev/null ||
            log "couldn't give all of $CONFIG to uid 1000"
    fi
    if (($$ == 1)); then
        exec setpriv --reuid=1000 --regid=1000 --init-groups --inh-caps=-all /usr/bin/tini -- "$0" "$@"
    fi
    exec setpriv --reuid=1000 --regid=1000 --init-groups --inh-caps=-all "$0" "$@"
fi
(($$ == 1)) && exec /usr/bin/tini -- "$0" "$@"

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

# One virtual screen for everything Wine runs here: nothing is shown, but the game opens a window
start_display() {
    rm -f /tmp/.X99-lock /tmp/.X11-unix/X99
    Xvfb :99 -screen 0 640x480x24 -nolisten tcp >/dev/null 2>&1 &
    export DISPLAY=:99
    for _ in $(seq 100); do
        [[ -e /tmp/.X11-unix/X99 ]] && return 0
        sleep 0.1
    done
    die "the virtual screen (Xvfb) did not start"
}

# A Wine command with a time limit and its output in a file: Wine's background processes keep
# pipes open after the command ends, so $(wine ...) can wait forever
wine_to_file() {
    local seconds=$1 out=$2
    shift 2
    timeout -k 10 "$seconds" wine "$@" </dev/null >"$out" 2>&1
}

# Why a selftest step failed: the command's output and the processes still running
selftest_fail() {
    echo "[openswbf2] error: $1. Its output:" >&2
    cat "$2" >&2
    echo "[openswbf2] Processes:" >&2
    for p in /proc/[0-9]*; do
        tr '\0' ' ' 2>/dev/null <"$p/cmdline" && echo
    done | grep -v '^$' >&2
    exit 1
}

# The shipped tools under Wine, as a check that the image works without any game data
if [[ ${1:-} == selftest ]]; then
    [[ -f $APP/BattlefrontII.exe && -f $APP/LuaCompiler.exe && -f $APP/data/scripts/index.json ]] ||
        die "the OpenSWBF2 package is incomplete"
    start_display
    out=$(mktemp)
    cd "$APP"
    status=0
    wine_to_file 180 "$out" LuaCompiler.exe || status=$?
    grep -q 'usage: luac' "$out" || selftest_fail "LuaCompiler did not run under Wine (exit $status)" "$out"
    status=0
    wine_to_file 180 "$out" cmd /c echo wine64-ok || status=$?
    grep -q 'wine64-ok' "$out" || selftest_fail "Wine's cmd did not run (exit $status)" "$out"
    "$DEPOT" >"$out" 2>&1 || true
    grep -q 'DepotDownloader v' "$out" || selftest_fail "DepotDownloader did not run" "$out"
    log "selftest ok: OpenSWBF2 $VERSION, $(wine --version), $(grep -o 'DepotDownloader v[0-9.]*' "$out")"
    wineserver -k >/dev/null 2>&1 || true
    exit 0
fi

# DepotDownloader with its login store in STEAM_DIR, run from a folder of its own. It aborts when
# a login can't go ahead; no core dump of that lands in /config.
depot() {
    mkdir -p "$STEAM_DIR/work"
    (ulimit -c 0 && cd "$STEAM_DIR/work" && XDG_DATA_HOME=$STEAM_DIR "$DEPOT" "$@")
}

check_config() {
    mkdir -p "$STEAM_DIR" 2>/dev/null && touch "$CONFIG/.write-test" 2>/dev/null && rm -f "$CONFIG/.write-test" ||
        die "$CONFIG is not writable by uid $(id -u). Remove 'user:' from compose.yaml so the server can fix that itself, or run 'sudo chown -R $(id -u):$(id -g)' on the host folder you mounted there"
}

# --- steam-login: once, interactively (docker compose exec openswbf2 steam-login) --------------
# A QR code scanned in the Steam app, or with --password the account name, password and Steam
# Guard code. Either way DepotDownloader keeps only a login token, and the check below proves the
# account owns the game.
if [[ ${1:-} == steam-login ]]; then
    check_config
    out=$(mktemp)
    rm -rf "$STEAM_DIR/check"
    if [[ ${2:-} == --password ]]; then
        read -r -p "Steam account name: " user
        [[ $user =~ ^[A-Za-z0-9_]{1,64}$ ]] || die "'$user' isn't a Steam account name (the name you sign in with)"
        depot -app "$STEAM_APP" -depot "$STEAM_DEPOT" -os windows -username "$user" -remember-password \
            -manifest-only -dir "$STEAM_DIR/check" || true
    else
        log "In the Steam app on your phone, open Steam Guard (the shield), choose 'Scan a QR code' and scan this:"
        depot -app "$STEAM_APP" -depot "$STEAM_DEPOT" -os windows -qr -remember-password \
            -manifest-only -dir "$STEAM_DIR/check" 2>&1 | tee "$out" || true
        user=$(sed -n -e "s/.*-username \([^ ]*\) -remember-password.*/\1/p" \
            -e "s/.*Logging '\([^']*\)' into Steam3.*/\1/p" "$out" | tail -n 1)
        if [[ -z $user ]]; then
            read -r -p "Steam account name (the name you sign in with): " user
        fi
    fi
    if ! compgen -G "$STEAM_DIR/check/manifest_${STEAM_DEPOT}_*" >/dev/null; then
        if grep -q 'is not available from this account' "$out" 2>/dev/null; then
            die "this Steam account doesn't own STAR WARS Battlefront II (Classic, 2005)"
        fi
        die "the Steam login didn't go through; run steam-login again (or 'steam-login --password')"
    fi
    echo "$user" >"$STEAM_DIR/username"
    rm -rf "$STEAM_DIR/check"
    log "Logged in to Steam as $user. The server downloads the game (about 13 GB) on its own now if it's"
    log "waiting, or on its next start: docker compose up -d && docker compose logs -f"
    exit 0
fi

# --- The player's game data --------------------------------------------------------------------
# The folder holding data/_lvl_pc/core.lvl below $1, in any letter case and up to two folders
# deeper than expected (a whole GameData folder copied in, say). Sets GAMEDATA, DATA_DIR, LVL_DIR.
find_gamedata() {
    local found
    found=$(find "$1" -maxdepth 5 -ipath '*/data/_lvl_pc/core.lvl' -print -quit 2>/dev/null) || true
    [[ -n $found ]] || return 1
    LVL_DIR=${found%/*}
    DATA_DIR=${LVL_DIR%/*}
    GAMEDATA=${DATA_DIR%/*}
}

# The four core lvls must be the stock ones LuaCompiler rebuilds from (the package lists them)
stock_lvls() {
    python3 - "$1" <<'EOF'
import hashlib, json, os, sys
lvl_dir = sys.argv[1]
stock = json.load(open("/opt/openswbf2/manifest.json"))["stockLvls"]
names = {name.lower(): name for name in os.listdir(lvl_dir)}
bad = []
for lvl, hashes in stock.items():
    name = names.get(lvl + ".lvl")
    digest = name and hashlib.sha256(open(os.path.join(lvl_dir, name), "rb").read()).hexdigest()
    if digest not in hashes:
        bad.append(lvl + ".lvl")
print(" ".join(bad))
EOF
}

# Download the game from the logged-in Steam account into STEAM_GAME. A dropped download picks up
# where it stopped on the next start. A login Steam no longer takes is forgotten, so the server
# waits for steam-login again instead of failing over and over.
steam_download() {
    local user out free
    user=$(<"$STEAM_DIR/username")
    out=$CONFIG/logs/steam-download.log
    free=$(df -Pk "$CONFIG" | awk 'NR == 2 { print int($4 / 1048576) }')
    ((free >= 15)) || log "warning: $CONFIG has ${free} GB free; the game needs about 14 GB"
    # Only the data folder: the dedicated server needs nothing else from the game
    printf 'regex:(?i)^(GameData/)?data/\n' >"$STEAM_DIR/files.txt"
    log "Downloading STAR WARS Battlefront II from Steam account $user into $STEAM_GAME (about 13 GB, once)"
    local status=0
    depot -app "$STEAM_APP" -depot "$STEAM_DEPOT" -os windows -username "$user" -remember-password \
        -filelist "$STEAM_DIR/files.txt" -dir "$STEAM_GAME" </dev/null 2>&1 |
        tee "$out" |
        awk '/^Pre-allocating / { next } /^ *[0-9.]+%/ { p = int($1 / 5); if (p > last) { last = p; print "[openswbf2] Steam download " p * 5 "%" }; next } { print }' ||
        status=$?
    if grep -q -E 'Enter account password|Access token was rejected|Unable to login to Steam3: (InvalidPassword|InvalidSignature|AccessDenied|Expired|Revoked)' "$out"; then
        rm -f "$STEAM_DIR/username"
        log "Steam no longer takes the saved login. Log in again: docker compose exec openswbf2 steam-login"
        return 0
    fi
    grep -q 'is not available from this account' "$out" &&
        die "Steam account $user doesn't own STAR WARS Battlefront II (Classic, 2005)"
    ((status == 0)) || die "the Steam download stopped (exit $status; $out has the details); it carries on at the next start"
    find_gamedata "$STEAM_GAME" || die "the Steam download has no data/_lvl_pc/core.lvl ($out has the details)"
    local bad
    bad=$(stock_lvls "$LVL_DIR")
    [[ -z $bad ]] || die "the downloaded $bad aren't the stock files OpenSWBF2 rebuilds from"
    touch "$STEAM_GAME/.complete"
    log "The game is downloaded"
}

check_config
mkdir -p "$CONFIG/settings" "$CONFIG/logs" "$CONFIG/lvl" "$UPLOADS"
GAMEDATA=
waited=0
while :; do
    if find_gamedata "$MOUNTED"; then
        [[ $GAMEDATA == "$MOUNTED" ]] || log "Using the game data in $GAMEDATA"
        break
    fi
    if [[ -f $STEAM_GAME/.complete ]] && find_gamedata "$STEAM_GAME"; then
        break
    fi
    if [[ -s $STEAM_DIR/username ]]; then
        steam_download
        continue
    fi
    if ((waited % 60 == 0)); then
        log "No game data yet. The server needs your copy of STAR WARS Battlefront II (2005), either:"
        log "  - from Steam, once: docker compose exec openswbf2 steam-login   (scan the QR code with the Steam app)"
        log "  - or your own files: copy GameData/data into the folder mounted at $MOUNTED (see README.md)"
        log "Waiting; it carries on by itself once either is done."
    fi
    waited=$((waited + 1))
    sleep 10
done
for lvl in core shell common ingame mission; do
    [[ -n $(find "$LVL_DIR" -maxdepth 1 -iname "$lvl.lvl" -print -quit) ]] ||
        die "no $lvl.lvl in $LVL_DIR: the game data is incomplete"
done
# Wine's path to it, for LuaCompiler
GAMEDATA_WIN="Z:${GAMEDATA//\//\\}"

# --- Settings ----------------------------------------------------------------------------------
# Only what the launcher's admin screen can't set comes from compose.yaml. The name, join password,
# playlist, player limit, tick rate, lobby and game options are the server's own settings, kept in
# /config/settings and set from the launcher (Server manager > Connect to server as admin).
PORT=$(int_env PORT 3658 1024 65534)

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

SCRIPT_MODS=$(bool_env SCRIPT_MODS 0)

# Mod downloads: TCP on the game's port number by default (the game itself only uses UDP)
MOD_SHARING=$(bool_env MOD_SHARING 1)
MOD_PORT=$(int_env MOD_PORT "$PORT" 1024 65535)
MOD_RATE=$(int_env MOD_RATE 50 0 100000)
MOD_UPLOADS=$(bool_env MOD_UPLOADS 1)

# The maps go on the command line, where their tickets stick
: >"$CONFIG/settings/ServerRotation.cfg"

# The server's settings for the next game start: what an admin saved, over the defaults (a new
# server's first start takes an old compose.yaml's values once). Writes ServerSettings.cfg, which
# the game reads at start, with EXTRA_SETTINGS last. Sets GAME_NAME, GAME_PLAYERS, GAME_TICK,
# GAME_LOBBY, GAME_JOIN and GAME_MAPS.
apply_settings() {
    local settings=()
    mapfile -t settings < <(python3 /opt/docker/modserver.py settings --config "$CONFIG")
    ((${#settings[@]} == 6)) && [[ -n ${settings[0]} && -n ${settings[5]} ]] ||
        die "the server's settings couldn't be read (config/settings/admin.json)"
    GAME_NAME=${settings[0]}
    GAME_PLAYERS=${settings[1]}
    GAME_TICK=${settings[2]}
    GAME_LOBBY=${settings[3]}
    GAME_JOIN=${settings[4]}
    read -r -a GAME_MAPS <<<"${settings[5]}"
}

# --- Core lvls: rebuilt from the stock ones for this OpenSWBF2 version, once ------------------
LVL=$CONFIG/lvl/$VERSION
mkdir -p "$LVL"
log "Core game files for OpenSWBF2 $VERSION (built once, then cached in /config/lvl)"
start_display
(cd "$APP" && timeout -k 10 1800 wine LuaCompiler.exe lvl --index 'data\scripts\index.json' --gamedata "$GAMEDATA_WIN" --out-dir "Z:\\config\\lvl\\$VERSION" </dev/null) ||
    die "building the core game files failed: are $LVL_DIR's shell, common, ingame and mission.lvl the original Steam/GOG ones?"

# --- The game folder: links to the package, the player's data, the rebuilt lvls and the mods ---
# Made again before each game start, so mods uploaded since then are in it
assemble() {
    rm -rf "${GAME:?}"/*
    mkdir -p "$GAME/data/_lvl_pc" "$GAME/SaveGames" "$GAME/AddOn"
    local file entry name skipped=0
    for file in "$APP"/*.exe "$APP"/*.dll; do
        ln -s "$file" "$GAME/"
    done
    for entry in "$DATA_DIR"/*; do
        name=${entry##*/}
        [[ ${name,,} == _lvl_pc ]] || ln -s "$entry" "$GAME/data/$name"
    done
    for entry in "$LVL_DIR"/*; do
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

    # Mod folders: the read-only addons first, then the ones admins uploaded; the first of a name wins
    local root dir mods=()
    local -A taken=()
    for root in "$ADDONS" "$UPLOADS"; do
        for dir in "$root"/*/; do
            [[ -d $dir ]] || continue
            dir=${dir%/}
            name=${dir##*/}
            if [[ -n ${taken[${name,,}]:-} ]]; then
                log "Uploaded mod $name is left out: the addons folder has one with that name"
                continue
            fi
            taken[${name,,}]=1
            ln -s "$dir" "$GAME/AddOn/$name"
            mods+=("$name")
        done
    done
    ((${#mods[@]} > 0)) && log "Mods: ${mods[*]}"
    return 0
}

# The maps this server can load, mods included, for the launcher's admin screen (GET /missions.json):
# the game lists them itself (-listmissions), again only when the version or the mods change
list_missions() {
    ((MOD_SHARING == 1)) || return 0
    local key dir out=$CONFIG/settings/missions.json
    key=$({
        echo "$VERSION"
        for dir in "$GAME"/AddOn/*/; do
            [[ -d $dir ]] || continue
            dir=${dir%/}
            echo "${dir##*/}:$(stat -L -c %Y "$dir/addme.script" 2>/dev/null)"
        done
    } | sha256sum | cut -d' ' -f1)
    [[ -s $out && -f $out.key && $(<"$out.key") == "$key" ]] && return 0
    log "Listing the server's maps for the launcher's admin screen"
    rm -f "$out" "$out.key"
    if wine_to_file 180 "$CONFIG/logs/missions-wine.log" BattlefrontII.exe -listmissions 'Z:\config\settings\missions.json' \
        -lang 0 -log 'Z:\config\logs\missions.log' && [[ -s $out ]]; then
        echo "$key" >"$out.key"
    else
        log "Couldn't list the maps (config/logs/missions.log); the admin screen offers the admin's own maps instead"
    fi
    wineserver -k >/dev/null 2>&1
    return 0
}

# --- Mod downloads -----------------------------------------------------------------------------
modserver=
start_modserver() {
    ((MOD_SHARING == 1)) || return 0
    ENTRYPOINT_PID=$$ PORT=$PORT MOD_PORT=$MOD_PORT MOD_RATE=$MOD_RATE MOD_UPLOADS=$MOD_UPLOADS \
        ADMIN_PASSWORD=$ADMIN_PASSWORD OPENSWBF2_VERSION=$VERSION \
        python3 /opt/docker/modserver.py serve --addons "$ADDONS" --config "$CONFIG" &
    modserver=$!
}

# --- Run ---------------------------------------------------------------------------------------
# The game's command line, from apply_settings
game_args() {
    args=(-dedicated "${GAME_MAPS[@]}")
    ((GAME_LOBBY == 1)) && args+=(-lobby)
    args+=(/configdir 'Z:\config\settings' /gamename "$GAME_NAME")
    [[ -n $GAME_JOIN ]] && args+=(/password "$GAME_JOIN")
    args+=(/adminpw "$ADMIN_PASSWORD" /playerlimit "$GAME_PLAYERS" /tps "$GAME_TICK" -port "$PORT")
    args+=(-logcmds 'all Warning, LogNet Inform, LogCore Inform' -log 'Z:\config\logs\server.log')
    if [[ -n ${EXTRA_ARGS:-} ]]; then
        local extra=()
        read -r -a extra <<<"$EXTRA_ARGS"
        args+=("${extra[@]}")
    fi
}

child=
restart=0

# WM_CLOSE first, the server's own clean stop (as the launcher's ServerHost does)
stop_game() {
    [[ -n $child ]] || return 0
    wine taskkill /IM BattlefrontII.exe >/dev/null 2>&1 || true
    for _ in $(seq 15); do
        kill -0 "$child" 2>/dev/null || return 0
        sleep 1
    done
    wineserver -k >/dev/null 2>&1 || true
    kill "$child" 2>/dev/null || true
    return 0
}

# docker stop
stop() {
    log "Stopping the server"
    stop_game
    [[ -n $modserver ]] && kill "$modserver" 2>/dev/null
    exit 0
}

# The mod service asks for this to load mods an admin uploaded (or to unload removed ones), and the
# name, player limit and tick rate an admin saved
restart_game() {
    restart=1
    log "Restarting the game, as an admin asked"
    stop_game
}
trap stop TERM INT
trap restart_game USR1

set +e
cd "$GAME"
while true; do
    python3 /opt/docker/modserver.py apply-pending --config "$CONFIG" || log "Couldn't put the uploaded mods in place"
    apply_settings
    assemble
    list_missions
    if ((MOD_SHARING == 1)); then
        [[ -n $modserver ]] && kill -0 "$modserver" 2>/dev/null || start_modserver
        log "Mod downloads on TCP $MOD_PORT ($( ((MOD_RATE > 0)) && echo "up to $MOD_RATE Mbit/s" || echo "no speed cap"))"
    fi
    game_args
    maps=0
    for word in "${GAME_MAPS[@]}"; do
        [[ $word =~ ^[0-9]+$ ]] || maps=$((maps + 1))
    done
    log "Starting \"$GAME_NAME\" on UDP $PORT: $maps maps, up to $GAME_PLAYERS players, $GAME_TICK turns a second"
    restart=0
    # On the virtual screen start_display made; $child is the game's own process
    wine BattlefrontII.exe "${args[@]}" </dev/null &
    child=$!
    wait "$child"
    status=$?
    # A trapped signal cuts the wait short: wait until the game has really gone
    while kill -0 "$child" 2>/dev/null; do
        wait "$child"
        status=$?
    done
    child=
    wineserver -k >/dev/null 2>&1
    if ((restart == 1)); then
        restart=0
        continue
    fi
    break
done
[[ -n $modserver ]] && kill "$modserver" 2>/dev/null
log "The server exited ($status)"
exit "$status"
