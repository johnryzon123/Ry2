#include <iostream>
#include <map>
#include <memory>
#include "loader.h"
#include "value.h"

namespace RyRuntime {
	typedef void (*DKRegisterFn)(const char *name, NativeFn fn, int arity, void *mapPtr);
	typedef void (*DKInitFn)(DKRegisterFn, void *);

	static void register_callback(const char *name, NativeFn fn, int arity, void *mapPtr) {
		auto *map = static_cast<std::map<RyValue, RyValue> *>(mapPtr);
		auto native = std::make_shared<Frontend::RyNative>(fn, name, arity);
		(*map)[RyValue(name)] = RyValue(native);
	}

	inline RyValue ry_use(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
		if (argCount < 1 || !args[0].isString())
			return RyValue();

		std::string libName = args[0].to_string();
		LibHandle handle = Backend::RyLoader::open(libName);

		if (!handle) {
			std::cerr << "Ry Library Error: " << Backend::RyLoader::getError() << std::endl;
			return RyValue();
		}

		// Create the Map that will be returned to the Ry script
		auto moduleMap = std::make_shared<std::map<RyValue, RyValue>>();

		// Load the "init_ry_module" symbol
		auto init_module = (DKInitFn) Backend::RyLoader::getSymbol(handle, "init_ry_module");

		if (init_module) {
			init_module(register_callback, moduleMap.get());
		} else {
			std::cerr << "Ry Symbol Error: " << Backend::RyLoader::getError() << std::endl;
		}

		// Return the Map as a RyValue
		return RyValue(moduleMap);
	}
} // namespace RyRuntime
