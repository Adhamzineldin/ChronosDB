# ChronosDB Server Dockerfile - Production Ready
# Location: installers/docker/Dockerfile

# ============================================================================
# Stage 1: Build Stage
# ============================================================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    ca-certificates \
    curl \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY src ./src
COPY test ./test
COPY CMakeLists.txt .
COPY examples ./examples
COPY libaries ./libaries

# Build with testing DISABLED to skip Windows-only tests
RUN mkdir -p build && \
    cd build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTING=OFF \
          -DCMAKE_INSTALL_PREFIX=/opt/chronosdb .. && \
    cmake --build . --config Release -j$(nproc) && \
    cmake --install . && \
    cd / && \
    rm -rf /build

# ============================================================================
# Stage 2: Runtime Stage
# ============================================================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r chronosdb && useradd -r -g chronosdb chronosdb

COPY --from=builder /opt/chronosdb /opt/chronosdb

RUN mkdir -p /opt/chronosdb/data /opt/chronosdb/log /opt/chronosdb/etc && \
    chown -R chronosdb:chronosdb /opt/chronosdb && \
    chmod 755 /opt/chronosdb && \
    chmod 700 /opt/chronosdb/data /opt/chronosdb/log

# --- ADDED: Generate Default Config ---
RUN echo "port = 2501" > /opt/chronosdb/etc/chronosdb.conf && \
    echo "bind_address = \"0.0.0.0\"" >> /opt/chronosdb/etc/chronosdb.conf && \
    echo "data_directory = \"/opt/chronosdb/data\"" >> /opt/chronosdb/etc/chronosdb.conf && \
    echo "log_directory = \"/opt/chronosdb/log\"" >> /opt/chronosdb/etc/chronosdb.conf && \
    chown chronosdb:chronosdb /opt/chronosdb/etc/chronosdb.conf
# ---------------------------------------

USER chronosdb
WORKDIR /opt/chronosdb
EXPOSE 2501

# REMOVED HEALTHCHECK to prevent protocol mismatch errors
# Re-enable if you implement an HTTP /health endpoint

ENTRYPOINT ["/opt/chronosdb/bin/chronosdb_server"]
CMD ["--config", "/opt/chronosdb/etc/chronosdb.conf"]