#include "../include/lexer.h"
#include <cctype>
#include <cstring>

namespace RyLSP {

	auto JsonLexer::scanAllTokens() -> std::vector<JsonToken> {
		std::vector<JsonToken> tokens;
		while (true) {
			JsonToken token = scanToken();
			tokens.push_back(token);
			if (token.type == JsonTokenType::END_OF_FILE)
				break;
		}
		return tokens;
	}

	auto JsonLexer::scanToken() -> JsonToken {
		skipWhitespace();
		start = current;

		if (isAtEnd())
			return makeToken(JsonTokenType::END_OF_FILE);

		char c = advance();

		if (isdigit(c))
			return number();
		if (isalpha(c))
			return identifier();

		switch (c) {
			case '{':
				return makeToken(JsonTokenType::LEFT_BRACE);
			case '}':
				return makeToken(JsonTokenType::RIGHT_BRACE);
			case '[':
				return makeToken(JsonTokenType::LEFT_BRACKET);
			case ']':
				return makeToken(JsonTokenType::RIGHT_BRACKET);
			case ',':
				return makeToken(JsonTokenType::COMMA);
			case ':':
				return makeToken(JsonTokenType::COLON);
			case '-':
				return number(); // JSON numbers can start with minus
			case '"':
				return string();
		}

		return errorToken("Unexpected character.");
	}

	auto JsonLexer::string() -> JsonToken {
		while (!isAtEnd()) {
			char c = peek();
			if (c == '"')
				break;

			if (c == '\\') {
				advance();
				if (!isAtEnd())
					advance();
			} else {
				if (c == '\n')
					line++;
				advance();
			}
		}

		if (isAtEnd())
			return errorToken("Unterminated string.");
		advance(); // consume closing quote
		return makeToken(JsonTokenType::STRING);
	}

	auto JsonLexer::number() -> JsonToken {
		while (isdigit(peek()))
			advance();

		// Look for a fractional part
		if (peek() == '.' && isdigit(peekNext())) {
			advance(); // Consume the "."
			while (isdigit(peek()))
				advance();
		}

		return makeToken(JsonTokenType::NUMBER);
	}

	auto JsonLexer::identifier() -> JsonToken {
		while (isalnum(peek()))
			advance();

		std::string_view text = source.substr(start, current - start);

		if (text == "true")
			return makeToken(JsonTokenType::TRUE);
		if (text == "false")
			return makeToken(JsonTokenType::FALSE);
		if (text == "null")
			return makeToken(JsonTokenType::NIL);

		return errorToken("Unexpected identifier.");
	}

	void JsonLexer::skipWhitespace() {
		for (;;) {
			char c = peek();
			switch (c) {
				case ' ':
				case '\r':
				case '\t':
					advance();
					break;
				case '\n':
					line++;
					advance();
					break;
				default:
					return;
			}
		}
	}

	auto JsonLexer::makeToken(JsonTokenType type) -> JsonToken {
		return {type, source.substr(start, current - start), line};
	}

	auto JsonLexer::errorToken(std::string_view message) -> JsonToken { return {JsonTokenType::ERROR, message, line}; }

} // namespace RyLSP
