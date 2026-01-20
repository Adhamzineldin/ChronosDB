#pragma once

#include <string>
#include <vector>
#include <map>
#include "parser/token.h"

namespace francodb {

    class Lexer {
    public:
        explicit Lexer(std::string input) : input_(std::move(input)), cursor_(0) {}

        Token NextToken();
        std::vector<Token> Tokenize();
        
        // Static method to access keywords map for dynamic help generation
        static const std::map<std::string, TokenType>& GetKeywords();
        
        // Helper to get English name for a token type
        static std::string GetTokenTypeName(TokenType type);

    private:
        void SkipWhitespace();
        // Combined logic to handle Franco keywords starting with digits (e.g., 2E5TAR)
        Token ReadIdentifierOrNumber(); 
        Token ReadString();

        std::string input_;
        size_t cursor_;
    };

} // namespace francodb