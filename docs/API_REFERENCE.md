# ChronosDB REST API Reference

All endpoints are served on the same port as the ChronosDB server (default **2501**). The web admin API provides full management capabilities for databases, tables, users, queries, and the AI layer.

**Base URL:** `http://localhost:2501`

---

## Table of Contents

- [Authentication](#authentication)
- [Databases](#databases)
- [Tables](#tables)
- [Views](#views)
- [Query Execution](#query-execution)
- [Users](#users)
- [System](#system)
- [AI Layer](#ai-layer)
- [Query History](#query-history)
- [Schedules](#schedules)
- [Replication](#replication)
- [Schema](#schema)
- [Response Format](#response-format)

---

## Authentication

All API endpoints (except `POST /api/login`) require authentication. Authentication is provided via one of the following mechanisms:

- **Session cookie** -- Set automatically after a successful login.
- **Bearer token** -- Pass the token returned from login in the `Authorization` header:
  ```
  Authorization: Bearer <token>
  ```

Unauthenticated requests will receive a `401 Unauthorized` response.

---

### POST /api/login

Authenticate a user and establish a session.

**Request Body:**

```json
{
  "username": "chronos",
  "password": "root"
}
```

**Response (200 OK):**

```json
{
  "success": true,
  "username": "chronos",
  "role": "SUPERADMIN",
  "token": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
}
```

**Response (401 Unauthorized):**

```json
{
  "success": false,
  "error": "Invalid username or password"
}
```

---

### POST /api/logout

End the current session and clear the session cookie.

**Request Body:** None

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Logged out successfully"
}
```

---

### GET /api/me

Retrieve information about the currently authenticated user.

**Response (200 OK):**

```json
{
  "success": true,
  "username": "chronos",
  "role": "SUPERADMIN"
}
```

**Response (401 Unauthorized):**

```json
{
  "success": false,
  "error": "Not authenticated"
}
```

---

## Databases

### GET /api/databases

List all databases available on the server.

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["database_name"],
    "rows": [
      ["default"],
      ["mydb"],
      ["analytics"]
    ]
  },
  "row_count": 3
}
```

---

### POST /api/databases/use

Switch the active database for the current session.

**Request Body:**

```json
{
  "database": "mydb"
}
```

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Using database mydb"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Database 'nonexistent' does not exist"
}
```

---

### POST /api/databases/create

Create a new database.

**Request Body:**

```json
{
  "name": "analytics"
}
```

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Database 'analytics' created successfully"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Database 'analytics' already exists"
}
```

---

### DELETE /api/databases/:name

Drop a database and all of its contents.

**URL Parameters:**

| Parameter | Type   | Description                   |
|-----------|--------|-------------------------------|
| `name`    | string | Name of the database to drop  |

**Example:** `DELETE /api/databases/analytics`

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Database 'analytics' dropped successfully"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Cannot drop the default database"
}
```

---

## Tables

### GET /api/tables

List all tables and views in the current database.

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["table_name"],
    "rows": [
      ["users"],
      ["orders"],
      ["products"]
    ]
  },
  "row_count": 3
}
```

---

### GET /api/tables/:name/schema

Get the schema (column definitions) for a specific table. Equivalent to `DESCRIBE <table>`.

**URL Parameters:**

| Parameter | Type   | Description            |
|-----------|--------|------------------------|
| `name`    | string | Name of the table      |

**Example:** `GET /api/tables/users/schema`

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["column_name", "type", "nullable", "key", "default"],
    "rows": [
      ["id", "INT", "NO", "PRI", "NULL"],
      ["username", "VARCHAR(255)", "NO", "", "NULL"],
      ["email", "VARCHAR(255)", "YES", "", "NULL"],
      ["created_at", "DATETIME", "YES", "", "CURRENT_TIMESTAMP"]
    ]
  },
  "row_count": 4
}
```

---

### GET /api/tables/:name/data

Retrieve all rows from a table. Equivalent to `SELECT * FROM <table>`.

**URL Parameters:**

| Parameter | Type   | Description            |
|-----------|--------|------------------------|
| `name`    | string | Name of the table      |

**Example:** `GET /api/tables/users/data`

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["id", "username", "email", "created_at"],
    "rows": [
      ["1", "chronos", "admin@example.com", "2026-01-15 10:30:00"],
      ["2", "alice", "alice@example.com", "2026-02-01 14:22:00"]
    ]
  },
  "row_count": 2
}
```

---

### POST /api/tables/:name/export

Export a table's data as a CSV file. The response is returned with `Content-Type: text/csv` and a `Content-Disposition` header for file download.

**URL Parameters:**

| Parameter | Type   | Description            |
|-----------|--------|------------------------|
| `name`    | string | Name of the table      |

**Example:** `POST /api/tables/users/export`

**Response (200 OK):**

```
Content-Type: text/csv
Content-Disposition: attachment; filename="users.csv"

id,username,email,created_at
1,chronos,admin@example.com,2026-01-15 10:30:00
2,alice,alice@example.com,2026-02-01 14:22:00
```

---

### POST /api/tables/:name/import

Import CSV data into a table. The CSV content is sent as a string in the request body.

**URL Parameters:**

| Parameter | Type   | Description            |
|-----------|--------|------------------------|
| `name`    | string | Name of the table      |

**Request Body:**

```json
{
  "csv": "id,username,email,created_at\n3,bob,bob@example.com,2026-02-20 09:00:00\n4,carol,carol@example.com,2026-02-21 11:15:00"
}
```

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Imported 2 rows into 'users'"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "CSV column mismatch: expected 4 columns, got 3"
}
```

---

## Views

### GET /api/views

List all views in the current database.

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["view_name"],
    "rows": [
      ["active_users"],
      ["order_summary"]
    ]
  },
  "row_count": 2
}
```

---

### POST /api/views

Create a new view.

**Request Body:**

```json
{
  "name": "active_users",
  "query": "SELECT id, username FROM users WHERE active = 1"
}
```

**Response (200 OK):**

```json
{
  "success": true,
  "message": "View 'active_users' created successfully"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "View 'active_users' already exists"
}
```

---

### DELETE /api/views/:name

Drop a view.

**URL Parameters:**

| Parameter | Type   | Description           |
|-----------|--------|-----------------------|
| `name`    | string | Name of the view      |

**Example:** `DELETE /api/views/active_users`

**Response (200 OK):**

```json
{
  "success": true,
  "message": "View 'active_users' dropped successfully"
}
```

---

### GET /api/views/:name/data

Retrieve the data produced by a view.

**URL Parameters:**

| Parameter | Type   | Description           |
|-----------|--------|-----------------------|
| `name`    | string | Name of the view      |

**Example:** `GET /api/views/active_users/data`

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["id", "username"],
    "rows": [
      ["1", "chronos"],
      ["2", "alice"]
    ]
  },
  "row_count": 2
}
```

---

## Query Execution

### POST /api/query

Execute a single SQL or CQL statement.

**Request Body:**

```json
{
  "sql": "SELECT * FROM users WHERE id = 1"
}
```

**Response (200 OK) -- SELECT query:**

```json
{
  "success": true,
  "data": {
    "columns": ["id", "username", "email", "created_at"],
    "rows": [
      ["1", "chronos", "admin@example.com", "2026-01-15 10:30:00"]
    ]
  },
  "row_count": 1
}
```

**Response (200 OK) -- INSERT/UPDATE/DELETE:**

```json
{
  "success": true,
  "message": "1 row(s) affected",
  "row_count": 1
}
```

**Response (200 OK) -- DDL statement:**

```json
{
  "success": true,
  "message": "Table 'products' created successfully"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Syntax error near 'FORM' at position 14"
}
```

---

### POST /api/query/batch

Execute multiple SQL statements in sequence. Each statement is executed independently. If a statement fails, subsequent statements are still executed.

**Request Body:**

```json
{
  "queries": [
    "INSERT INTO users (id, username) VALUES (5, 'dave')",
    "INSERT INTO users (id, username) VALUES (6, 'eve')",
    "UPDATE users SET email = 'dave@example.com' WHERE id = 5"
  ]
}
```

**Response (200 OK):**

```json
{
  "success": true,
  "results": [
    {
      "success": true,
      "message": "1 row(s) affected",
      "row_count": 1
    },
    {
      "success": true,
      "message": "1 row(s) affected",
      "row_count": 1
    },
    {
      "success": true,
      "message": "1 row(s) affected",
      "row_count": 1
    }
  ]
}
```

**Response with partial failure:**

```json
{
  "success": false,
  "results": [
    {
      "success": true,
      "message": "1 row(s) affected",
      "row_count": 1
    },
    {
      "success": false,
      "error": "Duplicate key: id = 6 already exists"
    },
    {
      "success": true,
      "message": "1 row(s) affected",
      "row_count": 1
    }
  ]
}
```

---

## Users

### GET /api/users

List all users. Requires ADMIN or SUPERADMIN role.

**Response (200 OK):**

```json
{
  "success": true,
  "data": {
    "columns": ["username", "role"],
    "rows": [
      ["chronos", "SUPERADMIN"],
      ["alice", "ADMIN"],
      ["bob", "USER"],
      ["guest", "READONLY"]
    ]
  },
  "row_count": 4
}
```

---

### POST /api/users

Create a new user. Requires ADMIN or SUPERADMIN role.

**Request Body:**

```json
{
  "username": "newuser",
  "password": "securepassword123",
  "role": "USER"
}
```

Valid roles: `SUPERADMIN`, `ADMIN`, `USER`, `READONLY`, `DENIED`

**Response (200 OK):**

```json
{
  "success": true,
  "message": "User 'newuser' created with role USER"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "User 'newuser' already exists"
}
```

**Response (403 Forbidden):**

```json
{
  "success": false,
  "error": "Insufficient privileges to create users"
}
```

---

### DELETE /api/users/:name

Delete a user. Requires ADMIN or SUPERADMIN role.

**URL Parameters:**

| Parameter | Type   | Description              |
|-----------|--------|--------------------------|
| `name`    | string | Username to delete       |

**Example:** `DELETE /api/users/bob`

**Response (200 OK):**

```json
{
  "success": true,
  "message": "User 'bob' deleted successfully"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Cannot delete the default superadmin user"
}
```

---

### PUT /api/users/:name/role

Change a user's role. Requires ADMIN or SUPERADMIN role. Optionally scope the role to a specific database.

**URL Parameters:**

| Parameter | Type   | Description              |
|-----------|--------|--------------------------|
| `name`    | string | Username to modify       |

**Request Body:**

```json
{
  "role": "ADMIN",
  "database": "mydb"
}
```

The `database` field is optional. If omitted, the role applies globally.

**Response (200 OK):**

```json
{
  "success": true,
  "message": "User 'alice' role updated to ADMIN"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Invalid role: 'SUPERUSER'"
}
```

---

## System

### GET /api/status

Retrieve the current system status of the ChronosDB server.

**Response (200 OK):**

```json
{
  "success": true,
  "status": "running",
  "version": "1.0.0",
  "uptime": "3d 14h 22m",
  "current_database": "mydb",
  "connections": 5
}
```

---

### GET /api/buffer-stats

Retrieve buffer pool statistics.

**Response (200 OK):**

```json
{
  "success": true,
  "buffer_pool_size": 65536,
  "pages_in_use": 1024
}
```

---

## AI Layer

ChronosDB includes a built-in AI layer for anomaly detection, query optimization, and security. The following endpoints expose the status and controls for the AI subsystem.

### GET /api/ai/status

Get a summary of the AI layer status.

**Response (200 OK):**

```json
{
  "success": true,
  "ai_enabled": true,
  "learning_engine": "active",
  "immune_system": "active",
  "temporal_index": "active"
}
```

---

### GET /api/ai/anomalies

Retrieve anomalies detected by the immune system.

**Response (200 OK):**

```json
{
  "success": true,
  "anomalies": [
    {
      "id": 1,
      "type": "SQL_INJECTION",
      "query": "SELECT * FROM users WHERE id = 1 OR 1=1",
      "severity": "HIGH",
      "detected_at": "2026-02-22 08:15:30",
      "status": "blocked"
    },
    {
      "id": 2,
      "type": "XSS",
      "query": "INSERT INTO comments (body) VALUES ('<script>alert(1)</script>')",
      "severity": "MEDIUM",
      "detected_at": "2026-02-22 09:22:10",
      "status": "blocked"
    }
  ]
}
```

---

### GET /api/ai/stats

Retrieve execution statistics, including data from the UCB1 multi-armed bandit strategy selector.

**Response (200 OK):**

```json
{
  "success": true,
  "total_queries": 15230,
  "avg_execution_time_ms": 12.4,
  "strategy_stats": {
    "sequential_scan": {
      "uses": 4500,
      "avg_reward": 0.72
    },
    "index_scan": {
      "uses": 8200,
      "avg_reward": 0.91
    },
    "hash_join": {
      "uses": 2530,
      "avg_reward": 0.85
    }
  }
}
```

---

### GET /api/ai/detailed

Get full details of the AI layer, including the learning engine, immune system state, and temporal index data.

**Response (200 OK):**

```json
{
  "success": true,
  "learning_engine": {
    "status": "active",
    "model_version": 3,
    "training_samples": 12450,
    "last_trained": "2026-02-22 06:00:00"
  },
  "immune_system": {
    "status": "active",
    "threats_detected": 14,
    "threats_blocked": 12,
    "false_positives": 1,
    "rules_active": 28
  },
  "temporal_index": {
    "status": "active",
    "indexed_intervals": 3200,
    "query_patterns": 156
  }
}
```

---

### GET /api/ai/index-suggestions

Get AI-generated index recommendations based on query patterns and workload analysis.

**Response (200 OK):**

```json
{
  "success": true,
  "suggestions": [
    {
      "table": "orders",
      "columns": ["customer_id", "order_date"],
      "type": "BTREE",
      "reason": "Frequent range queries on order_date filtered by customer_id",
      "estimated_improvement": "45%"
    },
    {
      "table": "products",
      "columns": ["category"],
      "type": "BTREE",
      "reason": "High-frequency equality lookups on category",
      "estimated_improvement": "30%"
    }
  ]
}
```

---

### GET /api/ai/blocked-queries

List queries that have been blocked by the AI firewall and are pending review.

**Response (200 OK):**

```json
{
  "success": true,
  "blocked_queries": [
    {
      "id": "bq-001",
      "query": "DROP TABLE users",
      "reason": "Destructive operation flagged by immune system",
      "blocked_at": "2026-02-22 10:05:00",
      "user": "bob"
    },
    {
      "id": "bq-002",
      "query": "SELECT * FROM users WHERE 1=1 UNION SELECT * FROM credentials",
      "reason": "Potential SQL injection pattern detected",
      "blocked_at": "2026-02-22 10:12:30",
      "user": "guest"
    }
  ]
}
```

---

### POST /api/ai/approve/:id

Approve a previously blocked query, allowing it to execute.

**URL Parameters:**

| Parameter | Type   | Description                        |
|-----------|--------|------------------------------------|
| `id`      | string | ID of the blocked query to approve |

**Example:** `POST /api/ai/approve/bq-001`

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Query 'bq-001' approved and executed",
  "result": {
    "success": true,
    "message": "Table 'users' dropped successfully"
  }
}
```

**Response (404 Not Found):**

```json
{
  "success": false,
  "error": "Blocked query 'bq-999' not found"
}
```

---

## Query History

### GET /api/history

Retrieve recent query history with execution timing.

**Query Parameters:**

| Parameter | Type    | Default | Description                         |
|-----------|---------|---------|-------------------------------------|
| `limit`   | integer | 50      | Maximum number of records to return |

**Example:** `GET /api/history?limit=10`

**Response (200 OK):**

```json
{
  "success": true,
  "records": [
    {
      "query": "SELECT * FROM users",
      "database": "mydb",
      "user": "chronos",
      "execution_time_ms": 3.2,
      "timestamp": "2026-02-22 10:30:15",
      "status": "success"
    },
    {
      "query": "INSERT INTO orders (id, product) VALUES (101, 'Widget')",
      "database": "mydb",
      "user": "alice",
      "execution_time_ms": 5.8,
      "timestamp": "2026-02-22 10:29:58",
      "status": "success"
    },
    {
      "query": "SELECT * FROM nonexistent",
      "database": "mydb",
      "user": "bob",
      "execution_time_ms": 1.1,
      "timestamp": "2026-02-22 10:29:40",
      "status": "error"
    }
  ],
  "qps": 42.7
}
```

---

## Schedules

### GET /api/schedules

List all scheduled jobs.

**Response (200 OK):**

```json
{
  "success": true,
  "schedules": [
    {
      "name": "daily_cleanup",
      "interval": "86400",
      "sql": "DELETE FROM logs WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 DAY)",
      "last_run": "2026-02-21 00:00:00",
      "next_run": "2026-02-22 00:00:00",
      "status": "active"
    },
    {
      "name": "hourly_stats",
      "interval": "3600",
      "sql": "INSERT INTO stats_snapshot SELECT COUNT(*) FROM orders",
      "last_run": "2026-02-22 10:00:00",
      "next_run": "2026-02-22 11:00:00",
      "status": "active"
    }
  ]
}
```

---

### POST /api/schedules

Create a new scheduled job.

**Request Body:**

```json
{
  "name": "weekly_backup_check",
  "interval": "604800",
  "sql": "SELECT COUNT(*) FROM backup_log WHERE status = 'failed'"
}
```

The `interval` is specified in seconds.

| Interval Value | Duration |
|----------------|----------|
| `60`           | 1 minute |
| `3600`         | 1 hour   |
| `86400`        | 1 day    |
| `604800`       | 1 week   |

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Schedule 'weekly_backup_check' created successfully"
}
```

**Response (400 Bad Request):**

```json
{
  "success": false,
  "error": "Schedule 'weekly_backup_check' already exists"
}
```

---

### DELETE /api/schedules/:name

Delete a scheduled job.

**URL Parameters:**

| Parameter | Type   | Description                   |
|-----------|--------|-------------------------------|
| `name`    | string | Name of the schedule to delete|

**Example:** `DELETE /api/schedules/weekly_backup_check`

**Response (200 OK):**

```json
{
  "success": true,
  "message": "Schedule 'weekly_backup_check' deleted successfully"
}
```

---

## Replication

### GET /api/replication/status

Retrieve the current replication status.

**Response (200 OK):**

```json
{
  "success": true,
  "replication_enabled": true,
  "role": "primary",
  "replicas": [
    {
      "host": "replica1.example.com:2501",
      "status": "in_sync",
      "lag_ms": 12
    },
    {
      "host": "replica2.example.com:2501",
      "status": "in_sync",
      "lag_ms": 25
    }
  ]
}
```

**Response when replication is disabled:**

```json
{
  "success": true,
  "replication_enabled": false
}
```

---

## Schema

### GET /api/schema/full

Retrieve the full database schema, suitable for rendering ER diagrams. Returns all tables with their columns, primary keys, and foreign key relationships.

**Response (200 OK):**

```json
{
  "success": true,
  "tables": [
    {
      "name": "users",
      "columns": [
        {
          "name": "id",
          "type": "INT",
          "nullable": false
        },
        {
          "name": "username",
          "type": "VARCHAR(255)",
          "nullable": false
        },
        {
          "name": "email",
          "type": "VARCHAR(255)",
          "nullable": true
        }
      ],
      "primary_keys": ["id"],
      "foreign_keys": []
    },
    {
      "name": "orders",
      "columns": [
        {
          "name": "id",
          "type": "INT",
          "nullable": false
        },
        {
          "name": "user_id",
          "type": "INT",
          "nullable": false
        },
        {
          "name": "product",
          "type": "VARCHAR(255)",
          "nullable": false
        },
        {
          "name": "total",
          "type": "FLOAT",
          "nullable": true
        }
      ],
      "primary_keys": ["id"],
      "foreign_keys": [
        {
          "column": "user_id",
          "references_table": "users",
          "references_column": "id"
        }
      ]
    }
  ]
}
```

---

## Response Format

All API responses follow the **ChronosResult** structure:

```typescript
interface ChronosResult {
  success: boolean;
  data?: {
    columns: string[];
    rows: string[][];
  };
  message?: string;
  error?: string;
  row_count?: number;
}
```

| Field       | Type       | Description                                              |
|-------------|------------|----------------------------------------------------------|
| `success`   | `boolean`  | Whether the operation completed successfully             |
| `data`      | `object`   | Present for queries that return tabular results          |
| `data.columns` | `string[]` | Column names for the result set                      |
| `data.rows` | `string[][]` | Row data, where each row is an array of string values |
| `message`   | `string`   | Human-readable success message for non-SELECT operations |
| `error`     | `string`   | Error description when `success` is `false`              |
| `row_count` | `number`   | Number of rows returned or affected                      |

### HTTP Status Codes

| Code | Meaning               | When Used                                      |
|------|------------------------|-------------------------------------------------|
| 200  | OK                     | Request succeeded                               |
| 400  | Bad Request            | Invalid request body, SQL syntax error, or invalid parameters |
| 401  | Unauthorized           | Missing or invalid authentication               |
| 403  | Forbidden              | User lacks required role/permissions             |
| 404  | Not Found              | Resource (table, view, user, etc.) not found     |
| 500  | Internal Server Error  | Unexpected server-side failure                   |

### Content Types

- **Request:** `application/json`
- **Response:** `application/json` (except CSV export which returns `text/csv`)
