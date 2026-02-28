#include "vm.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <stdarg.h>
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "func.h"
#include "lexer.h"
#include "native.hpp"
#include "parser.h"
#include "tools.h"

namespace RyRuntime {
	static std::string vmSource;
	void setVMSource(const std::string &source) { vmSource = source; }
	int calculateDistance(const std::string &s1, const std::string &s2) {
		int n = s1.length();
		int m = s2.length();

		// If lengths are too different, don't even bother
		if (std::abs(n - m) > 2)
			return 99;

		// We use two vectors (rows) instead of a whole matrix
		std::vector<int> prev(m + 1);
		std::vector<int> curr(m + 1);

		for (int j = 0; j <= m; j++)
			prev[j] = j;

		for (int i = 1; i <= n; i++) {
			curr[0] = i;
			for (int j = 1; j <= m; j++) {
				int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
				curr[j] = std::min({curr[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost});
			}
			prev = curr;
		}
		return prev[m];
	}

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

		push(RyValue(std::string(buffer)));
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
#define FRAME (frames[frameCount - 1])
#define READ_BYTE() (*FRAME.ip++)
#define READ_CONSTANT() (FRAME.function->chunk.constants[READ_BYTE()])
#define READ_SHORT() (FRAME.ip += 2, (uint16_t) ((FRAME.ip[-2] << 8) | FRAME.ip[-1]))
#define RY_PANIC(format, ...)                                                                                          \
	{                                                                                                                    \
		runtimeError(format, ##__VA_ARGS__);                                                                               \
		push(RyValue(buffer));                                                                                             \
		goto trigger_panic;                                                                                                \
	}

		for (;;) {
			// Debug: Print stack height
			/* std::cout << "Stack height: " << (long) (stackTop - stack) << " Frames: " << frames << std::endl;
			std::cout << "--- STACK DEBUG (Height: " << (stackTop - stack) << ") ---" << std::endl;
			for (RyValue *slot = stack; slot < stackTop; slot++) {
				std::cout << "[" << (slot - stack) << "]" << " Value: " << slot->to_string()
									<< std::endl;
			}
			std::cout << "--------------------------" << std::endl;
			*/
			if (stackTop < stack) {
				runtimeError("Stack Underflow! Pointer: %p, Base: %p", stackTop, stack);
				goto trigger_panic;
			}
			if (stackTop >= stack + STACK_MAX) {
				runtimeError("Stack Overflow!");
				goto trigger_panic;
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

					if (a.isList()) {
						auto newList = std::make_shared<std::vector<RyValue>>(*a.asList());

						if (b.isList()) {
							auto bList = b.asList();
							newList->insert(newList->end(), bList->begin(), bList->end());
						} else {
							newList->push_back(b);
						}
						push(RyValue(newList));
					} else if (a.isNumber() && b.isNumber()) {
						push(RyValue(a.asNumber() + b.asNumber()));
					} else if (a.isString() || b.isString()) {
						push(RyValue(a.to_string() + b.to_string()));
					} else {
						runtimeError("Operands must be numbers, strings, or lists.");
						goto trigger_panic;
					}
					break;
				}
				case OP_SUBTRACT: {
					RyValue b = pop();
					RyValue a = pop();

					if (a.isNumber() && b.isNumber()) {
						push(RyValue(a.asNumber() - b.asNumber()));
					} else {
						runtimeError("Operands must be numbers");
						goto trigger_panic;
					}
					break;
				}
				case OP_MULTIPLY: {
					RyValue b = pop();
					RyValue a = pop();

					if (a.isList()) {
						auto newList = std::make_shared<std::vector<RyValue>>(*a.asList());

						if (b.isList()) {
							auto bList = b.asList();
							newList->insert(newList->end(), bList->begin(), bList->end());
						} else {
							newList->push_back(b);
						}
						push(RyValue(newList));
					} else if (a.isNumber() && b.isNumber()) {
						push(RyValue(a.asNumber() * b.asNumber()));
					} else if (a.isNumber() && b.isString()) {
						std::string result;
						result.reserve(a.asNumber() * b.to_string().length());
						for (size_t i = 0; i < a.asNumber(); ++i) {
							result += b.to_string();
						}
						push(RyValue(result));
					} else if (a.isString() && b.isNumber()) {
						std::string result;
						result.reserve(b.asNumber() * a.to_string().length());
						for (size_t i = 0; i < b.asNumber(); ++i) {
							result += a.to_string();
						}
						push(RyValue(result));
					} else {
						runtimeError("Operands must be numbers, strings, or lists.");
						goto trigger_panic;
					}
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
					// Debug: FRAME.slots[slot] = *(stackTop - 1);
					FRAME.slots[slot] = pop();
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
					RyValue nameValue = READ_CONSTANT();
					std::string name = nameValue.to_string();
					auto it = globals.find(name);

					if (it == globals.end()) {
						std::string bestMatch = "";
						int minDistance = 3;

						for (auto const &[key, val]: globals) {
							int dist = calculateDistance(name, key);
							if (dist < minDistance) {
								minDistance = dist;
								bestMatch = key;
							}
						}

						if (!bestMatch.empty()) {
							runtimeError("Undefined variable '%s'. Did you mean '%s'?", name.c_str(), bestMatch.c_str());
						} else {
							runtimeError("Undefined variable '%s'.", name.c_str());
						}

						goto trigger_panic;
					}
					push(it->second);
					break;
				}
				case OP_SET_GLOBAL: {
					RyValue nameValue = READ_CONSTANT();
					std::string name = nameValue.to_string();
					auto it = globals.find(name);

					if (it == globals.end()) {
						std::string bestMatch = "";
						int minDistance = 3;

						for (auto const &[key, val]: globals) {
							int dist = calculateDistance(name, key);
							if (dist < minDistance) {
								minDistance = dist;
								bestMatch = key;
							}
						}

						if (!bestMatch.empty()) {
							runtimeError("Cannot set undefined variable '%s'. Did you mean '%s'?", name.c_str(), bestMatch.c_str());
						} else {
							runtimeError("Undefined variable '%s'.", name.c_str());
						}

						goto trigger_panic;
					}

					// D it->second = *(stackTop - 1);
					it->second = pop();
					break;
				}
				case OP_PANIC: {
				trigger_panic:
					RyValue message = pop();
					std::string output = message.isNil() ? "Unknown Panic" : message.to_string();

					if (panicStack.empty()) {
						if (frameCount > 0) {
							auto &frame = frames[frameCount - 1];
							size_t instruction = frame.ip - frame.function->chunk.code.data() - 1;
							int line = frame.function->chunk.lines[instruction];
							int column = frame.function->chunk.columns[instruction];

							RyTools::report(line, column, "", output, vmSource);
						}

						resetStack();
						return INTERPRET_RUNTIME_ERROR;
					}

					ControlBlock block = panicStack.back();
					panicStack.pop_back();

					stackTop = stack + block.stackDepth;
					push(RyValue(output));

					FRAME.ip = FRAME.function->chunk.code.data() + block.handlerIP;
					break;
				}
				case OP_CALL: {
					uint8_t argCount = READ_BYTE();
					RyValue callee = *(stackTop - 1 - argCount);

					if (callee.isNative()) {
						try {
							auto nativeObj = callee.asNative();
							RyValue result = nativeObj->function(argCount, stackTop - argCount, globals);

							// Identify the callee's index
							int calleeIndex = 1 + argCount;

							// Check if there's a receiver (the list sitting at calleeIndex + 1)
							bool hasReceiver = (stackTop - calleeIndex - 1 >= stack) && (stackTop - calleeIndex - 1)->isList();

							stackTop -= calleeIndex; // Pop args and function

							if (hasReceiver) {
								pop(); // Pop the list receiver
							}

							push(result);
						} catch (const std::runtime_error &e) {
							runtimeError("%s", e.what());
							goto trigger_panic;
						}
					} else if (callee.isFunction()) {
						// Check arity here!
						if (argCount != callee.asFunction()->arity) {
							runtimeError("Expected %d arguments but got %d.", callee.asFunction()->arity, argCount);
							goto trigger_panic;
						}

						CallFrame *frame = &frames[frameCount++];
						frame->function = callee.asFunction();
						frame->ip = frame->function->chunk.code.data();
						frame->slots = stackTop - argCount - 1;
					} else {
						runtimeError("Can only call functions and classes.");
						goto trigger_panic;
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

					if (!indexValue.isNumber()) {
						std::cerr << "\n[ENGINE ERROR] Stack Corruption Detected!" << std::endl;
						std::cerr << "Expected Number (Double) for loop index, but found: " << indexValue.to_string() << std::endl;

						// THE STACK TRACE
						std::cerr << "--- VM STACK TRACE ---" << std::endl;
						for (RyValue *slot = stackTop - 1; slot >= stack; slot--) {
							int index = slot - stack;

							std::cerr << "[" << index << "]: " << slot->to_string() << std::endl;
						}
						std::cerr << "----------------------" << std::endl;

						// Now we can exit gracefully or throw to see the GDB trace
						exit(1);
					}

					if (collectionValue.isRange()) {
						RyRange range = collectionValue.asRange();

						// Calculate current value: start + index
						// For '1 to 10', if index is 0, value is 1.
						double current = range.start + index;

						// Check bounds
						bool isInBounds = (range.start < range.end) ? (current < range.end) : (current > range.end);

						if (isInBounds) {
							*(stackTop - 1) = RyValue((double) (index + 1));
							push(RyValue((double) current));
						} else {
							FRAME.ip += offset;
						}
					} else if (collectionValue.isList()) {
						auto list = collectionValue.asList();
						if (index < list->size()) {
							*(stackTop - 1) = RyValue((double) (index + 1));
							push((*list)[index]);
						} else {
							FRAME.ip += offset;
						}
					} else {
						runtimeError("Can only use 'each' on lists or ranges.");
						goto trigger_panic;
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
						goto trigger_panic;
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
							goto trigger_panic;
						}
						int i = (int) index.asNumber();
						if (i >= 0 && i < list->size()) {
							push((*list)[i]);
						} else {
							runtimeError("List index out of bounds.");
							goto trigger_panic;
						}
					} else if (object.isMap()) {
						auto ryMap = object.asMap();

						if (ryMap->find(index) != ryMap->end()) {
							push((*ryMap)[index]);
						} else {
							runtimeError("Key '%s' not found in map.", index.to_string().c_str());
							goto trigger_panic;
						}
					} else {
						runtimeError("Can only index lists and maps.");
						goto trigger_panic;
					}
					break;
				}
				case OP_GET_PROPERTY: {
					RyValue nameValue = READ_CONSTANT();
					std::string propertyName = nameValue.to_string();

					// 1. Peek instead of Pop! Keep the list on the stack for now.
					RyValue object = peek(0);

					// Handle properties that REPLACE the object (like .len)
					if (propertyName == "len") {
						pop(); // Now we can safely remove the list
						if (object.isList())
							push(RyValue((double) object.asList()->size()));
						else if (object.isString())
							push(RyValue((double) object.to_string().length()));
						else if (object.isMap())
							push(RyValue((double) object.asMap()->size()));
						break;
					}

					// Handle methods (the object stays on the stack as the 'receiver')
					if (propertyName == "pop") {
						// We leave the list at peek(0) and push the function on top
						auto nativeObj = std::make_shared<Frontend::RyNative>(ry_pop, 0);
						push(RyValue(nativeObj));
						break;
					}

					// If it's not a special property, check if it's a map key
					if (object.isMap()) {
						auto ryMap = object.asMap();
						auto it = ryMap->find(nameValue);
						if (it != ryMap->end()) {
							pop(); // Remove the map
							push(it->second); // Push the value found
							break;
						}
					}

					// If we found nothing, pop the object before throwing the error
					pop();
					runtimeError("Property '%s' not found on type.", propertyName.c_str());
					goto trigger_panic;
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
							goto trigger_panic;
						}
						(*list)[(int) index.asNumber()] = value;
						// D push(value);
					} else {
						runtimeError("Only lists can be indexed currently.");
						goto trigger_panic;
					}
					break;
				}

				case OP_BITWISE_AND: {
					RyValue b = pop();
					RyValue a = pop();

					// Ensure that they are numbers to avoid crashing
					if (!a.isNumber() || !b.isNumber()) {
						runtimeError("Operands must be numbers for bitwise operations.");
						goto trigger_panic;
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
						goto trigger_panic;
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
						goto trigger_panic;
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
						goto trigger_panic;
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
						goto trigger_panic;
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
				case OP_IMPORT: {
					RyValue fileNameValue = pop();
					if (!fileNameValue.isString()) {
						runtimeError("Import path must be a string.");
						goto trigger_panic;
					}
					std::string fileName = RyTools::findModulePath(fileNameValue.to_string(), false);

					// Read the file
					std::ifstream file(fileName);
					if (!file.is_open()) {
						runtimeError("Could not open script file '%s'.", fileName.c_str());
						goto trigger_panic;
					}
					std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

					// Compile the imported script
					Backend::Lexer lexer(source);
					auto tokens = lexer.scanTokens();

					// Use a temporary set for aliases if needed
					std::set<std::string> tempAliases;
					Backend::Parser parser(tokens, tempAliases, source);
					auto statements = parser.parse();

					Compiler compiler = Compiler(source);
					Chunk chunk;
					if (!compiler.compile(statements, &chunk)) {
						runtimeError("Failed to compile imported script '%s'.", fileName.c_str());
						goto trigger_panic;
					}

					// Execute the script immediately
					auto function = std::make_shared<Frontend::RyFunction>(std::move(chunk), fileName, 0);

					// We push the function and call it like a regular Ry function
					// This ensures its 'data' and 'fn' definitions hit the 'globals' map
					push(RyValue(function));
					CallFrame *frame = &frames[frameCount++];
					frame->function = function;
					frame->ip = function->chunk.code.data();
					frame->slots = stackTop - 1;

					// The VM will now continue running the code inside the imported file
					// before returning to the original script.
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
