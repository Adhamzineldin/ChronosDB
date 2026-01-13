#include "parser/lexer.h"
#include <map>
#include <cctype>
#include <algorithm>

namespace francodb {

static const std::map<std::string, TokenType> kKeywords = {
    {"2E5TAR", TokenType::SELECT},
    {"MEN",    TokenType::FROM},
    {"LAMA",   TokenType::WHERE},
    {"2E3MEL", TokenType::CREATE},
    {"GADWAL", TokenType::TABLE},
    {"EMLA",   TokenType::INSERT},
    {"GOWA",   TokenType::INTO},
    {"ELKEYAM",TokenType::VALUES},
    {"RAKAM",  TokenType::INT_TYPE},
    {"ESM",    TokenType::STRING_TYPE}
};

Token Lexer::NextToken() {
    SkipWhitespace();
    if (cursor_ >= input_.length()) return {TokenType::EOF_TOKEN, ""};

    char c = input_[cursor_];

    // Handle alphanumeric blocks (Keywords like 2E5TAR, Identifiers, or pure Numbers)
    if (std::isalnum(c)) {
        return ReadIdentifierOrNumber();
    }

    // Handle string literals
    if (c == '\'') return ReadString();

    // Handle symbols
    cursor_++;
    switch (c) {
        case '*': return {TokenType::STAR, "*"};
        case ',': return {TokenType::COMMA, ","};
        case '(': return {TokenType::L_PAREN, "("};
        case ')': return {TokenType::R_PAREN, ")"};
        case ';': return {TokenType::SEMICOLON, ";"};
        case '=': return {TokenType::EQUALS, "="};
        default:  return {TokenType::INVALID, std::string(1, c)};
    }
}

    Token Lexer::ReadIdentifierOrNumber() {
    size_t start = cursor_;
    bool has_letter = false;

    while (cursor_ < input_.length() && (std::isalnum(input_[cursor_]) || input_[cursor_] == '_')) {
        if (std::isalpha(input_[cursor_])) {
            has_letter = true;
        }
        cursor_++;
    }

    std::string text = input_.substr(start, cursor_ - start);

    // --- CASE INSENSITIVITY LOGIC ---
    std::string uppercase_text = text;
    std::transform(uppercase_text.begin(), uppercase_text.end(), uppercase_text.begin(), ::toupper);

    // 1. Check if the UPPERCASE version is a Franco Keyword
    auto it = kKeywords.find(uppercase_text);
    if (it != kKeywords.end()) {
        return {it->second, text}; // Return original text but the Keyword type
    }
    // --------------------------------

    // 2. If it's purely digits, it's a Number literal
    if (!has_letter) {
        return {TokenType::NUMBER, text};
    }

    // 3. Otherwise, it's a regular name (Keep original case for table/column names)
    return {TokenType::IDENTIFIER, text};
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

} // namespace francodb