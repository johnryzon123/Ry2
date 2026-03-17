#include "../include/value.h"
#include <stdexcept>

namespace RyLSP {

	const JsonValue &JsonValue::operator[](const std::string &key) const {
		if (const auto *obj = std::get_if<JsonObject>(&data)) {
			auto it = obj->find(key);
			if (it != obj->end()) {
				return it->second;
			}
			throw std::runtime_error("Key '" + key + "' not found in JSON object.");
		}
		throw std::runtime_error("Attempted to access key on non-object JSON value.");
	}

	const JsonValue &JsonValue::operator[](size_t index) const {
		if (const auto *arr = std::get_if<JsonArray>(&data)) {
			if (index < arr->size()) {
				return (*arr)[index];
			}
			throw std::runtime_error("Index out of bounds in JSON array.");
		}
		throw std::runtime_error("Attempted to access index on non-array JSON value.");
	}

	// Helper for efficient string building to avoid recursion overhead
	static void stringifyHelper(const JsonValue &value, std::string &out) {
		if (value.isNull()) {
			out += "null";
			return;
		}
		if (value.isBool()) {
			out += value.asBool() ? "true" : "false";
			return;
		}
		if (value.isNumber()) {
			out += std::to_string(value.asNumber());
			return;
		}
		if (value.isString()) {
			out += "\"";
			for (char c: value.asString()) {
				switch (c) {
					case '\"':
						out += "\\\"";
						break;
					case '\\':
						out += "\\\\";
						break;
					case '\b':
						out += "\\b";
						break;
					case '\f':
						out += "\\f";
						break;
					case '\n':
						out += "\\n";
						break;
					case '\r':
						out += "\\r";
						break;
					case '\t':
						out += "\\t";
						break;
					default:
						out += c;
						break;
				}
			}
			out += "\"";
			return;
		}

		if (value.isArray()) {
			out += "[";
			const auto &arr = value.asArray();
			for (size_t i = 0; i < arr.size(); ++i) {
				stringifyHelper(arr[i], out);
				if (i < arr.size() - 1)
					out += ",";
			}
			out += "]";
			return;
		}

		if (value.isObject()) {
			out += "{";
			const auto &obj = value.asObject();
			for (auto it = obj.begin(); it != obj.end(); ++it) {
				out += "\"" + it->first + "\":";
				stringifyHelper(it->second, out);
				if (std::next(it) != obj.end())
					out += ",";
			}
			out += "}";
			return;
		}
		out += "null";
	}

	std::string stringify(const JsonValue &value) {
		std::string out;
		out.reserve(256);
		stringifyHelper(value, out);
		return out;
	}

} // namespace RyLSP
