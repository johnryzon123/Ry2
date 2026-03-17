#include "value.h"

namespace RyRuntime {
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
		return (double)_;
#else
		// Linux/macOS standard clear
		auto _ = system("clear");
		return (double)_;
#endif
		return nullptr;
	};
} // namespace RyRuntime
