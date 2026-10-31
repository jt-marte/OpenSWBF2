# OpenSWBF2 dedicated server: OpenSWBF2's own release package (the x64 game, LuaCompiler and the
# scripts) under Wine. No game data is in the image: mount your own copy (see README.md).
FROM debian:bookworm-slim

ARG OPENSWBF2_VERSION=0.2.4
ARG OPENSWBF2_REPO=jt-marte/OpenSWBF2

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

# The entrypoint, the mod download service and the rcon tool (also a module the service uses)
COPY entrypoint.sh modserver.py rcon.py /opt/docker/
RUN sed -i 's/\r$//' /opt/docker/entrypoint.sh /opt/docker/modserver.py /opt/docker/rcon.py \
 && chmod 755 /opt/docker/entrypoint.sh /opt/docker/modserver.py /opt/docker/rcon.py \
 && ln -s /opt/docker/rcon.py /usr/local/bin/rcon

# The Wine prefix, made once here so the server starts quickly
USER swbf2
RUN xvfb-run -a wineboot -i && wineserver -w

WORKDIR /srv/game
# UDP: the game and admin; TCP: mod downloads
EXPOSE 3658/udp 3658/tcp
VOLUME ["/config"]
# A first start builds the core lvls, which takes a minute or two
HEALTHCHECK --interval=30s --timeout=10s --start-period=300s --retries=3 CMD ["rcon", "--ping"]
ENTRYPOINT ["/usr/bin/tini", "--", "/opt/docker/entrypoint.sh"]
