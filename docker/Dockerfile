# OpenSWBF2 dedicated server: OpenSWBF2's own release package (the x64 game, LuaCompiler and the
# scripts) under Wine. No game data is in the image: it downloads the game from the player's own
# Steam account on first start, or uses a copy they mount (see README.md).
# Debian 13 for Wine 10: the game's FFmpeg DLLs import bcryptprimitives.dll, which Wine 8 (Debian 12)
# doesn't have, so the game couldn't load there
FROM debian:trixie-slim

ARG OPENSWBF2_VERSION=0.2.4
ARG OPENSWBF2_REPO=jt-marte/OpenSWBF2
ARG DEPOTDOWNLOADER_VERSION=3.4.0
ARG DEPOTDOWNLOADER_SHA256=a999dec66b4850fc961bd50366696d23c2d0fad7b18790e6a5647b2f19097a53

ENV DEBIAN_FRONTEND=noninteractive \
    HOME=/home/swbf2 \
    WINEPREFIX=/home/swbf2/.wine \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES="mscoree,mshtml="

# 64-bit Wine only: the game and LuaCompiler are both x64. (Docker's default seccomp profile
# blocks the socket calls 32-bit Wine makes, so a 32-bit program can't run here.)
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        wine wine64 xvfb xauth tini python3 ca-certificates curl unzip \
 && rm -rf /var/lib/apt/lists/*

RUN groupadd -g 1000 swbf2 \
 && useradd -m -u 1000 -g 1000 -s /bin/bash swbf2 \
 && mkdir -p /opt/openswbf2 /srv/game /config \
 && chown swbf2:swbf2 /srv/game /config

# The release package, checked against its published SHA-256
RUN set -eu; \
    name="OpenSWBF2-${OPENSWBF2_VERSION}-x64.zip"; \
    base="https://github.com/${OPENSWBF2_REPO}/releases/download/v${OPENSWBF2_VERSION}"; \
    cd /tmp; \
    curl -fsSLO "$base/$name"; \
    curl -fsSLO "$base/$name.sha256"; \
    sha256sum -c "$name.sha256"; \
    unzip -q "$name" -d /opt/openswbf2; \
    rm -f "$name" "$name.sha256"; \
    python3 -c 'import struct, sys; b = open(sys.argv[1], "rb").read(); pe = struct.unpack_from("<I", b, 0x3C)[0]; sys.exit(struct.unpack_from("<H", b, pe + 4)[0] != 0x8664)' \
        /opt/openswbf2/LuaCompiler.exe \
     || { echo "OpenSWBF2 ${OPENSWBF2_VERSION} ships a 32-bit LuaCompiler: the image needs 0.2.3 or later" >&2; exit 1; }

# DepotDownloader (GPL-2.0, github.com/SteamRE/DepotDownloader) fetches the game from the player's
# own Steam account. It is 64-bit: Valve's steamcmd is 32-bit, which Docker's default seccomp
# profile keeps offline, as it does 32-bit Wine.
RUN set -eu; \
    cd /tmp; \
    curl -fsSL -o dd.zip "https://github.com/SteamRE/DepotDownloader/releases/download/DepotDownloader_${DEPOTDOWNLOADER_VERSION}/DepotDownloader-linux-x64.zip"; \
    echo "${DEPOTDOWNLOADER_SHA256}  dd.zip" | sha256sum -c -; \
    unzip -q dd.zip -d /opt/depotdownloader; \
    rm dd.zip; \
    chmod 755 /opt/depotdownloader/DepotDownloader
# No ICU in the image; DepotDownloader needs no culture data
ENV DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1

# The entrypoint, the mod download service and the rcon tool (also a module the service uses);
# steam-login runs the entrypoint's one-time Steam login
COPY entrypoint.sh modserver.py rcon.py /opt/docker/
RUN sed -i 's/\r$//' /opt/docker/entrypoint.sh /opt/docker/modserver.py /opt/docker/rcon.py \
 && chmod 755 /opt/docker/entrypoint.sh /opt/docker/modserver.py /opt/docker/rcon.py \
 && ln -s /opt/docker/rcon.py /usr/local/bin/rcon \
 && printf '#!/bin/sh\nexec /opt/docker/entrypoint.sh steam-login "$@"\n' >/usr/local/bin/steam-login \
 && chmod 755 /usr/local/bin/steam-login

# The Wine prefix, made once here so the server starts quickly
USER swbf2
RUN xvfb-run -a wineboot -i && wineserver -w

# The entrypoint starts as root only to make /config the server's own (Docker creates a missing
# host folder root-owned), then runs everything as swbf2 (uid 1000) through setpriv
USER root

WORKDIR /srv/game
# UDP: the game and admin; TCP: mod downloads
EXPOSE 3658/udp 3658/tcp
VOLUME ["/config"]
# A first start downloads the game (once) and builds the core lvls, so it counts as unhealthy until then
HEALTHCHECK --interval=30s --timeout=10s --start-period=300s --retries=3 CMD ["rcon", "--ping"]
# The entrypoint starts tini itself, after leaving root
ENTRYPOINT ["/opt/docker/entrypoint.sh"]
