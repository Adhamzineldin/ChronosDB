# ChronosDB Architecture Documentation

## Table of Contents

1. [System Overview](#system-overview)
2. [Component Architecture](#component-architecture)
3. [Execution Flow](#execution-flow)
4. [Design Patterns](#design-patterns)
5. [Data Flow Diagram](#data-flow-diagram)

---

## System Overview

ChronosDB is a high-performance, multi-protocol database management system built with C++20. It is designed as a self-contained DBMS with persistent storage, crash recovery, role-based access control, and an embedded AI layer for adaptive query optimization and anomaly detection.

### Key Characteristics

- **Language**: C++20
- **Multi-Protocol Server**: Supports TEXT, JSON, and BINARY database protocols on a single port (default 2501)
- **Embedded HTTP Server**: Serves a React-based web administration interface on the same port
- **Query Language**: CQL (Chronos Query Language) -- full SQL support with Franco-Arab transliterated keyword alternatives
- **Storage Format**: Custom `.chronosdb` file format with page-level I/O
- **Default Configuration**: 256MB buffer pool, 4KB page size, port 2501

### Connection String Format

```
chronos://user:pass@host:port/database
```

Example:

```
chronos://chronos:root@localhost:2501/mydb
```

### Default Credentials

- Username: `chronos`
- Password: `root`

---

## Component Architecture

ChronosDB is organized into ten major subsystems, each with clearly defined responsibilities.

### 1. Storage Layer (`src/storage/`, `src/buffer/`)

The storage layer is responsible for all persistent data management, from raw disk I/O to structured tuple storage and indexing.

#### DiskManager

- Manages page-level I/O to `.chronosdb` database files
- Reads and writes fixed-size pages (4KB by default) at specific offsets
- Handles file creation, opening, and closing
- Provides the lowest-level abstraction over the filesystem

#### BufferPoolManager

- Maintains an in-memory page cache with a default capacity of 65,536 pages (256MB)
- Implements page pinning, unpinning, and dirty page tracking
- Supports multiple buffer replacement strategies
- Coordinates with the DiskManager for page fetches and flushes

#### Adaptive Partitioned Buffer Pool

- An advanced buffer pool variant that partitions memory across multiple regions
- Adapts partition sizes based on workload patterns
- Reduces contention under concurrent access

#### B+ Tree Index

- Provides ordered key lookup for range queries and sorted access
- Supports insert, delete, and search operations
- Stored persistently across pages managed by the buffer pool

#### Hash Index

- O(1) equality lookups via extendible hashing
- Dynamically splits buckets as data grows
- Optimized for point queries (e.g., `WHERE id = 42`)

#### TableHeap

- Slotted-page storage format for variable-length tuples
- Each page contains a header with a slot array pointing to tuple data
- Supports sequential scans and record-level operations

#### FreePageManager

- Tracks allocated and free pages within a database file
- Provides page allocation and deallocation services
- Prevents page leaks and enables space reclamation

---

### 2. Catalog (`src/catalog/`)

The catalog is the metadata backbone of the system.

- Stores schema definitions for tables, columns, indexes, and views
- Manages metadata for procedures, triggers, and schedules
- Serialized to a dedicated metadata section within the `.chronosdb` file
- Provides lookup APIs used by the execution engine to resolve table and column references during query processing

---

### 3. Parser (`src/parser/`)

The parser transforms raw query text into structured representations suitable for execution.

#### Lexer

- Tokenizes input SQL/CQL strings into a stream of tokens
- Supports 100+ token types including all standard SQL keywords
- Recognizes Franco-Arab transliterated keywords (e.g., `EKHTAR` for `SELECT`, `MEN` for `FROM`)
- Handles identifiers, string literals, numeric literals, and operators

#### Parser

- Implements a recursive descent parsing strategy
- Consumes the token stream produced by the lexer
- Produces a Statement AST (Abstract Syntax Tree) representing the parsed query
- Supports DDL, DML, transaction control, system commands, and procedural constructs

---

### 4. Execution Engine (`src/execution/`)

The execution engine is the central dispatching layer that routes parsed statements to specialized executors.

#### ExecutorFactory (`executor_registry.cpp`)

- Implements the Factory Pattern for executor creation
- Uses a dispatch map to route statement types to handlers
- Follows the Open/Closed Principle: new statement types can be added without modifying existing code

#### Specialized Executors

| Executor | Responsibility |
|---|---|
| **DDLExecutor** | CREATE, ALTER, DROP for tables, indexes, views, procedures, triggers |
| **DMLExecutor** | SELECT, INSERT, UPDATE, DELETE with full WHERE clause evaluation |
| **SystemExecutor** | SHOW, DESCRIBE, STATUS, EXPLAIN, ANALYZE, EXPORT/IMPORT, BACKUP/RESTORE |
| **UserExecutor** | CREATE USER, DROP USER, ALTER USER, GRANT, REVOKE |
| **DatabaseExecutor** | CREATE DATABASE, DROP DATABASE, USE, database-level operations |
| **TransactionExecutor** | BEGIN, COMMIT, ROLLBACK, CHECKPOINT, RECOVER |

Each executor follows the Single Responsibility Principle, encapsulating all logic for its domain.

---

### 5. Network Layer (`src/network/`)

The network layer handles all client communication and multi-database instance management.

#### ConnectionHandler

- Listens on a single TCP port (default 2501)
- Performs multi-protocol detection on each incoming connection
- Distinguishes between database protocols (TEXT, JSON, BINARY) and HTTP requests
- Routes database queries to the execution engine and HTTP requests to the web handler

#### DatabaseRegistry

- Manages multiple database instances within a single server process
- Maps database names to their corresponding storage and catalog instances
- Handles database creation, opening, closing, and deletion

#### SessionContext

- Maintains per-connection state including authenticated user, current database, and transaction status
- Tracks session-specific settings and temporary data

---

### 6. Recovery System (`src/recovery/`)

The recovery system ensures data durability and supports point-in-time restoration.

#### Write-Ahead Logging (WAL)

- All modifications are logged before being applied to data pages
- Ensures atomicity and durability even in the event of crashes
- Log records include before-images and after-images of modified data

#### CheckpointManager

- Periodically writes consistent state snapshots to reduce recovery time
- Coordinates buffer pool flushing with log truncation
- Reduces the volume of WAL records that must be replayed during recovery

#### TimeTravelEngine

- Enables point-in-time recovery using a reverse delta strategy
- Stores change deltas that can be applied backward to reconstruct past states
- Supports querying historical data at specific timestamps

#### SnapshotManager

- Creates full database state snapshots
- Used for backup, replication seeding, and disaster recovery
- Coordinates with the WAL to ensure snapshot consistency

---

### 7. Authentication and Authorization (`src/common/auth_manager.cpp`)

The auth system implements role-based access control (RBAC) with five privilege levels.

#### Roles

| Role | Privileges |
|---|---|
| **SUPERADMIN** | Full system access, user management, all databases |
| **ADMIN** | Database administration, DDL, DML, user management within scope |
| **NORMAL** | Standard DDL and DML operations |
| **READONLY** | SELECT queries only, no modifications |
| **DENIED** | No access (account disabled) |

#### Features

- Per-database role assignment: a user can have different roles on different databases
- Password-based authentication at connection time
- Permission checks enforced at the execution engine level before each operation

---

### 8. AI Layer (`src/ai/`)

The AI layer provides adaptive optimization, anomaly detection, and intelligent index management.

#### AIManager

- Central coordinator for all AI subsystems
- Manages lifecycle and configuration of AI components
- Provides a unified interface for the execution engine to interact with AI features

#### LearningEngine

- Implements UCB1 (Upper Confidence Bound) multi-armed bandit algorithm
- Dynamically selects between scan strategies (sequential scan, index scan, etc.) based on observed performance
- Adapts to workload changes over time through activity-based decay

#### QueryPlanOptimizer

- Performs multi-dimensional query plan optimization
- Considers factors such as estimated row counts, available indexes, and historical performance
- Works alongside the LearningEngine to improve plan selection

#### ImmuneSystem

- Anomaly detection inspired by biological immune systems
- Uses z-score thresholds to identify unusual query patterns or system behavior
- Triggers alerts and defensive measures when anomalies are detected

#### AnomalyDetector

- Rate-based anomaly detection for query traffic
- Monitors query rates and flags sudden spikes or unusual patterns
- Feeds information to the QueryFirewall for protective actions

#### TemporalIndexManager

- Analyzes time-based access patterns to optimize temporal queries
- Suggests and manages indexes tailored for time-series workloads

#### IndexAdvisor

- Monitors query patterns to suggest beneficial indexes
- Analyzes WHERE clause columns, JOIN conditions, and ORDER BY clauses
- Recommends index creation or removal based on workload analysis

#### QueryFirewall

- Rate limiting to prevent denial-of-service through excessive queries
- SQL injection detection using pattern matching
- XSS (Cross-Site Scripting) detection for web-facing inputs
- Blocks or flags suspicious queries before execution

#### DMLObserver

- Event-driven monitoring of all DML operations (INSERT, UPDATE, DELETE)
- Publishes events to the DMLObserverRegistry
- Enables AI components to react to data modification patterns in real time

---

### 9. Web Layer (`src/web/`)

The web layer embeds a full HTTP server within ChronosDB for administrative access.

#### HttpHandler

- Implements HTTP/1.1 request parsing and response generation
- Embedded directly into the main server, sharing port 2501
- Routes API calls to the appropriate database operations

#### Web Administration Interface

- Serves a React single-page application (SPA) from `web-admin/client/build`
- Provides a graphical interface for database management, query execution, and monitoring
- Communicates with the server through REST API endpoints

#### REST API

- Exposes endpoints for all database operations: schema management, data manipulation, user administration, and system monitoring
- Returns JSON responses for programmatic consumption

---

### 10. Replication (`src/network/replication.h`)

The replication subsystem enables high availability through data redundancy.

#### Primary-Replica Architecture

- One primary server accepts writes; replicas receive streaming updates
- WAL-based streaming replication transmits log records to replicas
- Replicas apply WAL records to maintain synchronized copies

#### Automatic Failover

- Health checks monitor primary and replica status
- Automatic promotion of a replica to primary when the current primary becomes unavailable
- Ensures minimal downtime during failures

---

## Execution Flow

The following describes the complete lifecycle of a query from client connection to result delivery.

### Step 1: Connection

A client connects via TCP to port 2501. The `ConnectionHandler` accepts the connection and reads the initial bytes to detect the protocol.

### Step 2: Protocol Detection

The handler distinguishes between:
- **Database protocols** (TEXT, JSON, BINARY): Routed to the query processing pipeline
- **HTTP requests**: Routed to the `HttpHandler` for web administration

### Step 3: Lexing

For database protocol queries, the `Lexer` tokenizes the raw query string into a stream of tokens. Both standard SQL keywords and CQL (Franco-Arab) keywords are recognized and normalized to a common internal representation.

### Step 4: Parsing

The `Parser` consumes the token stream using recursive descent parsing and produces a `Statement` AST node. The AST captures the full structure of the query including clauses, expressions, and identifiers.

### Step 5: Dispatch

The `ExecutionEngine` receives the AST and consults its internal dispatch map (registered by `ExecutorFactory`) to find the appropriate executor for the statement type.

### Step 6: Execution

The selected executor (e.g., `DMLExecutor` for a SELECT query) performs the operation:
- Resolves table and column names through the `Catalog`
- Reads and writes data pages through the `BufferPoolManager`
- Logs modifications through the `LogManager` (WAL)
- Checks permissions through the `AuthManager`
- Publishes events to the `DMLObserverRegistry` for AI monitoring

### Step 7: Response

The result is formatted according to the client's protocol (TEXT table, JSON object, or BINARY encoding) and sent back over the connection.

---

## Design Patterns

ChronosDB employs several well-established design patterns throughout its architecture.

### Factory Pattern

The `ExecutorFactory` in `src/execution/executor_registry.cpp` creates executor instances based on statement type. This decouples statement classification from executor instantiation.

### Dispatch Map

Statement routing uses a map of statement types to handler functions rather than switch statements. This adheres to the Open/Closed Principle: adding a new statement type requires registering a new entry in the map, not modifying a monolithic switch block.

### Singleton Pattern

Several components use the singleton pattern to ensure a single instance across the system:
- AI components (AIManager, LearningEngine, ImmuneSystem)
- QueryHistory
- Scheduler
- ReplicationManager

### Observer Pattern

The `DMLObserverRegistry` implements the observer pattern for AI event monitoring. When a DML operation occurs, all registered observers (AI components) are notified, enabling reactive behavior without tight coupling.

### Interface Abstraction

Abstract interfaces decouple high-level logic from storage implementation details:
- `IBufferManager`: Abstract interface for buffer pool operations
- `ITableStorage`: Abstract interface for table data access

These interfaces are defined in `src/include/storage/storage_interface.h` and enable testing with mock implementations and future storage engine alternatives.

---

## Data Flow Diagram

```
Client
  │
  ▼
ConnectionHandler (port 2501)
  │
  ├──── [HTTP Request] ──── HttpHandler ──── React SPA / REST API
  │
  └──── [DB Protocol] ──── Lexer ──── Parser ──── Statement AST
                                                       │
                                                       ▼
                                               ExecutionEngine
                                               (dispatch_map_)
                                                       │
                            ┌──────────────────────────┼──────────────────────────┐
                            │              │              │              │              │
                            ▼              ▼              ▼              ▼              ▼
                      DDLExecutor    DMLExecutor    SystemExecutor  UserExecutor  TransactionExecutor
                            │              │              │              │              │
                            ▼              ▼              ▼              ▼              ▼
                        Catalog    BufferPoolManager  AuthManager   AuthManager   LogManager
                            │              │                                         │
                            ▼              ▼                                         ▼
                       DiskManager   LogManager (WAL)                          CheckpointManager
                            │              │
                            ▼              ▼
                     .chronosdb file   WAL file
```

### AI Monitoring Data Flow

```
DMLExecutor ──── executes INSERT/UPDATE/DELETE
     │
     ▼
DMLObserverRegistry ──── notifies registered observers
     │
     ├──── LearningEngine (updates scan strategy statistics)
     ├──── AnomalyDetector (monitors query rates)
     ├──── IndexAdvisor (tracks column access patterns)
     └──── ImmuneSystem (checks for anomalous behavior)
```

### Recovery Data Flow

```
Normal Operation:
  Executor ──► LogManager (write WAL record) ──► BufferPoolManager (modify page)

Checkpoint:
  CheckpointManager ──► BufferPoolManager (flush dirty pages) ──► LogManager (truncate WAL)

Crash Recovery:
  Server Start ──► LogManager (read WAL) ──► Redo/Undo operations ──► Consistent State
```

---

## File Organization

```
src/
├── ai/                    # AI layer (optimization, anomaly detection)
├── buffer/                # Buffer pool manager implementations
├── catalog/               # Schema and metadata management
├── cmd/
│   ├── server/            # Server entry point (main.cpp)
│   ├── shell/             # Interactive shell (shell.cpp)
│   ├── setup/             # Initial setup utility
│   └── service/           # Windows service wrapper
├── common/                # Shared utilities, auth_manager, config
├── execution/             # Execution engine and specialized executors
├── include/               # Header files mirroring src/ structure
├── network/               # Network layer, connection handling, replication
├── parser/                # Lexer and parser
├── recovery/              # WAL, checkpoints, time travel, snapshots
├── storage/               # Disk manager, indexes, table heap, free page manager
└── web/                   # HTTP handler and web admin serving

web-admin/
└── client/                # React SPA for web administration

test/                      # Test suites organized by module
```
