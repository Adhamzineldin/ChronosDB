# FrancoDB Server Dockerfile - Production Ready
# Multi-stage build for smaller final image
# Location: installers/docker/Dockerfile

# ============================================================================
# Stage 1: Build Stage
# ============================================================================
FROM ubuntu:22.04 AS builder

# Set non-interactive mode
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
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

# Copy source code from project root
# Docker context is set to project root (../..) in docker-compose.yml
COPY src ./src
COPY test ./test
COPY CMakeLists.txt .
COPY examples ./examples
COPY libaries ./libaries

# Build the project with error handling
RUN mkdir -p build && \
    cd build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/opt/francodb .. && \
    cmake --build . --config Release -j$(nproc) && \
    cmake --install . && \
    cd / && \
    rm -rf /build

# ============================================================================
# Stage 2: Runtime Stage
# ============================================================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies (much smaller)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create francodb user for security (don't run as root)
RUN groupadd -r francodb && useradd -r -g francodb francodb

# Copy binaries from builder
COPY --from=builder /opt/francodb /opt/francodb

# Create required directories with proper permissions
RUN mkdir -p /opt/francodb/data /opt/francodb/log /opt/francodb/etc && \
    chown -R francodb:francodb /opt/francodb && \
    chmod 755 /opt/francodb && \
    chmod 700 /opt/francodb/data /opt/francodb/log

# Switch to francodb user
USER francodb

WORKDIR /opt/francodb

# Expose port
EXPOSE 2501

# Health check - verifies server is running
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:2501/health || exit 1

# Startup script with error handling
ENTRYPOINT ["/opt/francodb/bin/francodb_server"]
CMD ["--config", "/opt/francodb/etc/francodb.conf"]
