#include <algorithm>
#include <string>
#include <vector>
#include "value.h"

// Forward declarations to keep it clean
namespace Frontend {
	class RyCallable;
}

typedef RyValue (*RawNativeFn)(std::vector<RyValue>);
typedef void (*RegisterFn)(const char *, RawNativeFn, void *);

RyValue string_upper(std::vector<RyValue> args) {
	if (args.empty())
		return RyValue();

	try {
		if (args[0].isString()) {
			std::string s = args[0].asString();
			std::transform(s.begin(), s.end(), s.begin(), ::toupper);
			return s;
		}
	} catch (...) {
		// Guard against internal type errors
	}
	return args[0];
}

RyValue string_lower(std::vector<RyValue> args) {
	if (args.empty())
		return RyValue();

	try {
		if (args[0].isString()) {
			std::string s = args[0].asString();
			std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			return s;
		}
	} catch (...) {
	}
	return args[0];
}

RyValue string_substr(std::vector<RyValue> args) {
	// Expecting: substr(string, start, length)
	if (args.size() < 3)
		return RyValue();

	try {
		if (args[0].isString() && args[1].isNumber() && args[2].isNumber()) {

			std::string s = args[0].asString();
			int start = static_cast<int>(args[1].asNumber());
			int len = static_cast<int>(args[2].asNumber());

			// Safety: check bounds to prevent C++ crashes
			if (start < 0 || start >= s.length())
				return std::string("");

			return s.substr(start, len);
		}
	} catch (...) {
	}
	return RyValue();
}

extern "C" void register_ry_module(RegisterFn register_fn, void *target) {
	register_fn("upper", string_upper, target);
	register_fn("lower", string_lower, target);
	register_fn("substr", string_substr, target);
}
