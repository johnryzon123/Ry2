#ifndef ry_json_lexer_h
#define ry_json_lexer_h

#include <string_view>
#include <vector>

namespace RyLSP {

  enum class JsonTokenType {
    // Single-character tokens
    LEFT_BRACE, RIGHT_BRACE,    // { }
    LEFT_BRACKET, RIGHT_BRACKET, // [ ]
    COMMA, COLON,               // , :

    // Literals
    STRING, NUMBER,

    // Keywords
    TRUE, FALSE, NIL,

    // Special
    END_OF_FILE, ERROR
  };

  struct JsonToken {
    JsonTokenType type;
    std::string_view lexeme; // Points into the source string (no copying!)
    int line;
  };

  class JsonLexer {
  public:
    JsonLexer(std::string_view source) 
      : source(source) {}

    std::vector<JsonToken> scanAllTokens();
    JsonToken scanToken();

  private:
    std::string_view source;
    size_t start{};
    size_t current{};
    int line{};

    // Helpers
    [[nodiscard]] auto isAtEnd() const -> bool { return current >= source.length(); }
    auto advance() -> char { return source[current++]; }
    [[nodiscard]] auto peek() const -> char { return isAtEnd() ? '\0' : source[current]; }
    [[nodiscard]] auto peekNext() const -> char { 
      if (current + 1 >= source.length()) return '\0';
      return source[current + 1];
    }
    
    auto match(char expected) -> bool {
      if (isAtEnd() || source[current] != expected) return false;
      current++;
      return true;
    }

    auto makeToken(JsonTokenType type) -> JsonToken;
    auto errorToken(std::string_view message) -> JsonToken;
    
    void skipWhitespace();
    auto string() -> JsonToken;
    auto number() -> JsonToken;
    auto identifier() -> JsonToken; // For true, false, null
  };

} // namespace RyLSP

#endif