# ChronosDB Administration Guide

## Installation

### Prerequisites
- C++20 compatible compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- CMake 3.16+
- Ninja build system (recommended)
- Node.js 18+ (for web admin)

### Building from Source

```bash
git clone <repository>
cd ChronosDB
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Building the Web Admin

```bash
cd web-admin/client
npm install
npm run build
```

The build output goes to `web-admin/client/dist/` which is automatically served by the server.

## Running the Server

### Basic Startup

```bash
./build/chronosdb_server
```

The server listens on port **2501** by default, handling both database connections and HTTP requests on the same port.

### Windows Service

```bash
./build/chronosdb_service install
./build/chronosdb_service start
```

### Connecting

**Shell client:**
```bash
./build/chronosdb_shell
# Or with connection string:
./build/chronosdb_shell chronos://chronos:root@localhost:2501/mydb
```

**Web admin:** Open `http://localhost:2501` in a browser.

## Configuration

### Key Settings (`src/include/common/config.h`)

| Parameter | Default | Description |
|-----------|---------|-------------|
| `PAGE_SIZE` | 4096 | Disk page size in bytes |
| `BUFFER_POOL_SIZE` | 65536 | Number of buffer pages (256MB total) |
| `DEFAULT_PORT` | 2501 | Server listen port |
| `DEFAULT_USER` | chronos | Default admin username |
| `DEFAULT_PASSWORD` | root | Default admin password |

### Data Directory

Database files are stored in the data directory (default: `./data/`). Each database gets its own subdirectory:

```
data/
  chronosdb/
    chronosdb.chronosdb    # Main data file
    chronosdb.wal          # Write-ahead log
  mydb/
    mydb.chronosdb
    mydb.wal
```

## Database Management

### Creating Databases

```sql
CREATE DATABASE mydb;
USE mydb;
```

### Listing Databases

```sql
SHOW DATABASES;
```

### Dropping Databases

```sql
-- Switch to a different database first
USE chronosdb;
DROP DATABASE mydb;
```

## User Management

### Default Credentials

The system ships with a default superadmin account:
- Username: `chronos`
- Password: `root`

**Change the default password immediately in production.**

### Creating Users

```sql
CREATE USER 'analyst' PASS 'secure_password' ROLE READONLY;
CREATE USER 'developer' PASS 'dev_pass' ROLE ADMIN;
```

### Role Hierarchy

| Role | Permissions |
|------|-------------|
| `SUPERADMIN` | Full access to all databases and operations |
| `ADMIN` | Read/write, create/drop tables, manage users in assigned databases |
| `NORMAL` | Read/write operations (SELECT, INSERT, UPDATE, DELETE) |
| `READONLY` | SELECT only |
| `DENIED` | No access |

### Changing Roles

```sql
-- Change role globally
ALTER USER 'analyst' ROLE ADMIN;

-- Change role for specific database
ALTER USER 'analyst' ROLE READONLY IN production_db;
```

### Deleting Users

```sql
DELETE USER 'analyst';
```

## Backup & Recovery

### Manual Backup

```sql
BACKUP DATABASE TO './backups/mydb_20260222';
```

This copies all database files (data file, WAL, metadata) to the specified directory.

### Restore from Backup

```sql
RESTORE DATABASE FROM './backups/mydb_20260222';
```

### Time Travel (Point-in-Time Recovery)

ChronosDB maintains a Write-Ahead Log (WAL) that enables recovery to any point in time:

```sql
-- Recover to a specific timestamp (microseconds since epoch)
RECOVER TO 1708646400000000;

-- Recover to latest state
RECOVER TO LATEST;
```

The system uses a **Reverse Delta** strategy for recent recovery (efficient) and falls back to **Forward Replay** for distant past recovery.

### Checkpoints

Force a checkpoint to flush all dirty pages and WAL:

```sql
CHECKPOINT;
```

## Export & Import

### Exporting Data

```sql
-- Export to CSV file
EXPORT TABLE users TO './exports/users.csv';
```

Via web admin: Navigate to Tables, select a table, click Export.

### Importing Data

```sql
-- Import from CSV file
IMPORT FROM './exports/users.csv' INTO users_copy;
```

Via web admin API: `POST /api/tables/:name/import` with CSV data in the request body.

## Scheduled Jobs

### Creating Schedules

```sql
-- Run cleanup every hour
CREATE SCHEDULE cleanup EVERY 3600 SECONDS DO 'DELETE FROM logs WHERE ts < 1000000;';

-- Run stats collection every 5 minutes
CREATE SCHEDULE stats EVERY 300 SECONDS DO 'INSERT INTO metrics VALUES (0, 0);';
```

### Managing Schedules

```sql
SHOW SCHEDULES;
DROP SCHEDULE cleanup;
```

## Replication

### Setting Up Primary-Replica

On the primary server:
```sql
SET REPLICATION ROLE PRIMARY;
ADD REPLICA 'replica1.example.com:2501';
ADD REPLICA 'replica2.example.com:2501';
```

On each replica:
```sql
SET REPLICATION ROLE REPLICA;
```

### Monitoring Replication

```sql
SHOW REPLICATION STATUS;
```

### Manual Failover

On a replica to promote it to primary:
```sql
PROMOTE;
```

## AI Layer Management

### Monitoring AI Status

```sql
SHOW AI STATUS;
SHOW EXECUTION STATS;
SHOW ANOMALIES;
```

### Index Recommendations

The AI layer tracks column access patterns and suggests indexes:

```sql
SHOW INDEX SUGGESTIONS;
```

Output example:
```
| Table | Column | Type  | Reason                        | Suggested SQL                              |
|-------|--------|-------|-------------------------------|--------------------------------------------|
| users | age    | BTREE | Frequent range lookups (50q)  | CREATE INDEX idx_users_age ON users(age);  |
| orders| user_id| HASH  | High equality lookups (120q)  | CREATE HASH INDEX idx_orders_uid ON orders(user_id); |
```

### Query Firewall

The firewall automatically blocks suspicious queries:

```sql
-- View blocked queries
SHOW BLOCKED QUERIES;

-- Approve a false positive
APPROVE QUERY 42;
```

The firewall detects:
- **Rate limiting**: Blocks users exceeding 1000 queries/minute
- **SQL injection**: Detects patterns like `'; DROP`, `1=1`, `UNION SELECT`, `--`
- **XSS attempts**: Detects `<script>` tags and `javascript:` URIs

## Query History

```sql
-- Show recent queries
SHOW HISTORY;
SHOW HISTORY LIMIT 100;
```

Query history tracks SQL text, user, database, success/failure, and execution time.

## Performance Tuning

### Index Strategy

- Use **B+ Tree indexes** for range queries (`<`, `>`, `BETWEEN`, `ORDER BY`)
- Use **Hash indexes** for equality lookups (`=`) - O(1) performance
- Check `SHOW INDEX SUGGESTIONS` for AI-powered recommendations

### EXPLAIN Queries

```sql
-- Show query plan
EXPLAIN SELECT * FROM users WHERE id = 5;

-- Run query and show actual timing
EXPLAIN ANALYZE SELECT * FROM users WHERE age > 25;
```

### Buffer Pool

Monitor buffer pool utilization via the web admin Real-time Dashboard or:
- `GET /api/buffer-stats` returns `{ buffer_pool_size, pages_in_use }`

## Monitoring

### Web Admin Dashboard

The real-time dashboard (`http://localhost:2501`, navigate to "Real-time") shows:
- Query throughput (queries/second)
- Buffer pool usage
- AI metrics
- Anomaly alerts
- Recent query log

### System Status

```sql
SHOW STATUS;
```

Returns current user, database, role, and authentication status.

## Troubleshooting

### Server Won't Start
- Check if port 2501 is already in use
- Verify data directory permissions
- Check for corrupted WAL files

### Slow Queries
1. Run `EXPLAIN ANALYZE` to see the query plan
2. Check `SHOW INDEX SUGGESTIONS` for missing indexes
3. Add indexes on frequently filtered columns
4. Use `LIMIT` for large result sets

### Recovery Issues
- Use `RECOVER TO LATEST` to replay all WAL entries
- If WAL is corrupted, restore from the most recent backup
- Checkpoints reduce recovery time; run `CHECKPOINT` periodically

### Server Shutdown

```sql
STOP SERVER;
```

Only SUPERADMIN can execute this command. The server completes current operations before stopping.
