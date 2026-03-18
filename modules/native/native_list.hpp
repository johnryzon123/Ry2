#pragma once
#include <stdexcept>
#include "value.h"

namespace RyRuntime {
	inline auto ry_pop(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) -> RyValue {
		// We look for the list receiver (usually at args[-1] if argCount is 0)
		RyValue *listPtr = nullptr;
		for (int i = 0; i >= -5; i--) {
			if (args[i].isList()) {
				listPtr = &args[i];
				break;
			}
		}

		if (listPtr) {
			auto list = listPtr->asList();
			if (list->empty())
				throw std::runtime_error("Empty list pop.");
			RyValue val = list->back();
			list->pop_back();
			return val;
		}
		throw std::runtime_error("pop() called on non-list.");
	}
} // namespace RyRuntime
