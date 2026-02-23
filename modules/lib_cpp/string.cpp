#include <algorithm>
#include <string>
#include "value.h"

typedef RyValue (*RawNativeFn)(int, RyValue *, std::unordered_map<std::string, RyValue> &);
typedef void (*RegisterFn)(const char *, RawNativeFn, int, void *);

RyValue string_upper(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue();

	std::string s = args[0].to_string();
	std::transform(s.begin(), s.end(), s.begin(), ::toupper);
	return RyValue(s);
}

RyValue string_lower(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue();

	std::string s = args[0].to_string();
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
	return RyValue(s);
}

RyValue string_substr(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
	if (argCount < 3 || !args[0].isString() || !args[1].isNumber() || !args[2].isNumber())
		return RyValue(""); // Return empty string instead of Nil for consistency

	std::string s = args[0].asString();
	int start = (int) args[1].asNumber();
	int len = (int) args[2].asNumber();

	// Clamp start to 0 if negative, or return empty if way out of bounds
	if (start < 0)
		start = 0;
	if (start >= (int) s.length())
		return RyValue("");

	// Ensure len doesn't go past the end of the string
	if (start + len > (int) s.length()) {
		len = (int) s.length() - start;
	}

	return RyValue(s.substr(start, len));
}

extern "C" void init_ry_module(RegisterFn register_fn, void *target) {
	register_fn("upper", string_upper, 1, target);
	register_fn("lower", string_lower, 1, target);
	register_fn("substr", string_substr, 3, target);
}
