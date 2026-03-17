/**
	Author: JOHNRYZON Z. ABEJERO
	Date: February 16, 2026
	File: vm.h
*/

#pragma once // Include guard
#include <array>
#include <map>
#include <memory>
#include <vector>
#include "chunk.h" // For the byte chunk
#include "func.h"

namespace RyRuntime {
	struct RyUpValue {
		std::string typeName;
		bool typeLocked = false;
		RyValue *location; // Points to the stack slot
		RyValue closed; // Stores the value when the stack frame dies
		std::shared_ptr<RyUpValue> next; // Useful for the VM to track open upvalues
	};
	struct RyClosure {
		std::shared_ptr<Frontend::RyFunction> function;
		// The "Backpack" - pointers to the captured variables
		std::vector<std::shared_ptr<RyUpValue>> upvalues;

		RyClosure(std::shared_ptr<Frontend::RyFunction> func) : function(func) {
			// Initialize the backpack based on what the compiler told us
			upvalues.resize(func->upvalueCount, nullptr);
		}
	};
	// Used for functions
	struct CallFrame {
		std::shared_ptr<RyClosure> closure; // The function being run
		Instruction *ip; // The IP inside THIS function
		int reg_base; // Base index into the main register array
	};

	// Used for panics
	struct ControlBlock {
		int stackDepth; // Where to reset the stack
		int handlerIP; // Where the 'catch' code starts
		int frameDepth; // The frame depth when the block was created
	};

	// Possible exit states for the VM
	enum InterpretResult { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR };


	// The main virtual machine class
	class VM {
	public:
		VM(); // Constructor
		~VM() = default; // Default Constructor

		// The main entry point to run a piece of Ry code
		auto interpret(std::shared_ptr<Frontend::RyFunction> function) -> InterpretResult;

	private:
		auto run() -> InterpretResult; // Runs ry
		std::map<std::string, RyValue> globals; // Data outside classes/functions
		std::vector<ControlBlock> panicStack; // Stacks caused by a panic
		std::shared_ptr<RyUpValue> openUpvalues;
		std::map<std::string, std::shared_ptr<RyClosure>> moduleCache;
		RyValue lastException;
		uint64_t instruction_count;
		std::string last_error_message;

		CallFrame frames[64]; // The "Call Stack"
		int frameCount; // Current depth
		std::map<std::string, std::string> globalTypes; // global types

		// --- The Register File ---
		static const int REGISTER_COUNT = 256 * 64; // 256 registers per call frame
		RyValue registers[REGISTER_COUNT];
		// --- Register Type Infos ---
		std::array<std::string, 256> registerTypeNames;
		std::array<bool, 256> registerTypeLocked;


		// Runtime helpers
		void runtimeError(const char *format, ...); // Calls report() for advance error reporting
		auto isTruthy(RyValue value) -> bool;
		auto captureUpvalue(RyValue *local) -> std::shared_ptr<RyUpValue>;
		void closeUpvalues(int last_reg_base);
	};
} // namespace RyRuntime
