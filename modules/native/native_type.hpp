#pragma once
#include <string>
#include "value.h"

namespace RyRuntime {
	inline auto ry_type(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) -> RyValue {
		RyValue value = args[0];

		if (value.isNil())
			return std::string("nil");
		if (value.isNumber())
			return std::string("number");
		if (value.isBool())
			return std::string("boolean");
		if (value.isChar())
			return std::string("char");
		if (value.isString()) {
			return std::string("string");
		}
		if (value.isList())
			return std::string("list");
		if (value.isMap())
			return std::string("map");
		if (value.isRange())
			return std::string("range");
		if (value.isFunction())
			return std::string("function");
		if (value.isNative())
			return std::string("native");
		if (value.isClosure())
			return std::string("closure");
		if (value.isClass())
			return std::string("class");
		if (value.isInstance())
			return std::string("instance");
		if (value.isBoundMethod())
			return std::string("boundmethod");

		return std::string("unknown");
	}
} // namespace RyRuntime
