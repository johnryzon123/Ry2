#ifndef ry_json_value_h
#define ry_json_value_h

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace RyLSP {

	class JsonValue;

	// Using names that make the code readable
	using JsonObject = std::map<std::string, JsonValue>;
	using JsonArray = std::vector<JsonValue>;
	std::string stringify(const JsonValue &value);

	class JsonValue {
	public:
		// monostate represents "null" in JSON
		std::variant<std::monostate, bool, double, std::string, JsonArray, JsonObject> data;

		// Constructors
		JsonValue() : data(std::monostate{}) {}
		JsonValue(std::monostate v) : data(v) {}
		JsonValue(bool v) : data(v) {}
		JsonValue(double v) : data(v) {}
		JsonValue(std::string v) : data(std::move(v)) {}
		JsonValue(JsonArray v) : data(std::move(v)) {}
		JsonValue(JsonObject v) : data(std::move(v)) {}

		// Type Checks
		bool isNull() const { return std::holds_alternative<std::monostate>(data); }
		bool isBool() const { return std::holds_alternative<bool>(data); }
		bool isNumber() const { return std::holds_alternative<double>(data); }
		bool isString() const { return std::holds_alternative<std::string>(data); }
		bool isArray() const { return std::holds_alternative<JsonArray>(data); }
		bool isObject() const { return std::holds_alternative<JsonObject>(data); }

		const std::string &asString() const { return std::get<std::string>(data); }
		double asNumber() const { return std::get<double>(data); }
		bool asBool() const { return std::get<bool>(data); }
		const JsonObject &asObject() const { return std::get<JsonObject>(data); }
		const JsonArray &asArray() const { return std::get<JsonArray>(data); }


		// Accessors for easy navigation
		const JsonValue &operator[](const std::string &key) const;
		const JsonValue &operator[](size_t index) const;
	};

} // namespace RyLSP

#endif
