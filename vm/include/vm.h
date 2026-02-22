/**
	Author: JOHNRYZON Z. ABEJERO
	Date: February 16, 2026
	File: vm.h
*/

#pragma once // Include guard
#include "chunk.h" // For the byte chunk
#include "map" // For map
#include "unordered_map" // For unordered map

namespace RyRuntime {
	// Used for functions
	struct CallFrame {
		std::shared_ptr<Frontend::RyFunction> function; // The function being run
		uint8_t *ip; // The IP inside THIS function
		RyValue *slots; // Where this function's stack begins
	};

	// Used for panics
	struct ControlBlock {
		int stackDepth; // Where to reset the stack
		int handlerIP; // Where the 'catch' code starts
	};

	// Possible exit states for the VM
	enum InterpretResult { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR };


	// The main virtual machine class
	class VM {
	public:
		VM(); // Constructor
		~VM() = default; // Default Constructor

		// The main entry point to run a piece of Ry code
		InterpretResult interpret(std::shared_ptr<Frontend::RyFunction> function);

		// Resolver
		void resolve(Backend::Expr *expr, int depth) { locals[expr] = depth; }

	private:
		InterpretResult run(); // Runs ry
		std::unordered_map<std::string, RyValue> globals; // Data outside classes/functions
		std::vector<ControlBlock> panicStack; // Stacks caused by a panic

		uint8_t *ip; // Points to the NEXT byte to be executed
		CallFrame frames[64]; // The "Call Stack"
		int frameCount; // Current depth

		// The bytecode it is currently running
		Chunk *chunk;

		std::map<Backend::Expr *, int> locals; // Data inside classes/functions

		// --- The Stack ---
		static const int STACK_MAX = 256; // Maximum stack
		RyValue stack[STACK_MAX]; // The stack
		RyValue *stackTop; // Points to where the next pushed value will go
		RyValue peek(int distance); // Returns the stack based on the distance

		// Stack helpers
		void resetStack(); // Reset's the stack
		void push(RyValue value); // Adds a stack
		RyValue pop(); // Removes a stack

		// Runtime helpers
		void runtimeError(const char *format, ...); // Calls report() for advance error reporting
		bool isTruthy(RyValue value);
	};
} // namespace RyRuntime
