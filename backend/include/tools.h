//
// Created by ryzon on 1/30/26.
//
#pragma once

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "colors.h"
#include "token.h"
#include "value.h"

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
			bool found = false;
			for (int i = 0; i < line; ++i) {
				if (!std::getline(ss, lineText)) {
					found = false;
					break;
				}
				found = true;
			}

			if (showCaret && found) {
				int padding = std::to_string(line).length();
				int maxWidth = 60;

				std::string displayLine = lineText;
				int displayCol = col;

				if (lineText.length() > maxWidth) {
					int start = std::max(0, col - (maxWidth / 2));
					displayLine = "..." + lineText.substr(start, maxWidth) + "...";
					displayCol = (col - start) + 3;
				}

				std::cerr << RyColor::CYAN << " " << line << " | " << RyColor::RESET << displayLine << std::endl;
				std::cerr << RyColor::CYAN << std::string(padding + 1, ' ') << " | " << RyColor::RESET
									<< std::string(displayCol - 1, ' ') << RyColor::RED << "^~~" << RyColor::RESET << std::endl;
			}
		}

		hadError = true;
	}
	inline auto findModulePath(const std::string &name, bool isDirectory = false) -> std::string {
		std::vector<std::string> searchPaths = {"./", "./modules", "./modules/library"};

#ifdef _WIN32
		// Windows logic
		searchPaths.emplace_back("C:/ry/modules");
#else
		// Unix (Linux/Mac) logic
		searchPaths.emplace_back("/usr/lib/ry/");
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

	inline auto countIndentation(const std::string &line) -> int {
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

		RyRuntimeError(Backend::Token token, std::string message, RyValue type = RyValue(), bool isPanicking = false) :
				token(std::move(token)), message(std::move(message)), type(std::move(type)), isPanicking(isPanicking) {}
	};
	struct ParseError : public std::runtime_error {
		ParseError() : std::runtime_error("") {}
	};
} // namespace RyTools
