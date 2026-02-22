# ChronosDB AI Layer Documentation

The AI Layer is an integrated machine learning and security system that continuously improves database performance and protects against threats. It consists of five major components.

## Architecture Overview

```
                        AIManager (Coordinator)
                              |
          ┌───────────┬───────┼───────┬────────────┐
          |           |       |       |            |
   LearningEngine  ImmuneSystem  TemporalIndex  IndexAdvisor  QueryFirewall
       |               |            |               |              |
   UCB1 Bandit    AnomalyDetector  Hotspot       Column Stats   Rate Limiter
   PlanOptimizer  ThreatDetector   Analysis      Suggestions    Pattern Match
```

All components are singletons initialized when the server starts and shut down gracefully on exit.

## 1. Learning Engine (UCB1 Bandit)

### Purpose
Automatically selects the optimal scan strategy (sequential scan vs index scan) for queries by learning from observed execution performance.

### Algorithm: Upper Confidence Bound (UCB1)
UCB1 is a multi-armed bandit algorithm that balances **exploitation** (using the best-known strategy) with **exploration** (trying less-used strategies to gather data).

**UCB Score** = Average Reward + C * sqrt(ln(total_pulls) / arm_pulls)

Where:
- **Average Reward** = sum of rewards / number of pulls for this arm
- **C** = exploration constant (balances exploration vs exploitation)
- **total_pulls** = total queries observed across all strategies
- **arm_pulls** = queries where this strategy was used

### Arms (Strategies)
| Strategy | When Best |
|----------|-----------|
| Sequential Scan | Small tables, full table reads, no applicable index |
| Index Scan | Selective queries on indexed columns |

### How It Works
1. For each SELECT query, the engine observes which scan strategy was used
2. It measures the execution time as the reward signal
3. It updates the UCB1 statistics for the chosen strategy
4. Over time, it converges on the optimal strategy for each query pattern

### Monitoring
```sql
SHOW EXECUTION STATS;
```

Output:
```
| Strategy         | Pulls | Avg Reward | UCB Score |
|-----------------|-------|------------|-----------|
| Sequential Scan | 150   | 0.6234     | 0.7891    |
| Index Scan      | 85    | 0.8912     | 0.9543    |
| Total Queries   | 235   |            |           |
```

### Query Plan Optimizer
The multi-dimensional optimizer extends UCB1 to additional decisions:
- **Filter reordering**: Placing more selective filters first
- **Early termination**: Stopping scans when LIMIT is satisfied
- **Join ordering**: Choosing optimal join sequence

### Configuration
- `MIN_SAMPLES_BEFORE_LEARNING`: Minimum queries before the engine starts making decisions (default: configurable in `ai_config.h`)

## 2. Immune System

### Purpose
Detects anomalous query patterns that may indicate security issues, application bugs, or performance problems.

### Anomaly Detection Algorithm
Uses **z-score** based statistical anomaly detection:

1. Maintains a sliding window of query rates per table
2. Computes mean and standard deviation of query rates
3. Flags queries where `z-score = |current_rate - mean| / std_dev` exceeds thresholds

### Severity Levels

| Severity | Z-Score Threshold | Action |
|----------|------------------|--------|
| LOW | > 2.0 | Logged, visible in dashboard |
| MEDIUM | > 3.0 | Logged, alert generated |
| HIGH | > 4.0 | Table operations may be blocked |

### Threat Detection
The immune system includes pattern-based threat detectors:

**SQL Injection Detection:**
- `'; DROP` patterns
- `1=1` tautologies
- `UNION SELECT` injection
- Comment injection (`--`)

**XSS Detection:**
- `<script>` tag injection in values
- `javascript:` URI injection

### Table/User Blocking
When high-severity anomalies are detected repeatedly:
- Tables can be temporarily blocked from further modifications
- Users with suspicious patterns can be rate-limited
- Blocks can be reviewed and lifted by administrators

### Monitoring
```sql
SHOW ANOMALIES;
SHOW AI STATUS;
```

The web admin AI Layer page shows real-time anomaly data with severity indicators.

## 3. Index Advisor

### Purpose
Analyzes query patterns and recommends index creation to improve performance.

### How It Works
1. **Tracking**: Records every column accessed in WHERE clauses with the operation type (equality `=` vs range `>`, `<`, `>=`, `<=`)
2. **Analysis**: After sufficient queries (>5 per column), evaluates indexing potential
3. **Recommendation**: Suggests index type based on access patterns:
   - **Equality-heavy** (more `=` than range): Recommends **HASH** index
   - **Range-heavy** (more `>`, `<`, etc.): Recommends **B+ Tree** index
4. **Filtering**: Skips columns that already have indexes
5. **Scoring**: Ranks suggestions by access frequency

### Using Index Suggestions
```sql
SHOW INDEX SUGGESTIONS;
```

Output:
```
| Table  | Column | Type  | Reason                         | Query Count | Suggested SQL                                    |
|--------|--------|-------|-------------------------------|-------------|------------------------------------------------|
| users  | email  | HASH  | High equality lookups (45q)    | 45          | CREATE HASH INDEX idx_users_email ON users(email); |
| orders | date   | BTREE | Frequent range lookups (32q)   | 32          | CREATE INDEX idx_orders_date ON orders(date);      |
```

You can copy and execute the suggested SQL directly.

### Integration Points
- The DML executor records column accesses during WHERE clause evaluation
- The web admin provides a visual interface at `/api/ai/index-suggestions`
- Suggestions update in real-time as query patterns change

## 4. Query Firewall

### Purpose
Protects the database from malicious or abusive queries through rate limiting and pattern detection.

### Rate Limiting
- **Default limit**: 1000 queries per user per 60-second window
- When exceeded, queries are blocked with reason `RATE_LIMIT`
- Configurable via `QueryFirewall::SetRateLimit()`

### Pattern Detection

**SQL Injection Patterns** (case-insensitive):
| Pattern | Description |
|---------|-------------|
| `'; DROP` | Drop table injection |
| `1=1` | Tautology injection |
| `UNION SELECT` | Union-based data extraction |
| `--` | Comment-based injection |

**XSS Patterns**:
| Pattern | Description |
|---------|-------------|
| `<script` | Script tag injection |
| `javascript:` | URI scheme injection |

### Blocked Query Workflow
1. Suspicious query is detected and blocked
2. Query is stored with ID, SQL, user, reason, and timestamp
3. Admin reviews blocked queries:
   ```sql
   SHOW BLOCKED QUERIES;
   ```
4. Admin can approve false positives:
   ```sql
   APPROVE QUERY 42;
   ```

### Buffer
The firewall maintains a ring buffer of the last 1000 blocked queries. Older entries are automatically purged.

## 5. Temporal Index Manager

### Purpose
Analyzes time-based access patterns to optimize temporal queries and trigger proactive snapshots.

### How It Works
1. Records timestamps of all data accesses
2. Uses density-based clustering to identify **temporal hotspots** - time ranges with unusually high query activity
3. Can trigger automatic snapshots for frequently-accessed time ranges

### Hotspot Detection
A hotspot is characterized by:
- **Center timestamp**: The peak of query activity
- **Range**: Start and end timestamps defining the active period
- **Access count**: Number of queries hitting this range
- **Density**: Queries per time unit

### Monitoring
```sql
SHOW AI STATUS;
```

The temporal index section shows:
- Total accesses tracked
- Total snapshots triggered
- Current hotspot locations and densities

## Web Admin AI Dashboard

The web admin provides rich visualization of all AI components at the AI Layer page:

### Learning Engine Panel
- UCB1 arm statistics with pulls and rewards
- Query plan optimizer dimensions
- Summary of learning progress

### Immune System Panel
- Total anomaly count
- Blocked tables and users
- Recent anomaly timeline with severity coloring
- Z-score thresholds configuration
- Threat detection counters (SQL injection, XSS)

### Temporal Index Panel
- Access count and snapshot statistics
- Hotspot visualization

### Real-time Dashboard Integration
The Real-time Dashboard page includes:
- Live anomaly alerts
- Blocked query notifications
- AI metrics updated every 3 seconds

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/ai/status` | GET | Basic AI status |
| `/api/ai/detailed` | GET | Full AI layer details |
| `/api/ai/anomalies` | GET | Recent anomalies |
| `/api/ai/stats` | GET | UCB1 bandit statistics |
| `/api/ai/index-suggestions` | GET | Index recommendations |
| `/api/ai/blocked-queries` | GET | Blocked queries list |
| `/api/ai/approve/:id` | POST | Approve a blocked query |

## Dynamic Decay

AI components implement **activity-based decay** to adapt to changing workloads:
- Decay factor is computed based on recent query activity
- High-activity periods reduce decay (preserve learned knowledge)
- Low-activity periods increase decay (allow faster adaptation to new patterns)
- Applied during periodic maintenance cycles
