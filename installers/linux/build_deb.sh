#!/bin/bash
# ChronosDB .deb Package Builder for Linux
# Creates a production-ready .deb installer
# Location: installers/linux/build_deb.sh

set -e

# Script is in installers/linux/, so project root is two levels up
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DEB_DIR=$(mktemp -d -t chronosdb-deb.XXXXXX)

BUILD_DIR="${PROJECT_ROOT}/build"
OUTPUT_DIR="${SCRIPT_DIR}/Output"
mkdir -p "$OUTPUT_DIR"
VERSION="1.0.0"
ARCH="amd64"

echo "=========================================="
echo "  ChronosDB .deb Package Builder"
echo "=========================================="
echo "Script Dir:   $SCRIPT_DIR"
echo "Project Root: $PROJECT_ROOT"
echo "Build Dir:    $BUILD_DIR"
echo "Output Dir:   $OUTPUT_DIR"
echo "Version:      $VERSION"
echo ""

# Clean previous build
if [ -d "$DEB_DIR" ]; then
    rm -rf "$DEB_DIR"
fi

# Create directory structure for .deb
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/opt/chronosdb/bin"
mkdir -p "$DEB_DIR/opt/chronosdb/etc"
mkdir -p "$DEB_DIR/opt/chronosdb/data"
mkdir -p "$DEB_DIR/opt/chronosdb/log"
mkdir -p "$DEB_DIR/etc/systemd/system"
mkdir -p "$DEB_DIR/usr/local/bin"
mkdir -p "$DEB_DIR/usr/share/doc/chronosdb"
mkdir -p "$DEB_DIR/usr/share/man/man1"

# IMPORTANT: Set correct permissions on DEBIAN directory (must be 0755 or 0775 for dpkg-deb)
# Also set umask to ensure new files have proper permissions
chmod 755 "$DEB_DIR/DEBIAN"
umask 0022

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

echo "Building ChronosDB from source..."

# Build if not already built
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # FIX: Use standard flags instead of presets
    # -DBUILD_TESTING=OFF : Skips tests (speeds up build, avoids winsock error)
    # -DCMAKE_BUILD_TYPE=Release : Optimizes for production
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "$PROJECT_ROOT"
    
    cmake --build . -j$(nproc)
    
    cd "$SCRIPT_DIR"
fi

# Copy binaries
echo "Copying binaries..."
if [ -f "$BUILD_DIR/chronosdb_server" ]; then
    cp "$BUILD_DIR/chronosdb_server" "$DEB_DIR/opt/chronosdb/bin/"
    chmod 755 "$DEB_DIR/opt/chronosdb/bin/chronosdb_server"
fi

if [ -f "$BUILD_DIR/chronosdb_shell" ]; then
    cp "$BUILD_DIR/chronosdb_shell" "$DEB_DIR/opt/chronosdb/bin/"
    chmod 755 "$DEB_DIR/opt/chronosdb/bin/chronosdb_shell"
fi

if [ -f "$BUILD_DIR/chronosdb_service" ]; then
    cp "$BUILD_DIR/chronosdb_service" "$DEB_DIR/opt/chronosdb/bin/"
    chmod 755 "$DEB_DIR/opt/chronosdb/bin/chronosdb_service"
fi

# Copy configuration template
cat > "$DEB_DIR/opt/chronosdb/etc/chronosdb.conf.template" << 'EOF'
# ChronosDB Configuration File
# Edit this file and save as chronosdb.conf

# Network Configuration
port = 2501
bind_address = "0.0.0.0"

# Data Storage
data_directory = "/opt/chronosdb/data"
log_directory = "/opt/chronosdb/log"

# Performance
buffer_pool_size = 1024
autosave_interval = 30

# Logging
log_level = "INFO"
log_format = "json"

# Encryption
encryption_enabled = false

# Root User (set during installation)
root_username = "chronos"
root_password = "change_me"
EOF
chmod 644 "$DEB_DIR/opt/chronosdb/etc/chronosdb.conf.template"

# Create symlinks
ln -s /opt/chronosdb/bin/chronosdb_server "$DEB_DIR/usr/local/bin/chronosdb_server"
ln -s /opt/chronosdb/bin/chronosdb_shell "$DEB_DIR/usr/local/bin/chronosdb"

# Copy systemd service file
cat > "$DEB_DIR/etc/systemd/system/chronosdb.service" << 'EOF'
[Unit]
Description=ChronosDB Database Server
Documentation=https://github.com/yourusername/ChronosDB
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=chronosdb
Group=chronosdb
WorkingDirectory=/opt/chronosdb
ExecStart=/opt/chronosdb/bin/chronosdb_server --config /opt/chronosdb/etc/chronosdb.conf
ExecReload=/bin/kill -HUP $MAINPID
KillMode=process
Restart=on-failure
RestartSec=10s

# Security
ProtectSystem=strict
ProtectHome=yes
NoNewPrivileges=true
ReadWritePaths=/opt/chronosdb/data /opt/chronosdb/log

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
EOF
chmod 644 "$DEB_DIR/etc/systemd/system/chronosdb.service"

# Create DEBIAN/control file
cat > "$DEB_DIR/DEBIAN/control" << EOF
Package: chronosdb
Version: $VERSION
Architecture: $ARCH
Maintainer: ChronosDB Team <dev@chronosdb.io>
Homepage: https://github.com/adhamzineldin/ChronosDB
Description: ChronosDB - S+ Grade Enterprise Database System
 ChronosDB is a high-performance relational database with:
 - Advanced SQL support (JOINs, GROUP BY, aggregates)
 - FOREIGN KEY constraints with referential integrity
 - Nullable column support with default values
 - ACID transactions with isolation levels
 - B+ tree indexes for fast lookups
 - Comprehensive query optimization
 .
 This package provides the server and shell client.
Depends: libc6 (>= 2.31), libssl3 (>= 1.1.1)
Suggests: sqlite3 | postgresql-client
Section: database
Priority: optional
Essential: no
EOF

# Create preinst script - runs before installation
cat > "$DEB_DIR/DEBIAN/preinst" << 'EOF'
#!/bin/bash
set -e

# Create chronosdb user if it doesn't exist
if ! id chronosdb >/dev/null 2>&1; then
    echo "Creating chronosdb user..."
    useradd -r -s /sbin/nologin -d /opt/chronosdb -m chronosdb || true
fi

exit 0
EOF
chmod 755 "$DEB_DIR/DEBIAN/preinst"

# Create postinst script - runs after installation
cat > "$DEB_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
set -e

# Set permissions
chown -R chronosdb:chronosdb /opt/chronosdb
chmod 755 /opt/chronosdb
chmod 700 /opt/chronosdb/data /opt/chronosdb/log
chmod 644 /opt/chronosdb/etc/*.template

# Copy config if not exists
if [ ! -f /opt/chronosdb/etc/chronosdb.conf ]; then
    cp /opt/chronosdb/etc/chronosdb.conf.template /opt/chronosdb/etc/chronosdb.conf
    chmod 600 /opt/chronosdb/etc/chronosdb.conf
    chown chronosdb:chronosdb /opt/chronosdb/etc/chronosdb.conf
fi

# Reload systemd
systemctl daemon-reload || true

echo ""
echo "ChronosDB installed successfully!"
echo ""
echo "Next steps:"
echo "1. Edit configuration: sudo nano /opt/chronosdb/etc/chronosdb.conf"
echo "2. Start the service: sudo systemctl start chronosdb"
echo "3. Enable on boot: sudo systemctl enable chronosdb"
echo ""

exit 0
EOF
chmod 755 "$DEB_DIR/DEBIAN/postinst"

# Create prerm script - runs before removal
cat > "$DEB_DIR/DEBIAN/prerm" << 'EOF'
#!/bin/bash
set -e

# Stop the service
systemctl stop chronosdb || true
systemctl disable chronosdb || true

exit 0
EOF
chmod 755 "$DEB_DIR/DEBIAN/prerm"

# Create postrm script - runs after removal
cat > "$DEB_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/bash
set -e

# Remove user if uninstall was clean
if [ "$1" = "purge" ]; then
    userdel -r chronosdb || true
fi

systemctl daemon-reload || true

exit 0
EOF
chmod 755 "$DEB_DIR/DEBIAN/postrm"

# Create copyright file
cat > "$DEB_DIR/usr/share/doc/chronosdb/copyright" << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: ChronosDB
Upstream-Contact: ChronosDB Team <dev@chronosdb.io>
Source: https://github.com/yourusername/ChronosDB

Files: *
Copyright: 2024-2026 ChronosDB Team
License: MIT

License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
EOF

# Create changelog
cat > "$DEB_DIR/usr/share/doc/chronosdb/changelog" << 'EOF'
chronosdb (1.0.0) stable; urgency=medium

  * Initial release
  * Added JOIN operations (INNER, LEFT, RIGHT, FULL, CROSS)
  * Added FOREIGN KEY constraints with CASCADE actions
  * Added NULLABLE column support with DEFAULT values
  * Added GROUP BY aggregates (COUNT, SUM, AVG, MIN, MAX)
  * Added ORDER BY sorting with multi-column support
  * Added LIMIT/OFFSET pagination
  * Added SELECT DISTINCT
  * SOLID principles architecture
  * Enterprise-grade code quality

 -- ChronosDB Team <dev@chronosdb.io>  Sat, 19 Jan 2026 00:00:00 +0000
EOF

# Build the .deb package
DEB_FILENAME="chronosdb_${VERSION}_${ARCH}.deb"
echo ""
echo "Building .deb package..."

# Final permission check - ensure DEBIAN directory has correct permissions before building
chmod 755 "$DEB_DIR/DEBIAN"
chmod 644 "$DEB_DIR/DEBIAN/control"
chmod 755 "$DEB_DIR/DEBIAN/preinst" "$DEB_DIR/DEBIAN/postinst" "$DEB_DIR/DEBIAN/prerm" "$DEB_DIR/DEBIAN/postrm" 2>/dev/null || true

dpkg-deb --build "$DEB_DIR" "${OUTPUT_DIR}/${DEB_FILENAME}"

echo "✓ Package built successfully!"
echo ""
echo "=========================================="
echo "  Build Complete"
echo "=========================================="
echo "Package: ${OUTPUT_DIR}/${DEB_FILENAME}"
echo ""
echo "To install:"
echo "  sudo dpkg -i ${DEB_FILENAME}"
echo ""
echo "To start service:"
echo "  sudo systemctl start chronosdb"
echo ""

# Create MD5 checksum
cd "$OUTPUT_DIR"
md5sum "${DEB_FILENAME}" > "${DEB_FILENAME}.md5"
echo "✓ Checksum: ${DEB_FILENAME}.md5"
 
echo "Done!"


echo "✓ Checksum: ${DEB_FILENAME}.md5"

# Clean up the temporary staging directory
rm -rf "$DEB_DIR"

echo "Done!"
