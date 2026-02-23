#pragma once
#include <unordered_map>
#include "native_io.hpp"
#include "native_sys.hpp"
#include "native_list.hpp"
#include "native_type.hpp"
#include "native_use.hpp"

namespace RyRuntime {
	inline void registerNatives(std::unordered_map<std::string, RyValue> &globals) {
		auto define = [&](std::string name, NativeFn fn, int arity) {
			auto native = std::make_shared<Frontend::RyNative>(fn, name, arity);
			globals[name] = RyValue(native);
		};

		define("out", ry_out, 1);
		define("input", ry_input, 1);
		define("clock", ry_clock, 0);
		define("clear", ry_clear, 0);
		define("exit", ry_exit, 1);
		define("len", ry_len, 1);
		define("pop", ry_pop, 1);
		define("type", ry_type, 1);
		define("use", ry_use, 1);
	}
} // namespace RyRuntime
