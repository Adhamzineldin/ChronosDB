# FrancoDB Architecture & Glossary

## Complete System Documentation

This document provides a comprehensive reference for all modules, components, abbreviations, and professional terminology used in FrancoDB.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Core Modules](#core-modules)
3. [Storage Layer](#storage-layer)
4. [Buffer Management](#buffer-management)
5. [Catalog System](#catalog-system)
6. [Parser & Lexer](#parser--lexer)
7. [Execution Engine](#execution-engine)
8. [Recovery System](#recovery-system)
9. [Concurrency Control](#concurrency-control)
10. [Network Layer](#network-layer)
11. [Professional Database Terminology](#professional-database-terminology)
12. [Franco (Arabic) SQL Keywords](#franco-arabic-sql-keywords)

---

## System Overview

FrancoDB is a **disk-based relational database management system (RDBMS)** built from scratch in C++20. It implements core database concepts including:

- **ACID Transactions** - Atomicity, Consistency, Isolation, Durability
- **WAL (Write-Ahead Logging)** - All changes logged before applied
- **ARIES Recovery** - Industry-standard crash recovery algorithm
- **B+ Tree Indexes** - Efficient ordered data access
- **Buffer Pool Management** - In-memory page caching
- **Time Travel Queries** - Point-in-time data recovery (AS OF / RECOVER TO)

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         CLIENT APPLICATIONS                          │
│                    (Shell, Python, JavaScript, C++)                  │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          NETWORK LAYER                               │
│              FrancoServer, ConnectionHandler, Protocol               │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          PARSER & LEXER                              │
│                Lexer → Parser → AST (Statement Tree)                 │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        EXECUTION ENGINE                              │
│    DDL Executor │ DML Executor │ Query Executors │ Tx Executor       │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┼───────────────┐
                    ▼               ▼               ▼
┌──────────────────────┐ ┌──────────────────┐ ┌──────────────────────┐
│      CATALOG         │ │  BUFFER POOL     │ │   LOG MANAGER        │
│  (Table Metadata)    │ │  (Page Cache)    │ │   (WAL / Recovery)   │
└──────────────────────┘ └──────────────────┘ └──────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         STORAGE LAYER                                │
│          DiskManager │ TableHeap │ TablePage │ B+Tree Index          │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                            DISK FILES                                │
│              .francodb (data) │ .log (WAL) │ .idx (indexes)          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Core Modules

### 1. `src/buffer/` - Buffer Pool Management

The buffer pool is the **heart of any disk-based database**. It manages a fixed-size pool of memory pages that cache disk data.

| File | Purpose |
|------|---------|
| `buffer_pool_manager.cpp/h` | Main BPM - manages page fetching, pinning, flushing |
| `partitioned_buffer_pool_manager.h` | High-concurrency BPM with multiple partitions (shards) |
| `lru_replacer.cpp/h` | **LRU (Least Recently Used)** page eviction policy |
| `clock_replacer.cpp/h` | **Clock/Second-Chance** page eviction policy |
| `i_buffer_manager.h` | Interface for polymorphic buffer pool usage |

**Key Concepts:**

| Term | Definition |
|------|------------|
| **Page** | Fixed-size block of data (default 4KB), the unit of I/O |
| **Pin** | Mark a page as "in use" - cannot be evicted |
| **Unpin** | Release a page - may be evicted if needed |
| **Dirty** | Page has been modified in memory, not yet written to disk |
| **Eviction** | Removing a page from memory to make room for another |
| **Hit Rate** | % of page requests satisfied from memory (vs disk) |

---

### 2. `src/storage/` - Storage Layer

Physical data storage on disk.

#### `storage/disk/`

| File | Purpose |
|------|---------|
| `disk_manager.cpp/h` | Low-level disk I/O: read/write pages, allocate pages |

**Key Concepts:**

| Term | Definition |
|------|------------|
| **Page ID** | Unique identifier for a page on disk |
| **Extent** | Group of contiguous pages (optimization) |
| **Free List** | Track of deallocated pages for reuse |

#### `storage/page/`

| File | Purpose |
|------|---------|
| `page.h` | Base page class with header (page_id, pin_count, dirty flag) |
| `table_page.h` | Slotted page format for variable-length tuples |
| `b_plus_tree_page.h` | Base class for B+ tree nodes |
| `b_plus_tree_internal_page.cpp/h` | Internal (non-leaf) B+ tree nodes |
| `b_plus_tree_leaf_page.cpp/h` | Leaf B+ tree nodes containing actual data |

**Slotted Page Layout:**
```
┌─────────────────────────────────────────────────────┐
│ HEADER: page_id │ prev_page │ next_page │ tuple_cnt │
├─────────────────────────────────────────────────────┤
│ SLOT ARRAY: [offset1, len1] [offset2, len2] ...     │
├─────────────────────────────────────────────────────┤
│                    FREE SPACE                        │
├─────────────────────────────────────────────────────┤
│ ← TUPLE DATA (grows backwards from end of page)     │
└─────────────────────────────────────────────────────┘
```

#### `storage/table/`

| File | Purpose |
|------|---------|
| `table_heap.cpp/h` | Linked list of TablePages, manages tuple CRUD |
| `table_page.cpp/h` | Single page of tuples with slot directory |
| `tuple.cpp/h` | Single row of data with values |

#### `storage/index/`

| File | Purpose |
|------|---------|
| `b_plus_tree.cpp/h` | B+ Tree index implementation |

**B+ Tree Properties:**
- All data in **leaf nodes** (internal nodes only have keys)
- Leaves are **linked** for range scans
- Self-balancing: O(log n) insert/delete/search
- High **fanout** = fewer disk reads

---

### 3. `src/catalog/` - Catalog (Data Dictionary)

Metadata about all database objects.

| File | Purpose |
|------|---------|
| `catalog.cpp/h` | Central registry of tables and indexes |
| `table_metadata.h` | Metadata for a single table (name, schema, heap) |
| `index_info.cpp/h` | Metadata for a single index |
| `schema.h` | Column definitions (name, type, constraints) |
| `column.h` | Single column definition |

**Key Concepts:**

| Term | Definition |
|------|------------|
| **Schema** | Definition of table structure (columns, types) |
| **OID** | Object Identifier - unique ID for catalog objects |
| **Data Dictionary** | System tables storing metadata |

---

### 4. `src/parser/` - Parser & Lexer

Converts SQL text into executable statement trees.

| File | Purpose |
|------|---------|
| `lexer.cpp/h` | **Lexer/Tokenizer** - breaks SQL into tokens |
| `parser.cpp/h` | **Parser** - builds AST from tokens |
| `token.h` | Token type definitions |
| `statement.h` | Statement classes (SelectStatement, InsertStatement, etc.) |

**Compilation Pipeline:**
```
SQL String → Lexer → Tokens → Parser → AST → Execution Engine
```

**Example:**
```sql
SELECT name FROM users WHERE id = 5;
```
Becomes tokens:
```
[SELECT] [IDENTIFIER:name] [FROM] [IDENTIFIER:users] [WHERE] [IDENTIFIER:id] [EQUALS] [NUMBER:5] [SEMICOLON]
```

---

### 5. `src/execution/` - Execution Engine

Executes parsed statements.

| File | Purpose |
|------|---------|
| `execution_engine.cpp/h` | Main dispatcher - routes statements to executors |
| `executor_context.h` | Context passed to all executors (bpm, catalog, txn) |
| `dml_executor.cpp/h` | Data Manipulation: SELECT, INSERT, UPDATE, DELETE |
| `ddl_executor.cpp/h` | Data Definition: CREATE, DROP, ALTER |
| `database_executor.cpp/h` | CREATE/USE DATABASE |
| `transaction_executor.cpp/h` | BEGIN, COMMIT, ROLLBACK |
| `user_executor.cpp/h` | User management, authentication |
| `system_executor.cpp/h` | SHOW, DESCRIBE, STATUS |

#### `execution/executors/` - Volcano Model Executors

| File | Purpose |
|------|---------|
| `abstract_executor.h` | Base class with `Init()` and `Next(Tuple*)` interface |
| `seq_scan_executor.cpp/h` | Full table scan |
| `index_scan_executor.cpp/h` | Index-based lookup |
| `insert_executor.cpp/h` | Tuple insertion |
| `update_executor.cpp/h` | Tuple modification |
| `delete_executor.cpp/h` | Tuple removal |
| `join_executor.cpp/h` | Nested loop join |
| `query_executors.cpp/h` | GROUP BY, aggregations, ORDER BY |

**Volcano/Iterator Model:**
```cpp
executor.Init();           // Initialize state
while (executor.Next(&tuple)) {  // Pull one tuple at a time
    // Process tuple
}
```

---

### 6. `src/recovery/` - Recovery System

ARIES-based crash recovery and time travel.

| File | Purpose |
|------|---------|
| `log_manager.cpp/h` | WAL management - append/flush log records |
| `log_record.h` | Log record structure (LSN, type, data) |
| `recovery_manager.cpp/h` | ARIES recovery: Analysis, Redo, Undo |
| `checkpoint_manager.cpp/h` | Periodic checkpointing |
| `snapshot_manager.h` | Time travel query support (AS OF) |
| `transaction.h` | Transaction state tracking |
| `transaction_manager.cpp/h` | Transaction lifecycle management |

**ARIES Recovery Phases:**

| Phase | Purpose |
|-------|---------|
| **Analysis** | Scan log to find active transactions and dirty pages |
| **Redo** | Replay ALL logged changes (even committed) |
| **Undo** | Rollback uncommitted transactions |

**WAL Protocol:**
> Before writing any data page to disk, all log records up to that page's LSN must be flushed.

**Key Concepts:**

| Term | Definition |
|------|------------|
| **LSN** | Log Sequence Number - unique ID for each log record |
| **WAL** | Write-Ahead Logging - log before data |
| **Checkpoint** | Snapshot of system state to speed up recovery |
| **ATT** | Active Transaction Table |
| **DPT** | Dirty Page Table |

---

### 7. `src/concurrency/` - Concurrency Control

| File | Purpose |
|------|---------|
| `lock_manager.h` | 2PL (Two-Phase Locking) lock management |
| `transaction.h` | Transaction state and isolation level |

**Lock Modes:**

| Mode | Symbol | Compatibility |
|------|--------|---------------|
| **Shared (S)** | Read lock | Compatible with other S locks |
| **Exclusive (X)** | Write lock | Incompatible with all locks |

**Two-Phase Locking (2PL):**
1. **Growing Phase**: Acquire locks, never release
2. **Shrinking Phase**: Release locks, never acquire

---

### 8. `src/network/` - Network Layer

Client-server communication.

| File | Purpose |
|------|---------|
| `franco_server.cpp/h` | TCP server, connection handling |
| `connection_handler.cpp/h` | Per-client session management |
| `database_registry.cpp/h` | Multi-database instance management |
| `session_context.h` | Per-session state (current_db, user, role) |
| `protocol.h` | Wire protocol definitions |

---

### 9. `src/common/` - Common Utilities

| File | Purpose |
|------|---------|
| `config.h` | Configuration constants (page size, pool size) |
| `config_manager.cpp/h` | Runtime configuration |
| `exception.h` | Custom exception types |
| `rid.cpp/h` | **RID** - Record Identifier (page_id + slot_id) |
| `value.cpp/h` | Type-safe value wrapper |
| `thread_pool.h` | Worker thread pool |
| `auth_manager.cpp/h` | Authentication and authorization |

---

## Professional Database Terminology

### A

| Term | Definition |
|------|------------|
| **ACID** | Atomicity, Consistency, Isolation, Durability - transaction guarantees |
| **ARIES** | Algorithms for Recovery and Isolation Exploiting Semantics |
| **ATT** | Active Transaction Table - tracks in-flight transactions |

### B

| Term | Definition |
|------|------------|
| **B+ Tree** | Balanced tree index with all data in leaves |
| **Buffer Pool** | In-memory cache of disk pages |
| **Btree Fanout** | Number of children per internal node |

### C

| Term | Definition |
|------|------------|
| **Catalog** | Database metadata repository (data dictionary) |
| **Checkpoint** | Consistent snapshot for faster recovery |
| **Clustered Index** | Index where leaf nodes contain actual data rows |
| **Concurrency Control** | Managing simultaneous access |
| **Cursor** | Iterator over query results |

### D

| Term | Definition |
|------|------------|
| **DDL** | Data Definition Language (CREATE, DROP, ALTER) |
| **DML** | Data Manipulation Language (SELECT, INSERT, UPDATE, DELETE) |
| **DPT** | Dirty Page Table - pages modified in memory |
| **Durability** | Committed data survives crashes |

### E

| Term | Definition |
|------|------------|
| **Eviction** | Removing a page from buffer pool |
| **Extent** | Group of contiguous pages |
| **Executor** | Component that runs query operations |

### F

| Term | Definition |
|------|------------|
| **Fanout** | Branching factor of a tree |
| **Flush** | Write dirty pages to disk |
| **Foreign Key** | Reference to another table's primary key |

### H

| Term | Definition |
|------|------------|
| **Heap** | Unordered collection of pages (TableHeap) |
| **Hit Rate** | Cache efficiency metric |

### I

| Term | Definition |
|------|------------|
| **Index** | Data structure for fast lookups |
| **Isolation** | Transactions don't see each other's uncommitted changes |

### L

| Term | Definition |
|------|------------|
| **Latch** | Short-term lock for internal synchronization |
| **Lock** | Long-term protection for transaction isolation |
| **Log Record** | Single entry in the WAL |
| **LRU** | Least Recently Used (eviction policy) |
| **LSN** | Log Sequence Number |

### M

| Term | Definition |
|------|------------|
| **MVCC** | Multi-Version Concurrency Control |
| **Master Record** | File storing last checkpoint LSN |

### N

| Term | Definition |
|------|------------|
| **Non-clustered Index** | Index separate from data |

### O

| Term | Definition |
|------|------------|
| **OID** | Object Identifier |
| **OLAP** | Online Analytical Processing |
| **OLTP** | Online Transaction Processing |

### P

| Term | Definition |
|------|------------|
| **Page** | Fixed-size unit of storage (4KB default) |
| **Pin** | Hold a page in memory |
| **Primary Key** | Unique identifier for each row |

### Q

| Term | Definition |
|------|------------|
| **Query Plan** | Execution strategy for a query |
| **Query Optimizer** | Chooses best execution plan |

### R

| Term | Definition |
|------|------------|
| **Redo** | Reapply logged changes during recovery |
| **RID** | Record Identifier (page_id, slot_id) |
| **Rollback** | Undo a transaction's changes |

### S

| Term | Definition |
|------|------------|
| **Schema** | Table structure definition |
| **Slotted Page** | Variable-length tuple storage format |
| **Snapshot** | Point-in-time view of data |

### T

| Term | Definition |
|------|------------|
| **Transaction** | Unit of work with ACID properties |
| **Tuple** | Single row in a table |
| **2PL** | Two-Phase Locking |

### U

| Term | Definition |
|------|------------|
| **Undo** | Reverse uncommitted changes |
| **Unpin** | Release a page back to buffer pool |

### W

| Term | Definition |
|------|------------|
| **WAL** | Write-Ahead Logging |

---

## Franco (Arabic) SQL Keywords

FrancoDB supports dual-language SQL syntax. Here are all supported keywords:

### Data Definition (DDL)

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `2E3MEL` | `CREATE` | Create object |
| `GADWAL` | `TABLE` | Table |
| `DATABASE` | `DATABASE` | Database |
| `FEHRIS` | `INDEX` | Index |
| `2EMSA7` | `DROP` | Drop object |
| `3ADEL` | `ALTER` | Modify object |
| `ADAF` | `ADD` | Add column |
| `GHAYER_ESM` | `RENAME` | Rename |
| `3AMOD` | `COLUMN` | Column |

### Data Types

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `RAKAM` | `INT` | Integer |
| `GOMLA` | `VARCHAR/TEXT` | String |
| `BOOL` | `BOOL` | Boolean |
| `TARE5` | `DATE` | Date |
| `KASR` | `DECIMAL` | Decimal |

### Data Manipulation (DML)

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `2E5TAR` | `SELECT` | Query data |
| `MEN` | `FROM` | From table |
| `LAMA` | `WHERE` | Filter condition |
| `EMLA` | `INSERT` | Insert data |
| `GOWA` | `INTO` | Into table |
| `ELKEYAM` | `VALUES` | Value list |
| `3ADEL` | `UPDATE` | Update data |
| `5ALY` | `SET` | Set values |
| `2EMSA7` | `DELETE` | Delete data |

### Constraints

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `ASASI` / `MOFTA7` | `PRIMARY KEY` | Primary key |
| `5AREGY` | `FOREIGN` | Foreign key |
| `WAHED` | `UNIQUE` | Unique constraint |
| `FADY` | `NULL` | Null value |
| `MESH` | `NOT` | Negation |
| `EFRADY` | `DEFAULT` | Default value |
| `TAZAYED` | `AUTO_INCREMENT` | Auto increment |

### Aggregations & Grouping

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `MAGMO3A` | `GROUP` | Group by |
| `B` | `BY` | By |
| `ETHA` | `HAVING` | Having clause |
| `3ADD` | `COUNT` | Count |
| `MAG3MO3` | `SUM` | Sum |
| `MOTO3ASET` | `AVG` | Average |
| `ASGAR` | `MIN` | Minimum |
| `AKBAR` | `MAX` | Maximum |

### Ordering & Limiting

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `RATEB` | `ORDER` | Order by |
| `TASE3DI` | `ASC` | Ascending |
| `TANAZOLI` | `DESC` | Descending |
| `7ADD` | `LIMIT` | Limit rows |
| `EBDA2MEN` | `OFFSET` | Skip rows |

### Joins

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `ENTEDAH` | `JOIN` | Join |
| `DA5ELY` | `INNER` | Inner join |
| `SHMAL` | `LEFT` | Left join |
| `YAMEN` | `RIGHT` | Right join |
| `5AREGY` | `OUTER` | Outer join |
| `TAQATE3` | `CROSS` | Cross join |
| `3ALA` | `ON` | Join condition |

### Transactions

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `2EBDA2` | `BEGIN` | Start transaction |
| `2AKED` | `COMMIT` | Commit transaction |
| `2ERGA3` | `ROLLBACK` | Rollback transaction |

### Recovery / Time Travel

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `CHECKPOINT` / `SAVE` | `CHECKPOINT` | Create checkpoint |
| `ERGA3` | `RECOVER` | Time travel |
| `ELA` | `TO` | To (destination) |
| `A5ER` / `ASLHA` | `LATEST` | Latest state |
| `DELWA2TY` | `NOW` | Current time |
| `7ALY` | `CURRENT` | Current state |
| `K` | `AS` | As (for AS OF) |

### Logical Operators

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `WE` | `AND` | Logical AND |
| `AW` | `OR` | Logical OR |
| `FE` | `IN` | In list |

### System Commands

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `WARENY` | `SHOW` | Show objects |
| `WASF` | `DESCRIBE` | Describe table |
| `ANAMEEN` | `WHOAMI` | Current user |
| `7ALAH` | `STATUS` | System status |
| `2ESTA5DEM` | `USE` | Use database |

### Boolean Literals

| Franco/Arabic | English | Purpose |
|--------------|---------|---------|
| `AH` | `TRUE` | True |
| `LA` | `FALSE` | False |

---

## File Organization

```
FrancoDB/
├── src/
│   ├── include/          # Header files (.h)
│   │   ├── buffer/       # Buffer pool headers
│   │   ├── catalog/      # Catalog headers
│   │   ├── common/       # Shared utilities
│   │   ├── concurrency/  # Lock manager, transactions
│   │   ├── execution/    # Executor headers
│   │   ├── network/      # Server headers
│   │   ├── parser/       # Lexer/Parser headers
│   │   ├── recovery/     # WAL/Recovery headers
│   │   └── storage/      # Storage layer headers
│   ├── buffer/           # Buffer pool implementations
│   ├── catalog/          # Catalog implementations
│   ├── cmd/              # Entry points (server, shell, setup)
│   ├── common/           # Shared implementations
│   ├── execution/        # Execution engine
│   ├── network/          # Network layer
│   ├── parser/           # Lexer & Parser
│   ├── recovery/         # Recovery system
│   └── storage/          # Storage layer
├── test/                 # Test suites
├── docs/                 # Documentation
├── libaries/             # Client libraries (Python, JS)
├── examples/             # Example code
└── installers/           # Installation scripts
```

---

## Quick Reference Card

### Essential Commands

```sql
-- Create database
2E3MEL DATABASE mydb;
CREATE DATABASE mydb;

-- Use database
2ESTA5DEM mydb;
USE mydb;

-- Create table
2E3MEL GADWAL users (id RAKAM ASASI, name GOMLA);
CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR);

-- Insert data
EMLA GOWA users ELKEYAM (1, 'Ahmed');
INSERT INTO users VALUES (1, 'Ahmed');

-- Query data
2E5TAR * MEN users LAMA id = 1;
SELECT * FROM users WHERE id = 1;

-- Time travel
2E5TAR * MEN users AS OF '22/01/2026 10:00';
RECOVER TO LATEST;
RECOVER TO '22/01/2026 09:00';

-- Transactions
2EBDA2;      -- BEGIN
2AKED;       -- COMMIT
2ERGA3;      -- ROLLBACK
```

---

*Document Version: 2.0 | Last Updated: January 22, 2026*

