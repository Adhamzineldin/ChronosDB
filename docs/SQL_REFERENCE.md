# ChronosDB SQL Reference

**ChronosDB** is a high-performance, multi-protocol database management system featuring CQL (Chronos Query Language) -- a SQL-compatible language with Franco-Arab keyword alternatives. This document is the authoritative reference for all supported SQL statements, syntax, data types, and features.

---

## Table of Contents

1. [Connecting to ChronosDB](#connecting-to-chronosdb)
2. [Data Types](#data-types)
3. [Franco-Arab Keyword Alternatives](#franco-arab-keyword-alternatives)
4. [DDL -- Data Definition Language](#ddl----data-definition-language)
   - [CREATE TABLE](#create-table)
   - [ALTER TABLE](#alter-table)
   - [DROP TABLE](#drop-table)
   - [TRUNCATE TABLE](#truncate-table)
   - [CREATE INDEX](#create-index)
   - [CREATE HASH INDEX](#create-hash-index)
   - [DROP INDEX](#drop-index)
   - [CREATE VIEW](#create-view)
   - [DROP VIEW](#drop-view)
5. [DML -- Data Manipulation Language](#dml----data-manipulation-language)
   - [INSERT](#insert)
   - [SELECT](#select)
   - [UPDATE](#update)
   - [DELETE](#delete)
6. [SELECT Clauses In Depth](#select-clauses-in-depth)
   - [WHERE](#where-clause)
   - [JOIN](#join-clause)
   - [GROUP BY and HAVING](#group-by-and-having)
   - [ORDER BY](#order-by)
   - [LIMIT and OFFSET](#limit-and-offset)
   - [DISTINCT](#distinct)
   - [Subqueries](#subqueries)
   - [Common Table Expressions (CTEs)](#common-table-expressions-ctes)
   - [Window Functions](#window-functions)
   - [Time Travel Queries (AS OF)](#time-travel-queries-as-of)
7. [Aggregate Functions](#aggregate-functions)
8. [Table Partitioning](#table-partitioning)
9. [Transactions](#transactions)
10. [Database Management](#database-management)
11. [User Management](#user-management)
12. [System Commands](#system-commands)
13. [Recovery and Checkpoints](#recovery-and-checkpoints)
14. [EXPLAIN / EXPLAIN ANALYZE](#explain--explain-analyze)
15. [Export and Import](#export-and-import)
16. [Backup and Restore](#backup-and-restore)
17. [Stored Procedures](#stored-procedures)
18. [Triggers](#triggers)
19. [Query History](#query-history)
20. [Scheduled Jobs](#scheduled-jobs)
21. [Replication](#replication)
22. [Index Advisor](#index-advisor)
23. [Query Firewall](#query-firewall)
24. [Server Control](#server-control)

---

## Connecting to ChronosDB

ChronosDB listens on port **2501** by default and supports TEXT, JSON, and BINARY protocols.

**Connection String Format:**

```
chronos://username:password@host:port/database
```

**Example:**

```
chronos://chronos:root@localhost:2501/mydb
```

**Default credentials:** username = `chronos`, password = `root`

---

## Data Types

ChronosDB supports the following data types:

| Type | Aliases | Franco-Arab | Size | Description |
|------|---------|-------------|------|-------------|
| `INT` | `INTEGER` | `RAKAM` | 4 bytes | 32-bit signed integer |
| `BIGINT` | -- | -- | 8 bytes | 64-bit signed integer |
| `DECIMAL` | `FLOAT`, `DOUBLE` | `KASR` | 8 bytes | Double-precision floating point |
| `VARCHAR(n)` | `TEXT`, `STRING` | `GOMLA` | Variable | Variable-length string (default 255) |
| `BOOLEAN` | `BOOL` | -- | 1 byte | `TRUE`/`FALSE` (`AH`/`LA`) |
| `DATE` | `DATETIME` | `TARE5` | 8 bytes | Timestamp value |

**Boolean Literals:**

| English | Franco-Arab | Value |
|---------|-------------|-------|
| `TRUE` | `AH` | 1 |
| `FALSE` | `LA` | 0 |

**NULL Literal:**

| English | Franco-Arab |
|---------|-------------|
| `NULL` | `FADY` |

---

## Franco-Arab Keyword Alternatives

ChronosDB supports Franco-Arab (Arabic transliteration using numbers for special Arabic characters) as alternatives to standard SQL keywords. All keywords are case-insensitive.

| English Keyword | Franco-Arab Alternative | Meaning |
|----------------|------------------------|---------|
| `SELECT` | `2E5TAR` | Choose |
| `FROM` | `MEN` | From |
| `WHERE` | `LAMA` | When/Where |
| `INSERT` | `EMLA` | Fill |
| `INTO` | `GOWA` | Inside |
| `VALUES` | `ELKEYAM` | The values |
| `UPDATE` | `3ADEL` | Modify |
| `SET` | `5ALY` | Make it |
| `DELETE` | `2EMSA7` | Erase |
| `CREATE` | `2E3MEL` | Make |
| `TABLE` | `GADWAL` | Table |
| `INDEX` | `FEHRIS` | Index |
| `USE` | `2ESTA5DEM` | Use |
| `USER` | `MOSTA5DEM` | User |
| `SHOW` | `WARENY` | Show me |
| `WHOAMI` | `ANAMEEN` | Who am I |
| `DESCRIBE` | `WASF` | Describe |
| `AND` | `WE` | And |
| `OR` | `AW` | Or |
| `IN` | `FE` | In |
| `ON` | `3ALA` | On |
| `NOT` | `MESH` | Not |
| `PRIMARY` | `ASASI` | Primary |
| `KEY` | `MOFTA7` | Key |
| `BEGIN` | `2EBDA2` | Start |
| `COMMIT` | `2AKED` | Confirm |
| `ROLLBACK` | `2ERGA3` | Go back |
| `GROUP` | `MAGMO3A` | Group |
| `HAVING` | `ETHA` | If |
| `ORDER` | `RATEB` | Arrange |
| `ASC` | `TASE3DI` | Ascending |
| `DESC` | `TANAZOLI` | Descending |
| `LIMIT` | `7ADD` | Limit |
| `OFFSET` | `EBDA2MEN` | Start from |
| `DISTINCT` | `MOTA3MEZ` | Distinct |
| `JOIN` | `ENTEDAH` | Join |
| `INNER` | `DA5ELY` | Inner |
| `LEFT` | `SHMAL` | Left |
| `RIGHT` | `YAMEN` | Right |
| `OUTER` | `5AREGY` | Outer |
| `CROSS` | `TAQATE3` | Intersect |
| `REFERENCES` | `YOSHEER` | Points to |
| `CASCADE` | `TATABE3` | Follow |
| `RESTRICT` | `MANE3` | Prevent |
| `DEFAULT` | `EFRADY` | Default |
| `UNIQUE` | `WAHED` | Unique |
| `CHECK` | `FA7S` | Check |
| `AUTO_INCREMENT` | `TAZAYED` | Increment |
| `COUNT` | `3ADD` | Count |
| `SUM` | `MAG3MO3` | Sum |
| `AVG` | `MOTOWASET` | Average |
| `MIN` | `ASGAR` | Smallest |
| `MAX` | `AKBAR` | Biggest |
| `TO` | `ELA` | To |
| `AS` | `K` | As |
| `IF` | `LAW` | If |
| `EXISTS` | `MAWGOOD` | Exists |
| `CHECKPOINT` | `SAVE` | Save |
| `RECOVER` | `ERGA3` | Return |
| `LATEST` | `A5ER` | Latest |
| `NOW` | `DELWA2TY` | Now |
| `WITH` | `MA3` | With |
| `OVER` | `FAWK` | Over |
| `PARTITION` | `TAQSEEM` | Division |
| `ROW_NUMBER` | `RAQAM_SAFF` | Row number |
| `RANK` | `MARTABA` | Rank |
| `LAG` | `SABE2` | Previous |
| `LEAD` | `TALE` | Next |
| `RANGE` | `MADAA` | Range |
| `EXPORT` | `SADDR` | Export |
| `IMPORT` | `ESTRAD` | Import |
| `BACKUP` | `N5A_E7TYATY` | Backup copy |
| `RESTORE` | `ESTER3A3` | Restore |
| `PROCEDURE` | `EGRA2` | Procedure |
| `CALL` | `NADY` | Call |
| `TRIGGER` | `MESHAGHAL` | Trigger |
| `BEFORE` | `QABL` | Before |
| `AFTER` | `BA3D` | After |
| `END` | `5ALAS` | Done |
| `STOP` | `WA2AF` | Stop |
| `SCHEDULE` | `GADWAL_ZAMANY` | Scheduled |
| `EVERY` | `KOL_MARRA` | Every time |
| `REPLICATION` | `NASAKHA` | Replication |
| `REPLICA` | `SOORAH` | Copy |
| `APPROVE` | `WAFE2` | Approve |
| `BLOCKED` | `MAHMEY` | Blocked |

**Example -- the same query in English and Franco-Arab:**

```sql
-- English
SELECT name, age FROM users WHERE age > 18 ORDER BY name ASC LIMIT 10;

-- Franco-Arab
2E5TAR name, age MEN users LAMA age > 18 RATEB B name TASE3DI 7ADD 10;
```

---

## DDL -- Data Definition Language

### CREATE TABLE

Creates a new table with column definitions and optional constraints.

**Syntax:**

```sql
CREATE TABLE table_name (
    column_name data_type [constraints...],
    column_name data_type [constraints...],
    [FOREIGN KEY (columns) REFERENCES ref_table (ref_columns) [ON DELETE action] [ON UPDATE action],]
    [CHECK [name] (expression),]
    ...
) [PARTITION BY RANGE|HASH (column) (partition_definitions)];
```

**Column Constraints:**

| Constraint | Franco-Arab | Description |
|-----------|-------------|-------------|
| `PRIMARY KEY` | `ASASI MOFTA7` | Marks column as primary key (implies NOT NULL) |
| `NOT NULL` | `MESH FADY` | Disallows NULL values |
| `NULL` | `FADY` | Explicitly allows NULL values |
| `UNIQUE` | `WAHED` | Ensures all values are unique |
| `AUTO_INCREMENT` | `TAZAYED` | Auto-incrementing integer (also: `SERIAL`) |
| `DEFAULT value` | `EFRADY value` | Sets a default value |
| `CHECK (expr)` | `FA7S (expr)` | Column-level check constraint |

**Examples:**

```sql
-- Basic table
CREATE TABLE users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    age INT,
    active BOOLEAN DEFAULT TRUE
);

-- With foreign key
CREATE TABLE orders (
    id INT PRIMARY KEY AUTO_INCREMENT,
    user_id INT NOT NULL,
    total DECIMAL,
    order_date DATE,
    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE ON UPDATE NO ACTION
);

-- With check constraint
CREATE TABLE products (
    id INT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL NOT NULL,
    quantity INT DEFAULT 0,
    CHECK price_check (price > 0)
);

-- Franco-Arab
2E3MEL GADWAL users (
    id RAKAM ASASI MOFTA7 TAZAYED,
    name GOMLA(100) MESH FADY,
    email GOMLA(255) WAHED,
    age RAKAM
);
```

**Referential Actions (for FOREIGN KEY):**

| Action | Franco-Arab | Description |
|--------|-------------|-------------|
| `CASCADE` | `TATABE3` | Propagate changes to child rows |
| `RESTRICT` | `MANE3` | Prevent the parent modification |
| `SET NULL` | -- | Set foreign key columns to NULL |
| `NO ACTION` | -- | Do nothing (default) |

---

### ALTER TABLE

Modifies the structure of an existing table.

**Syntax:**

```sql
-- Add a column
ALTER TABLE table_name ADD COLUMN column_name data_type [constraints];

-- Drop a column
ALTER TABLE table_name DROP COLUMN column_name;

-- Rename a column
ALTER TABLE table_name RENAME COLUMN old_name TO new_name;
```

**Examples:**

```sql
ALTER TABLE users ADD COLUMN phone VARCHAR(20);
ALTER TABLE users DROP COLUMN phone;
ALTER TABLE users RENAME COLUMN name TO full_name;
```

---

### DROP TABLE

Removes a table and all its data.

**Syntax:**

```sql
DROP TABLE [IF EXISTS] table_name;
```

**Examples:**

```sql
DROP TABLE orders;
DROP TABLE IF EXISTS temp_data;

-- Franco-Arab alternative (using DELETE TABLE)
2EMSA7 GADWAL orders;
```

---

### TRUNCATE TABLE

Deletes all rows from a table without dropping the table structure. Faster than `DELETE FROM table`.

**Syntax:**

```sql
TRUNCATE TABLE table_name;
```

---

### CREATE INDEX

Creates a B+ Tree index on a table column for faster lookups.

**Syntax:**

```sql
CREATE INDEX index_name ON table_name (column_name);
```

**Examples:**

```sql
CREATE INDEX idx_users_email ON users (email);

-- Franco-Arab
2E3MEL FEHRIS idx_users_email 3ALA users (email);
```

---

### CREATE HASH INDEX

Creates a hash-based index optimized for exact-match lookups.

**Syntax:**

```sql
CREATE HASH INDEX index_name ON table_name (column_name);
```

**Example:**

```sql
CREATE HASH INDEX idx_users_id ON users (id);
```

---

### DROP INDEX

Removes an index.

**Syntax:**

```sql
DROP INDEX [IF EXISTS] index_name;
```

**Example:**

```sql
DROP INDEX idx_users_email;
```

---

### CREATE VIEW

Creates a virtual table based on a SELECT query.

**Syntax:**

```sql
CREATE VIEW view_name AS SELECT ...;
```

**Examples:**

```sql
CREATE VIEW active_users AS SELECT id, name, email FROM users WHERE active = TRUE;

CREATE VIEW order_summary AS SELECT user_id, COUNT(*) FROM orders GROUP BY user_id;
```

---

### DROP VIEW

Removes a view.

**Syntax:**

```sql
DROP VIEW view_name;
```

**Example:**

```sql
DROP VIEW active_users;
```

---

## DML -- Data Manipulation Language

### INSERT

Inserts one or more rows into a table.

**Syntax:**

```sql
-- Single row
INSERT INTO table_name [(column_list)] VALUES (value_list);

-- Multi-row
INSERT INTO table_name [(column_list)] VALUES (row1), (row2), (row3);
```

**Examples:**

```sql
-- Single row
INSERT INTO users (name, email, age) VALUES ('Ahmed', 'ahmed@example.com', 25);

-- Multi-row insert
INSERT INTO users (name, email, age) VALUES
    ('Ahmed', 'ahmed@example.com', 25),
    ('Ali', 'ali@example.com', 30),
    ('Omar', 'omar@example.com', 22);

-- Without column list (must provide all columns in order)
INSERT INTO users VALUES (1, 'Ahmed', 'ahmed@example.com', 25, TRUE);

-- Franco-Arab
EMLA GOWA users (name, email, age) ELKEYAM ('Ahmed', 'ahmed@example.com', 25);
```

**Supported Value Types:**

| Type | Examples |
|------|---------|
| Integer | `42`, `-5` |
| Decimal | `3.14`, `-0.5` |
| String | `'hello world'` |
| Boolean | `TRUE` / `FALSE` (`AH` / `LA`) |

---

### SELECT

Retrieves data from one or more tables.

**Full Syntax:**

```sql
SELECT [DISTINCT] columns | * | aggregates | window_functions
FROM table_name
[JOIN type table ON condition]
[WHERE conditions]
[GROUP BY columns]
[HAVING conditions]
[ORDER BY columns [ASC|DESC]]
[LIMIT n [OFFSET m]]
[AS OF timestamp];
```

**Basic Examples:**

```sql
-- Select all columns
SELECT * FROM users;

-- Select specific columns
SELECT name, email FROM users;

-- With WHERE condition
SELECT * FROM users WHERE age > 18;

-- Franco-Arab
2E5TAR * MEN users LAMA age > 18;
```

---

### UPDATE

Modifies existing rows in a table.

**Syntax:**

```sql
UPDATE table_name SET column = value [WHERE conditions];
```

**Examples:**

```sql
UPDATE users SET age = 26 WHERE name = 'Ahmed';
UPDATE users SET active = FALSE WHERE age < 18;

-- Franco-Arab
3ADEL users 5ALY age = 26 LAMA name = 'Ahmed';
```

---

### DELETE

Removes rows from a table.

**Syntax:**

```sql
DELETE FROM table_name [WHERE conditions];
```

**Examples:**

```sql
DELETE FROM users WHERE active = FALSE;
DELETE FROM orders WHERE order_date < '01/01/2025 00:00';

-- Franco-Arab
2EMSA7 MEN users LAMA active = LA;
```

---

## SELECT Clauses In Depth

### WHERE Clause

Filters rows based on conditions. Supports multiple conditions joined by `AND`/`OR`.

**Operators:**

| Operator | Franco-Arab | Description |
|----------|-------------|-------------|
| `=` | `=` | Equal |
| `>` | `>` | Greater than |
| `<` | `<` | Less than |
| `>=` | `>=` | Greater than or equal |
| `<=` | `<=` | Less than or equal |
| `IN (values)` | `FE (values)` | Value in list |
| `AND` | `WE` | Logical AND |
| `OR` | `AW` | Logical OR |

**Examples:**

```sql
SELECT * FROM users WHERE age >= 18 AND active = TRUE;
SELECT * FROM products WHERE category IN ('Electronics', 'Books', 'Games');
SELECT * FROM users WHERE name = 'Ahmed' OR name = 'Ali';

-- Franco-Arab
2E5TAR * MEN users LAMA age >= 18 WE active = AH;
2E5TAR * MEN products LAMA category FE ('Electronics', 'Books', 'Games');
```

---

### JOIN Clause

Combines rows from two or more tables based on a related column.

**Supported Join Types:**

| Join Type | Franco-Arab | Description |
|-----------|-------------|-------------|
| `[INNER] JOIN` | `[DA5ELY] ENTEDAH` | Returns matching rows from both tables |
| `LEFT JOIN` | `SHMAL ENTEDAH` | Returns all rows from left table, matching from right |
| `RIGHT JOIN` | `YAMEN ENTEDAH` | Returns all rows from right table, matching from left |
| `CROSS JOIN` | `TAQATE3 ENTEDAH` | Cartesian product (no ON condition) |

**Syntax:**

```sql
SELECT columns
FROM table1
INNER JOIN table2 ON table1.col = table2.col
[LEFT JOIN table3 ON table2.col = table3.col];
```

**Examples:**

```sql
-- Inner join
SELECT users.name, orders.total
FROM users
INNER JOIN orders ON users.id = orders.user_id;

-- Left join
SELECT users.name, orders.total
FROM users
LEFT JOIN orders ON users.id = orders.user_id;

-- Multiple joins
SELECT u.name, o.total, p.name
FROM users
INNER JOIN orders ON users.id = orders.user_id
INNER JOIN products ON orders.product_id = products.id;

-- Cross join
SELECT * FROM colors CROSS JOIN sizes;
```

---

### GROUP BY and HAVING

Groups rows sharing common values and optionally filters groups.

**Syntax:**

```sql
SELECT column, aggregate(column)
FROM table
[WHERE conditions]
GROUP BY column [, column2, ...]
[HAVING aggregate_condition];
```

**Examples:**

```sql
-- Count users by city
SELECT city, COUNT(*) FROM users GROUP BY city;

-- Average salary by department with filter
SELECT department, AVG(salary)
FROM employees
GROUP BY department
HAVING AVG(salary) > 50000;

-- Multiple group columns
SELECT country, city, SUM(revenue)
FROM stores
GROUP BY country, city;
```

---

### ORDER BY

Sorts the result set by one or more columns.

**Syntax:**

```sql
SELECT columns FROM table ORDER BY column1 [ASC|DESC] [, column2 [ASC|DESC]];
```

| Direction | Franco-Arab Aliases | Description |
|-----------|-------------------|-------------|
| `ASC` | `TASE3DI`, `TALE3` | Ascending (default) |
| `DESC` | `TANAZOLI`, `NAZL` | Descending |

**Examples:**

```sql
SELECT * FROM users ORDER BY name ASC;
SELECT * FROM products ORDER BY price DESC, name ASC;

-- Franco-Arab
2E5TAR * MEN users RATEB B name TASE3DI;
```

---

### LIMIT and OFFSET

Restricts the number of returned rows and optionally skips rows.

**Syntax:**

```sql
SELECT columns FROM table LIMIT count [OFFSET skip];
```

**Examples:**

```sql
-- First 10 rows
SELECT * FROM users LIMIT 10;

-- Rows 11-20 (pagination)
SELECT * FROM users LIMIT 10 OFFSET 10;

-- Franco-Arab
2E5TAR * MEN users 7ADD 10 EBDA2MEN 10;
```

---

### DISTINCT

Removes duplicate rows from the result set.

**Syntax:**

```sql
SELECT DISTINCT columns FROM table;
```

**Examples:**

```sql
SELECT DISTINCT city FROM users;
SELECT DISTINCT department, role FROM employees;

-- Franco-Arab
2E5TAR MOTA3MEZ city MEN users;
```

---

### Subqueries

Subqueries can be used in the `IN` clause of a WHERE condition.

**Syntax:**

```sql
SELECT columns FROM table
WHERE column IN (SELECT column FROM other_table [WHERE conditions]);
```

**Examples:**

```sql
-- Find users who have placed orders
SELECT * FROM users
WHERE id IN (SELECT user_id FROM orders);

-- Find products never ordered
SELECT * FROM products
WHERE id IN (SELECT DISTINCT product_id FROM orders WHERE total > 100);
```

---

### Common Table Expressions (CTEs)

CTEs provide a way to define temporary named result sets within a query using the `WITH` clause.

**Syntax:**

```sql
WITH cte_name AS (
    SELECT ...
)
[, cte_name2 AS (
    SELECT ...
)]
SELECT ... FROM cte_name;
```

**Examples:**

```sql
-- Simple CTE
WITH active_users AS (
    SELECT id, name, email FROM users WHERE active = TRUE
)
SELECT * FROM active_users;

-- CTE with aggregation
WITH dept_salaries AS (
    SELECT department, AVG(salary) FROM employees GROUP BY department
)
SELECT * FROM dept_salaries;

-- Multiple CTEs
WITH
    high_earners AS (
        SELECT id, name, salary FROM employees WHERE salary > 80000
    ),
    recent_orders AS (
        SELECT user_id, COUNT(*) FROM orders GROUP BY user_id
    )
SELECT * FROM high_earners;

-- Franco-Arab
MA3 active_users K (
    2E5TAR id, name, email MEN users LAMA active = AH
)
2E5TAR * MEN active_users;
```

---

### Window Functions

Window functions perform calculations across a set of rows related to the current row, without collapsing the result set like aggregate functions do.

**Supported Window Functions:**

| Function | Franco-Arab | Description |
|----------|-------------|-------------|
| `ROW_NUMBER()` | `RAQAM_SAFF()` | Sequential row number within partition |
| `RANK()` | `MARTABA()` | Rank with gaps for ties |
| `DENSE_RANK()` | -- | Rank without gaps for ties |
| `LAG(col, offset)` | `SABE2(col, offset)` | Value from a preceding row |
| `LEAD(col, offset)` | `TALE(col, offset)` | Value from a following row |

**Syntax:**

```sql
SELECT
    columns,
    window_function() OVER (
        [PARTITION BY column [, ...]]
        [ORDER BY column [ASC|DESC] [, ...]]
    ) [AS alias]
FROM table;
```

**Examples:**

```sql
-- Row numbering
SELECT name, department,
    ROW_NUMBER() OVER (ORDER BY salary DESC) AS rank
FROM employees;

-- Row number within each department
SELECT name, department, salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank
FROM employees;

-- Rank with ties
SELECT name, score,
    RANK() OVER (ORDER BY score DESC) AS rank,
    DENSE_RANK() OVER (ORDER BY score DESC) AS dense_rank
FROM students;

-- Compare with previous row
SELECT name, salary,
    LAG(salary, 1) OVER (ORDER BY hire_date ASC) AS prev_salary,
    LEAD(salary, 1) OVER (ORDER BY hire_date ASC) AS next_salary
FROM employees;

-- Franco-Arab
2E5TAR name, department,
    RAQAM_SAFF() FAWK (TAQSEEM B department RATEB B salary TANAZOLI) K dept_rank
MEN employees;
```

---

### Time Travel Queries (AS OF)

Query historical data at a specific point in time.

**Syntax:**

```sql
SELECT columns FROM table [WHERE conditions] AS OF timestamp;
```

The timestamp can be:
- A date string: `'DD/MM/YYYY HH:MM'` or `'DD/MM/YYYY HH:MM:SS'`
- A raw microsecond timestamp number

**Examples:**

```sql
-- Query data as it was on a specific date
SELECT * FROM users AS OF '15/01/2026 14:30';

-- With conditions
SELECT name, balance FROM accounts WHERE id = 42 AS OF '01/12/2025 00:00:00';
```

---

## Aggregate Functions

Aggregate functions compute a single result from a set of input values.

| Function | Franco-Arab | Description |
|----------|-------------|-------------|
| `COUNT(column)` | `3ADD(column)` | Count of non-null values |
| `COUNT(*)` | `3ADD(*)` | Count of all rows |
| `SUM(column)` | `MAG3MO3(column)` | Sum of values |
| `AVG(column)` | `MOTOWASET(column)` | Average of values |
| `MIN(column)` | `ASGAR(column)` | Minimum value |
| `MAX(column)` | `AKBAR(column)` | Maximum value |

**Examples:**

```sql
SELECT COUNT(*) FROM users;
SELECT department, AVG(salary), MAX(salary), MIN(salary) FROM employees GROUP BY department;
SELECT SUM(total) FROM orders WHERE user_id = 1;

-- Franco-Arab
2E5TAR 3ADD(*) MEN users;
2E5TAR department, MOTOWASET(salary) MEN employees MAGMO3A B department;
```

---

## Table Partitioning

Tables can be partitioned by range on a column to improve performance for large datasets.

**Syntax:**

```sql
CREATE TABLE table_name (
    column_definitions...
) PARTITION BY RANGE(column) (
    PARTITION partition_name VALUES LESS THAN value,
    PARTITION partition_name VALUES LESS THAN value,
    PARTITION partition_name VALUES LESS THAN MAXVALUE
);
```

**Examples:**

```sql
-- Range partitioning by age
CREATE TABLE employees (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    age INT,
    department VARCHAR(50)
) PARTITION BY RANGE(age) (
    PARTITION p_young VALUES LESS THAN 25,
    PARTITION p_mid VALUES LESS THAN 50,
    PARTITION p_senior VALUES LESS THAN MAXVALUE
);

-- Range partitioning by order amount
CREATE TABLE orders (
    id INT PRIMARY KEY,
    user_id INT,
    total INT,
    status VARCHAR(20)
) PARTITION BY RANGE(total) (
    PARTITION p_small VALUES LESS THAN 100,
    PARTITION p_medium VALUES LESS THAN 1000,
    PARTITION p_large VALUES LESS THAN 10000,
    PARTITION p_huge VALUES LESS THAN MAXVALUE
);
```

**Hash Partitioning** is also supported for even distribution:

```sql
CREATE TABLE logs (
    id INT PRIMARY KEY,
    message VARCHAR(500),
    severity INT
) PARTITION BY HASH(id) (
    PARTITION p0 VALUES LESS THAN 0,
    PARTITION p1 VALUES LESS THAN 0,
    PARTITION p2 VALUES LESS THAN 0,
    PARTITION p3 VALUES LESS THAN 0
);
```

---

## Transactions

ChronosDB supports ACID transactions with `BEGIN`, `COMMIT`, and `ROLLBACK`.

**Syntax:**

```sql
BEGIN;
-- statements...
COMMIT;

-- Or to undo:
BEGIN;
-- statements...
ROLLBACK;
```

| Command | Franco-Arab | Aliases | Description |
|---------|-------------|---------|-------------|
| `BEGIN` | `2EBDA2` | `START` | Start a transaction |
| `COMMIT` | `2AKED` | -- | Commit the transaction |
| `ROLLBACK` | `2ERGA3` | `UNDO` | Roll back the transaction |

**Example:**

```sql
BEGIN;
INSERT INTO accounts (name, balance) VALUES ('Ahmed', 1000);
UPDATE accounts SET balance = 500 WHERE name = 'Ali';
COMMIT;
```

```sql
-- If something goes wrong
BEGIN;
DELETE FROM users WHERE id = 1;
ROLLBACK;  -- Undo the delete
```

---

## Database Management

### CREATE DATABASE

```sql
CREATE DATABASE database_name;
```

### USE DATABASE

Switch to a database for subsequent queries.

```sql
USE database_name;
```

### DROP DATABASE

```sql
DROP DATABASE database_name;
```

**Franco-Arab alternatives:**

```sql
2E3MEL DATABASE mydb;
2ESTA5DEM mydb;
2EMSA7 DATABASE mydb;  -- or DROP DATABASE mydb;
```

---

## User Management

ChronosDB implements Role-Based Access Control (RBAC) with five roles:

| Role | Franco-Arab | Description |
|------|-------------|-------------|
| `SUPERADMIN` | -- | Full system access |
| `ADMIN` | `MODEER` | Database administration |
| `NORMAL` | `3ADI` | Standard read/write access |
| `READONLY` | `MOSHAHED` | Read-only access |
| `DENIED` | `MAMNO3` | All access denied |

### CREATE USER

```sql
CREATE USER username PASS password ROLE role;
```

**Examples:**

```sql
CREATE USER ahmed PASS secret123 ROLE ADMIN;
CREATE USER viewer PASS readpass ROLE READONLY;

-- Franco-Arab
2E3MEL MOSTA5DEM ahmed PASS secret123 WAZEFA MODEER;
```

### ALTER USER ROLE

```sql
ALTER USER username ROLE new_role [IN database];
-- or
UPDATE USER username ROLE new_role [IN database];
```

**Examples:**

```sql
ALTER USER ahmed ROLE SUPERADMIN;
UPDATE USER ahmed ROLE READONLY IN production;
```

### DELETE USER

```sql
DELETE USER username;
```

**Example:**

```sql
DELETE USER ahmed;
```

### WHOAMI

Displays the currently authenticated user and their role.

```sql
WHOAMI;
```

### SHOW USERS

Lists all users and their roles.

```sql
SHOW USERS;
```

---

## System Commands

### SHOW TABLES

Lists all tables in the current database.

```sql
SHOW TABLES;
```

### SHOW DATABASES

Lists all available databases.

```sql
SHOW DATABASES;
```

### SHOW STATUS

Displays server status information.

```sql
SHOW STATUS;
-- or
STATUS;
```

### DESCRIBE / DESC

Shows the structure of a table including columns, types, and constraints.

```sql
DESCRIBE table_name;
-- or
DESC table_name;
```

### SHOW CREATE TABLE

Displays the SQL statement that would recreate the table.

```sql
SHOW CREATE TABLE table_name;
```

### SHOW AI STATUS

Displays the status of ChronosDB's built-in AI components (learning engine, immune system, temporal analysis).

```sql
SHOW AI STATUS;
```

### SHOW ANOMALIES

Lists detected anomalous queries and access patterns.

```sql
SHOW ANOMALIES;
```

### SHOW EXECUTION STATS

Displays query execution statistics and performance metrics.

```sql
SHOW EXECUTION STATS;
```

**Franco-Arab alternatives:**

```sql
WARENY GADWAL;             -- SHOW TABLES
WARENY DATABASES;          -- SHOW DATABASES
WARENY 7ALAH;              -- SHOW STATUS
WASF users;                -- DESCRIBE users
WARENY ZAKA2 7ALAH;        -- SHOW AI STATUS
WARENY SHOZOOZ;            -- SHOW ANOMALIES
WARENY TANFEEZ E7SA2EYAT;  -- SHOW EXECUTION STATS
```

---

## Recovery and Checkpoints

### CHECKPOINT

Forces a write of all dirty pages to disk and creates a recovery point.

```sql
CHECKPOINT;
```

### RECOVER TO

Recovers the database to a specific point in time.

**Syntax:**

```sql
-- Recover to latest state
RECOVER TO LATEST;
RECOVER TO NOW;
RECOVER TO CURRENT;

-- Recover to specific date
RECOVER TO 'DD/MM/YYYY HH:MM';
RECOVER TO 'DD/MM/YYYY HH:MM:SS';

-- Recover to raw timestamp (microseconds)
RECOVER TO 1706400000000000;
```

**Examples:**

```sql
CHECKPOINT;

-- After a disaster, recover:
RECOVER TO LATEST;
RECOVER TO '15/01/2026 14:30';

-- Franco-Arab
ERGA3 ELA A5ER;
ERGA3 ELA '15/01/2026 14:30';
```

---

## EXPLAIN / EXPLAIN ANALYZE

Shows the query execution plan for a statement, useful for performance analysis and debugging.

**Syntax:**

```sql
EXPLAIN query;
EXPLAIN ANALYZE query;
```

`EXPLAIN` shows the planned execution strategy. `EXPLAIN ANALYZE` actually executes the query and reports real performance metrics.

**Examples:**

```sql
EXPLAIN SELECT * FROM users WHERE age > 25;
EXPLAIN ANALYZE SELECT name, department FROM employees INNER JOIN departments ON employees.dept_id = departments.id;

-- Franco-Arab
SHAREH 2E5TAR * MEN users LAMA age > 25;
SHAREH 7ALLEL 2E5TAR * MEN users;
```

---

## Export and Import

### EXPORT TABLE

Exports table data to a CSV file.

**Syntax:**

```sql
EXPORT TABLE table_name TO 'file_path';
```

**Examples:**

```sql
EXPORT TABLE users TO 'users_backup.csv';
EXPORT TABLE orders TO '/data/exports/orders_2026.csv';

-- Franco-Arab
SADDR GADWAL users ELA 'users_backup.csv';
```

### IMPORT FROM

Imports data from a CSV file into an existing table.

**Syntax:**

```sql
IMPORT FROM 'file_path' INTO table_name;
```

**Examples:**

```sql
IMPORT FROM 'new_users.csv' INTO users;
IMPORT FROM '/data/imports/products.csv' INTO products;

-- Franco-Arab
ESTRAD MEN 'new_users.csv' GOWA users;
```

---

## Backup and Restore

### BACKUP DATABASE

Creates a full backup of the current database.

**Syntax:**

```sql
BACKUP DATABASE TO 'backup_path';
```

**Examples:**

```sql
BACKUP DATABASE TO '/backups/mydb_2026_02_22.bak';
BACKUP DATABASE TO 'daily_backup.bak';

-- Franco-Arab
N5A_E7TYATY DATABASE ELA '/backups/mydb.bak';
```

### RESTORE DATABASE

Restores a database from a backup file.

**Syntax:**

```sql
RESTORE DATABASE FROM 'backup_path';
```

**Examples:**

```sql
RESTORE DATABASE FROM '/backups/mydb_2026_02_22.bak';

-- Franco-Arab
ESTER3A3 DATABASE MEN '/backups/mydb.bak';
```

---

## Stored Procedures

Stored procedures allow you to group multiple SQL statements into reusable, callable routines.

### CREATE PROCEDURE

**Syntax:**

```sql
CREATE PROCEDURE procedure_name (param1 TYPE, param2 TYPE, ...)
BEGIN
    -- SQL statements
END;
```

Supported parameter types: `INT`, `VARCHAR`, `DECIMAL`, `BOOLEAN`.

**Examples:**

```sql
-- Simple procedure
CREATE PROCEDURE add_user(username VARCHAR, user_age INT)
BEGIN
    INSERT INTO users (name, age) VALUES (username, user_age);
END;

-- Procedure with multiple statements
CREATE PROCEDURE reset_orders(user_id INT)
BEGIN
    DELETE FROM orders WHERE user_id = user_id;
    UPDATE users SET order_count = 0 WHERE id = user_id;
END;

-- Franco-Arab
2E3MEL EGRA2 add_user(username GOMLA, user_age RAKAM)
2EBDA2
    EMLA GOWA users (name, age) ELKEYAM (username, user_age);
5ALAS;
```

### CALL

Executes a stored procedure.

**Syntax:**

```sql
CALL procedure_name(arg1, arg2, ...);
```

**Examples:**

```sql
CALL add_user('Ahmed', 25);
CALL reset_orders(42);

-- Franco-Arab
NADY add_user('Ahmed', 25);
```

### DROP PROCEDURE

Removes a stored procedure.

```sql
DROP PROCEDURE procedure_name;
```

---

## Triggers

Triggers automatically execute SQL statements in response to data modification events on a table.

### CREATE TRIGGER

**Syntax:**

```sql
CREATE TRIGGER trigger_name
BEFORE|AFTER INSERT|UPDATE|DELETE ON table_name
[FOR EACH ROW]
BEGIN
    -- SQL statements (can reference NEW and OLD row values)
END;
```

**Timing options:**

| Timing | Franco-Arab | Description |
|--------|-------------|-------------|
| `BEFORE` | `QABL` | Execute before the triggering event |
| `AFTER` | `BA3D` | Execute after the triggering event |

**Event types:** `INSERT`, `UPDATE`, `DELETE`

**Special references:**
- `NEW` (`GEDEED`) -- references the new row values (for INSERT and UPDATE)
- `OLD` (`2ADEEM`) -- references the old row values (for UPDATE and DELETE)

**Examples:**

```sql
-- Audit trigger
CREATE TRIGGER audit_user_changes
AFTER UPDATE ON users
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (table_name, action, timestamp) VALUES ('users', 'UPDATE', NOW);
END;

-- Before insert validation
CREATE TRIGGER validate_order
BEFORE INSERT ON orders
FOR EACH ROW
BEGIN
    UPDATE inventory SET quantity = quantity - 1 WHERE product_id = NEW.product_id;
END;

-- Franco-Arab
2E3MEL MESHAGHAL audit_changes
BA3D 3ADEL 3ALA users
LEKOL KOL_WAHD SAFF
2EBDA2
    EMLA GOWA audit_log (table_name, action) ELKEYAM ('users', 'UPDATE');
5ALAS;
```

### DROP TRIGGER

```sql
DROP TRIGGER trigger_name;
```

---

## Query History

View the history of executed queries on the server.

**Syntax:**

```sql
SHOW HISTORY;
SHOW HISTORY LIMIT n;
```

**Examples:**

```sql
-- Show last 100 queries (default)
SHOW HISTORY;

-- Show last 50 queries
SHOW HISTORY LIMIT 50;

-- Show last 10 queries
SHOW HISTORY LIMIT 10;
```

---

## Scheduled Jobs

Schedule SQL statements to run automatically at regular intervals.

### CREATE SCHEDULE

**Syntax:**

```sql
CREATE SCHEDULE schedule_name EVERY interval SECONDS|MINUTES|HOURS DO 'sql_statement';
```

| Time Unit | Franco-Arab | Description |
|-----------|-------------|-------------|
| `SECONDS` | `SAWANY` | Interval in seconds |
| `MINUTES` | `DA2AYE2` | Interval in minutes |
| `HOURS` | `SA3AT` | Interval in hours |

**Examples:**

```sql
-- Run cleanup every 60 seconds
CREATE SCHEDULE cleanup_job EVERY 60 SECONDS DO 'DELETE FROM sessions WHERE expired = TRUE;';

-- Run backup every 24 hours
CREATE SCHEDULE daily_backup EVERY 24 HOURS DO 'BACKUP DATABASE TO /backups/auto.bak;';

-- Run stats collection every 5 minutes
CREATE SCHEDULE collect_stats EVERY 5 MINUTES DO 'INSERT INTO stats (ts, count) VALUES (NOW, 0);';
```

### SHOW SCHEDULES

Lists all active scheduled jobs.

```sql
SHOW SCHEDULES;
```

### DROP SCHEDULE

Removes a scheduled job.

```sql
DROP SCHEDULE schedule_name;
```

**Example:**

```sql
DROP SCHEDULE cleanup_job;
```

---

## Replication

ChronosDB supports primary-replica replication for high availability.

### SET REPLICATION ROLE

Configures the replication role of the current server.

**Syntax:**

```sql
SET REPLICATION ROLE PRIMARY;
SET REPLICATION ROLE REPLICA ['host:port'];
SET REPLICATION ROLE STANDALONE;
```

**Examples:**

```sql
-- Set as primary server
SET REPLICATION ROLE PRIMARY;

-- Set as replica pointing to primary
SET REPLICATION ROLE REPLICA 'primary-host:2501';

-- Return to standalone mode
SET REPLICATION ROLE STANDALONE;
```

### PROMOTE

Promotes a replica server to primary (failover).

```sql
PROMOTE;
```

### ADD REPLICA

Registers a replica server with the primary.

```sql
ADD REPLICA 'host:port';
```

### SHOW REPLICATION STATUS

Displays the current replication configuration and status.

```sql
SHOW REPLICATION STATUS;
```

---

## Index Advisor

The built-in AI-powered index advisor analyzes query patterns and suggests indexes that could improve performance.

**Syntax:**

```sql
SHOW INDEX SUGGESTIONS;
```

**Example output:**

```
Suggested indexes based on query patterns:
  - CREATE INDEX idx_users_age ON users (age)     -- 45 queries would benefit
  - CREATE INDEX idx_orders_date ON orders (date)  -- 32 queries would benefit
```

---

## Query Firewall

The query firewall detects and blocks potentially dangerous queries (SQL injection, XSS attempts, etc.). Blocked queries can be reviewed and approved by administrators.

### SHOW BLOCKED QUERIES

Lists queries that have been blocked by the firewall.

```sql
SHOW BLOCKED QUERIES;
```

### APPROVE QUERY

Approves a previously blocked query, allowing it to execute.

**Syntax:**

```sql
APPROVE QUERY query_id;
```

**Example:**

```sql
-- View blocked queries first
SHOW BLOCKED QUERIES;

-- Approve a specific query by its ID
APPROVE QUERY 42;
```

---

## Server Control

### STOP SERVER

Gracefully shuts down the ChronosDB server.

```sql
STOP;
-- or
SHUTDOWN;
```

**Franco-Arab:**

```sql
WA2AF;
2AFOL;
```

---

## Quick Reference Card

### DDL

```sql
CREATE TABLE t (col TYPE [constraints], ...);
ALTER TABLE t ADD COLUMN col TYPE;
ALTER TABLE t DROP COLUMN col;
ALTER TABLE t RENAME COLUMN old TO new;
DROP TABLE [IF EXISTS] t;
CREATE INDEX idx ON t (col);
CREATE HASH INDEX idx ON t (col);
DROP INDEX idx;
CREATE VIEW v AS SELECT ...;
DROP VIEW v;
```

### DML

```sql
INSERT INTO t (cols) VALUES (vals);
INSERT INTO t (cols) VALUES (row1), (row2), ...;
SELECT [DISTINCT] cols FROM t [JOIN ...] [WHERE ...] [GROUP BY ...] [HAVING ...] [ORDER BY ...] [LIMIT n OFFSET m];
UPDATE t SET col = val [WHERE ...];
DELETE FROM t [WHERE ...];
```

### Advanced Queries

```sql
WITH cte AS (SELECT ...) SELECT ... FROM cte;
SELECT ROW_NUMBER() OVER (PARTITION BY col ORDER BY col) FROM t;
SELECT * FROM t AS OF 'DD/MM/YYYY HH:MM';
```

### Administration

```sql
CREATE DATABASE db;          USE db;                  DROP DATABASE db;
CREATE USER u PASS p ROLE r; ALTER USER u ROLE r;     DELETE USER u;
SHOW TABLES;                 SHOW DATABASES;          SHOW USERS;
DESCRIBE t;                  SHOW CREATE TABLE t;     SHOW STATUS;
WHOAMI;                      SHOW AI STATUS;          SHOW ANOMALIES;
SHOW EXECUTION STATS;        SHOW HISTORY [LIMIT n];  SHOW INDEX SUGGESTIONS;
SHOW BLOCKED QUERIES;        APPROVE QUERY id;
EXPLAIN [ANALYZE] query;
```

### Data Operations

```sql
EXPORT TABLE t TO 'path';                IMPORT FROM 'path' INTO t;
BACKUP DATABASE TO 'path';              RESTORE DATABASE FROM 'path';
```

### Procedures, Triggers, Scheduling

```sql
CREATE PROCEDURE p(args) BEGIN ... END;  CALL p(args);         DROP PROCEDURE p;
CREATE TRIGGER t BEFORE|AFTER event ON table FOR EACH ROW BEGIN ... END; DROP TRIGGER t;
CREATE SCHEDULE s EVERY n SECONDS|MINUTES|HOURS DO 'sql';      DROP SCHEDULE s;
SHOW SCHEDULES;
```

### Replication

```sql
SET REPLICATION ROLE PRIMARY|REPLICA|STANDALONE;
PROMOTE;
SHOW REPLICATION STATUS;
```

### Recovery

```sql
CHECKPOINT;
RECOVER TO LATEST|NOW|CURRENT;
RECOVER TO 'DD/MM/YYYY HH:MM';
```

### Server

```sql
STOP;
SHUTDOWN;
```
