# CQL (Chronos Query Language) Reference

## Introduction

CQL is the query language for ChronosDB. It is fully compatible with standard SQL while also supporting Franco-Arab transliterated keyword alternatives. Every SQL keyword has a CQL equivalent, allowing users to write queries using Arabic-style keywords transliterated into Latin characters.

Both forms can be used interchangeably within the same query. The lexer normalizes all keywords to their internal representation, so `SELECT` and `EKHTAR` are identical to the engine.

---

## Table of Contents

1. [Quick Reference Table](#quick-reference-table)
2. [DDL Keywords](#ddl-keywords)
3. [DML Keywords](#dml-keywords)
4. [Clauses and Operators](#clauses-and-operators)
5. [Data Types](#data-types)
6. [Constraints](#constraints)
7. [System Commands](#system-commands)
8. [Procedural and Advanced Keywords](#procedural-and-advanced-keywords)
9. [Complete Examples](#complete-examples)

---

## Quick Reference Table

| SQL Keyword | CQL Equivalent | Category |
|---|---|---|
| CREATE | ENSHAA | DDL |
| TABLE | GADWAL | DDL |
| DROP | EHZAF | DDL |
| ALTER | 3ADDEL | DDL |
| ADD | DAYEF | DDL |
| COLUMN | 3AMOUD | DDL |
| RENAME | GHAYYER_ESM | DDL |
| INDEX | FAHRASA | DDL |
| VIEW | 3ARD | DDL |
| SELECT | EKHTAR | DML |
| INSERT | DA5AL | DML |
| UPDATE | 7ADDES | DML |
| DELETE | EMSAH | DML |
| FROM | MEN | DML |
| WHERE | HEES | DML |
| INTO | FE | DML |
| VALUES | 2EYAM | DML |
| SET | 7ADED | DML |
| AND | WA | Clause |
| OR | AW | Clause |
| NOT | MESH | Clause |
| AS | KA | Clause |
| ON | 3ALA | Clause |
| JOIN | DAM | Clause |
| LEFT | YESAR | Clause |
| RIGHT | YAMEEN | Clause |
| INNER | DA5LY | Clause |
| ORDER | RATTEB | Clause |
| BY | HASAB | Clause |
| ASC | SA3ED | Clause |
| DESC | NAZEL | Clause |
| GROUP | GAME3 | Clause |
| HAVING | BE_SHART | Clause |
| LIMIT | 7ADD | Clause |
| OFFSET | EBTADA2_MEN | Clause |
| DISTINCT | MOKHTALEF | Clause |
| BETWEEN | BEEN | Clause |
| LIKE | ZAYY | Clause |
| IN | FEL | Clause |
| INT / INTEGER | RAQAM | Type |
| FLOAT | 3ASHRY | Type |
| VARCHAR | NASS | Type |
| BOOLEAN | MANTIQY | Type |
| PRIMARY | ASASY | Constraint |
| KEY | MOFTAH | Constraint |
| FOREIGN | KHARIGY | Constraint |
| REFERENCES | YOSHER_ELA | Constraint |
| UNIQUE | FARID | Constraint |
| NULL | FARIGH | Constraint |
| DEFAULT | EFTIRADY | Constraint |
| CHECK | TA7AQOQ | Constraint |
| NOT NULL | MESH_FARIGH | Constraint |
| AUTO_INCREMENT | TAZAYOD_TLQA2Y | Constraint |
| SHOW | WARRAY | System |
| DESCRIBE | WASF | System |
| DATABASE | QA3DET_BAYANAT | System |
| USE | ESTA5DEM | System |
| STATUS | 7ALA | System |
| BEGIN | EBDA2 | System |
| COMMIT | THABBET | System |
| ROLLBACK | TARA3A3 | System |
| CHECKPOINT | NO2TET_7EFZ | System |
| RECOVER | ESTER3A3 | System |
| EXPLAIN | ESHRA7 | System |
| ANALYZE | 7ALLEL | System |
| TRUNCATE | FARRAGH | System |
| STOP | AW2AF | System |

---

## DDL Keywords

DDL (Data Definition Language) keywords are used to define and modify database schema objects such as tables, indexes, and views.

### CREATE / ENSHAA

Creates a new database object.

```sql
-- SQL
CREATE TABLE employees (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    salary FLOAT DEFAULT 0.0
);

-- CQL
ENSHAA GADWAL employees (
    id RAQAM ASASY MOFTAH TAZAYOD_TLQA2Y,
    name NASS(100) MESH_FARIGH,
    salary 3ASHRY EFTIRADY 0.0
);
```

### DROP / EHZAF

Removes a database object.

```sql
-- SQL
DROP TABLE employees;

-- CQL
EHZAF GADWAL employees;
```

### ALTER / 3ADDEL

Modifies an existing database object.

```sql
-- SQL
ALTER TABLE employees ADD COLUMN email VARCHAR(255);

-- CQL
3ADDEL GADWAL employees DAYEF 3AMOUD email NASS(255);
```

### RENAME / GHAYYER_ESM

Renames a database object.

```sql
-- SQL
ALTER TABLE employees RENAME TO staff;

-- CQL
3ADDEL GADWAL employees GHAYYER_ESM TO staff;
```

### INDEX / FAHRASA

Creates or drops an index.

```sql
-- SQL
CREATE INDEX idx_name ON employees (name);

-- CQL
ENSHAA FAHRASA idx_name 3ALA employees (name);
```

### VIEW / 3ARD

Creates or drops a view.

```sql
-- SQL
CREATE VIEW active_employees AS SELECT * FROM employees WHERE active = 1;

-- CQL
ENSHAA 3ARD active_employees KA EKHTAR * MEN employees HEES active = 1;
```

---

## DML Keywords

DML (Data Manipulation Language) keywords are used to query and modify data within tables.

### SELECT / EKHTAR

Retrieves data from one or more tables.

```sql
-- SQL
SELECT name, salary FROM employees WHERE salary > 50000;

-- CQL
EKHTAR name, salary MEN employees HEES salary > 50000;
```

### INSERT / DA5AL

Adds new rows to a table.

```sql
-- SQL
INSERT INTO employees (name, salary) VALUES ('Ahmed', 60000);

-- CQL
DA5AL FE employees (name, salary) 2EYAM ('Ahmed', 60000);
```

### UPDATE / 7ADDES

Modifies existing rows.

```sql
-- SQL
UPDATE employees SET salary = 70000 WHERE name = 'Ahmed';

-- CQL
7ADDES employees 7ADED salary = 70000 HEES name = 'Ahmed';
```

### DELETE / EMSAH

Removes rows from a table.

```sql
-- SQL
DELETE FROM employees WHERE salary < 30000;

-- CQL
EMSAH MEN employees HEES salary < 30000;
```

---

## Clauses and Operators

### Logical Operators

| SQL | CQL | Description |
|---|---|---|
| AND | WA | Logical AND |
| OR | AW | Logical OR |
| NOT | MESH | Logical NOT |

```sql
-- SQL
SELECT * FROM employees WHERE salary > 50000 AND department = 'Engineering';

-- CQL
EKHTAR * MEN employees HEES salary > 50000 WA department = 'Engineering';
```

```sql
-- SQL
SELECT * FROM products WHERE category = 'Electronics' OR category = 'Books';

-- CQL
EKHTAR * MEN products HEES category = 'Electronics' AW category = 'Books';
```

```sql
-- SQL
SELECT * FROM employees WHERE NOT active = 0;

-- CQL
EKHTAR * MEN employees HEES MESH active = 0;
```

### Aliases: AS / KA

```sql
-- SQL
SELECT name AS employee_name FROM employees;

-- CQL
EKHTAR name KA employee_name MEN employees;
```

### Joins

| SQL | CQL | Description |
|---|---|---|
| JOIN | DAM | Join tables |
| ON | 3ALA | Join condition |
| LEFT | YESAR | Left outer join |
| RIGHT | YAMEEN | Right outer join |
| INNER | DA5LY | Inner join |

```sql
-- SQL
SELECT e.name, d.department_name
FROM employees e
INNER JOIN departments d ON e.dept_id = d.id;

-- CQL
EKHTAR e.name, d.department_name
MEN employees e
DA5LY DAM departments d 3ALA e.dept_id = d.id;
```

```sql
-- SQL
SELECT e.name, d.department_name
FROM employees e
LEFT JOIN departments d ON e.dept_id = d.id;

-- CQL
EKHTAR e.name, d.department_name
MEN employees e
YESAR DAM departments d 3ALA e.dept_id = d.id;
```

### Ordering

| SQL | CQL | Description |
|---|---|---|
| ORDER | RATTEB | Sort results |
| BY | HASAB | Sort criterion |
| ASC | SA3ED | Ascending order |
| DESC | NAZEL | Descending order |

```sql
-- SQL
SELECT * FROM employees ORDER BY salary DESC;

-- CQL
EKHTAR * MEN employees RATTEB HASAB salary NAZEL;
```

```sql
-- SQL
SELECT * FROM employees ORDER BY name ASC, salary DESC;

-- CQL
EKHTAR * MEN employees RATTEB HASAB name SA3ED, salary NAZEL;
```

### Grouping

| SQL | CQL | Description |
|---|---|---|
| GROUP | GAME3 | Group rows |
| BY | HASAB | Group criterion |
| HAVING | BE_SHART | Group filter |

```sql
-- SQL
SELECT department, COUNT(*) FROM employees GROUP BY department HAVING COUNT(*) > 5;

-- CQL
EKHTAR department, COUNT(*) MEN employees GAME3 HASAB department BE_SHART COUNT(*) > 5;
```

### Limiting Results

| SQL | CQL | Description |
|---|---|---|
| LIMIT | 7ADD | Maximum rows |
| OFFSET | EBTADA2_MEN | Skip rows |

```sql
-- SQL
SELECT * FROM employees LIMIT 10 OFFSET 20;

-- CQL
EKHTAR * MEN employees 7ADD 10 EBTADA2_MEN 20;
```

### DISTINCT / MOKHTALEF

```sql
-- SQL
SELECT DISTINCT department FROM employees;

-- CQL
EKHTAR MOKHTALEF department MEN employees;
```

### BETWEEN / BEEN

```sql
-- SQL
SELECT * FROM employees WHERE salary BETWEEN 40000 AND 80000;

-- CQL
EKHTAR * MEN employees HEES salary BEEN 40000 WA 80000;
```

### LIKE / ZAYY

```sql
-- SQL
SELECT * FROM employees WHERE name LIKE 'A%';

-- CQL
EKHTAR * MEN employees HEES name ZAYY 'A%';
```

### IN / FEL

```sql
-- SQL
SELECT * FROM employees WHERE department IN ('Engineering', 'Marketing');

-- CQL
EKHTAR * MEN employees HEES department FEL ('Engineering', 'Marketing');
```

---

## Data Types

| SQL Type | CQL Type | Description |
|---|---|---|
| INT / INTEGER | RAQAM | Integer numbers |
| FLOAT | 3ASHRY | Floating-point numbers |
| VARCHAR(n) | NASS(n) | Variable-length text (max n characters) |
| BOOLEAN | MANTIQY | True/false values |

```sql
-- SQL
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(200),
    price FLOAT,
    available BOOLEAN
);

-- CQL
ENSHAA GADWAL products (
    id RAQAM ASASY MOFTAH,
    name NASS(200),
    price 3ASHRY,
    available MANTIQY
);
```

---

## Constraints

### PRIMARY KEY / ASASY MOFTAH

Defines the primary key for a table.

```sql
-- SQL
CREATE TABLE users (
    id INT PRIMARY KEY,
    username VARCHAR(50)
);

-- CQL
ENSHAA GADWAL users (
    id RAQAM ASASY MOFTAH,
    username NASS(50)
);
```

### FOREIGN KEY / KHARIGY MOFTAH ... REFERENCES / YOSHER_ELA

Defines a foreign key relationship.

```sql
-- SQL
CREATE TABLE orders (
    id INT PRIMARY KEY,
    user_id INT,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

-- CQL
ENSHAA GADWAL orders (
    id RAQAM ASASY MOFTAH,
    user_id RAQAM,
    KHARIGY MOFTAH (user_id) YOSHER_ELA users(id)
);
```

### UNIQUE / FARID

Ensures all values in a column are unique.

```sql
-- SQL
CREATE TABLE users (
    id INT PRIMARY KEY,
    email VARCHAR(255) UNIQUE
);

-- CQL
ENSHAA GADWAL users (
    id RAQAM ASASY MOFTAH,
    email NASS(255) FARID
);
```

### NOT NULL / MESH_FARIGH

Prevents null values in a column.

```sql
-- SQL
CREATE TABLE users (
    id INT PRIMARY KEY,
    username VARCHAR(50) NOT NULL
);

-- CQL
ENSHAA GADWAL users (
    id RAQAM ASASY MOFTAH,
    username NASS(50) MESH_FARIGH
);
```

### DEFAULT / EFTIRADY

Sets a default value for a column.

```sql
-- SQL
CREATE TABLE users (
    id INT PRIMARY KEY,
    role VARCHAR(20) DEFAULT 'user'
);

-- CQL
ENSHAA GADWAL users (
    id RAQAM ASASY MOFTAH,
    role NASS(20) EFTIRADY 'user'
);
```

### CHECK / TA7AQOQ

Adds a check constraint.

```sql
-- SQL
CREATE TABLE employees (
    id INT PRIMARY KEY,
    age INT CHECK (age >= 18)
);

-- CQL
ENSHAA GADWAL employees (
    id RAQAM ASASY MOFTAH,
    age RAQAM TA7AQOQ (age >= 18)
);
```

### AUTO_INCREMENT / TAZAYOD_TLQA2Y

Automatically generates sequential values.

```sql
-- SQL
CREATE TABLE logs (
    id INT PRIMARY KEY AUTO_INCREMENT,
    message VARCHAR(500)
);

-- CQL
ENSHAA GADWAL logs (
    id RAQAM ASASY MOFTAH TAZAYOD_TLQA2Y,
    message NASS(500)
);
```

---

## System Commands

### SHOW / WARRAY

Displays database objects.

```sql
-- SQL
SHOW TABLES;
SHOW DATABASES;

-- CQL
WARRAY GADWAL;
WARRAY QA3DET_BAYANAT;
```

### DESCRIBE / WASF

Displays table structure.

```sql
-- SQL
DESCRIBE employees;

-- CQL
WASF employees;
```

### USE / ESTA5DEM

Switches the active database.

```sql
-- SQL
USE mydb;

-- CQL
ESTA5DEM mydb;
```

### DATABASE / QA3DET_BAYANAT

Used with CREATE, DROP, or SHOW for database-level operations.

```sql
-- SQL
CREATE DATABASE company;
DROP DATABASE company;

-- CQL
ENSHAA QA3DET_BAYANAT company;
EHZAF QA3DET_BAYANAT company;
```

### STATUS / 7ALA

Shows server or database status.

```sql
-- SQL
SHOW STATUS;

-- CQL
WARRAY 7ALA;
```

### EXPLAIN / ESHRA7

Shows the query execution plan.

```sql
-- SQL
EXPLAIN SELECT * FROM employees WHERE id = 1;

-- CQL
ESHRA7 EKHTAR * MEN employees HEES id = 1;
```

### ANALYZE / 7ALLEL

Analyzes table statistics.

```sql
-- SQL
ANALYZE TABLE employees;

-- CQL
7ALLEL GADWAL employees;
```

### TRUNCATE / FARRAGH

Removes all rows from a table without logging individual row deletions.

```sql
-- SQL
TRUNCATE TABLE employees;

-- CQL
FARRAGH GADWAL employees;
```

### Transaction Control

| SQL | CQL | Description |
|---|---|---|
| BEGIN | EBDA2 | Start a transaction |
| COMMIT | THABBET | Commit the transaction |
| ROLLBACK | TARA3A3 | Roll back the transaction |

```sql
-- SQL
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
COMMIT;

-- CQL
EBDA2;
7ADDES accounts 7ADED balance = balance - 100 HEES id = 1;
7ADDES accounts 7ADED balance = balance + 100 HEES id = 2;
THABBET;
```

### Recovery Commands

| SQL | CQL | Description |
|---|---|---|
| CHECKPOINT | NO2TET_7EFZ | Create a checkpoint |
| RECOVER | ESTER3A3 | Recover from WAL |

```sql
-- SQL
CHECKPOINT;
RECOVER;

-- CQL
NO2TET_7EFZ;
ESTER3A3;
```

### STOP / AW2AF

Stops the server gracefully.

```sql
-- SQL
STOP;

-- CQL
AW2AF;
```

---

## Procedural and Advanced Keywords

### Window Functions

| SQL | CQL | Description |
|---|---|---|
| WITH | MA3 | Common Table Expression / Window clause |
| OVER | FAWK | Window specification |
| PARTITION | TAGSEE2 | Window partitioning |
| ROW_NUMBER | RAQAM_SAFF | Row number function |
| RANK | MARTABA | Rank function |
| LAG | SABE2 | Previous row value |
| LEAD | TALE | Next row value |

```sql
-- SQL
SELECT name, salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS rank
FROM employees;

-- CQL
EKHTAR name, salary,
    RAQAM_SAFF() FAWK (TAGSEE2 HASAB department RATTEB HASAB salary NAZEL) KA rank
MEN employees;
```

```sql
-- SQL
SELECT name, salary,
    LAG(salary) OVER (ORDER BY hire_date) AS prev_salary,
    LEAD(salary) OVER (ORDER BY hire_date) AS next_salary
FROM employees;

-- CQL
EKHTAR name, salary,
    SABE2(salary) FAWK (RATTEB HASAB hire_date) KA prev_salary,
    TALE(salary) FAWK (RATTEB HASAB hire_date) KA next_salary
MEN employees;
```

### Stored Procedures

| SQL | CQL | Description |
|---|---|---|
| PROCEDURE | EGRA2 | Define a procedure |
| CALL | NADY | Execute a procedure |

```sql
-- SQL
CREATE PROCEDURE give_raise(emp_id INT, amount FLOAT)
BEGIN
    UPDATE employees SET salary = salary + amount WHERE id = emp_id;
END;

CALL give_raise(1, 5000);

-- CQL
ENSHAA EGRA2 give_raise(emp_id RAQAM, amount 3ASHRY)
EBDA2
    7ADDES employees 7ADED salary = salary + amount HEES id = emp_id;
END;

NADY give_raise(1, 5000);
```

### Triggers

| SQL | CQL | Description |
|---|---|---|
| TRIGGER | MESHAGHAL | Define a trigger |
| BEFORE | 2ABL | Before event |
| AFTER | BA3D | After event |
| EACH | KOL | Each row |
| ROW | SAFF | Row reference |
| FOR | LEKOL | For each |
| NEW | GEDEED | New row values |
| OLD | 2ADEEM | Old row values |

```sql
-- SQL
CREATE TRIGGER log_salary_change
AFTER UPDATE ON employees
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (emp_id, old_salary, new_salary)
    VALUES (OLD.id, OLD.salary, NEW.salary);
END;

-- CQL
ENSHAA MESHAGHAL log_salary_change
BA3D 7ADDES 3ALA employees
LEKOL KOL SAFF
EBDA2
    DA5AL FE audit_log (emp_id, old_salary, new_salary)
    2EYAM (2ADEEM.id, 2ADEEM.salary, GEDEED.salary);
END;
```

### Export and Import

| SQL | CQL | Description |
|---|---|---|
| EXPORT | SADDR | Export data |
| IMPORT | ESTRAD | Import data |

```sql
-- SQL
EXPORT TABLE employees TO 'employees_backup.csv';
IMPORT TABLE employees FROM 'employees_backup.csv';

-- CQL
SADDR GADWAL employees TO 'employees_backup.csv';
ESTRAD GADWAL employees FROM 'employees_backup.csv';
```

### Backup and Restore

| SQL | CQL | Description |
|---|---|---|
| BACKUP | N5A_E7TYATY | Create a backup |
| RESTORE | ESTER3A3 | Restore from backup |

```sql
-- SQL
BACKUP DATABASE mydb;
RESTORE DATABASE mydb;

-- CQL
N5A_E7TYATY QA3DET_BAYANAT mydb;
ESTER3A3 QA3DET_BAYANAT mydb;
```

### Scheduling

| SQL | CQL | Description |
|---|---|---|
| SCHEDULE | GADWAL_ZAMANY | Create a schedule |
| EVERY | KOL | Interval keyword |
| SECONDS | SAWANY | Seconds unit |
| MINUTES | DA2AYE2 | Minutes unit |
| HOURS | SA3AT | Hours unit |
| DO | NAFFEZ | Action to execute |

```sql
-- SQL
CREATE SCHEDULE cleanup_job EVERY 30 MINUTES DO DELETE FROM logs WHERE created_at < NOW() - 7;

-- CQL
ENSHAA GADWAL_ZAMANY cleanup_job KOL 30 DA2AYE2 NAFFEZ EMSAH MEN logs HEES created_at < NOW() - 7;
```

### Query History

| SQL | CQL | Description |
|---|---|---|
| HISTORY | TARE5_ESTE3LAMAT | View query history |

```sql
-- SQL
SHOW HISTORY;

-- CQL
WARRAY TARE5_ESTE3LAMAT;
```

### Control Flow (Procedural)

| SQL | CQL | Description |
|---|---|---|
| DECLARE | 3ARREF | Declare a variable |
| IF | LAW | Conditional |
| ELSE | GHEER | Else branch |
| WHILE | TALAMA | Loop |
| RETURN | ARGA3 | Return value |

```sql
-- SQL
DECLARE @counter INT = 0;
WHILE @counter < 10
BEGIN
    SET @counter = @counter + 1;
END;

-- CQL
3ARREF @counter RAQAM = 0;
TALAMA @counter < 10
EBDA2
    7ADED @counter = @counter + 1;
END;
```

### Replication

| SQL | CQL | Description |
|---|---|---|
| REPLICATION | NASAKHA | Replication configuration |
| REPLICA | NOOSKHA | Replica instance |
| PRIMARY | ASASY | Primary server |
| PROMOTE | RA2QY | Promote replica to primary |

```sql
-- SQL
SHOW REPLICATION STATUS;

-- CQL
WARRAY NASAKHA 7ALA;
```

---

## Complete Examples

### Example 1: Full Database Setup

```sql
-- SQL Version
CREATE DATABASE school;
USE school;

CREATE TABLE students (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(100) NOT NULL,
    age INT CHECK (age >= 16),
    gpa FLOAT DEFAULT 0.0,
    email VARCHAR(255) UNIQUE
);

CREATE TABLE courses (
    id INT PRIMARY KEY AUTO_INCREMENT,
    title VARCHAR(200) NOT NULL,
    credits INT DEFAULT 3
);

CREATE TABLE enrollments (
    id INT PRIMARY KEY AUTO_INCREMENT,
    student_id INT,
    course_id INT,
    FOREIGN KEY (student_id) REFERENCES students(id),
    FOREIGN KEY (course_id) REFERENCES courses(id)
);

CREATE INDEX idx_student_name ON students (name);

INSERT INTO students (name, age, gpa, email) VALUES ('Omar', 20, 3.8, 'omar@university.edu');
INSERT INTO courses (title, credits) VALUES ('Database Systems', 4);
INSERT INTO enrollments (student_id, course_id) VALUES (1, 1);
```

```sql
-- CQL Version
ENSHAA QA3DET_BAYANAT school;
ESTA5DEM school;

ENSHAA GADWAL students (
    id RAQAM ASASY MOFTAH TAZAYOD_TLQA2Y,
    name NASS(100) MESH_FARIGH,
    age RAQAM TA7AQOQ (age >= 16),
    gpa 3ASHRY EFTIRADY 0.0,
    email NASS(255) FARID
);

ENSHAA GADWAL courses (
    id RAQAM ASASY MOFTAH TAZAYOD_TLQA2Y,
    title NASS(200) MESH_FARIGH,
    credits RAQAM EFTIRADY 3
);

ENSHAA GADWAL enrollments (
    id RAQAM ASASY MOFTAH TAZAYOD_TLQA2Y,
    student_id RAQAM,
    course_id RAQAM,
    KHARIGY MOFTAH (student_id) YOSHER_ELA students(id),
    KHARIGY MOFTAH (course_id) YOSHER_ELA courses(id)
);

ENSHAA FAHRASA idx_student_name 3ALA students (name);

DA5AL FE students (name, age, gpa, email) 2EYAM ('Omar', 20, 3.8, 'omar@university.edu');
DA5AL FE courses (title, credits) 2EYAM ('Database Systems', 4);
DA5AL FE enrollments (student_id, course_id) 2EYAM (1, 1);
```

### Example 2: Complex Query with Joins and Aggregation

```sql
-- SQL Version
SELECT s.name, COUNT(e.id) AS course_count, AVG(s.gpa) AS avg_gpa
FROM students s
INNER JOIN enrollments e ON s.id = e.student_id
INNER JOIN courses c ON e.course_id = c.id
WHERE s.age BETWEEN 18 AND 25
GROUP BY s.name
HAVING COUNT(e.id) > 2
ORDER BY avg_gpa DESC
LIMIT 10;
```

```sql
-- CQL Version
EKHTAR s.name, COUNT(e.id) KA course_count, AVG(s.gpa) KA avg_gpa
MEN students s
DA5LY DAM enrollments e 3ALA s.id = e.student_id
DA5LY DAM courses c 3ALA e.course_id = c.id
HEES s.age BEEN 18 WA 25
GAME3 HASAB s.name
BE_SHART COUNT(e.id) > 2
RATTEB HASAB avg_gpa NAZEL
7ADD 10;
```

### Example 3: Transaction with Error Handling

```sql
-- SQL Version
BEGIN;
UPDATE accounts SET balance = balance - 500 WHERE id = 1;
UPDATE accounts SET balance = balance + 500 WHERE id = 2;
COMMIT;
```

```sql
-- CQL Version
EBDA2;
7ADDES accounts 7ADED balance = balance - 500 HEES id = 1;
7ADDES accounts 7ADED balance = balance + 500 HEES id = 2;
THABBET;
```

### Example 4: Window Functions

```sql
-- SQL Version
SELECT name, department, salary,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank,
    ROW_NUMBER() OVER (ORDER BY salary DESC) AS overall_rank
FROM employees
WHERE salary > 30000;
```

```sql
-- CQL Version
EKHTAR name, department, salary,
    MARTABA() FAWK (TAGSEE2 HASAB department RATTEB HASAB salary NAZEL) KA dept_rank,
    RAQAM_SAFF() FAWK (RATTEB HASAB salary NAZEL) KA overall_rank
MEN employees
HEES salary > 30000;
```

### Example 5: Scheduled Backup with Cleanup

```sql
-- SQL Version
CREATE SCHEDULE nightly_backup EVERY 24 HOURS DO BACKUP DATABASE production;
CREATE SCHEDULE log_cleanup EVERY 1 HOURS DO DELETE FROM system_logs WHERE timestamp < NOW() - 168;
```

```sql
-- CQL Version
ENSHAA GADWAL_ZAMANY nightly_backup KOL 24 SA3AT NAFFEZ N5A_E7TYATY QA3DET_BAYANAT production;
ENSHAA GADWAL_ZAMANY log_cleanup KOL 1 SA3AT NAFFEZ EMSAH MEN system_logs HEES timestamp < NOW() - 168;
```

---

## Franco-Arab Transliteration Guide

The CQL keywords use a common Franco-Arab (Arabizi) transliteration system where Arabic sounds that have no direct Latin equivalent are represented by numbers:

| Number | Arabic Sound | Example |
|---|---|---|
| 2 | Hamza (ء / أ) | 2EYAM (values), EBDA2 (begin) |
| 3 | Ain (ع) | 3ADDEL (alter), 3AMOUD (column) |
| 5 | Kha (خ) | DA5AL (insert), ESTA5DEM (use) |
| 7 | Ha (ح) | 7ADDES (update), 7ADD (limit) |

This transliteration system is widely used in informal Arabic digital communication across the Middle East and North Africa, making CQL keywords intuitive for Arabic-speaking developers.
