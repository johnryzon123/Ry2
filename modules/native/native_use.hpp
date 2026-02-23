#include <iostream>
#include <memory>
#include <unordered_map>
#include "loader.h"
#include "value.h"

namespace RyRuntime {
    // Name, Function Pointer, Arity, and the Map Pointer
    typedef void (*RegisterFn)(const char* name, NativeFn fn, int arity, void* mapPtr);
    
    typedef void (*InitFnType)(RegisterFn, void*);

    inline RyValue ry_use(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
        if (argCount < 1 || !args[0].isString()) return RyValue();

        std::string libName = args[0].to_string();
        LibHandle handle = Backend::RyLoader::open(libName);

        if (!handle) {
            std::cerr << "Ry Library Error: " << Backend::RyLoader::getError() << std::endl;
            return RyValue(); 
        }

        // Create the Map that will be returned to the Ry script
        auto moduleMap = std::make_shared<std::unordered_map<RyValue, RyValue, RyValueHasher>>();

        // The Bridge: This lambda must NOT capture [&] to be used as a raw function pointer
        auto register_callback = [](const char* name, NativeFn fn, int arity, void* mapPtr) {
            auto* map = static_cast<std::unordered_map<RyValue, RyValue, RyValueHasher>*>(mapPtr);
            
            // Wrap the C++ function into a Ry Native Object
            auto native = std::make_shared<Frontend::RyNative>(fn, name, arity);
            
            // Insert into the Map
            (*map)[RyValue(name)] = RyValue(native);
        };

        // Load the "init_ry_module" symbol
        InitFnType init_module = (InitFnType)Backend::RyLoader::getSymbol(handle, "init_ry_module");

        if (init_module) {
            init_module(register_callback, moduleMap.get());
        } else {
            std::cerr << "Ry Symbol Error: " << Backend::RyLoader::getError() << std::endl;
        }

        // Return the Map as a RyValue
        return RyValue(moduleMap);
    }
}