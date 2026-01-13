#pragma once
#include <string>

namespace francodb {

    enum class TokenType {
        // Keywords (Your Custom Franco Style)
        SELECT,      // 2E5TAR
        FROM,        // MEN
        WHERE,       // LAMA
        CREATE,      // 2E3MEL
        TABLE,       // GADWAL
        INSERT,      // EMLA
        INTO,        // GOWA
        VALUES,      // ELKEYAM
    
        // Types
        INT_TYPE,    // RAKAM
        STRING_TYPE, // ESM
    
        // Literals & Symbols
        IDENTIFIER,  // The name used in code (e.g., users, id)
        NUMBER,      // 123
        STRING_LIT,  // 'Ahmed'
        COMMA,       // ,
        L_PAREN,     // (
        R_PAREN,     // )
        SEMICOLON,   // ;
        EQUALS,      // =
        STAR,        // *
        EOF_TOKEN,   // End of query
        INVALID      // For characters the lexer doesn't recognize
    };

    struct Token {
        TokenType type;
        std::string text;
    };

} // namespace francodb