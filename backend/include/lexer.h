//
// Created by ryzon on 12/29/25.
//

#pragma once
#include <vector>
#include "token.h"

namespace Backend {

	class Lexer {
	public:
		Lexer(std::string src) : source(std::move(src)){};
		~Lexer() = default;
		[[nodiscard]] std::vector<Token> getTokens() const;
		std::vector<Token> scanTokens();

	private:
		std::string source;
		std::vector<Token> tokens;

		// Position
		int start = 0;
		int current = 0;
		int line = 1;
		int column = 1;
		int tokenStartColumn = 1;

		// functions:
		//  function: char
		[[nodiscard]] char peek() const;
		[[nodiscard]] char peekNext() const;
		char next();

		// function: bool
		[[nodiscard]] bool isAtEnd() const;
		bool match(char expected);

		// function: void
		inline void addToken(Backend::TokenType type);
		inline void addToken(TokenType type, RyValue literal);
		inline void number();
		inline void identifier();
		inline void str();

		inline void scanToken();
	};

} // namespace Backend
