#pragma once
#include <string>

namespace francodb {

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
        INVALID      // Error
    };

    struct Token {
        TokenType type;
        std::string text;
    };

} // namespace francodb