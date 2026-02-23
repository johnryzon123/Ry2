#include <stdexcept>
#include "value.h"
#include "env.h" // IWYU pragma: keep

namespace RyRuntime {
	inline RyValue ry_len(int argCount, RyValue *args, std::unordered_map<std::string, RyValue>& globals) {
		RyValue arg = args[0];

		if (arg.isList()) {
			auto vec = arg.asList();
			return static_cast<double>(vec->size());
		}

		if (arg.isString()) {
			auto str = arg.asString();
			return static_cast<double>(str.length());
		}

		if (arg.isMap()) {
			auto env = arg.asMap();
			return (double) env->size();
		}

		throw std::runtime_error("Argument to len() must be a list, string, or map.");
	}
	inline RyValue ry_pop(int argCount, RyValue *args, std::unordered_map<std::string, RyValue>& globals) {
		RyValue &arg = args[0];
		if (arg.isList()) {
			auto list = arg.asList();
			if (list->empty()) {
				throw std::runtime_error("Cannot pop from an empty list.");
			}
			RyValue lastElement = list->back();
			list->pop_back();
			return lastElement;
		}

		throw std::runtime_error("Argument to pop() must be a list.");
	}
} // namespace Frontend
