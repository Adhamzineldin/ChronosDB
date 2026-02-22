# Plan: 18 Major Features + Documentation for ChronosDB

## Context
ChronosDB already has a mature SQL engine with JOINs, GROUP BY, ORDER BY, LIMIT/OFFSET, aggregates, subqueries, B+ tree + hash indexes, views, EXPLAIN/EXPLAIN ANALYZE, transactions, time travel, AI layer (UCB1, immune system, anomaly detection), RBAC, web admin panel, and a benchmarking suite. This plan adds 18 new features to make it a comprehensive enterprise-grade database system, plus full documentation.

---

## Phase 1: SQL Engine Enhancements (Backend C++)

### Feature 1: Common Table Expressions (CTEs)
**What**: `WITH temp AS (SELECT ...) SELECT * FROM temp` — define temporary named result sets.

**Tokens**: `WITH` (already have `AS`)
**Keywords**: `WITH` / `MA3` (Arabic: مع)
**Statement**: New `CTEDefinition` struct inside `SelectStatement`

**Files to modify**:
- `src/include/parser/token.h` — Add `WITH` token
- `src/parser/lexer.cpp` — Add `"WITH"/"MA3"` keyword mapping
- `src/parser/parser.cpp` — In `ParseQuery()`, if token is `WITH`, parse CTE defs then parse SELECT
- `src/include/parser/statement.h` — Add `cte_definitions_` to `SelectStatement`:
  ```cpp
  struct CTEDefinition {
      std::string name;
      std::unique_ptr<SelectStatement> query;
  };
  std::vector<CTEDefinition> cte_definitions_;
  ```
- `src/execution/dml_executor.cpp` — In `Select()`, before main query: execute each CTE, store results in a temporary map `cte_name → ResultSet`. When resolving table_name, check CTEs first.

**No new executor needed** — handled within DMLExecutor::Select().

---

### Feature 2: Window Functions
**What**: `ROW_NUMBER() OVER (PARTITION BY col ORDER BY col)`, `RANK()`, `DENSE_RANK()`, `LAG(col, offset)`, `LEAD(col, offset)`

**Tokens**: `OVER`, `PARTITION`, `ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`
**Keywords**: `OVER`/`FAWK`, `PARTITION`/`TAGSEE2`, `ROW_NUMBER`/`RAQAM_SAFF`, `RANK`/`MARTABA`, `LAG`/`SABE2`, `LEAD`/`TALE`

**Files to modify**:
- `src/include/parser/token.h` — Add 7 new tokens
- `src/parser/lexer.cpp` — Add keyword mappings
- `src/include/parser/statement.h` — Add to `SelectStatement`:
  ```cpp
  struct WindowFunction {
      std::string function; // ROW_NUMBER, RANK, etc.
      std::string argument_column; // for LAG/LEAD
      int offset = 1; // for LAG/LEAD
      std::vector<std::string> partition_by;
      std::vector<OrderByClause> order_by;
      std::string alias;
  };
  std::vector<WindowFunction> window_functions_;
  ```
- `src/parser/parser.cpp` — In column parsing, detect `FUNC_NAME(...)` followed by `OVER`. Parse `OVER (PARTITION BY ... ORDER BY ...)`.
- `src/execution/dml_executor.cpp` — After base SELECT result, apply window functions:
    1. Group rows by PARTITION BY columns
    2. Sort within partitions by ORDER BY
    3. Compute function values (ROW_NUMBER=sequential counter, RANK=rank with gaps, etc.)
    4. Add computed columns to result set

---

### Feature 3: Table Partitioning
**What**: `CREATE TABLE ... PARTITION BY RANGE(col) (PARTITION p1 VALUES LESS THAN 100, ...)` — split large tables into physical partitions.

**Tokens**: `PARTITION`, `PARTITIONS`, `RANGE_KW`, `LESS`, `THAN`
**Keywords**: `PARTITION`/`TAGSEE2`, `RANGE`/`MADAA`, `LESS`/`A2AL`, `THAN`/`MEN`

**Files to modify**:
- `src/include/parser/token.h` — Add partition-related tokens
- `src/parser/lexer.cpp` — Add keyword mappings
- `src/include/parser/statement.h` — Add partition info to `CreateStatement`:
  ```cpp
  struct PartitionDef {
      std::string name;
      Value upper_bound; // For RANGE
  };
  std::string partition_column_;
  std::string partition_type_; // "RANGE" or "HASH"
  std::vector<PartitionDef> partitions_;
  int hash_partition_count_ = 0;
  ```
- `src/include/catalog/catalog.h` — Add `PartitionInfo` struct, track partition→table mappings
- `src/catalog/catalog.cpp` — `CreatePartitionedTable()` creates N physical sub-tables + metadata
- `src/execution/ddl_executor.cpp` — Handle CREATE TABLE with partitions
- `src/execution/dml_executor.cpp` — Route INSERT/SELECT/UPDATE/DELETE to correct partition
    - INSERT: compute partition from key value, insert into correct sub-table
    - SELECT: scan all partitions (or prune based on WHERE clause)

**Catalog serialization**: `PARTITION <parent_table> <column> <type> <partition_name> <bound>` lines

---

### Feature 4: Export/Import (CSV)
**What**: `EXPORT TABLE name TO 'file.csv';` and `IMPORT FROM 'file.csv' INTO table;`

**Tokens**: `EXPORT`, `IMPORT`
**Keywords**: `EXPORT`/`SADDR`, `IMPORT`/`ESTRAD`
**Statements**: `ExportStatement`, `ImportStatement` with `StatementType::EXPORT`, `StatementType::IMPORT`

**Files to modify**:
- `src/include/parser/token.h` — Add `EXPORT`, `IMPORT` tokens
- `src/parser/lexer.cpp` — Add keyword mappings
- `src/parser/parser.cpp` — Parse `EXPORT TABLE name TO 'path';` and `IMPORT FROM 'path' INTO table;`
- `src/include/parser/statement.h` — Add statement types + classes:
  ```cpp
  class ExportStatement : public Statement {
      std::string table_name_;
      std::string file_path_;
      std::string format_; // "CSV" default
  };
  class ImportStatement : public Statement {
      std::string file_path_;
      std::string table_name_;
      std::string format_;
  };
  ```
- `src/execution/execution_engine.cpp` — Register in dispatch map
- New `src/execution/executors/io_executor.cpp` + `src/include/execution/io_executor.h`:
    - `Export()`: SELECT * FROM table, write header + rows as CSV
    - `Import()`: Read CSV, parse rows, INSERT into table (batch for performance)

**HTTP handler routes**:
- `POST /api/tables/:name/export` — Returns CSV as downloadable file
- `POST /api/tables/:name/import` — Accepts CSV upload

---

### Feature 5: Backup/Restore
**What**: `BACKUP DATABASE TO 'path';` and `RESTORE DATABASE FROM 'path';`

Unlike time-travel (which replays WAL within the same instance), backup creates a portable archive of all database files that can be copied to another machine.

**Tokens**: `BACKUP`, `RESTORE`
**Keywords**: `BACKUP`/`N5A_E7TYATY`, `RESTORE`/`ESTER3A3`
**Statements**: `BackupStatement`, `RestoreStatement`

**Files to modify**:
- `src/include/parser/token.h` — Add tokens
- `src/parser/lexer.cpp` — Add keywords
- `src/parser/parser.cpp` — Parse `BACKUP DATABASE TO 'path';`, `RESTORE DATABASE FROM 'path';`
- `src/include/parser/statement.h` — Statement classes
- `src/execution/execution_engine.cpp` — Register in dispatch map
- New method in `database_executor.cpp`:
    - `Backup()`: Flush buffer pool, copy data files + catalog + WAL into a tarball/directory
    - `Restore()`: Shutdown access to DB, copy files from backup, reload catalog

---

### Feature 6: Stored Procedures
**What**: `CREATE PROCEDURE name(param1 INT, param2 VARCHAR) BEGIN ... END;` with IF/ELSE, WHILE, DECLARE, SET.

**Tokens**: `PROCEDURE`, `CALL`, `BEGIN_BLOCK`, `END_BLOCK`, `DECLARE`, `IF_KW`, `ELSE_KW`, `WHILE_KW`, `RETURN_KW`
**Keywords**: `PROCEDURE`/`EGRA2`, `CALL`/`NADY`, `DECLARE`/`3ARREF`, `IF`/`LAW`, `ELSE`/`GHEER`, `WHILE`/`TALAMA`, `RETURN`/`ARGA3`

**Statement**: `CreateProcedureStatement`, `CallProcedureStatement`

**Procedure body storage**: Store as raw SQL text in catalog. On CALL:
1. Parse procedure body into list of statements
2. Create local variable scope
3. Execute statements sequentially with IF/WHILE control flow

**Files to modify**:
- `src/include/parser/token.h` — Add tokens
- `src/parser/lexer.cpp` — Add keywords
- `src/parser/parser.cpp` — `ParseCreateProcedure()`, `ParseCall()`
- `src/include/parser/statement.h` — Statement classes
- `src/include/catalog/catalog.h` — Add `procedures_` map, `ProcedureInfo` struct
- `src/catalog/catalog.cpp` — CRUD for procedures + serialization
- New `src/execution/executors/procedure_executor.cpp` + header:
    - `CreateProcedure()`, `CallProcedure()`, `ExecuteBlock()` (recursive for IF/WHILE)
- `src/execution/execution_engine.cpp` — Register in dispatch map

---

### Feature 7: Triggers
**What**: `CREATE TRIGGER name BEFORE/AFTER INSERT/UPDATE/DELETE ON table FOR EACH ROW BEGIN ... END;`

**Tokens**: `TRIGGER`, `BEFORE`, `AFTER`, `EACH`, `ROW_KW`, `FOR_KW`, `NEW_KW`, `OLD_KW`
**Keywords**: `TRIGGER`/`MESH3AL`, `BEFORE`/`2ABL`, `AFTER`/`BA3D`, `EACH`/`KOL`, `ROW`/`SAFF`, `FOR`/`LEKOL`, `NEW`/`GEDEED`, `OLD`/`2ADEEM`

**Files to modify**:
- `src/include/parser/token.h` — Add tokens
- `src/parser/lexer.cpp` — Add keywords
- `src/parser/parser.cpp` — `ParseCreateTrigger()`
- `src/include/parser/statement.h` — `CreateTriggerStatement`, `DropTriggerStatement`
- `src/include/catalog/catalog.h` — `TriggerInfo` struct, `triggers_` map per table
- `src/catalog/catalog.cpp` — CRUD for triggers + serialization
- `src/execution/dml_executor.cpp` — Before/after INSERT/UPDATE/DELETE, check triggers and execute:
  ```cpp
  // In Insert():
  auto triggers = catalog_->GetTriggers(table_name, "BEFORE", "INSERT");
  for (auto& trigger : triggers) ExecuteTriggerBody(trigger, new_row, nullptr);
  // ... do actual insert ...
  triggers = catalog_->GetTriggers(table_name, "AFTER", "INSERT");
  for (auto& trigger : triggers) ExecuteTriggerBody(trigger, new_row, nullptr);
  ```

---

### Feature 8: Query History
**What**: `SHOW HISTORY;` or `SHOW HISTORY LIMIT 50;` — tracks all executed queries with timing.

**Tokens**: `HISTORY` (reuse existing `LIMIT`)
**Keywords**: `HISTORY`/`TARE5_ESTE3LAMAT`
**Statement**: `ShowHistoryStatement` with optional limit

**Implementation**:
- `src/include/common/query_history.h` — Singleton ring buffer:
  ```cpp
  struct QueryRecord {
      std::string sql;
      std::string user;
      std::string database;
      bool success;
      double elapsed_ms;
      uint64_t timestamp_us;
  };
  class QueryHistory {
      std::deque<QueryRecord> records_;
      size_t max_records_ = 10000;
  public:
      static QueryHistory& Instance();
      void Record(const QueryRecord& r);
      std::vector<QueryRecord> GetRecent(size_t limit);
  };
  ```
- `src/execution/execution_engine.cpp` — In `Execute()`, wrap with timing and record:
  ```cpp
  auto start = high_resolution_clock::now();
  auto result = handler(stmt, session, txn);
  auto elapsed = duration<double, milli>(high_resolution_clock::now() - start).count();
  QueryHistory::Instance().Record({sql, user, db, result.success, elapsed, now_us});
  ```
- Register `SHOW_HISTORY` in dispatch map → `system_executor_->ShowHistory()`
- HTTP handler: `GET /api/history` route

---

### Feature 9: Scheduled Jobs
**What**: `CREATE SCHEDULE "name" EVERY 60 SECONDS DO 'SQL';` — run SQL on a timer.

**Tokens**: `SCHEDULE`, `EVERY`, `SECONDS`, `MINUTES`, `HOURS`, `DO_KW`
**Keywords**: `SCHEDULE`/`GADWAL_ZAMANY`, `EVERY`/`KOL`, `SECONDS`/`SAWANY`, `MINUTES`/`DA2AYE2`, `HOURS`/`SA3AT`, `DO`/`NAFFEZ`

**Files**:
- New `src/include/common/scheduler.h`:
  ```cpp
  struct ScheduledJob {
      std::string name;
      std::string sql;
      int interval_seconds;
      bool enabled;
      uint64_t last_run;
      int run_count;
  };
  class Scheduler {
      std::vector<ScheduledJob> jobs_;
      std::thread worker_thread_;
      std::atomic<bool> running_;
  public:
      void Start(ExecutionEngine* engine);
      void Stop();
      void AddJob(const ScheduledJob& job);
      void RemoveJob(const std::string& name);
      std::vector<ScheduledJob> GetJobs();
  };
  ```
- New statements: `CreateScheduleStatement`, `DropScheduleStatement`, `ShowSchedulesStatement`
- Catalog persistence: `SCHEDULE <name> <interval_s> <sql_base64>` lines
- Dispatch map: `CREATE_SCHEDULE`, `DROP_SCHEDULE`, `SHOW_SCHEDULES`
- HTTP handler: `GET /api/schedules`, `POST /api/schedules`, `DELETE /api/schedules/:name`

---

### Feature 10: Replication (Primary-Replica)
**What**: WAL-based replication. Primary streams WAL to replicas. Replicas are read-only.

**Implementation approach**: Full replication with automatic failover.

**Files**:
- New `src/include/network/replication.h`:
  ```cpp
  class ReplicationManager {
  public:
      enum class Role { STANDALONE, PRIMARY, REPLICA };

      void SetRole(Role role);
      Role GetRole() const;

      // Primary side
      void AddReplica(const std::string& host, int port);
      void RemoveReplica(const std::string& host, int port);
      void StreamWAL(const std::vector<uint8_t>& log_record);

      // Replica side
      void ConnectToPrimary(const std::string& host, int port);
      void ApplyWAL(const std::vector<uint8_t>& log_record);

      // Health & Failover
      void StartHealthCheck(int interval_ms = 5000);
      void PromoteToMaster(); // Replica becomes primary
      bool IsPrimaryAlive();

      struct ReplicationStatus {
          Role role;
          int replica_count;
          uint64_t last_wal_lsn; // Log Sequence Number
          uint64_t replica_lag_bytes;
          bool primary_alive;
          std::vector<std::string> replica_hosts;
      };
      ReplicationStatus GetStatus();
  };
  ```
- New `src/network/replication.cpp` — TCP connection management, WAL streaming, heartbeat thread
- `src/recovery/log_manager.cpp` — After writing WAL record, call `ReplicationManager::StreamWAL()`
- Health check: Dedicated thread pings primary every N seconds. If primary unresponsive for 3 consecutive checks, trigger automatic failover (promote replica).
- New SQL: `SET REPLICATION ROLE PRIMARY;`, `SET REPLICATION ROLE REPLICA;`, `ADD REPLICA 'host:port';`, `REMOVE REPLICA 'host:port';`, `SHOW REPLICATION STATUS;`, `PROMOTE;` (manual failover)
- New tokens: `REPLICATION`, `REPLICA`, `PRIMARY_KW`, `PROMOTE`

---

### Feature 11: Automatic Index Advisor
**What**: Analyzes query patterns and suggests CREATE INDEX commands. `SHOW INDEX SUGGESTIONS;`

**Implementation**: Extends existing AI layer.
- Track columns used in WHERE clauses with their frequency
- Track columns used in JOIN conditions
- Compare query time with/without index (using EXPLAIN data)
- Suggest indexes for frequently-filtered columns that lack indexes

**Files**:
- `src/include/ai/index_advisor.h`:
  ```cpp
  struct IndexSuggestion {
      std::string table;
      std::string column;
      std::string reason;
      int query_count; // How many queries would benefit
      std::string suggested_sql; // e.g., "CREATE INDEX idx_users_age ON users(age);"
      std::string index_type; // "BTREE" or "HASH"
  };
  class IndexAdvisor {
      std::map<std::string, int> column_access_counts_; // "table.column" → count
  public:
      void RecordAccess(const std::string& table, const std::string& column, const std::string& op);
      std::vector<IndexSuggestion> GetSuggestions(Catalog* catalog);
  };
  ```
- `src/ai/index_advisor.cpp` — Implementation
- Integrate into `DMLExecutor::Select()` — after WHERE evaluation, record column accesses
- New statement: `ShowIndexSuggestionsStatement` → `SHOW INDEX SUGGESTIONS;`
- HTTP handler: `GET /api/ai/index-suggestions`

---

### Feature 12: Anomaly-based Query Firewall
**What**: Block suspicious queries. Admin approval workflow.

**Extends** existing `ImmuneSystem` in `src/ai/immune/`.

**Files**:
- `src/include/ai/query_firewall.h`:
  ```cpp
  struct BlockedQuery {
      int id;
      std::string sql;
      std::string user;
      std::string reason; // "SQL_INJECTION", "RATE_LIMIT", "UNUSUAL_PATTERN"
      uint64_t timestamp;
      bool approved;
  };
  class QueryFirewall {
      std::vector<BlockedQuery> blocked_;
      std::map<std::string, int> user_rate_; // queries per minute
      int rate_limit_ = 100;
  public:
      bool Check(const std::string& sql, const std::string& user); // returns false if blocked
      void Approve(int query_id);
      std::vector<BlockedQuery> GetBlocked();
  };
  ```
- Integrate into `ExecutionEngine::Execute()` — check before dispatch
- New SQL: `SHOW BLOCKED QUERIES;`, `APPROVE QUERY <id>;`
- HTTP handler: `GET /api/ai/blocked-queries`, `POST /api/ai/approve/:id`

---

## Phase 2: Shell Enhancement

### Feature 13: Shell Autocomplete
**What**: Tab completion for SQL keywords, table names, column names, database names.

**Implementation**: Platform-specific readline-like functionality.

**Files to modify**:
- `src/cmd/shell/shell.cpp` — Major refactor of REPL loop:
    - Windows: Use `ReadConsoleInput` for raw key capture, custom tab handler
    - Linux: Use readline library if available, fallback to custom
    - On connection, fetch table/column/database names from server
    - On Tab press: match partial input against completions, show options
    - New shell commands: `.tables`, `.columns tablename`, `.databases`
- `src/include/parser/lexer.h` — Expose `GetKeywords()` (already exists)
- Consider adding a new protocol message type for metadata requests: `META:TABLES`, `META:COLUMNS:tablename`

**Completion sources**:
1. SQL keywords (from Lexer::GetKeywords())
2. Table names (from `SHOW TABLES` result)
3. Column names (from `DESCRIBE table` result, contextual)
4. Database names (from `SHOW DATABASES` result)

---

## Phase 3: Frontend Features (React/TypeScript)

All frontend features communicate with the C++ HTTP handler via `/api/*` endpoints.

### Feature 14: Visual Query Builder
**What**: GUI to build SELECT queries by clicking — select tables, columns, filters, joins, ordering.

**New component**: `web-admin/client/src/components/QueryBuilder.tsx`
**New page**: Add `'query-builder'` to `Page` type

**UI Design**:
1. **Table selector**: Dropdown of available tables (from `GET /api/tables`)
2. **Column picker**: Checkboxes for columns (from `GET /api/tables/:name/schema`)
3. **WHERE builder**: Dynamic rows with column dropdown + operator dropdown + value input
4. **JOIN builder**: Add join with table + condition inputs
5. **GROUP BY / ORDER BY**: Drag columns into grouping/ordering slots
6. **LIMIT/OFFSET**: Number inputs
7. **SQL Preview**: Live-generated SQL shown in a code block
8. **Execute button**: Runs the generated SQL, shows results in table

**API calls**: Uses existing endpoints — `GET /api/tables`, `GET /api/tables/:name/schema`, `POST /api/query`

---

### Feature 15: Real-time Dashboard
**What**: Live-updating dashboard with charts for throughput, connections, AI metrics.

**New component**: `web-admin/client/src/components/RealtimeDashboard.tsx`
**New page**: Add `'realtime'` to `Page` type

**Implementation**: Polling every 2-5 seconds (simpler than WebSocket, already have HTTP handler).

**Dashboard panels**:
1. **Query Throughput**: Line chart (queries/sec over last 5 min) — from `GET /api/history` (new endpoint)
2. **Active Sessions**: Count card — from `GET /api/status`
3. **Buffer Pool Stats**: Hit rate gauge + eviction count — new `GET /api/buffer-stats` endpoint
4. **AI Metrics**: UCB1 arm rewards chart — from `GET /api/ai/detailed`
5. **Recent Queries**: Scrolling table — from `GET /api/history?limit=20`
6. **Anomaly Alerts**: Real-time anomaly notifications — from `GET /api/ai/anomalies`

**New HTTP handler endpoints**:
- `GET /api/history` — Returns recent query records
- `GET /api/buffer-stats` — Returns buffer pool hit/miss/eviction stats

**Charts**: Use Chart.js via CDN for professional animated charts. Add `<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>` to `index.html`.

---

### Feature 16: ER Diagram Viewer
**What**: Auto-generate entity-relationship diagrams from table schemas and foreign keys.

**New component**: `web-admin/client/src/components/ERDiagram.tsx`
**New page**: Add `'er-diagram'` to `Page` type

**Implementation**:
- Fetch all tables + schemas: `GET /api/tables`, then `GET /api/tables/:name/schema` for each
- Parse foreign key info from schema (already stored in `CreateStatement::ForeignKey`)
- Render using SVG:
    - Each table = rectangular box with columns listed
    - Foreign keys = lines/arrows between tables
    - Draggable positioning
    - Color-code: Primary keys gold, foreign keys blue, nullable gray

**New HTTP handler endpoint**:
- `GET /api/schema/full` — Returns all tables with columns, types, constraints, foreign keys in one response (avoids N+1 API calls)

---

### Feature 17: Query Plan Visualizer
**What**: Graphical tree view of EXPLAIN output instead of just a table.

**New component**: `web-admin/client/src/components/QueryPlanVisualizer.tsx`
Integrated into **SQLEditor** component (shown after EXPLAIN queries).

**Implementation**:
- When EXPLAIN query is executed, parse the result into a tree structure
- Render as:
    - Tree nodes with boxes connected by lines
    - Each node shows: operation, estimated rows, AI insight
    - For EXPLAIN ANALYZE: show actual rows + timing with color-coded performance bars
    - Green = fast, yellow = moderate, red = slow
- Node types: SEQ SCAN, INDEX SCAN, HASH SCAN, FILTER, JOIN, AGGREGATE, SORT

---

### Feature 18: Natural Language to SQL
**What**: Type natural language query → get SQL. "Show me all users older than 25" → `SELECT * FROM users WHERE age > 25`

**New component**: `web-admin/client/src/components/NLQueryInput.tsx`
Integrated into **SQLEditor** component.

**Implementation**: Local rule-based pattern matching (no external API dependency).

**Pattern matching engine** (in TypeScript, frontend-only):
- "show/get/find/list all X" → `SELECT * FROM X`
- "show/get X where/with Y > Z" → `SELECT * FROM X WHERE Y > Z`
- "count/how many X" → `SELECT COUNT(*) FROM X`
- "average/mean of Y in/from X" → `SELECT AVG(Y) FROM X`
- "X ordered/sorted by Y" → `SELECT * FROM X ORDER BY Y`
- "top N X" → `SELECT * FROM X LIMIT N`
- "X joined with Y on Z" → `SELECT * FROM X JOIN Y ON Z`
- "distinct Y from X" → `SELECT DISTINCT Y FROM X`
- Uses table/column names from schema for entity recognition and validation
- Fuzzy matching for column names (case-insensitive, underscore-tolerant)
- Supports both English and transliterated Arabic patterns

---

## Phase 4: Documentation

### Feature 19: Comprehensive Documentation
**What**: Full documentation covering all features, SQL syntax, API reference, architecture.

**Files to create**:
- `docs/README.md` — Overview and quick start
- `docs/SQL_REFERENCE.md` — Complete SQL syntax reference with examples for ALL features:
    - DDL: CREATE TABLE, ALTER TABLE, DROP TABLE, CREATE INDEX, CREATE VIEW, etc.
    - DML: SELECT, INSERT, UPDATE, DELETE with all clauses
    - CTEs, Window Functions, Subqueries
    - Procedures, Triggers
    - System: SHOW, DESCRIBE, EXPLAIN, BACKUP/RESTORE, EXPORT/IMPORT
    - Scheduled Jobs, Replication
- `docs/CQL_REFERENCE.md` — Arabic keyword alternatives (Franco-Arab)
- `docs/API_REFERENCE.md` — All REST API endpoints with request/response examples
- `docs/ARCHITECTURE.md` — Component diagrams, data flow, storage layout
- `docs/ADMIN_GUIDE.md` — Installation, configuration, web admin usage
- `docs/AI_LAYER.md` — UCB1 learning, immune system, index advisor, query firewall
- Update `src/cmd/shell/shell.cpp` `DisplayDynamicSyntax()` to include all new features

---

## Implementation Order (Dependency Graph)

### Batch 1 — No dependencies (can be parallelized)
1. **Query History** (Feature 8) — foundational for dashboard
2. **Export/Import** (Feature 4) — standalone
3. **Backup/Restore** (Feature 5) — standalone
4. **CTEs** (Feature 1) — SQL engine only

### Batch 2 — Depends on Batch 1
5. **Window Functions** (Feature 2) — SQL engine only
6. **Table Partitioning** (Feature 3) — storage + SQL
7. **Stored Procedures** (Feature 6) — parser + new executor
8. **Triggers** (Feature 7) — depends on procedure execution concepts

### Batch 3 — AI/Advanced features
9. **Index Advisor** (Feature 11) — extends AI layer
10. **Query Firewall** (Feature 12) — extends AI layer
11. **Scheduled Jobs** (Feature 9) — needs execution engine access
12. **Replication** (Feature 10) — needs WAL + network

### Batch 4 — Frontend features
13. **Visual Query Builder** (Feature 14)
14. **Real-time Dashboard** (Feature 15) — needs query history endpoint
15. **ER Diagram Viewer** (Feature 16)
16. **Query Plan Visualizer** (Feature 17)
17. **Natural Language to SQL** (Feature 18)

### Batch 5 — Polish
18. **Shell Autocomplete** (Feature 13)
19. **Documentation** (Feature 19)

---

## Key File Summary

### Backend C++ files to modify frequently:
| File | Purpose |
|------|---------|
| `src/include/parser/token.h` | Add ~30 new tokens |
| `src/parser/lexer.cpp` | Add ~60 new keyword mappings |
| `src/parser/parser.cpp` | Add ~10 new Parse methods |
| `src/include/parser/statement.h` | Add ~15 new statement types/classes |
| `src/execution/execution_engine.cpp` | Add ~15 new dispatch map entries |
| `src/include/catalog/catalog.h` | Add procedures_, triggers_, schedules_, partitions_ |
| `src/catalog/catalog.cpp` | Serialize/deserialize new metadata |
| `src/web/http_handler.cpp` | Add ~15 new API routes |
| `src/web/http_handler.h` | Declare new handler methods |

### New C++ files to create:
| File | Purpose |
|------|---------|
| `src/execution/executors/io_executor.cpp` | Export/Import |
| `src/include/execution/io_executor.h` | Export/Import header |
| `src/execution/executors/procedure_executor.cpp` | Stored procedures |
| `src/include/execution/procedure_executor.h` | Procedures header |
| `src/include/common/query_history.h` | Query history singleton |
| `src/common/query_history.cpp` | Query history impl |
| `src/include/common/scheduler.h` | Job scheduler |
| `src/common/scheduler.cpp` | Scheduler impl |
| `src/include/network/replication.h` | Replication manager |
| `src/network/replication.cpp` | Replication impl |
| `src/include/ai/index_advisor.h` | Index advisor |
| `src/ai/index_advisor.cpp` | Index advisor impl |
| `src/include/ai/query_firewall.h` | Query firewall |
| `src/ai/query_firewall.cpp` | Query firewall impl |

### Frontend files to create:
| File | Purpose |
|------|---------|
| `web-admin/client/src/components/QueryBuilder.tsx` | Visual query builder |
| `web-admin/client/src/components/RealtimeDashboard.tsx` | Live dashboard |
| `web-admin/client/src/components/ERDiagram.tsx` | ER diagram viewer |
| `web-admin/client/src/components/QueryPlanVisualizer.tsx` | EXPLAIN tree view |
| `web-admin/client/src/components/NLQueryInput.tsx` | Natural language input |

### Frontend files to modify:
| File | Purpose |
|------|---------|
| `web-admin/client/src/types.ts` | Add new page types + interfaces |
| `web-admin/client/src/api.ts` | Add ~15 new API methods |
| `web-admin/client/src/App.tsx` | Add route cases |
| `web-admin/client/src/components/Layout.tsx` | Add nav items |
| `web-admin/client/src/components/SQLEditor.tsx` | Integrate NL input + plan visualizer |
| `web-admin/client/src/components/Dashboard.tsx` | Add quick action cards |

### Documentation files to create:
| File | Purpose |
|------|---------|
| `docs/README.md` | Project overview |
| `docs/SQL_REFERENCE.md` | Complete SQL syntax |
| `docs/CQL_REFERENCE.md` | Arabic keywords |
| `docs/API_REFERENCE.md` | REST API docs |
| `docs/ARCHITECTURE.md` | System architecture |
| `docs/ADMIN_GUIDE.md` | Admin/ops guide |
| `docs/AI_LAYER.md` | AI features guide |

---

## Verification

### Build
```bash
cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
```

### Test Queries (after all features)
```sql
-- CTEs
WITH active AS (SELECT * FROM users WHERE active = 1)
SELECT * FROM active WHERE age > 25;

-- Window Functions
SELECT name, age, ROW_NUMBER() OVER (ORDER BY age DESC) FROM users;
SELECT dept, name, RANK() OVER (PARTITION BY dept ORDER BY salary DESC) FROM employees;

-- Table Partitioning
CREATE TABLE logs (id INT, ts INT, msg VARCHAR)
  PARTITION BY RANGE(ts) (
    PARTITION p2024 VALUES LESS THAN 1704067200,
    PARTITION p2025 VALUES LESS THAN 1735689600
  );

-- Export/Import
EXPORT TABLE users TO 'users_backup.csv';
IMPORT FROM 'users_backup.csv' INTO users_copy;

-- Backup/Restore
BACKUP DATABASE TO './backups/mydb_20260222';
RESTORE DATABASE FROM './backups/mydb_20260222';

-- Stored Procedures
CREATE PROCEDURE greet(name VARCHAR)
BEGIN
  DECLARE msg VARCHAR;
  SET msg = 'Hello';
  SELECT msg, name;
END;
CALL greet('World');

-- Triggers
CREATE TRIGGER log_insert AFTER INSERT ON users
FOR EACH ROW
BEGIN
  INSERT INTO audit_log VALUES (NEW.id, 'INSERT', NOW);
END;

-- Query History
SHOW HISTORY;
SHOW HISTORY LIMIT 10;

-- Scheduled Jobs
CREATE SCHEDULE cleanup EVERY 3600 SECONDS DO 'DELETE FROM logs WHERE ts < 1000000;';
SHOW SCHEDULES;
DROP SCHEDULE cleanup;

-- Index Advisor
SHOW INDEX SUGGESTIONS;

-- Query Firewall
SHOW BLOCKED QUERIES;
APPROVE QUERY 1;
```

### Frontend Testing
- Navigate to each new page (Query Builder, Real-time Dashboard, ER Diagram, etc.)
- Build visual query and verify SQL generation
- Check live dashboard updates
- Verify ER diagram renders tables and relationships
- Test NL to SQL with sample phrases
- Run EXPLAIN and verify tree visualization
