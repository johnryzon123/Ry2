#include <iostream>
#include "colors.h"
#include "value.h"

namespace RyRuntime {
	inline RyValue ry_exit(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
		int exitCode = args->asNumber();
		std::cout << RyColor::BOLD << RyColor::YELLOW << "[Ry] Exited Successfully with exit code: " << exitCode
							<< RyColor::RESET << std::endl;
		exit(0);
	}


	// Native 'clock()' - Useful for benchmarking Ry
	inline RyValue ry_clock(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
		return RyValue((double) clock() / CLOCKS_PER_SEC);
	}

	// Native 'clear()' - Useful for clearing output
	inline RyValue ry_clear(int argCount, RyValue *args, std::unordered_map<std::string, RyValue> &globals) {
#ifdef _WIN32
		// Windows specific clear
		(void) system("cls");
#else
		// Linux/macOS standard clear
		(void) system("clear");
#endif
		return nullptr;
	};
} // namespace RyRuntime
