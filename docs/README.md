# ChronosDB Documentation

ChronosDB is a high-performance, multi-protocol database management system written in C++20. It features role-based access control, persistent storage with crash recovery, an AI-powered optimization layer, and CQL (Chronos Query Language) - SQL with Arabic-style keyword alternatives.

## Quick Start

### Build from Source

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Run the Server

```bash
./build/chronosdb_server
```

The server starts on port **2501** by default, serving both the database protocol and the web admin interface.

### Connect via Shell

```bash
./build/chronosdb_shell
```

Connection string format: `chronos://username:password@host:port/database`

Default credentials: `chronos` / `root`

### Web Admin

Open `http://localhost:2501` in your browser after building the web admin:

```bash
cd web-admin/client
npm install
npm run build
```

Then restart the server. The web admin will be available at the server's port.

## Features Overview

### Core Database
- **SQL Engine**: Full SQL support with SELECT, INSERT, UPDATE, DELETE, JOINs, GROUP BY, ORDER BY, LIMIT/OFFSET, DISTINCT, subqueries
- **Common Table Expressions (CTEs)**: `WITH temp AS (...) SELECT * FROM temp`
- **Window Functions**: ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD with PARTITION BY and ORDER BY
- **Table Partitioning**: RANGE and HASH partitioning for large tables
- **Indexes**: B+ Tree and Hash indexes with automatic population
- **Views**: Stored SELECT queries as virtual tables
- **Stored Procedures**: CREATE PROCEDURE with IF/ELSE/WHILE control flow
- **Triggers**: BEFORE/AFTER INSERT/UPDATE/DELETE triggers
- **Transactions**: BEGIN, COMMIT, ROLLBACK with WAL-based recovery

### Data Management
- **Export/Import**: CSV export and import for data portability
- **Backup/Restore**: Full database backup and restore
- **Time Travel**: Recover database state to any point in time using WAL
- **Scheduled Jobs**: Automated SQL execution on configurable intervals

### Security & Access Control
- **RBAC**: 5 roles - SUPERADMIN, ADMIN, USER, READONLY, DENIED
- **Per-database permissions**: Users can have different roles per database
- **Query Firewall**: Rate limiting, SQL injection detection, XSS prevention
- **Blocked query approval workflow**: Admin review of suspicious queries

### AI Layer
- **UCB1 Bandit Learning**: Adaptive scan strategy selection (sequential vs index)
- **Immune System**: Anomaly detection for unusual query patterns
- **Threat Detection**: SQL injection and XSS pattern recognition
- **Index Advisor**: AI-powered index recommendations based on query patterns
- **Temporal Index Manager**: Time-based access pattern analysis

### Enterprise Features
- **Replication**: Primary-Replica WAL-based replication with automatic failover
- **Query History**: Full query audit trail with timing
- **EXPLAIN / EXPLAIN ANALYZE**: Query plan inspection with AI insights
- **Multi-database support**: Create and switch between databases

### Web Admin Panel
- **Dashboard**: System overview with AI status
- **SQL Editor**: Query editor with natural language input and query plan visualization
- **Visual Query Builder**: GUI for building SELECT queries
- **Real-time Dashboard**: Live metrics with QPS, buffer stats, anomaly alerts
- **ER Diagram Viewer**: Auto-generated entity-relationship diagrams
- **Table/View Browser**: Schema inspection and data viewing
- **User Management**: RBAC administration
- **Testing Suite**: Bulk insert and performance comparison tools

### Bilingual Support (CQL)
All SQL keywords have Arabic-style (Franco-Arab) alternatives:
- SELECT = EKHTAR, FROM = MEN, WHERE = HEES, INSERT = DA5AL
- CREATE = ENSHAA, TABLE = GADWAL, INDEX = FAHRASA
- And many more (see [CQL Reference](CQL_REFERENCE.md))

## Documentation Index

| Document | Description |
|----------|-------------|
| [SQL Reference](SQL_REFERENCE.md) | Complete SQL syntax with examples |
| [CQL Reference](CQL_REFERENCE.md) | Arabic keyword alternatives |
| [API Reference](API_REFERENCE.md) | REST API endpoints |
| [Architecture](ARCHITECTURE.md) | System architecture and design |
| [Admin Guide](ADMIN_GUIDE.md) | Installation, configuration, operations |
| [AI Layer](AI_LAYER.md) | AI features documentation |

## Configuration

Key settings in `src/include/common/config.h`:

| Setting | Default | Description |
|---------|---------|-------------|
| PAGE_SIZE | 4096 | Page size in bytes |
| BUFFER_POOL_SIZE | 65536 | Buffer pool pages (256MB) |
| Default Port | 2501 | Server listen port |
| Default User | chronos | Default username |
| Default Password | root | Default password |

## Platform Support

- **Windows**: MinGW/MSYS2 with Ninja, links ws2_32
- **Linux**: GCC/Clang with pthread, .deb package available
