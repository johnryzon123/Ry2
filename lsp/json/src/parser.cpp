#include "../include/parser.h"
#include <stdexcept>

namespace RyLSP {

	// Helper to unescape JSON strings (e.g. turning \" into " inside the string)
	static std::string unescape(std::string_view lexeme) {
		std::string res;
		res.reserve(lexeme.size());

		// Lexeme includes the surrounding quotes, so iterate from 1 to size-1
		for (size_t i = 1; i < lexeme.size() - 1; ++i) {
			char c = lexeme[i];
			if (c == '\\') {
				if (i + 1 < lexeme.size() - 1) {
					char next = lexeme[i + 1];
					switch (next) {
						case '"':
							res += '"';
							break;
						case '\\':
							res += '\\';
							break;
						case '/':
							res += '/';
							break;
						case 'b':
							res += '\b';
							break;
						case 'f':
							res += '\f';
							break;
						case 'n':
							res += '\n';
							break;
						case 'r':
							res += '\r';
							break;
						case 't':
							res += '\t';
							break;
						default:
							// For basic LSP needs, we just append the char if it's not a standard escape
							// (like \u handling which is complex, we pass the char through)
							res += next;
							break;
					}
					i++; // Skip the escaped character
				}
			} else {
				res += c;
			}
		}
		return res;
	}

	std::optional<JsonValue> JsonParser::parse() {
		try {
			if (isAtEnd())
				return std::nullopt;
			return parseValue();
		} catch (...) {
			return std::nullopt; // Basic error handling for the LSP
		}
	}

	JsonValue JsonParser::parseValue() {
		if (match(JsonTokenType::FALSE))
			return {false};
		if (match(JsonTokenType::TRUE))
			return {true};
		if (match(JsonTokenType::NIL))
			return {std::monostate{}};

		if (match(JsonTokenType::NUMBER)) {
			return {std::stod(std::string(previous().lexeme))};
		}

		if (match(JsonTokenType::STRING)) {
			return {unescape(previous().lexeme)};
		}

		if (match(JsonTokenType::LEFT_BRACKET))
			return {parseArray()};
		if (match(JsonTokenType::LEFT_BRACE))
			return {parseObject()};

		throw std::runtime_error("Unexpected token in JSON");
	}

	JsonObject JsonParser::parseObject() {
		JsonObject object;

		if (!check(JsonTokenType::RIGHT_BRACE)) {
			do {
				if (!match(JsonTokenType::STRING))
					throw std::runtime_error("Expected string key.");

				std::string key = unescape(previous().lexeme);

				if (!match(JsonTokenType::COLON))
					throw std::runtime_error("Expected ':'");

				object[key] = parseValue();
			} while (match(JsonTokenType::COMMA));
		}

		if (!match(JsonTokenType::RIGHT_BRACE))
			throw std::runtime_error("Expected '}'");
		return object;
	}

	JsonArray JsonParser::parseArray() {
		JsonArray array;

		if (!check(JsonTokenType::RIGHT_BRACKET)) {
			do {
				array.push_back(parseValue());
			} while (match(JsonTokenType::COMMA));
		}

		if (!match(JsonTokenType::RIGHT_BRACKET))
			throw std::runtime_error("Expected ']'");
		return array;
	}

	// Utilities
	const JsonToken &JsonParser::peek() const { return tokens[current]; }
	const JsonToken &JsonParser::previous() const { return tokens[current - 1]; }
	bool JsonParser::isAtEnd() const { return peek().type == JsonTokenType::END_OF_FILE; }

	const JsonToken &JsonParser::advance() {
		if (!isAtEnd())
			current++;
		return previous();
	}

	bool JsonParser::check(JsonTokenType type) const {
		if (isAtEnd())
			return false;
		return peek().type == type;
	}

	bool JsonParser::match(JsonTokenType type) {
		if (check(type)) {
			advance();
			return true;
		}
		return false;
	}

} // namespace RyLSP
