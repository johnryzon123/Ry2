#include "vm.h"
#include <cstdio>
#include <stdarg.h>
#include "chunk.h"
#include "common.h"
#include "func.h"
#include "native.hpp"
#include "tools.h"

namespace RyRuntime {
	static std::string vmSource;
	void setVMSource(const std::string &source) { vmSource = source; }

	void VM::push(RyValue value) {
		*stackTop = value;
		stackTop++;
	}

	RyValue VM::pop() {
		stackTop--;
		return *stackTop;
	}

	VM::VM() {
		resetStack();
		registerNatives(globals);
	}

	void VM::resetStack() {
		stackTop = stack;
		frameCount = 0;
	}

	// Helper for runtime errors to show line numbers
	void VM::runtimeError(const char *format, ...) {
		char buffer[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);

		// Sanity check: Ensure we actually have a frame to report from
		if (frameCount > 0) {
			auto &frame = frames[frameCount - 1];

			// Safety check: Ensure IP is within the code bounds
			size_t instruction = frame.ip - frame.function->chunk.code.data() - 1;

			if (instruction < frame.function->chunk.lines.size()) {
				int line = frame.function->chunk.lines[instruction];
				RyTools::report(line, 1, "", buffer, vmSource);
			} else {
				std::cerr << RyColor::RED << "Runtime Error: " << RyColor::RESET << buffer << "\n";
			}
		} else {
			std::cerr << RyColor::RED << "Runtime Error: " << RyColor::RESET << buffer << "\n";
		}

		resetStack();
	}

	InterpretResult VM::interpret(std::shared_ptr<Frontend::RyFunction> function) {
		resetStack(); // Always start fresh for a new REPL line
		push(RyValue(function));

		CallFrame *frame = &frames[frameCount++];
		frame->function = function;
		frame->ip = function->chunk.code.data();
		frame->slots = stack;

		return run();
	}
	bool VM::isTruthy(RyValue value) {
		if (value.isNumber()) {
			return value.asNumber() != 0;
		} else if (value.isBool()) {
			return value.asBool();
		} else if (value.isNil()) {
			return false;
		} else {
			return true;
		}
	}
	RyValue VM::peek(int distance) {
		// stackTop points to the NEXT empty slot,
		// so -1 is the current top, -2 is one below, etc.
		return stackTop[-1 - distance];
	}

	InterpretResult VM::run() {
// Everything points to the current frame
#define FRAME (frames[frameCount - 1])
#define READ_BYTE() (*FRAME.ip++)
#define READ_CONSTANT() (FRAME.function->chunk.constants[READ_BYTE()])
#define READ_SHORT() (FRAME.ip += 2, (uint16_t) ((FRAME.ip[-2] << 8) | FRAME.ip[-1]))

		for (;;) {

			if (stackTop < stack) {
				runtimeError("Stack Underflow! Pointer: %p, Base: %p", stackTop, stack);
				return INTERPRET_RUNTIME_ERROR;
			}
			if (stackTop >= stack + STACK_MAX) {
				runtimeError("Stack Overflow!");
				return INTERPRET_RUNTIME_ERROR;
			}

			uint8_t instruction;
			switch (instruction = READ_BYTE()) {
				case OP_POP: {
					pop();
					break;
				}
				case OP_NULL: {
					push(RyValue());
					break;
				}
				case OP_TRUE: {
					push(RyValue(true));
					break;
				}
				case OP_FALSE: {
					push(RyValue(false));
					break;
				}

				case OP_CONSTANT: {
					push(READ_CONSTANT());
					break;
				}
				case OP_ADD: {
					RyValue b = pop();
					RyValue a = pop();
					push(a + b);
					break;
				}
				case OP_SUBTRACT: {
					RyValue b = pop();
					RyValue a = pop();
					push(a - b);
					break;
				}
				case OP_MULTIPLY: {
					RyValue b = pop();
					RyValue a = pop();
					push(a * b);
					break;
				}
				case OP_DIVIDE: {
					RyValue b = pop();
					RyValue a = pop();

					if (b.asNumber() == 0) {
						// Option A: Trigger a Ry Panic (Catchable by 'attempt')
						push(RyValue("Division by zero")); // Push error message
						// Redirect to the OP_PANIC logic
						goto trigger_panic;
					}

					push(a / b);
					break;
				}
				case OP_NEGATE: {
					push(-pop());
					break;
				}
				case OP_NOT: {
					push(!pop());
					break;
				}
				case OP_EQUAL: {
					RyValue b = pop();
					RyValue a = pop();
					push(a == b);
					break;
				}
				case OP_GREATER: {
					RyValue b = pop();
					RyValue a = pop();
					push(a > b);
					break;
				}
				case OP_LESS: {
					RyValue b = pop();
					RyValue a = pop();
					push(a < b);
					break;
				}
				case OP_MODULO: {
					RyValue b = pop();
					RyValue a = pop();
					push(a % b);
					break;
				}
				case OP_GET_LOCAL: {
					uint8_t slot = READ_BYTE();
					push(FRAME.slots[slot]);
					break;
				}
				case OP_SET_LOCAL: {
					uint8_t slot = READ_BYTE();
					FRAME.slots[slot] = *(stackTop - 1);
					break;
				}
				case OP_JUMP: {
					uint16_t offset = READ_SHORT();
					FRAME.ip += offset;
					break;
				}
				case OP_JUMP_IF_FALSE: {
					uint16_t offset = READ_SHORT();
					if (!isTruthy(peek(0))) {
						FRAME.ip += offset;
					}
					break;
				}
				case OP_LOOP: {
					uint16_t offset = READ_SHORT();
					FRAME.ip -= offset;
					break;
				}
				case OP_DEFINE_GLOBAL: {
					RyValue name = READ_CONSTANT();
					globals[name.to_string()] = pop();
					break;
				}
				case OP_GET_GLOBAL: {
					RyValue name = READ_CONSTANT();
					auto it = globals.find(name.to_string());
					if (it == globals.end()) {
						runtimeError("Undefined variable '%s'.", name.to_string().c_str());
						return INTERPRET_RUNTIME_ERROR;
					}
					push(it->second);
					break;
				}
				case OP_SET_GLOBAL: {
					RyValue name = READ_CONSTANT();
					auto it = globals.find(name.to_string());
					if (it == globals.end()) {
						runtimeError("Undefined variable '%s'.", name.to_string().c_str());
						return INTERPRET_RUNTIME_ERROR;
					}
					it->second = *(stackTop - 1);
					break;
				}
				case OP_PANIC: {
				trigger_panic:
					RyValue message = pop();

					if (panicStack.empty()) {
						std::string output = message.isNil() ? "" : message.to_string();
						std::cerr << output << "\n";
						resetStack();
						return INTERPRET_RUNTIME_ERROR;
					}

					// Found an 'attempt' block! (The 'catch' equivalent)
					ControlBlock block = panicStack.back();
					panicStack.pop_back();

					stackTop = stack + block.stackDepth;
					push(message); // Pass the 'exception' object to the handler

					FRAME.ip = FRAME.function->chunk.code.data() + block.handlerIP;
					break;
				}
				case OP_CALL: {
					uint8_t argCount = READ_BYTE();
					RyValue callee = *(stackTop - 1 - argCount);

					if (callee.isNative()) {
						// Get the shared pointer to the RyNative object
						auto nativeObj = callee.asNative();

						// Call the 'function' member stored inside that object
						RyValue result = nativeObj->function(argCount, stackTop - argCount, globals);

						// Clean up: remove args and the function object itself
						stackTop -= argCount + 1;
						push(result);
					} else if (callee.isFunction()) {
						// Check arity here!
						if (argCount != callee.asFunction()->arity) {
							runtimeError("Expected %d arguments but got %d.", callee.asFunction()->arity, argCount);
							return INTERPRET_RUNTIME_ERROR;
						}

						CallFrame *frame = &frames[frameCount++];
						frame->function = callee.asFunction();
						frame->ip = frame->function->chunk.code.data();
						frame->slots = stackTop - argCount - 1;
					} else {
						runtimeError("Can only call functions and classes.");
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				case OP_RETURN: {
					RyValue result = pop();

					// Save the starting point of the frame it's are about to leave
					RyValue *currentFrameSlots = FRAME.slots;

					frameCount--;

					if (frameCount == 0) {
						if (stackTop > stack) {
							pop();
						}
						return INTERPRET_OK;
					}

					// Reset stackTop to where the CALLEE started (popping args + callee)
					stackTop = currentFrameSlots;
					push(result);
					break;
				}
				case OP_FOR_EACH_NEXT: {
					uint16_t offset = READ_SHORT();
					RyValue indexValue = peek(0);
					RyValue collectionValue = peek(1);

					int index = (int) indexValue.asNumber();


					if (collectionValue.isRange()) {
						RyRange range = collectionValue.asRange();

						// Calculate current value: start + index
						// For '1 to 10', if index is 0, value is 1.
						double current = range.start + index;

						// Check bounds
						bool isInBounds = (range.start <= range.end) ? (current <= range.end) : (current >= range.end);

						if (isInBounds) {
							push(RyValue(current));
							*(stackTop - 2) = RyValue((double) (index + 1));
						} else {
							FRAME.ip += offset;
						}
					} else if (collectionValue.isList()) {
						auto list = collectionValue.asList();
						if (index < list->size()) {
							push((*list)[index]);
							*(stackTop - 2) = RyValue((double) (index + 1));
						} else {
							FRAME.ip += offset;
						}
					} else {
						runtimeError("Can only use 'each' on lists or ranges.");
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				case OP_BUILD_RANGE_LIST: {
					double end = pop().asNumber();
					double start = pop().asNumber();
					push(RyValue(RyRange{start, end}));
					break;
				}

				case OP_BUILD_LIST: {
					uint8_t count = READ_BYTE();
					auto listVec = std::make_shared<std::vector<RyValue>>();

					// Elements are on stack in order, but we pop them in reverse
					// A simple way is to pre-size and fill from the end
					listVec->resize(count);
					for (int i = count - 1; i >= 0; i--) {
						(*listVec)[i] = pop();
					}

					push(RyValue(listVec));
					break;
				}
				case OP_ATTEMPT: {
					uint16_t jumpOffset = READ_SHORT();
					ControlBlock block;
					block.stackDepth = (int) (stackTop - stack);

					block.handlerIP = (int) ((FRAME.ip + jumpOffset) - FRAME.function->chunk.code.data());

					panicStack.push_back(block);
					break;
				}
				case OP_END_ATTEMPT: {
					if (!panicStack.empty()) {
						panicStack.pop_back();
					} else {
						runtimeError("Cannot end attempt if panicStack is empty.");
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				case OP_GET_INDEX: {
					RyValue index = pop();
					RyValue object = pop();

					if (object.isList()) {
						auto list = object.asList();
						// Ensure the index is a number
						if (!index.isNumber()) {
							runtimeError("List index must be a number.");
							return INTERPRET_RUNTIME_ERROR;
						}
						int i = (int) index.asNumber();
						if (i >= 0 && i < list->size()) {
							push((*list)[i]);
						} else {
							runtimeError("List index out of bounds.");
							return INTERPRET_RUNTIME_ERROR;
						}
					} else if (object.isMap()) {
						auto ryMap = object.asMap();

						if (ryMap->find(index) != ryMap->end()) {
							push((*ryMap)[index]);
						} else {
							runtimeError("Key '%s' not found in map.", index.to_string().c_str());
							return INTERPRET_RUNTIME_ERROR;
						}
					} else {
						runtimeError("Can only index lists and maps.");
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				case OP_GET_PROPERTY: {
					RyValue name = READ_CONSTANT();
					RyValue object = pop();

					if (object.isMap()) {
						auto ryMap = object.asMap();
						auto it = ryMap->find(name);

						if (it != ryMap->end()) {
							push(it->second);
						} else {
							runtimeError("Property '%s' not found.", name.to_string().c_str());
							return INTERPRET_RUNTIME_ERROR;
						}
					} else {
						runtimeError("Only maps and classes have properties.");
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}
				case OP_SET_INDEX: {
					RyValue value = pop();
					RyValue index = pop();
					RyValue object = pop();

					if (object.isList()) {
						auto list = object.asList();
						// Ensure the index is a number
						if (!index.isNumber()) {
							runtimeError("List index must be a number.");
							return INTERPRET_RUNTIME_ERROR;
						}
						(*list)[(int) index.asNumber()] = value;
						push(value);
					} else {
						runtimeError("Only lists can be indexed currently.");
						return INTERPRET_RUNTIME_ERROR;
					}
					break;
				}

				case OP_BITWISE_AND: {
					RyValue b = pop();
					RyValue a = pop();

					// Ensure that they are numbers to avoid crashing
					if (!a.isNumber() || !b.isNumber()) {
						runtimeError("Operands must be numbers for bitwise operations.");
						return INTERPRET_RUNTIME_ERROR;
					}

					// Cast to integers for the C++ bitwise & operator
					long result = (long) a.asNumber() & (long) b.asNumber();
					push(RyValue((double) result));
					break;
				}
				case OP_BITWISE_OR: {
					RyValue b = pop();
					RyValue a = pop();

					// Ensure that they are numbers to avoid crashing
					if (!a.isNumber() || !b.isNumber()) {
						runtimeError("Operands must be numbers for bitwise operations.");
						return INTERPRET_RUNTIME_ERROR;
					}

					// Cast to integers for the C++ bitwise | operator
					long result = (long) a.asNumber() | (long) b.asNumber();
					push(RyValue((double) result));
					break;
				}
				case OP_BITWISE_XOR: {
					RyValue b = pop();
					RyValue a = pop();

					// Ensure that they are numbers to avoid crashing
					if (!a.isNumber() || !b.isNumber()) {
						runtimeError("Operands must be numbers for bitwise operations.");
						return INTERPRET_RUNTIME_ERROR;
					}

					// Cast to integers for the C++ bitwise ^ operator
					long result = (long) a.asNumber() ^ (long) b.asNumber();
					push(RyValue((double) result));
					break;
				}
				case OP_LEFT_SHIFT: {
					RyValue b = pop();
					RyValue a = pop();

					// Ensure that they are numbers to avoid crashing
					if (!a.isNumber() || !b.isNumber()) {
						runtimeError("Operands must be numbers for bitwise operations.");
						return INTERPRET_RUNTIME_ERROR;
					}

					// Cast to integers for the C++ bitwise << operator
					long result = (long) a.asNumber() << (long) b.asNumber();
					push(RyValue((double) result));
					break;
				}
				case OP_RIGHT_SHIFT: {
					RyValue b = pop();
					RyValue a = pop();

					// Ensure that they are numbers to avoid crashing
					if (!a.isNumber() || !b.isNumber()) {
						runtimeError("Operands must be numbers for bitwise operations.");
						return INTERPRET_RUNTIME_ERROR;
					}

					// Cast to integers for the C++ bitwise >> operator
					long result = (long) a.asNumber() >> (long) b.asNumber();
					push(RyValue((double) result));
					break;
				}
				case OP_COPY: {
					push(peek(0));
					break;
				}
				case OP_BUILD_MAP: {
					uint8_t count = READ_BYTE();
					auto mapPtr = std::make_shared<std::unordered_map<RyValue, RyValue, RyValueHasher>>();

					for (int i = 0; i < count; i++) {
						RyValue value = pop();
						RyValue key = pop();
						(*mapPtr)[key] = value;
					}

					push(RyValue(mapPtr));
					break;
				}
				default:
					return INTERPRET_COMPILE_ERROR;
			}
		}

#undef FRAME
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
	}

} // namespace RyRuntime
