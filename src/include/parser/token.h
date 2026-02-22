#pragma once
#include <string>

namespace chronosdb {

    enum class TokenType {
        // --- KEYWORDS ---
        SELECT,      // 2E5TAR
        FROM,        // MEN
        WHERE,       // LAMA
        CREATE,      // 2E3MEL
        DELETE_CMD,  // 2EMSA7
        UPDATE_SET,  // 5ALY
        UPDATE_CMD,  // 3ADEL
        TABLE,       // GADWAL
        DATABASE,    // DATABASE
        USE,         // 2ESTA5DEM / USE
        LOGIN,       // LOGIN
        USER,        // USER / MOSTA5DEM
        ROLE,        // ROLE / WAZEFA / DOWR
        SHOW,        // SHOW / WARENY
        WHOAMI,      // ANAMEEN / WHOAMI
        STATUS,      // 7ALAH / STATUS
        DATABASES,   // DATABASES
        PASS,        // PASSWORD
        DESCRIBE,    // WASF / DESCRIBE / DESC (when standalone)
        ALTER,       // 3ADEL / ALTER (for ALTER TABLE)
        ADD,         // ADAF / ADD
        DROP,        // 2EMSA7 (when in context of ALTER)
        MODIFY,      // 3ADEL (when modifying column)
        RENAME,      // GHAYER_ESM / RENAME
        COLUMN,      // 3AMOD / COLUMN
        INSERT,      // EMLA
        INTO,        // GOWA
        VALUES,      // ELKEYAM

        // --- SPECIFIC ROLE TOKENS (For differentiation) ---
        ROLE_SUPERADMIN, // SUPERADMIN
        ROLE_ADMIN,      // ADMIN / MODEER
        ROLE_NORMAL,     // NORMAL / 3ADI
        ROLE_READONLY,   // READONLY / MOSHAHED
        ROLE_DENIED,     // DENIED / MAMNO3

        // --- TYPES ---
        INT_TYPE,    // RAKAM
        STRING_TYPE, // GOMLA
        BOOL_TYPE,   // BOOL
        DATE_TYPE,   // TARE5
        DECIMAL_TYPE,// KASR
        
        // --- CONSTRAINTS / INDEX ---
        INDEX,       // FEHRIS
        PRIMARY_KEY, // ASASI / MOFTA7
        ON,          // 3ALA

        // --- GROUP BY & AGGREGATES ---
        GROUP,       // MAGMO3A / GROUP
        BY,          // B / BY
        HAVING,      // ETHA / HAVING
        COUNT,       // 3ADD / COUNT
        SUM,         // MAG3MO3 / SUM
        AVG,         // MOTO3ASET / AVG
        MIN_AGG,     // ASGAR / MIN
        MAX_AGG,     // AKBAR / MAX
        
        // --- ORDER BY ---
        ORDER,       // RATEB / ORDER
        ASC,         // TASE3DI / ASC
        DESC,        // TANAZOLI / DESC
        
        // --- LIMIT / OFFSET ---
        LIMIT,       // 7ADD / LIMIT
        OFFSET,      // EBDA2MEN / OFFSET
        
        // --- DISTINCT ---
        DISTINCT,    // MOTA3MEZ / DISTINCT
        ALL,         // KOL / ALL
        
        // --- JOINS ---
        JOIN,        // ENTEDAH / JOIN
        INNER,       // DA5ELY / INNER
        LEFT,        // SHMAL / LEFT
        RIGHT,       // YAMEN / RIGHT
        OUTER,       // 5AREGY / OUTER
        CROSS,       // TAQATE3 / CROSS
        
        // --- FOREIGN KEYS ---
        FOREIGN,     // 5AREGY / FOREIGN
        KEY,         // MOFTA7 / KEY
        REFERENCES,  // YOSHEER / REFERENCES
        CASCADE,     // TATABE3 / CASCADE
        RESTRICT,    // MANE3 / RESTRICT
        SET,         // 5ALY / SET
        NO,          // LA / NO
        ACTION,      // E3RA2 / ACTION
        
        // --- CONSTRAINTS ---
        NULL_LIT,    // FADY / NULL
        NOT,         // MESH / NOT
        DEFAULT_KW,  // EFRADY / DEFAULT
        UNIQUE,      // WAHED / UNIQUE
        CHECK,       //فحص / CHECK
        AUTO_INCREMENT, // TAZAYED / AUTO_INCREMENT

        // --- CONDITIONAL ---
        IF,          // IF / LAW
        EXISTS,      // EXISTS / MAWGOOD

        // --- LITERALS ---
        DECIMAL_LITERAL,
        TRUE_LIT,    // AH
        FALSE_LIT,   // LA
        IDENTIFIER,  // names
        NUMBER,      // 123
        STRING_LIT,  // 'text'

        // --- TRANSACTIONS ---
        BEGIN_TXN,   // 2EBDA2
        ROLLBACK,    // 2ERGA3
        COMMIT,      // 2AKED

        // --- OPERATORS ---
        AND,         // WE
        OR,          // AW
        IN_OP,       // FE
        
        // --- SYMBOLS ---
        COMMA,       // ,
        L_PAREN,     // (
        R_PAREN,     // )
        SEMICOLON,   // ;
        EQUALS,      // =
        STAR,        // *
        GT,          // >
        LT,          // <
        EOF_TOKEN,   // End
        INVALID,      // Error
        
        CHECKPOINT, // "CHECKPOINT"
        RECOVER,    // "RECOVER"
        TO,         // "TO" / "ELA"
        
        // --- TIME TRAVEL ---
        LATEST,     // "LATEST" / "A5ER" - recover to most recent
        NOW,        // "NOW" / "DELWA2TY" - current time
        CURRENT,    // "CURRENT" / "7ALY" - current state
        AS,         // "AS" / "K" - for AS OF queries
        OF,         // "OF" / "MEN" - for AS OF queries
        
        // --- AI LAYER ---
        AI,         // "AI" / "ZAKA2" - for SHOW AI STATUS
        ANOMALIES,  // "ANOMALIES" / "SHOZOOZ" - for SHOW ANOMALIES
        EXECUTION,  // "EXECUTION" / "TANFEEZ" - for SHOW EXECUTION STATS
        STATS,      // "STATS" / "E7SA2EYAT" - for SHOW EXECUTION STATS

        // --- HASH INDEX ---
        HASH,       // "HASH" - for hash indexes

        // --- VIEWS ---
        VIEW,       // "VIEW" / "MANZAR" - for views

        // --- EXPLAIN / ANALYZE ---
        EXPLAIN,    // "EXPLAIN" / "SHAREH" - for explain
        ANALYZE,    // "ANALYZE" / "7ALLEL" - for analyze

        // --- SERVER CONTROL ---
        STOP,       // "STOP" / "WA2AF" - stop server
        SHUTDOWN,   // "SHUTDOWN" - shutdown server

        // --- CTEs ---
        WITH,       // "WITH" / "MA3" - Common Table Expressions

        // --- WINDOW FUNCTIONS ---
        OVER,       // "OVER" / "FAWK" - window frame
        PARTITION,  // "PARTITION" / "TAQSEEM" - partition by
        ROW_NUMBER, // "ROW_NUMBER" / "RAQAM_SAFF"
        RANK,       // "RANK" / "MARTABA"
        DENSE_RANK, // "DENSE_RANK"
        LAG,        // "LAG" / "SABE2"
        LEAD,       // "LEAD" / "TALE"

        // --- TABLE PARTITIONING ---
        RANGE_KW,   // "RANGE" / "MADAA" - range partition
        LESS,       // "LESS" / "A2AL"
        THAN,       // "THAN" / "MEN_KW"
        MAXVALUE,   // "MAXVALUE"
        PARTITIONS, // "PARTITIONS"

        // --- EXPORT / IMPORT ---
        EXPORT,     // "EXPORT" / "SADDR" - export data
        IMPORT,     // "IMPORT" / "ESTRAD" - import data

        // --- BACKUP / RESTORE ---
        BACKUP,     // "BACKUP" / "N5A_E7TYATY"
        RESTORE,    // "RESTORE" / "ESTER3A3"

        // --- STORED PROCEDURES ---
        PROCEDURE,  // "PROCEDURE" / "EGRA2"
        CALL,       // "CALL" / "NADY"
        DECLARE,    // "DECLARE" / "3ARREF"
        WHILE_KW,   // "WHILE" / "TALAMA"
        THEN,       // "THEN" / "YEB2A"
        ELSE_KW,    // "ELSE" / "WELLA"
        END_KW,     // "END" / "5ALAS"
        RETURN_KW,  // "RETURN" / "ARGA3"

        // --- TRIGGERS ---
        TRIGGER,    // "TRIGGER" / "MESHAGHAL"
        BEFORE,     // "BEFORE" / "QABL"
        AFTER,      // "AFTER" / "BA3D"
        EACH,       // "EACH" / "KOL_WAHD"
        ROW_KW,     // "ROW" / "SAFF"
        FOR_KW,     // "FOR" / "LEKOL"
        NEW_KW,     // "NEW" / "GEDEED"
        OLD_KW,     // "OLD" / "2ADEEM"

        // --- QUERY HISTORY ---
        HISTORY,    // "HISTORY" / "TARE5_ESTE3LAMAT"

        // --- SCHEDULED JOBS ---
        SCHEDULE,   // "SCHEDULE" / "GADWAL_ZAMANY"
        EVERY,      // "EVERY" / "KOL_MARRA"
        SECONDS_KW, // "SECONDS" / "SAWANY"
        MINUTES_KW, // "MINUTES" / "DA2AYE2"
        HOURS_KW,   // "HOURS" / "SA3AT"
        DO_KW,      // "DO" / "NAFFEZ"

        // --- REPLICATION ---
        REPLICATION, // "REPLICATION" / "NASAKHA"
        PRIMARY_SRV, // "PRIMARY" (server context)
        REPLICA,     // "REPLICA" / "SOORAH"
        PROMOTE,     // "PROMOTE"

        // --- INDEX ADVISOR ---
        SUGGESTIONS, // "SUGGESTIONS" / "EQTERA7AT"

        // --- QUERY FIREWALL ---
        BLOCKED,    // "BLOCKED" / "MAHMEY"
        APPROVE,    // "APPROVE" / "WAFE2"
        QUERY_KW    // "QUERY" / "ESTE3LAM"
    };

    struct Token {
        TokenType type;
        std::string text;
    };

} // namespace chronosdb