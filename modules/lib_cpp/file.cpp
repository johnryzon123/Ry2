#include <fstream>
#include <string>
#include <unordered_map>
#include "value.h"

typedef RyValue (*RawNativeFn)(int, RyValue*, std::unordered_map<std::string, RyValue>&);
typedef void (*RegisterFn)(const char*, RawNativeFn, int, void*);

// Native function: Read File
RyValue file_read_raw(int argCount, RyValue* args, std::unordered_map<std::string, RyValue>& globals) {
    if (argCount < 1 || !args[0].isString()) return RyValue();

    std::ifstream file(args[0].to_string());
    if (!file.is_open()) return RyValue();

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return RyValue(content);
}

// Native function: Write File
RyValue file_write_raw(int argCount, RyValue* args, std::unordered_map<std::string, RyValue>& globals) {
    if (argCount < 2 || !args[0].isString() || !args[1].isString()) return RyValue(false);

    std::ofstream file(args[0].to_string());
    if (!file.is_open()) return RyValue(false);

    file << args[1].to_string();
    return RyValue(true);
}

// The Entry Point
extern "C" void init_ry_module(RegisterFn register_fn, void *target) {
    // Register "read" and "write" functions
    register_fn("read", file_read_raw, 1, target);
    register_fn("write", file_write_raw, 2, target);
}