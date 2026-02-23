#pragma once
#include <string>
#include <unordered_map>
#include <utility>
#include "value.h"


namespace Backend {
	enum class TokenType {
		// Single Characters
		PLUS,
		MINUS,
		STAR,
		DIVIDE,
		EQUAL,
		GREATER,
		LESS,
		LPAREN,
		RPAREN,
		LBRACE,
		RBRACE,
		BANG,
		COMMA,
		COLON,
		LBRACKET,
		RBRACKET,
		DOT,
		PERCENT,
		AMPERSAND,
		CARET,
		PIPE,
		TILDE,

		// Double Characters
		EQUAL_EQUAL,
		LESS_EQUAL,
		GREATER_EQUAL,
		BANG_EQUAL,
		DOUBLE_COLON,
		LARROW,
		PLUS_PLUS,
		MINUS_MINUS,
		LESS_LESS,
		GREATER_GREATER,

		// Identifiers
		NUMBER,
		IDENTIFIER,
		STRING,

		// Keywords
		IF,
		ELSE,
		FUNC,
		WHILE,
		FOR,
		AND,
		OR,
		TRUE,
		FALSE,
		NULL_TOKEN,
		ALIAS,
		IMPORT,
		RETURN,
		AS,
		NAMESPACE,
		DATA,
		THIS,
		IN,
		EACH,
		TO,
		STOP,
		SKIP,
		UNLESS,
		UNTIL,
		DO,
		CLASS,
		PRIVATE,
		CHILDOF,
		ATTEMPT,
		FAIL,
		PANIC,
		FINALLY,


		// Misc
		EOF_TOKEN,
		Nothing_Here
	};

	struct Token {
		TokenType type;
		std::string lexeme;
		RyValue literal;
		int line;
		int column;
		Token(TokenType t, std::string lex, RyValue lit, int l, int c) :
				type(t), lexeme(std::move(lex)), literal(std::move(lit)), line(l), column(c) {}
	};

	inline static const std::unordered_map<std::string, TokenType> keywords{
			{"import", TokenType::IMPORT},	 {"func", TokenType::FUNC},				{"while", TokenType::WHILE},
			{"if", TokenType::IF},					 {"else", TokenType::ELSE},				{"true", TokenType::TRUE},
			{"false", TokenType::FALSE},		 {"null", TokenType::NULL_TOKEN}, {"for", TokenType::FOR},
			{"and", TokenType::AND},				 {"or", TokenType::OR},						{"alias", TokenType::ALIAS},
			{"return", TokenType::RETURN},	 {"as", TokenType::AS},						{"namespace", TokenType::NAMESPACE},
			{"data", TokenType::DATA},			 {"this", TokenType::THIS},				{"to", TokenType::TO},
			{"in", TokenType::IN},					 {"foreach", TokenType::EACH},		{"stop", TokenType::STOP},
			{"skip", TokenType::SKIP},			 {"unless", TokenType::UNLESS},		{"until", TokenType::UNTIL},
			{"do", TokenType::DO},					 {"class", TokenType::CLASS},			{"private", TokenType::PRIVATE},
			{"childof", TokenType::CHILDOF}, {"attempt", TokenType::ATTEMPT}, {"fail", TokenType::FAIL},
			{"panic", TokenType::PANIC}, {"finally", TokenType::FINALLY}};
} // namespace Backend
