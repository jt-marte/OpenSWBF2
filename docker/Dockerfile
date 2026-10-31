# OpenSWBF2 dedicated server: OpenSWBF2's own release package (the x64 game, LuaCompiler and the
# scripts) under Wine. No game data is in the image: mount your own copy (see README.md).
FROM debian:bookworm-slim

ARG OPENSWBF2_VERSION=0.2.2
ARG OPENSWBF2_REPO=jt-marte/OpenSWBF2

ENV DEBIAN_FRONTEND=noninteractive \
    HOME=/home/swbf2 \
    WINEPREFIX=/home/swbf2/.wine \
    WINEARCH=win64 \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES="mscoree,mshtml="

# 64-bit Wine runs the game; 32-bit Wine runs LuaCompiler, which builds the core lvls
RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
        wine wine64 wine32:i386 xvfb xauth tini python3 ca-certificates curl unzip \
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
    rm -f "$name" "$name.sha256"

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
