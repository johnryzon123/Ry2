#ifndef RY_NATIVE_IO_HPP
#define RY_NATIVE_IO_HPP

#include <ctime>
#include <iostream>
#include <string>
#include "value.h"


namespace RyRuntime {

	// --- Helpers ---

	// Reuse your recursive printing logic, adapted for RyValue to_string
	inline void printRyValue(const RyValue &value, bool newline = false) {
		std::cout << value.to_string();
		if (newline)
			std::cout << std::endl;
	}

	// --- Native Implementations ---

	// Native 'out(...args)'
	// Takes variadic arguments and prints them with spaces in between
	inline RyValue ry_out(int argCount, RyValue *args, std::unordered_map<std::string, RyValue>& globals) {
		for (int i = 0; i < argCount; i++) {
			std::cout << args[i].to_string();
			if (i < argCount - 1)
				std::cout << " ";
		}
		std::cout << std::endl;
		return RyValue(); // returns 'null'
	}

	// Native 'input(prompt)'
	inline RyValue ry_input(int argCount, RyValue *args, std::unordered_map<std::string, RyValue>& globals) {
		if (argCount > 0) {
			std::cout << args[0].to_string();
			std::cout.flush();
		}

		std::string line;
		if (!std::getline(std::cin, line)) {
			return RyValue(); // Return null on EOF
		}

		// Logic for auto-parsing numbers/bools (from your tree-walker)
		// You can keep this or just return the raw string.
		try {
			size_t idx = 0;
			double val = std::stod(line, &idx);
			if (idx == line.size())
				return RyValue(val);
		} catch (...) {
		}

		if (line == "true")
			return RyValue(true);
		if (line == "false")
			return RyValue(false);
		if (line == "null")
			return RyValue();

		return RyValue(line);
	}
} // namespace RyRuntime

#endif
