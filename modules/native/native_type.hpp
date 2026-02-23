#include <string>
#include "value.h"

namespace RyRuntime {
	inline RyValue ry_type(int argCount, RyValue *args, std::unordered_map<std::string, RyValue>& globals) {
		RyValue value = args[0];

		if (value.isNumber())
			return std::string("number");
		if (value.isString())
			return std::string("string");
		if (value.isBool())
			return std::string("bool");
		if (value.isList())
			return std::string("list");
		if (value.isMap())
			return std::string("map");

		return std::string("unknown");
	}
} // namespace Frontend
