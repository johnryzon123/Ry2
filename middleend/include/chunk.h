#ifndef ry_chunk_h
#define ry_chunk_h

#include "value.h"

namespace RyRuntime {

	// The Opcodes: The instructions for Ry
	enum OpCode {
		// Register/Constant Loading. p1=dest_reg, p2=...
		OP_MOVE, // p1, p2:     R[p1] = R[p2]
		OP_LOAD_CONST, // p1, p2p3:   R[p1] = constants[p2p3]
		OP_LOAD_NULL, // p1:         R[p1] = null
		OP_LOAD_TRUE, // p1:         R[p1] = true
		OP_LOAD_FALSE, // p1:         R[p1] = false
		OP_LOAD_UPVALUE, // p1, p2:     R[p1] = upvalues[p2]
		OP_SET_UPVALUE, // p1, p2:     upvalues[p2] = R[p1]
		OP_GET_GLOBAL, // p1, p2p3:   R[p1] = globals[constants[p2p3]]
		OP_SET_GLOBAL, // p1, p2p3:   globals[constants[p2p3]] = R[p1]
		OP_DEFINE_GLOBAL, // p1, p2p3:   globals[constants[p2p3]] = R[p1]

		// Properties. p1=dest/obj, p2=name_const_idx, p3=src
		OP_GET_PROPERTY, // p1, p2, p3: R[p1] = R[p2].constants[p3]
		OP_SET_PROPERTY,
		OP_GET_SUPER,

		// Unary Ops. p1=dest, p2=src
		OP_NEGATE,
		OP_NOT, // Logical not

		// Binary Ops. p1=dest, p2=left, p3=right
		OP_ADD,
		OP_SUBTRACT,
		OP_MULTIPLY,
		OP_DIVIDE,
		OP_MODULO,
		OP_BITWISE_OR,
		OP_BITWISE_XOR,
		OP_BITWISE_AND,
		OP_LEFT_SHIFT,
		OP_RIGHT_SHIFT,
		OP_EQUAL,
		OP_GREATER,
		OP_LESS,

		// Jumps. p1=reg_cond, p2p3=offset
		OP_JUMP,
		OP_JUMP_IF_FALSE,
		OP_LOOP, // p2p3=offset

		// Functions and Calls. p1=callee_reg, p2=arg_count
		OP_CALL,
		OP_CLOSURE, // p1=dest_reg, p2p3=func_const_idx
		OP_RETURN, // p1=return_val_reg

		// Data Structures
		OP_BUILD_LIST, // p1=dest, p2=start_reg, p3=count
		OP_BUILD_MAP, // p1=dest, p2=start_reg, p3=count
		OP_BUILD_RANGE_LIST,
		OP_GET_INDEX, // p1=dest, p2=obj, p3=index
		OP_SET_INDEX, // p1=obj, p2=index, p3=value
		OP_CLOSE_UPVALUE,
		OP_ASSIGN_LOCAL,
		OP_TYPE_LOCK,
		OP_TYPE_UNLOCK,

		// Classes
		OP_CLASS,
		OP_METHOD,
		OP_INHERIT,
		OP_VERIFY_ABSTRACT,
		OP_DEFINE_PRIVATE_FIELD,

		// Misc
		OP_PANIC,
		OP_IMPORT,
		OP_FOR_EACH_NEXT,
		OP_POP, // For expression statements that leave a value in a register
		OP_ATTEMPT,
		OP_END_ATTEMPT,
		OP_LOAD_EXCEPTION
	};

	// New instruction format for register machine
	struct Instruction {
		OpCode opcode;
		uint8_t p1 = 0; // Often destination register
		uint8_t p2 = 0; // Source register or 8-bit immediate
		uint8_t p3 = 0; // Second source register

		// Helper to combine p2 and p3 into a 16-bit value
		uint16_t p2p3() const { return (uint16_t) ((p2 << 8) | p3); }
		void set_p2p3(uint16_t val) {
			p2 = (val >> 8) & 0xff;
			p3 = val & 0xff;
		}
	};

	// The sequence of bytecode
	struct Chunk {
		std::vector<Instruction> code; // The Instructions
		std::vector<RyValue> constants; // For numbers/strings

		// For error reporting
		std::vector<int> lines;
		std::vector<int> columns;

		void write(Instruction instr, int line, int column) {
			code.emplace_back(instr);
			lines.emplace_back(line);
			columns.emplace_back(column);
		}

		// Returns the index of the constant in the pool
		int addConstant(RyValue value) {
			constants.emplace_back(value);
			return constants.size() - 1;
		}
	};
} // namespace RyRuntime

#endif
