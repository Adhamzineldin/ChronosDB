#include "parser/lexer.h"
#include <map>
#include <cctype>
#include <algorithm>

namespace francodb {
    // Keywords map - accessible via GetKeywords() static method
    static const std::map<std::string, TokenType> kKeywords = {
        // --- COMMANDS ---
        {"2E5TAR", TokenType::SELECT},
        {"MEN", TokenType::FROM},
        {"LAMA", TokenType::WHERE},
        {"2E3MEL", TokenType::CREATE},
        {"DATABASE", TokenType::DATABASE},
        {"DATABASES", TokenType::DATABASES},
        {"GADWAL", TokenType::TABLE},
        {"2ESTA5DEM", TokenType::USE},
        {"USE", TokenType::USE},
        {"LOGIN", TokenType::LOGIN},
        
        // --- USER MGMT ---
        {"MOSTA5DEM", TokenType::USER},
        {"USER", TokenType::USER},
        {"3ABD", TokenType::USER}, // "Slave" (Funny/Franco style for user)
        {"WAZEFA", TokenType::ROLE},
        {"ROLE",   TokenType::ROLE},
        {"DOWR",   TokenType::ROLE},
        {"PASSWORD", TokenType::PASS},
        {"WARENY", TokenType::SHOW},
        {"SHOW",   TokenType::SHOW},
        {"ANAMEEN", TokenType::WHOAMI},
        {"WHOAMI",  TokenType::WHOAMI},
        {"7ALAH",   TokenType::STATUS},
        {"STATUS",  TokenType::STATUS},

        // --- DATA MODIFICATION ---
        {"2EMSA7",  TokenType::DELETE_CMD}, 
        {"5ALY",    TokenType::UPDATE_SET},
        {"3ADEL",   TokenType::UPDATE_CMD},
        {"EMLA",    TokenType::INSERT},
        {"GOWA",    TokenType::INTO},
        {"ELKEYAM", TokenType::VALUES},

        // --- ROLES (NEW) ---
        {"SUPERADMIN", TokenType::ROLE_SUPERADMIN},
        {"ADMIN",      TokenType::ROLE_ADMIN},
        {"MODEER",     TokenType::ROLE_ADMIN},    // Arabic for Manager
        {"NORMAL",     TokenType::ROLE_NORMAL},
        {"3ADI",       TokenType::ROLE_NORMAL},   // Arabic for Normal
        {"READONLY",   TokenType::ROLE_READONLY},
        {"MOSHAHED",   TokenType::ROLE_READONLY}, // Arabic for Viewer/Watcher
        {"DENIED",     TokenType::ROLE_DENIED},
        {"MAMNO3",     TokenType::ROLE_DENIED},   // Arabic for Forbidden

        // --- TYPES ---
        {"RAKAM", TokenType::INT_TYPE},
        {"GOMLA", TokenType::STRING_TYPE},
        {"BOOL",  TokenType::BOOL_TYPE},
        {"TARE5", TokenType::DATE_TYPE},
        {"KASR",  TokenType::DECIMAL_TYPE},
        
        // --- VALUES ---
        {"AH",    TokenType::TRUE_LIT},
        {"LA",    TokenType::FALSE_LIT},

        // --- LOGIC / OPS ---
        {"WE",    TokenType::AND},
        {"AW",    TokenType::OR},
        {"FE",    TokenType::IN_OP},
        {"3ALA",  TokenType::ON},
        
        // --- INDEX / PK ---
        {"FEHRIS", TokenType::INDEX},
        {"ASASI",  TokenType::PRIMARY_KEY},
        {"MOFTA7", TokenType::PRIMARY_KEY},

        // --- TRANSACTIONS ---
        {"2EBDA2", TokenType::BEGIN_TXN},
        {"2ERGA3", TokenType::ROLLBACK},
        {"2AKED",  TokenType::COMMIT}
    };

    Token Lexer::NextToken() {
        SkipWhitespace();
        if (cursor_ >= input_.length()) return {TokenType::EOF_TOKEN, ""};

        char c = input_[cursor_];

        // 1. Handle Words and Positive Numbers
        if (std::isalnum(c)) {
            return ReadIdentifierOrNumber();
        }

        // 2. NEW: Handle Negative Numbers (Start with '-')
        // We verify: Is it a '-', and is the NEXT char a digit? (e.g. -5)
        if (c == '-') {
            if (cursor_ + 1 < input_.length() && std::isdigit(input_[cursor_ + 1])) {
                return ReadIdentifierOrNumber();
            }
        }

        // 3. Handle Strings
        if (c == '\'') return ReadString();

        // 4. Handle Symbols
        cursor_++;
        switch (c) {
            case '*': return {TokenType::STAR, "*"};
            case ',': return {TokenType::COMMA, ","};
            case '(': return {TokenType::L_PAREN, "("};
            case ')': return {TokenType::R_PAREN, ")"};
            case ';': return {TokenType::SEMICOLON, ";"};
            case '=': return {TokenType::EQUALS, "="};
            case '>':
                if (cursor_ < input_.length() && input_[cursor_] == '=') {
                    cursor_++;
                    return {TokenType::IDENTIFIER, ">="};
                } else {
                    return {TokenType::IDENTIFIER, ">"};
                }
            case '<':
                if (cursor_ < input_.length() && input_[cursor_] == '=') {
                    cursor_++;
                    return {TokenType::IDENTIFIER, "<="};
                } else {
                    return {TokenType::IDENTIFIER, "<"};
                }
            // Note: If we support math later, independent '-' would go here.
            default:  return {TokenType::INVALID, std::string(1, c)};
        }
    }

    Token Lexer::ReadIdentifierOrNumber() {
        size_t start = cursor_;
        bool has_letter = false;
        bool has_decimal_point = false;

        // Handle optional leading negative sign
        if (input_[cursor_] == '-') {
            cursor_++;
        }

        while (cursor_ < input_.length()) {
            char c = input_[cursor_];
        
            // 1. Alphanumeric? Keep reading
            if (std::isalnum(c) || c == '_') {
                if (std::isalpha(c)) has_letter = true;
                cursor_++;
            } 
            // 2. Decimal Point logic
            else if (c == '.' && !has_letter && !has_decimal_point) {
                if (cursor_ + 1 < input_.length() && std::isdigit(input_[cursor_ + 1])) {
                    has_decimal_point = true;
                    cursor_++;
                } else {
                    break; 
                }
            } 
            else {
                break; 
            }
        }

        std::string text = input_.substr(start, cursor_ - start);

        // If it's a Keyword/Identifier, we check the map
        // (Note: Identifiers usually don't start with -, so -5 is safe)
        if (has_letter) {
            // Uppercase conversion for Case Insensitivity
            std::string upper_text = text;
            std::transform(upper_text.begin(), upper_text.end(), upper_text.begin(), ::toupper);

            auto it = kKeywords.find(upper_text);
            if (it != kKeywords.end()) {
                return {it->second, text}; 
            }
            return {TokenType::IDENTIFIER, text};
        }

        // If we are here, it's a number
        if (has_decimal_point) {
            return {TokenType::DECIMAL_LITERAL, text};
        }

        return {TokenType::NUMBER, text};
    }

    Token Lexer::ReadString() {
        cursor_++; // Skip opening '
        size_t start = cursor_;
        while (cursor_ < input_.length() && input_[cursor_] != '\'') {
            cursor_++;
        }
        std::string text = input_.substr(start, cursor_ - start);
        if (cursor_ < input_.length()) cursor_++; // Skip closing '
        return {TokenType::STRING_LIT, text};
    }

    void Lexer::SkipWhitespace() {
        while (cursor_ < input_.length() && std::isspace(input_[cursor_])) {
            cursor_++;
        }
    }

    // Helper for bulk tokenization
    std::vector<Token> Lexer::Tokenize() {
        std::vector<Token> tokens;
        Token tok;
        while ((tok = NextToken()).type != TokenType::EOF_TOKEN) {
            tokens.push_back(tok);
        }
        tokens.push_back(tok); // Add EOF
        return tokens;
    }

    // Static method to access keywords map
    const std::map<std::string, TokenType>& Lexer::GetKeywords() {
        return kKeywords;
    }

    // Helper to get English name for a token type
    std::string Lexer::GetTokenTypeName(TokenType type) {
        switch(type) {
            // Commands
            case TokenType::SELECT: return "SELECT";
            case TokenType::FROM: return "FROM";
            case TokenType::WHERE: return "WHERE";
            case TokenType::CREATE: return "CREATE";
            case TokenType::DATABASE: return "DATABASE";
            case TokenType::DATABASES: return "DATABASES";
            case TokenType::TABLE: return "TABLE";
            case TokenType::USE: return "USE";
            case TokenType::LOGIN: return "LOGIN";
            case TokenType::DELETE_CMD: return "DELETE";
            case TokenType::UPDATE_SET: return "SET";
            case TokenType::UPDATE_CMD: return "UPDATE";
            case TokenType::INSERT: return "INSERT";
            case TokenType::INTO: return "INTO";
            case TokenType::VALUES: return "VALUES";
            
            // User Management
            case TokenType::USER: return "USER";
            case TokenType::ROLE: return "ROLE";
            case TokenType::PASS: return "PASSWORD";
            case TokenType::SHOW: return "SHOW";
            case TokenType::WHOAMI: return "WHOAMI";
            case TokenType::STATUS: return "STATUS";
            
            // Roles
            case TokenType::ROLE_SUPERADMIN: return "SUPERADMIN";
            case TokenType::ROLE_ADMIN: return "ADMIN";
            case TokenType::ROLE_NORMAL: return "NORMAL";
            case TokenType::ROLE_READONLY: return "READONLY";
            case TokenType::ROLE_DENIED: return "DENIED";
            
            // Types
            case TokenType::INT_TYPE: return "INT";
            case TokenType::STRING_TYPE: return "VARCHAR/STRING";
            case TokenType::BOOL_TYPE: return "BOOL";
            case TokenType::DATE_TYPE: return "DATE";
            case TokenType::DECIMAL_TYPE: return "DECIMAL/FLOAT";
            
            // Boolean Values
            case TokenType::TRUE_LIT: return "TRUE";
            case TokenType::FALSE_LIT: return "FALSE";
            
            // Logical Operators
            case TokenType::AND: return "AND";
            case TokenType::OR: return "OR";
            case TokenType::IN_OP: return "IN";
            case TokenType::ON: return "ON";
            
            // Index & Constraints
            case TokenType::INDEX: return "INDEX";
            case TokenType::PRIMARY_KEY: return "PRIMARY KEY";
            
            // Transactions
            case TokenType::BEGIN_TXN: return "BEGIN";
            case TokenType::COMMIT: return "COMMIT";
            case TokenType::ROLLBACK: return "ROLLBACK";
            
            default: return "UNKNOWN";
        }
    }
} // namespace francodb
