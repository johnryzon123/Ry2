#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include "rylib-dk.h"

// The function signatures for modules must match the 'SimpleNativeFn' type expected by the VM host.
// This includes the final 'globals' map parameter, even if it's not used by the module.
// typedef RyValue (*SimpleNativeFn)(int argCount, RyValue* args, std::map<std::string, RyValue>& globals);

RyValue file_read_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue(); // Returns NIL

	std::ifstream file(args[0].asString());
	if (!file.is_open())
		return RyValue(); // Returns NIL

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return RyValue(content);
}

// Native function: Write File
RyValue file_write_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 2 || !args[0].isString() || !args[1].isString())
		return RyValue(false);

	std::ofstream file(args[0].asString());
	if (!file.is_open())
		return RyValue(false);

	file << args[1].asString();
	return RyValue(true);
}

RyValue file_cwd_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount > 0)
		return RyValue();
	try {
		std::string path = std::filesystem::current_path().string();
		return RyValue(path);
	} catch (...) {
		return RyValue();
	}
}

RyValue file_exists_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue(false);
	try {
		return RyValue(std::filesystem::exists(args[0].asString()));
	} catch (...) {
		return RyValue(false);
	}
}

RyValue file_append_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 2 || !args[0].isString() || !args[1].isString())
		return RyValue(false);

	std::ofstream file(args[0].asString(), std::ios::app);
	if (!file.is_open())
		return RyValue(false);

	file << args[1].asString();
	return RyValue(true);
}

RyValue file_size_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue(-1.0);
	try {
		return RyValue((double) std::filesystem::file_size(args[0].asString()));
	} catch (...) {
		return RyValue(-1.0);
	}
}

RyValue file_remove_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue(false);
	try {
		return RyValue(std::filesystem::remove(args[0].asString()));
	} catch (...) {
		return RyValue(false);
	}
}

RyValue file_rename_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 2 || !args[0].isString() || !args[1].isString())
		return RyValue(false);
	try {
		std::filesystem::rename(args[0].asString(), args[1].asString());
		return RyValue(true);
	} catch (...) {
		return RyValue(false);
	}
}

RyValue file_is_dir_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue(false);
	try {
		return RyValue(std::filesystem::is_directory(args[0].asString()));
	} catch (...) {
		return RyValue(false);
	}
}

RyValue file_mkdir_dk(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) {
	if (argCount < 1 || !args[0].isString())
		return RyValue(false);
	try {
		return RyValue(std::filesystem::create_directory(args[0].asString()));
	} catch (...) {
		return RyValue(false);
	}
}

extern "C" void init_ry_module(RyRegisterFn reg, void *target) {
	reg("read", file_read_dk, 1, target);
	reg("write", file_write_dk, 2, target);
	reg("cwd", file_cwd_dk, 0, target);
	reg("exists", file_exists_dk, 1, target);
	reg("append", file_append_dk, 2, target);
	reg("size", file_size_dk, 1, target);
	reg("remove", file_remove_dk, 1, target);
	reg("rename", file_rename_dk, 2, target);
	reg("is_dir", file_is_dir_dk, 1, target);
	reg("mkdir", file_mkdir_dk, 1, target);
}
