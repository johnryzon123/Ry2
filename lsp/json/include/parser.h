#ifndef ry_json_parser_h
#define ry_json_parser_h

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "lexer.h"
#include "value.h"

namespace RyLSP {

	// Forward declaration for recursive structures
	class JsonValue;

	using JsonObject = std::map<std::string, JsonValue>;
	using JsonArray = std::vector<JsonValue>;

	class JsonParser {
	public:
		JsonParser(const std::vector<JsonToken> &tokens) : tokens(tokens), current(0) {}

		std::optional<JsonValue> parse();

	private:
		const std::vector<JsonToken> &tokens;
		size_t current;

		// Recursive descent functions
		JsonValue parseValue();
		JsonObject parseObject();
		JsonArray parseArray();

		// Utilities
		const JsonToken &peek() const;
		const JsonToken &previous() const;
		const JsonToken &advance();
		bool match(JsonTokenType type);
		bool check(JsonTokenType type) const;
		bool isAtEnd() const;
	};

} // namespace RyLSP

#endif
