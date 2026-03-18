#pragma once
#include <string>
#include <vector>
#include "value.h"

namespace RyRuntime {
	inline std::vector<std::string> sys_args;
	inline auto ry_exit(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) -> RyValue {
		int exitCode = args->asNumber();
		exit(0);
	}


	// Native 'clock()' - Useful for benchmarking Ry
	inline auto ry_clock(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) -> RyValue {
		return {(double) clock() / CLOCKS_PER_SEC};
	}

	// Native 'clear()' - Useful for clearing output
	inline auto ry_clear(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) -> RyValue {
#ifdef _WIN32
		// Windows specific clear
		auto _ system("cls");
		return (double) _;
#else
		// Linux/macOS standard clear
		auto _ = system("clear");
		return (double) _;
#endif
		return nullptr;
	};

	inline auto ry_args(int argCount, RyValue *args, std::map<std::string, RyValue> &globals) -> RyValue {
		auto list = std::make_shared<std::vector<RyValue>>();
		for (const auto &arg: sys_args) {
			list->push_back(RyValue(arg));
		}
		return RyValue(list);
	}
} // namespace RyRuntime
