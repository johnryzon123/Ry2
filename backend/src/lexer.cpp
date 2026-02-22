//
// Created by ryzon on 12/29/25.
//

#include "../include/lexer.h"
#include <utility>
#include "../include/tools.h"

using namespace Backend;

std::vector<Token> Lexer::getTokens() const { return tokens; }
char Lexer::peek() const {
	if (isAtEnd())
		return '\0';
	return source[current];
}

char Lexer::next() {
	char c = source[current++];
	if (c == '\n') {
		line++;
		column = 1;
	} else {
		column++;
	}
	return c;
}

char Lexer::peekNext() const {
	if (current + 1 >= source.length())
		return '\0';
	return source[current + 1];
}

bool Lexer::isAtEnd() const { return current >= source.length(); }

bool Lexer::match(const char expected) {
	if (expected != peek())
		return false;
	next();
	return true;
}

void Lexer::addToken(const Backend::TokenType type) { addToken(type, RyValue()); }

void Lexer::addToken(TokenType type, RyValue literal) {
	const std::string text = source.substr(start, current - start);
	tokens.emplace_back(type, text, std::move(literal), line, static_cast<int>(tokenStartColumn));
}

void Lexer::scanToken() {
	switch (const char c = next()) {
		case '#':
			while (peek() != '\n' && !isAtEnd())
				next();
			break;
		case '+':
			match('+') ? addToken(TokenType::PLUS_PLUS) : addToken(TokenType::PLUS);
			break;
		case '-':
			if (match('>')) {
				addToken(TokenType::LARROW);
			} else if (match('-')) {
				addToken(TokenType::MINUS_MINUS);
			} else {
				addToken(TokenType::MINUS);
			}
			break;
		case '*':
			addToken(TokenType::STAR);
			break;
		case '/':
			addToken(TokenType::DIVIDE);
			break;
		case '=':
			match('=') ? addToken(TokenType::EQUAL_EQUAL) : addToken(TokenType::EQUAL);
			break;
		case '<':
			if (match('<')) {
				addToken(TokenType::LESS_LESS);
				break;
			} else if (match('=')) {
				addToken(TokenType::LESS_EQUAL);
				break;
			} else {
				addToken(TokenType::LESS);
			}
			break;
		case '>':
			if (match('>')) {
				addToken(TokenType::GREATER_GREATER);
				break;
			} else if (match('=')) {
				addToken(TokenType::GREATER_EQUAL);
				break;
			} else {
				addToken(TokenType::GREATER);
			}
			break;
		case '!':
			match('=') ? addToken(TokenType::BANG_EQUAL) : addToken(TokenType::BANG);
			break;
		case '(':
			addToken(TokenType::LPAREN);
			break;
		case ')':
			addToken(TokenType::RPAREN);
			break;
		case '{':
			addToken(TokenType::LBRACE);
			break;
		case '}':
			addToken(TokenType::RBRACE);
			break;
		case ',':
			addToken(TokenType::COMMA);
			break;
		case ':':
			match(':') ? addToken(TokenType::DOUBLE_COLON) : addToken(TokenType::COLON);
			break;
		case '[':
			addToken(TokenType::LBRACKET);
			break;
		case ']':
			addToken(TokenType::RBRACKET);
			break;
		case '.':
			addToken(TokenType::DOT);
			break;
		case '%':
			addToken(TokenType::PERCENT);
			break;
		case '&':
			addToken(TokenType::AMPERSAND);
			break;
		case '^':
			addToken(TokenType::CARET);
			break;
		case '|':
			addToken(TokenType::PIPE);
			break;
		case '~':
			addToken(TokenType::TILDE);
			break;
		case '"':
			str();
			break;
		case ' ':
		case '\t':
			break;
		case '\n':
			break;
		default:
			if (std::isdigit(c)) {
				number();
			} else if (std::isalpha(c) || c == '_') {
				identifier();
			} else {
				const std::string s(1, c);
				RyTools::report(line, static_cast<int>(tokenStartColumn), "", "Unexpected character: '" + s + "'", source);
			}
			break;
	}
}

std::vector<Token> Lexer::scanTokens() {
	while (!isAtEnd()) {
		tokenStartColumn = column;
		start = current;
		scanToken();
	}

	addToken(TokenType::EOF_TOKEN);
	return tokens;
}

void Lexer::number() {
	while (std::isdigit(peek()))
		next();

	if (peek() == '.') {
		next();
		while (std::isdigit(peek()))
			next();
	}
	const std::string text = source.substr(start, current - start);
	addToken(TokenType::NUMBER, std::stod(text));
}
void Lexer::identifier() {
	while (std::isalnum(peek()) || peek() == '_')
		next();

	const std::string text = source.substr(start, current - start);

	if (const auto it = keywords.find(text); it != keywords.end()) {
		addToken(it->second);
	} else {
		addToken(TokenType::IDENTIFIER);
	}
}

void Lexer::str() {
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '$' && peekNext() == '{') {
			// Emit the string before the ${
			std::string segment = source.substr(start + 1, current - (start + 1));
			tokens.emplace_back(TokenType::STRING, segment, segment, line, column);
			tokens.emplace_back(TokenType::PLUS, "+", RyValue(), line, column);

			next();
			next();

			// Collect the variable name
			int varStart = current;
			while (peek() != '}' && !isAtEnd()) {
				next();
			}

			if (isAtEnd()) {
				RyTools::report(line, column, "", "Unterminated interpolation.", source);
				return;
			}

			std::string varName = source.substr(varStart, current - varStart);
			tokens.emplace_back(TokenType::IDENTIFIER, varName, RyValue(), line, column);

			next(); // skip '}'

			tokens.emplace_back(TokenType::PLUS, "+", RyValue(), line, column);

			// Reset start to current-1 so the next part of the string knows where it began
			start = current - 1;
		} else {
			next();
		}
	}

	if (isAtEnd()) {
		RyTools::report(line, column, "", "Unterminated string.", source);
		return;
	}

	next(); // skip closing "

	std::string finalSegment = source.substr(start + 1, current - start - 2);
	tokens.emplace_back(TokenType::STRING, finalSegment, finalSegment, line, column);
}
