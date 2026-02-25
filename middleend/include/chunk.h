#ifndef ry_chunk_h
#define ry_chunk_h

#include "value.h"

namespace RyRuntime {

	// The Opcodes: The instructions for Ry
	enum OpCode {
		// Literals
		OP_CONSTANT,
		OP_NULL, // null
		OP_TRUE, // true
		OP_FALSE, // false
		OP_POP,

		// Variables & Scopes
		OP_DEFINE_GLOBAL,
		OP_GET_GLOBAL,
		OP_SET_GLOBAL,
		OP_GET_LOCAL,
		OP_SET_LOCAL,
		OP_GET_PROPERTY,
		OP_SET_PROPERTY,

		// Math
		OP_ADD, // +
		OP_SUBTRACT, // -
		OP_MULTIPLY, // *
		OP_DIVIDE, // /
		OP_MODULO, // %
		OP_NEGATE, // -
		OP_GROUPING, // {
		OP_CLOSE_GROUPING, // }
		OP_BUILD_RANGE_LIST, // 0 to 10
		OP_BUILD_LIST, // [0,0,0,0]
		OP_GET_INDEX, // data i = [0,0,0]
		OP_SET_INDEX, // i[0] = 100
		OP_BITWISE_OR, // |
		OP_BITWISE_XOR, // ^
		OP_BITWISE_AND, // &
		OP_LEFT_SHIFT, // <<
		OP_RIGHT_SHIFT, // >>
		OP_COPY,
		OP_BUILD_MAP,



		// Comparison
		OP_EQUAL, // ==
		OP_GREATER, // >
		OP_LESS, // <
		OP_NOT, // not

		// Control Flow
		OP_JUMP, // if/else
		OP_JUMP_IF_FALSE,
		OP_LOOP, // while/for/until
		OP_FOR_EACH_NEXT,

		// Ry Specifics
		OP_CALL, // test()
		OP_CLASS, // class
		OP_INHERIT, // childof
		OP_PANIC, // panic
		OP_RETURN, // return
		OP_FUNCTION, // func test() {}
		OP_ATTEMPT, // attempt {} fail err {}
		OP_END_ATTEMPT,
		OP_IMPORT
	};

	// The sequence of bytecode
	struct Chunk {
		std::vector<uint8_t> code; // The Instructions
		std::vector<RyValue> constants; // For numbers/strings

		// For error reporting
		std::vector<int> lines;
		std::vector<int> columns; 

		void write(uint8_t byte, int line, int column) {
			code.push_back(byte);
			lines.push_back(line);
			columns.push_back(column);
		}

		// Returns the index of the constant in the pool
		int addConstant(RyValue value) {
			constants.push_back(value);
			return constants.size() - 1;
		}
	};
} // namespace RyRuntime

#endif
