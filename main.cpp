#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include "chunk.h"
#include "colors.h"
#include "compiler.h"
#include "func.h"
#include "lexer.h"
#include "native_sys.hpp"
#include "parser.h"
#include "tools.h"
#include "vm.h"


using namespace RyRuntime;

namespace RyRuntime {
	void setVMSource(const std::string &source);
}

void interpret(VM &vm, const std::string &source) {
	// Reset flag to stop infinite loops
	RyTools::hadError = false;

	RyRuntime::setVMSource(source);

	// Lexing
	Backend::Lexer lexer(source);
	std::vector<Backend::Token> tokens = lexer.scanTokens();

	// Check for unknown tokens from the lexer
	for (const auto &token: tokens) {
		if (token.type == Backend::TokenType::UNKNOWN) {
			RyTools::report(token.line, token.column, "", "Unexpected character '" + token.lexeme + "'", source);
		}
	}

	if (RyTools::hadError)
		return;

	// Setup Aliases & Parsing
	std::set<std::string> aliases; // Temporary set for the parser
	Backend::Parser parser(tokens, aliases, source);

	std::vector<std::shared_ptr<Backend::Stmt>> statements = parser.parse();

	if (RyTools::hadError)

		return;

	//  Compiling
	Compiler compiler = Compiler(nullptr, source);
	Chunk chunk;
	if (!compiler.compile(statements, &chunk)) {
		std::cout << "Compilation failed.\n";
		return;
	}

	auto function = std::make_shared<Frontend::RyFunction>(std::move(chunk), "<main>", 0);

	// Running
	vm.interpret(function);
	std::fflush(stdout);
	std::fflush(stderr);
	std::cout << std::flush;
	std::cerr << std::flush;
}

void runREPL(VM &vm) {
	std::string line;
	std::string buffer;
	int indentLevel = 0;
	std::cout << RyColor::BOLD << "Ry (Ry's for You) REPL - Bytecode Edition\n" << RyColor::RESET;

	while (true) {
		if (buffer.empty()) {
			std::cout << RyColor::BLUE << "ry> " << RyColor::RESET;
		} else {
			std::cout << std::string(indentLevel * 4, '.') << " ";
		}

		if (!std::getline(std::cin, line))
			break;

		if (line == "quit")
			break;
		if (line == "clear") {
			auto _ = system("clear");
			buffer.clear();
			indentLevel = 0;
			continue;
		}
		if (line == "!!") { // A manual 'abort' command
			buffer.clear();
			indentLevel = 0;
			std::cout << "Buffer cleared.\n";
			continue;
		}

		if (line.empty() && buffer.empty()) {
			continue;
		}

		int change = RyTools::countIndentation(line);
		indentLevel += change;
		buffer += line + "\n";

		if (indentLevel <= 0 && !buffer.empty() && buffer != "\n") {
			interpret(vm, buffer);
			// FORCE RESET here to ensure the prompt comes back
			buffer.clear();
			indentLevel = 0;
		} else if (indentLevel < 0) {
			// Safety check: don't allow negative indentation to break the prompt
			indentLevel = 0;
		}
	}
}

auto main(int argc, char *argv[]) -> int {
	VM vm;

	// Start at 3 to skip: [0]ry, [1]run, [2]test.ry
	for (int i = 3; i < argc; ++i) {
		RyRuntime::sys_args.emplace_back(argv[i]);
	}

	if (argc >= 2) {
		std::string command = argv[1];

		if (command == "run" && argc == 3) {
			std::ifstream inputFile(argv[2]);
			if (!inputFile.is_open()) {
				std::cerr << "Could not open file: " << argv[2] << "\n";
				return 1;
			}
			std::string src((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
			interpret(vm, src);
		} else if (command == "-v" || command == "--version") {
			std::cout << "Ry (ByteCode Edition) v2.0\n";
		} else {
			std::ifstream inputFile(argv[2]);
			if (!inputFile.is_open()) {
				std::cerr << "Type run to run a file.\n"
									<< "Type -v or --version to see the version.\n";
				return 1;
			}
			std::string src((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
			interpret(vm, src);
		}
	} else {
		runREPL(vm);
	}

	return 0;
}
