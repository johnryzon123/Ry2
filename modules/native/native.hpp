#pragma once
#include "native_io.hpp"
#include "native_list.hpp"
#include "native_sys.hpp"
#include "native_type.hpp"
#include "native_use.hpp"

namespace RyRuntime {
	inline std::vector<std::string> getNativeNames() { return {"out", "input", "type", "use", "Sys", "List"}; }
	inline void registerNatives(std::map<std::string, RyValue> &globals) {
		auto define = [&](std::string name, NativeFn fn, int arity) {
			auto native = std::make_shared<Frontend::RyNative>(fn, name, arity);
			globals[name] = RyValue(native);
		};

		define("out", ry_out, 1);
		define("input", ry_input, 1);
		define("type", ry_type, 1);
		define("use", ry_use, 1);

		auto defineProp = [&](std::shared_ptr<std::map<RyValue, RyValue>> map, std::string name, NativeFn fn, int arity) {
			auto native = std::make_shared<Frontend::RyNative>(fn, name, arity);
			(*map)[RyValue(name)] = RyValue(native);
		};

		// Sys Namespace
		auto sysMap = std::make_shared<std::map<RyValue, RyValue>>();
		defineProp(sysMap, "clock", ry_clock, 0);
		defineProp(sysMap, "clear", ry_clear, 0);
		defineProp(sysMap, "exit", ry_exit, 1);
		defineProp(sysMap, "args", ry_args, 0);
		globals["Sys"] = RyValue(sysMap);

		// List Namespace
		auto listMap = std::make_shared<std::map<RyValue, RyValue>>();
		defineProp(listMap, "pop", ry_pop, 1);
		globals["List"] = RyValue(listMap);
	}
} // namespace RyRuntime
