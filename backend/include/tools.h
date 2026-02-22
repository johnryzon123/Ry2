//
// Created by ryzon on 1/30/26.
//
#pragma once

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "colors.h"
#include "token.h"

namespace fs = std::filesystem;

namespace RyTools {
	// We use 'inline' so we don't get "multiple definition" errors
	// when including this file in different .cpp files.
	inline bool hadError = false;

	inline void report(int line, int col, const std::string &where, const std::string &message,
										 const std::string currentSourceCode, bool showCaret = true) {
		std::cerr << RyColor::RED << RyColor::BOLD << "Error" << RyColor::RESET << where << ": " << message << std::endl;

		// Extract the line from currentSourceCode
		if (!currentSourceCode.empty()) {
			std::stringstream ss(currentSourceCode);
			std::string lineText;
			for (int i = 0; i < line; ++i)
				std::getline(ss, lineText);

			if (showCaret) {
				// Print the caret
				std::cerr << "  " << RyColor::CYAN << line << " | " << RyColor::RESET << lineText << std::endl;
				std::cerr << RyColor::CYAN << "    | " << RyColor::RESET << std::string(col - 1, ' ') << RyColor::RED << "^~~"
									<< RyColor::RESET << std::endl;
			}
		}
		hadError = true;
	}
	inline std::string findModulePath(const std::string &name, bool isDirectory = false) {
		std::vector<std::string> searchPaths = {".", "./modules", "./modules/library"};

#ifdef _WIN32
		// Windows logic
		searchPaths.push_back("C:/ry/modules");
#else
		// Unix (Linux/Mac) logic
		searchPaths.push_back("/usr/lib/ry/");
#endif

		for (const auto &path: searchPaths) {
			fs::path fullPath = fs::path(path) / name;


			if (fs::exists(fullPath)) {
				if (isDirectory && fs::is_directory(fullPath))
					return fullPath.string();
				if (!isDirectory && fs::is_regular_file(fullPath))
					return fullPath.string();
			}
		}
		return "";
	}

	inline int countIndentation(const std::string &line) {
		int balance = 0;
		bool inString = false;

		for (int i = 0; i < line.length(); ++i) {
			char c = line[i];

			// If we hit a comment, ignore the rest of the line
			if (!inString && c == '#')
				break;

			// Handle string literals (skip brackets inside " ")
			if (c == '"' && (i == 0 || line[i - 1] != '\\')) {
				inString = !inString;
			}

			// Count brackets only if outside a string
			if (!inString) {
				if (c == '{' || c == '(' || c == '[')
					balance++;
				if (c == '}' || c == ')' || c == ']')
					balance--;
			}
		}
		return balance;
	}

	struct RyRuntimeError {
		const Backend::Token token;
		const std::string message;
		RyValue type;
		bool isPanicking;

		RyRuntimeError(Backend::Token token, std::string message, RyValue type = RyValue(), bool isPanicking=false) :
				token(std::move(token)), message(std::move(message)), type(type), isPanicking(isPanicking) {}
	};
	struct ParseError : public std::runtime_error {
		ParseError() : std::runtime_error("") {}
	};
} // namespace RyTools
