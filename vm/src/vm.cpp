#include "vm.h"
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <set>
#include "chunk.h"
#include "class.h"
#include "common.h"
#include "compiler.h"
#include "func.h"
#include "lexer.h"
#include "native.hpp"
#include "parser.h"
#include "tools.h"

namespace RyRuntime {
	static std::string vmSource;
	static std::string lastReplBlockSource;
	const uint64_t MAX_INSTRUCTIONS = 10000000000ULL; // 10 Billion (infinite loop detection)
	void setVMSource(const std::string &source) {
		// Heuristic to cache source code for function/class definitions in the REPL.
		// This helps provide correct error reporting for functions defined over multiple REPL inputs.
		if (source.find("func ") != std::string::npos || source.find("class ") != std::string::npos ||
				source.find('{') != std::string::npos) {
			lastReplBlockSource = source;
		}
		vmSource = source;
	}
	auto calculateDistance(const std::string &s1, const std::string &s2) -> int {
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

	auto VM::captureUpvalue(RyValue *local) -> std::shared_ptr<RyUpValue> {
		std::shared_ptr<RyUpValue> prevUpvalue = nullptr;
		std::shared_ptr<RyUpValue> upvalue = openUpvalues;

		while (upvalue != nullptr && upvalue->location > local) {
			prevUpvalue = upvalue;
			upvalue = upvalue->next;
		}

		if (upvalue != nullptr && upvalue->location == local) {
			return upvalue;
		}

		auto createdUpvalue = std::make_shared<RyUpValue>();
		createdUpvalue->location = local;
		createdUpvalue->next = upvalue;

		int regIndex = local - registers;
		createdUpvalue->typeLocked = registerTypeLocked[regIndex];
		if (createdUpvalue->typeLocked) {
			createdUpvalue->typeName = registerTypeNames[regIndex];
		}

		if (prevUpvalue == nullptr) {
			openUpvalues = createdUpvalue;
		} else {
			prevUpvalue->next = createdUpvalue;
		}

		return createdUpvalue;
	}

	VM::VM() {
		frameCount = 0;
		openUpvalues = nullptr;
		registerTypeLocked.fill(false);
		registerNatives(globals);
		instruction_count = 0;
	}


	// Helper for runtime errors to show line numbers
	void VM::runtimeError(const char *format, ...) {
		std::array<char, 1024> buffer;
		va_list args;
		va_start(args, format);
		vsnprintf(buffer.data(), buffer.size(), format, args);
		va_end(args);
		last_error_message = buffer.data();

		// The error message is now placed in a known register (e.g., R0 of the top frame)
		// For simplicity, we'll just handle it directly in the panic logic.
		// This function now just prepares the message.
		// A global error string could also be used.
	}

	auto VM::interpret(std::shared_ptr<Frontend::RyFunction> function) -> InterpretResult {
		// Reset the VM state for a fresh interpretation
		// Clear registers to prevent state corruption between REPL inputs
		for (int i = 0; i < 256; i++) {
			registers[i] = RyValue();
		}

		closeUpvalues(0);
		frameCount = 0;
		panicStack.clear();
		registerTypeLocked.fill(false);
		registerTypeNames.fill("");
		lastException = RyValue();

		std::shared_ptr<RyClosure> closure = std::make_shared<RyClosure>(function);
		registers[0] = RyValue(closure);

		CallFrame *frame = &frames[frameCount++];
		frame->closure = closure;
		frame->ip = function->chunk.code.data();
		frame->reg_base = 0;

		instruction_count = 0;

		return run();
	}
	auto VM::isTruthy(RyValue value) -> bool {
		if (value.isNil())
			return false;
		if (value.isNumber())
			return value.asNumber() != 0;
		if (value.isBool())
			return value.asBool();
		return true;
	}

	void VM::closeUpvalues(int last_reg_base) {
		while (openUpvalues != nullptr && (openUpvalues->location - registers) >= last_reg_base) {
			std::shared_ptr<RyUpValue> upvalue = openUpvalues;
			upvalue->closed = *upvalue->location;
			upvalue->location = &upvalue->closed;
			openUpvalues = upvalue->next;
		}
	}

	InterpretResult VM::run() {
// Direct threading setup
#if defined(__GNUC__) || defined(__clang__)
#define DIRECT_THREADING 1
#else
#define DIRECT_THREADING 0
#endif

#define FRAME (frames[frameCount - 1])
#define CURRENT_CHUNK (FRAME.closure->function->chunk)
#define CONSTANTS (CURRENT_CHUNK.constants)
#define REG(i) (registers[FRAME.reg_base + (i)])

#if DIRECT_THREADING
		static const std::array<void *, 55> dispatch_table = {&&op_move,
																													&&op_load_const,
																													&&op_load_null,
																													&&op_load_true,
																													&&op_load_false,
																													&&op_load_upvalue,
																													&&op_set_upvalue,
																													&&op_get_global,
																													&&op_set_global,
																													&&op_define_global,
																													&&op_get_property,
																													&&op_set_property,
																													&&op_get_super,
																													&&op_negate,
																													&&op_not, // Logical not
																													&&op_add,
																													&&op_subtract,
																													&&op_multiply,
																													&&op_divide,
																													&&op_modulo,
																													&&op_bitwise_or,
																													&&op_bitwise_xor,
																													&&op_bitwise_and,
																													&&op_left_shift,
																													&&op_right_shift,
																													&&op_equal,
																													&&op_greater,
																													&&op_less,
																													&&op_jump,
																													&&op_jump_if_false,
																													&&op_loop,
																													&&op_call,
																													&&op_closure,
																													&&op_return,
																													&&op_build_list,
																													&&op_build_map,
																													&&op_build_range_list,
																													&&op_get_index,
																													&&op_set_index,
																													&&op_close_upvalue,
																													&&op_assign_local,
																													&&op_type_lock,
																													&&op_type_unlock,
																													&&op_class,
																													&&op_method,
																													&&op_inherit,
																													&&op_verify_abstract,
																													&&op_define_private_field,
																													&&op_panic,
																													&&op_import,
																													&&op_for_each_next,
																													&&op_pop,
																													&&op_attempt,
																													&&op_end_attempt,
																													&&op_load_exception};

#define START_OPCODE(name) op_##name
#define READ_AND_DISPATCH()                                                                                            \
	if (++instruction_count > MAX_INSTRUCTIONS) {                                                                        \
		runtimeError("Potential infinite loop detected (exceeded 10 billion instructions).");                              \
		goto trigger_panic;                                                                                                \
	}                                                                                                                    \
	instruction = *FRAME.ip++;                                                                                           \
	goto *dispatch_table[instruction.opcode]
#else
#define START_OPCODE(name) case OP_##name
#define READ_AND_DISPATCH()                                                                                            \
	if (++instruction_count > MAX_INSTRUCTIONS) {                                                                        \
		runtimeError("Potential infinite loop detected (exceeded 10 nillion instructions).");                              \
		goto trigger_panic;                                                                                                \
	}                                                                                                                    \
	instruction = *FRAME.ip++;                                                                                           \
	goto switch_start
#endif

		Instruction instruction;
		READ_AND_DISPATCH();

#if !DIRECT_THREADING
		for (;;) {
		switch_start:
			switch (instruction.opcode) {
#endif
				START_OPCODE(move) : {
					REG(instruction.p1) = REG(instruction.p2);
					READ_AND_DISPATCH();
				}
				START_OPCODE(load_const) : {
					REG(instruction.p1) = CONSTANTS[instruction.p2p3()];
					READ_AND_DISPATCH();
				}
				START_OPCODE(load_null) : {
					REG(instruction.p1) = RyValue();
					READ_AND_DISPATCH();
				}
				START_OPCODE(load_true) : {
					REG(instruction.p1) = RyValue(true);
					READ_AND_DISPATCH();
				}
				START_OPCODE(load_false) : {
					REG(instruction.p1) = RyValue(false);
					READ_AND_DISPATCH();
				}
				START_OPCODE(negate) : {
					REG(instruction.p1) = -REG(instruction.p2);
					READ_AND_DISPATCH();
				}
				START_OPCODE(not ) : {
					REG(instruction.p1) = !REG(instruction.p2);
					READ_AND_DISPATCH();
				}
				START_OPCODE(load_upvalue) : {
					REG(instruction.p1) = *FRAME.closure->upvalues[instruction.p2]->location;
					READ_AND_DISPATCH();
				}
				START_OPCODE(set_upvalue) : {
					{
						auto upvalue = FRAME.closure->upvalues[instruction.p2];
						RyValue &newVal = REG(instruction.p1);

						if (upvalue->typeLocked) {
							std::string &lockedType = upvalue->typeName;
							std::string newType = newVal.typeName();
							if (lockedType != newType && !newVal.isNil()) {
								runtimeError("TypeError: This captured variable has a locked type of '%s' and cannot be reassigned to "
														 "a value of type '%s'.",
														 lockedType.c_str(), newType.c_str());
								goto trigger_panic;
							}
						} else if (!newVal.isNil()) {
							// First assignment, so lock the type.
							upvalue->typeName = newVal.typeName();
							upvalue->typeLocked = true;
						}
						*upvalue->location = newVal;
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(closure) : {
					{
						auto function = CONSTANTS[instruction.p2p3()].asFunction();
						auto closure = std::make_shared<RyClosure>(function);
						REG(instruction.p1) = RyValue(closure);
						for (int i = 0; i < function->upvalueCount; ++i) {
							Instruction next = *FRAME.ip++;
							bool isLocal = next.p1;
							uint8_t index = next.p2;
							closure->upvalues[i] = isLocal ? captureUpvalue(&REG(index)) : FRAME.closure->upvalues[index];
						}
					}
					READ_AND_DISPATCH();
				}

				START_OPCODE(add) : {
					{
						RyValue &a = REG(instruction.p2);
						RyValue &b = REG(instruction.p3);

						if (a.isList()) {
							if (b.isList()) {
								auto newList = std::make_shared<std::vector<RyValue>>(*a.asList());
								auto bList = b.asList();
								newList->insert(newList->end(), bList->begin(), bList->end());
								REG(instruction.p1) = RyValue(newList);
							} else {
								runtimeError("TypeError: Can only concatenate list with list, not with '%s'.", b.typeName().c_str());
								goto trigger_panic;
							}
						} else if (a.isString() || b.isString()) {
							REG(instruction.p1) = RyValue(a.to_string() + b.to_string());
						} else if (a.isNumber() && b.isNumber()) {
							REG(instruction.p1) = RyValue(a.asNumber() + b.asNumber());
						} else if (a.isChar() && b.isChar()) {
							REG(instruction.p1) = RyValue(static_cast<double>(a.asChar() + b.asChar()));
						} else if (a.isChar() && b.isNumber()) {
							REG(instruction.p1) = RyValue(static_cast<char>(a.asChar() + b.asNumber()));
						} else if (a.isNumber() && b.isChar()) {
							REG(instruction.p1) = RyValue(static_cast<char>(a.asNumber() + b.asChar()));
						} else {
							runtimeError("TypeError: Unsupported operand types for +: '%s' and '%s'.", a.typeName().c_str(),
													 b.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(subtract) : {
					{
						RyValue &a = REG(instruction.p2);
						RyValue &b = REG(instruction.p3);
						if (a.isNumber() && b.isNumber()) {
							REG(instruction.p1) = RyValue(a.asNumber() - b.asNumber());
						} else if (a.isChar() && b.isChar()) {
							REG(instruction.p1) = RyValue(static_cast<double>(a.asChar() - b.asChar()));
						} else if (a.isChar() && b.isNumber()) {
							REG(instruction.p1) = RyValue(static_cast<char>(a.asChar() - b.asNumber()));
						} else {
							runtimeError("TypeError: Unsupported operand types for -: '%s' and '%s'.", a.typeName().c_str(),
													 b.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(multiply) : {
					REG(instruction.p1) = REG(instruction.p2) * REG(instruction.p3);
					READ_AND_DISPATCH();
				}
				START_OPCODE(divide) : {
					{
						RyValue &b = REG(instruction.p3);
						if (b.asNumber() == 0) {
							runtimeError("ZeroDivisionError: Division by zero.");
							goto trigger_panic;
						}
						REG(instruction.p1) = REG(instruction.p2) / REG(instruction.p3);
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(equal) : {
					{
						RyValue &a = REG(instruction.p2);
						RyValue &b = REG(instruction.p3);
						if (a.isChar() && b.isChar()) {
							REG(instruction.p1) = RyValue(a.asChar() == b.asChar());
						} else {
							REG(instruction.p1) = (a == b);
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(greater) : {
					{
						RyValue &a = REG(instruction.p2);
						RyValue &b = REG(instruction.p3);
						if (a.isChar() && b.isChar()) {
							REG(instruction.p1) = RyValue(a.asChar() > b.asChar());
						} else {
							REG(instruction.p1) = (a > b);
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(less) : {
					RyValue &a = REG(instruction.p2);
					RyValue &b = REG(instruction.p3);
					if (a.isChar() && b.isChar()) {
						{
							REG(instruction.p1) = RyValue(a.asChar() < b.asChar());
						}
					} else {
						REG(instruction.p1) = (a < b);
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(modulo) : {
					REG(instruction.p1) = REG(instruction.p2) % REG(instruction.p3);
					READ_AND_DISPATCH();
				}
				START_OPCODE(bitwise_and) : {
					long a = (long) REG(instruction.p2).asNumber();
					long b = (long) REG(instruction.p3).asNumber();
					REG(instruction.p1) = RyValue((double) (a & b));
					READ_AND_DISPATCH();
				}
				START_OPCODE(bitwise_or) : {
					long a = (long) REG(instruction.p2).asNumber();
					long b = (long) REG(instruction.p3).asNumber();
					REG(instruction.p1) = RyValue((double) (a | b));
					READ_AND_DISPATCH();
				}
				START_OPCODE(bitwise_xor) : {
					long a = (long) REG(instruction.p2).asNumber();
					long b = (long) REG(instruction.p3).asNumber();
					REG(instruction.p1) = RyValue((double) (a ^ b));
					READ_AND_DISPATCH();
				}
				START_OPCODE(left_shift) : {
					long a = (long) REG(instruction.p2).asNumber();
					long b = (long) REG(instruction.p3).asNumber();
					REG(instruction.p1) = RyValue((double) (a << b));
					READ_AND_DISPATCH();
				}
				START_OPCODE(right_shift) : {
					long a = (long) REG(instruction.p2).asNumber();
					long b = (long) REG(instruction.p3).asNumber();
					REG(instruction.p1) = RyValue((double) (a >> b));
					READ_AND_DISPATCH();
				}
				START_OPCODE(pop) : {
					// No-op in register VM, used for side-effects in expression statements
					// or to keep instruction alignment if needed.
					READ_AND_DISPATCH();
				}
				START_OPCODE(jump) : {
					uint16_t offset = instruction.p2p3();
					FRAME.ip += offset;
					READ_AND_DISPATCH();
				}
				START_OPCODE(jump_if_false) : {
					uint16_t offset = instruction.p2p3();
					if (!isTruthy(REG(instruction.p1))) {
						FRAME.ip += offset;
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(loop) : {
					uint16_t offset = instruction.p2p3();
					FRAME.ip -= offset;
					READ_AND_DISPATCH();
				}
				START_OPCODE(define_global) : {
					{
						std::string name = CONSTANTS[instruction.p2p3()].to_string();
						RyValue &val = REG(instruction.p1);
						globals[name] = val;
						if (!val.isNil()) {
							globalTypes[name] = val.typeName();
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(get_global) : {
					{
						RyValue nameValue = CONSTANTS[instruction.p2p3()];
						std::string name = nameValue.to_string();
						auto it = globals.find(name);

						// If not found, try to resolve in the current namespace
						if (it == globals.end()) {
							std::string current_func_name = FRAME.closure->function->name;
							auto pos = current_func_name.rfind("::");
							if (pos != std::string::npos) {
								std::string ns = current_func_name.substr(0, pos);
								std::string new_name = ns + "::" + name;
								it = globals.find(new_name);
							}
						}

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
								runtimeError("NameError: Undefined variable '%s'. Did you mean '%s'?", name.c_str(), bestMatch.c_str());
							} else {
								runtimeError("NameError: Undefined variable '%s'.", name.c_str());
							}

							goto trigger_panic;
						}
						REG(instruction.p1) = it->second;
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(set_global) : {
					{
						RyValue nameValue = CONSTANTS[instruction.p2p3()];
						std::string name = nameValue.to_string();
						auto it = globals.find(name);

						// If not found, try to resolve in the current namespace
						if (it == globals.end()) {
							std::string current_func_name = FRAME.closure->function->name;
							auto pos = current_func_name.rfind("::");
							if (pos != std::string::npos) {
								std::string ns = current_func_name.substr(0, pos);
								std::string new_name = ns + "::" + name;
								it = globals.find(new_name);
							}
						}

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
								runtimeError("NameError: Cannot set undefined variable '%s'. Did you mean '%s'?", name.c_str(),
														 bestMatch.c_str());
							} else {
								runtimeError("NameError: Undefined variable '%s'.", name.c_str());
							}

							goto trigger_panic;
						}

						RyValue &val = REG(instruction.p1);
						auto type_it = globalTypes.find(name);

						if (type_it == globalTypes.end()) {
							// First assignment to a variable declared without a value.
							if (!val.isNil()) {
								globalTypes[name] = val.typeName();
							}
						} else {
							// This variable has a type, check for mismatch.
							std::string &lockedType = type_it->second;
							std::string newType = val.typeName();
							if (lockedType != newType && !val.isNil()) {
								runtimeError("TypeError: Global variable '%s' has a locked type of '%s' and cannot be reassigned to a "
														 "value of type '%s'.",
														 name.c_str(), lockedType.c_str(), newType.c_str());
								goto trigger_panic;
							}
						}
						it->second = val;
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(get_property) : {
					{
						RyValue obj = REG(instruction.p2);
						std::string name = CONSTANTS[instruction.p3].to_string();
						bool found = false;

						if (obj.isInstance()) {
							auto instance = obj.asInstance();
							if (instance->klass->privateFields.count(name)) {
								std::string &caller = FRAME.closure->function->ownerClassName;
								if (caller != instance->klass->name) {
									runtimeError("AccessError: Private field '%s' cannot be accessed from outside class '%s'.",
															 name.c_str(), instance->klass->name.c_str());
									goto trigger_panic;
								}
							}

							if (instance->fields.count(name)) {
								REG(instruction.p1) = instance->fields[name];
								found = true;
							} else {
								// Field & Method lookup in class chain
								auto klass = instance->klass;
								while (klass) {
									if (klass->fields.count(name)) {
										REG(instruction.p1) = klass->fields[name];
										found = true;
										break;
									}
									if (klass->methods.count(name)) {
										auto method = klass->methods[name];
										auto bound = std::make_shared<Frontend::RyBoundMethod>(obj, method);
										REG(instruction.p1) = RyValue(bound);
										found = true;
										break;
									}
									klass = klass->superclass;
								}
							}
						} else if (obj.isClass()) {
							auto klass = obj.asClass();
							if (klass->privateFields.count(name)) {
								std::string &caller = FRAME.closure->function->ownerClassName;
								if (caller != klass->name) {
									runtimeError("AccessError: Private field '%s' cannot be accessed from outside class '%s'.",
															 name.c_str(), klass->name.c_str());
									goto trigger_panic;
								}
							}

							if (klass->fields.count(name)) {
								REG(instruction.p1) = klass->fields[name];
								found = true;
							} else if (klass->methods.count(name)) {
								auto bound = std::make_shared<Frontend::RyBoundMethod>(obj, klass->methods[name]);
								REG(instruction.p1) = RyValue(bound);
								found = true;
							}
						} else if (obj.isMap()) {
							auto map = obj.asMap();
							RyValue key(name);
							if (map->count(key)) {
								REG(instruction.p1) = (*map)[key];
								found = true;
							} else {
								auto map = obj.asMap();
								RyValue key(name);
								if (map->count(key)) {
									REG(instruction.p1) = (*map)[key];
									found = true;
								}
							}
						} else if (obj.isList()) {
							if (name == "len") {
								REG(instruction.p1) = RyValue((double) obj.asList()->size());
								found = true;
							}
						} else if (obj.isString()) {
							if (name == "len") {
								REG(instruction.p1) = RyValue((double) obj.asString().length());
								found = true;
							}
						}

						if (!found) {
							runtimeError("AttributeError: Undefined property '%s' on object of type '%s'.", name.c_str(),
													 obj.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(set_property) : {
					{
						RyValue obj = REG(instruction.p1);
						std::string name = CONSTANTS[instruction.p2].to_string();
						RyValue val = REG(instruction.p3);

						if (obj.isInstance()) {
							auto instance = obj.asInstance();
							if (instance->klass->privateFields.count(name)) {
								std::string &caller = FRAME.closure->function->ownerClassName;
								if (caller != instance->klass->name) {
									runtimeError("AccessError: Private field '%s' cannot be accessed from outside class '%s'.",
															 name.c_str(), instance->klass->name.c_str());
									goto trigger_panic;
								}
							}
							obj.asInstance()->fields[name] = val;
						} else if (obj.isClass()) {
							auto klass = obj.asClass();
							if (klass->privateFields.count(name)) {
								std::string &caller = FRAME.closure->function->ownerClassName;
								if (caller != klass->name) {
									runtimeError("AccessError: Private field '%s' cannot be accessed from outside class '%s'.",
															 name.c_str(), klass->name.c_str());
									goto trigger_panic;
								}
							}
							klass->fields[name] = val;
						} else if (obj.isMap()) {
							(*obj.asMap())[RyValue(name)] = val;
						} else {
							runtimeError("AttributeError: Cannot set properties on type '%s'. Only instances, classes, and maps are "
													 "mutable.",
													 obj.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(get_super) : {
					{
						RyValue instance = REG(instruction.p2);
						std::string name = CONSTANTS[instruction.p3].to_string();
						bool found = false;
						if (instance.isInstance()) {
							auto superclass = instance.asInstance()->klass->superclass;
							if (superclass) {
								auto klass = superclass;
								while (klass) {
									if (klass->methods.count(name)) {
										auto bound = std::make_shared<Frontend::RyBoundMethod>(instance, klass->methods[name]);
										REG(instruction.p1) = RyValue(bound);
										found = true;
										break;
									}
									klass = klass->superclass;
								}
							}
						}
						if (!found) {
							runtimeError("AttributeError: Undefined property '%s' in superclass.", name.c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(build_list) : {
					{
						int start = instruction.p2;
						int count = instruction.p3;
						auto list = std::make_shared<std::vector<RyValue>>();
						list->reserve(count);
						for (int i = 0; i < count; ++i) {
							list->emplace_back(REG(start + i));
						}
						REG(instruction.p1) = RyValue(list);
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(build_map) : {
					{
						int start = instruction.p2;
						int count = instruction.p3;
						auto map = std::make_shared<std::map<RyValue, RyValue>>();
						for (int i = 0; i < count; ++i) {
							RyValue key = REG(start + i * 2);
							RyValue val = REG(start + i * 2 + 1);
							(*map)[key] = val;
						}
						REG(instruction.p1) = RyValue(map);
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(build_range_list) : {
					{
						RyValue startVal = REG(instruction.p2);
						RyValue endVal = REG(instruction.p3);

						if (startVal.isNil() || endVal.isNil()) {
							runtimeError("Range operands cannot be null.");
							goto trigger_panic;
						}

						REG(instruction.p1) = RyValue(RyRange{startVal.asNumber(), endVal.asNumber()});
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(get_index) : {
					{
						RyValue obj = REG(instruction.p2);
						RyValue idx = REG(instruction.p3);

						if (obj.isList()) {
							if (!idx.isNumber()) {
								runtimeError("TypeError: List index must be a number.");
								goto trigger_panic;
							}
							auto list = obj.asList();
							int i = (int) idx.asNumber();
							if (i < 0 || i >= (int) list->size()) {
								runtimeError("IndexError: List index out of bounds.");
								goto trigger_panic;
							}
							REG(instruction.p1) = (*list)[i];
						} else if (obj.isMap()) {
							auto map = obj.asMap();
							if (map->find(idx) == map->end()) {
								REG(instruction.p1) = RyValue(); // Return null if not found
							} else {
								REG(instruction.p1) = (*map)[idx];
							}
						} else if (obj.isString()) {
							// String indexing: return single character as string
							if (!idx.isNumber()) {
								runtimeError("TypeError: String index must be a number.");
								goto trigger_panic;
							}
							std::string s = obj.asString();
							int i = (int) idx.asNumber();
							if (i < 0 || i >= (int) s.length()) {
								runtimeError("IndexError: String index out of bounds.");
								goto trigger_panic;
							}
							REG(instruction.p1) = RyValue(s[i]);
						} else {
							runtimeError("TypeError: Type '%s' is not subscriptable.", obj.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(set_index) : {
					{
						RyValue obj = REG(instruction.p1);
						RyValue idx = REG(instruction.p2);
						RyValue val = REG(instruction.p3);

						if (obj.isList()) {
							if (!idx.isNumber()) {
								runtimeError("TypeError: List index must be a number.");
								goto trigger_panic;
							}
							auto list = obj.asList();
							int i = (int) idx.asNumber();
							if (i < 0 || i >= (int) list->size()) {
								runtimeError("IndexError: List index out of bounds.");
								goto trigger_panic;
							}
							(*list)[i] = val;
						} else if (obj.isMap()) {
							(*obj.asMap())[idx] = val;
						} else if (obj.isString()) {
							runtimeError("TypeError: Strings are immutable and do not support item assignment.");
							goto trigger_panic;
						} else {
							runtimeError("TypeError: Type '%s' does not support item assignment.", obj.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(import) : {
					{
						RyValue fileNameVal = REG(instruction.p2); // p1=dest, p2=path_reg
						if (!fileNameVal.isString()) {
							runtimeError("Import path must be a string.");
							goto trigger_panic;
						}
						std::string fileName = RyTools::findModulePath(fileNameVal.to_string(), false);

						if (moduleCache.count(fileName)) {
							REG(instruction.p1) = RyValue(moduleCache[fileName]);
						} else {
							std::ifstream file(fileName);
							if (!file.is_open()) {
								runtimeError("Could not open module '%s'.", fileName.c_str());
								goto trigger_panic;
							}
							std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

							// Compile module
							Backend::Lexer lexer(source);
							auto tokens = lexer.scanTokens();
							std::set<std::string> aliases;
							Backend::Parser parser(tokens, aliases, source);
							auto stmts = parser.parse();

							RyRuntime::Compiler compiler(nullptr, source);
							Chunk chunk;
							if (!compiler.compile(stmts, &chunk)) {
								runtimeError("Failed to compile module '%s'.", fileName.c_str());
								goto trigger_panic;
							}

							auto function = std::make_shared<Frontend::RyFunction>(std::move(chunk), fileName, 0);
							auto closure = std::make_shared<RyClosure>(function);
							moduleCache[fileName] = closure;
							REG(instruction.p1) = RyValue(closure);
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(class) : {
					{
						std::string name = CONSTANTS[instruction.p2p3()].to_string();
						auto klass = std::make_shared<Frontend::RyClass>(name);
						REG(instruction.p1) = RyValue(klass);
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(method) : {
					{
						RyValue klass = REG(instruction.p1);
						RyValue method = REG(instruction.p2);
						std::string name = CONSTANTS[instruction.p3].to_string();
						if (klass.isClass() && method.isClosure()) {
							klass.asClass()->methods[name] = method.asClosure();
						} else {
							runtimeError("Invalid method definition.");
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(inherit) : {
					{
						RyValue sub = REG(instruction.p1);
						RyValue super = REG(instruction.p2);
						if (!sub.isClass() || !super.isClass()) {
							runtimeError("Superclass must be a class.");
							goto trigger_panic;
						}
						sub.asClass()->superclass = super.asClass();
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(verify_abstract) : {
					{
						RyValue klassVal = REG(instruction.p1);
						if (klassVal.isClass()) {
							klassVal.asClass()->isAbstract = true;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(define_private_field) : {
					{
						RyValue klass = REG(instruction.p1);
						std::string name = CONSTANTS[instruction.p2p3()].to_string();
						if (klass.isClass()) {
							klass.asClass()->privateFields.insert(name);
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(for_each_next) : {
					{
						uint8_t iterReg = instruction.p1;
						// Use direct access to ensure we are reading/writing the actual register state
						double currentIdx = REG(iterReg).asNumber();
						RyValue colVal = REG(iterReg - 1); // Make a copy to avoid aliasing issues
						bool advanced = false;

						if (colVal.isRange()) {
							RyRange range = colVal.asRange();
							double val = range.start + currentIdx;
							if (val <= range.end) {
								REG(iterReg + 1) = RyValue(val);
								advanced = true;
							} else {
								// Loop has finished iterating
								advanced = false;
							}
						} else if (colVal.isList()) {
							const auto &list = colVal.asList();
							int i = static_cast<int>(currentIdx);
							if (i >= 0 && i < static_cast<int>(list->size())) {
								if (i < 0) {
									runtimeError("IndexError: loop is stuck due to negative index");
									goto trigger_panic;
								}
								if (currentIdx >= INT_MAX) {
									runtimeError("IndexError: loop is stuck due to large index");
									goto trigger_panic;
								}

								REG(iterReg + 1) = (*list)[i];
								advanced = true;
							} else {
								// Loop has finished iterating
								advanced = false;
							}
						} else if (colVal.isString()) {
							const std::string &s = colVal.asString();
							int i = static_cast<int>(currentIdx);
							if (i >= 0 && i < static_cast<int>(s.length())) {
								if (i < 0) {
									runtimeError("IndexError: loop is stuck due to negative index");
									goto trigger_panic;
								}
								if (currentIdx >= INT_MAX) {
									runtimeError("IndexError: loop is stuck due to large index");
									goto trigger_panic;
								}

								REG(iterReg + 1) = RyValue(s[i]);
								advanced = true;
							} else {
								// Loop has finished iterating
								advanced = false;
							}
						}


						if (advanced) {
							REG(iterReg) = RyValue(currentIdx + 1.0); // Increment internal index
						} else {
							FRAME.ip += instruction.p2p3(); // Jump to exit
						}
					}
					READ_AND_DISPATCH();
				}

				START_OPCODE(panic) : {
				trigger_panic: {
					std::string output;
					if (!last_error_message.empty()) {
						output = last_error_message;
						last_error_message.clear();
					} else if (instruction.opcode == OP_PANIC && REG(instruction.p1).isString()) {
						output = REG(instruction.p1).asString();
					} else {
						output = "Runtime Panic";
					}

					if (panicStack.empty()) {
						std::cerr << "Traceback (most recent calls)" << std::endl;
						std::string lastFile = "";
						for (int i = frameCount - 1; i >= 0; i--) {
							auto &frame = frames[i];
							auto func = frame.closure->function;
							size_t instruction_offset = (frame.ip - 1) - func->chunk.code.data();
							int line = func->chunk.lines[instruction_offset];

							std::string funcName = func->name;
							std::string fileName;

							if (funcName == "<main>" || funcName.empty()) {
								fileName = "main script";
								funcName = funcName.empty() ? "script" : "main";
							} else if (funcName.find(".ry") != std::string::npos) {
								fileName = funcName;
								funcName = "module top-level";
							} else {
								// It's a regular function. Find its file from a previous frame.
								for (int j = i; j >= 0; j--) {
									std::string potentialFile = frames[j].closure->function->name;
									if (potentialFile == "<main>" || potentialFile.find(".ry") != std::string::npos) {
										fileName = (potentialFile == "<main>") ? "main script" : potentialFile;
										break;
									}
								}
							}

							if (fileName != lastFile && !fileName.empty()) {
								std::cerr << " - in file <" << fileName << ">" << std::endl;
								lastFile = fileName;
							}
							std::cerr << "\t-> at line " << line << " in \"" << funcName << "\"" << std::endl;
						}
						std::cerr << std::endl;

						if (frameCount > 0) {
							auto &frame = frames[frameCount - 1];
							size_t instruction = (frame.ip - 1) - frame.closure->function->chunk.code.data();
							int line = frame.closure->function->chunk.lines[instruction];
							int column = frame.closure->function->chunk.columns[instruction];
							std::string sourceForReport;
							std::string functionName = frame.closure->function->name;

							if (functionName.find(".ry") != std::string::npos) {
								// Error is in an imported file. Read the file's source.
								std::ifstream file(functionName);
								if (file.is_open()) {
									sourceForReport.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
								}
							} else if (functionName != "<main>" && !functionName.empty() &&
												 functionName.find("::") == std::string::npos) {
								// Error is in a function defined in the REPL. Use the cached block source.
								sourceForReport = lastReplBlockSource;
							} else {
								// Error is in the top-level of the current REPL/script input.
								sourceForReport = vmSource;
							}
							RyTools::report(line, column, "", output, sourceForReport);
						}

						return INTERPRET_RUNTIME_ERROR;
					}

					ControlBlock block = panicStack.back();
					panicStack.pop_back();

					while (frameCount > block.frameDepth) {
						closeUpvalues(frames[frameCount - 1].reg_base);
						frameCount--;
					}

					// Store the exception so the catch block can retrieve it
					lastException = RyValue(output);

					FRAME.ip = FRAME.closure->function->chunk.code.data() + block.handlerIP;
				}
					READ_AND_DISPATCH();
				}
				START_OPCODE(call) : {
					{
						uint8_t calleeReg = instruction.p1;
						uint8_t argCount = instruction.p2;
						RyValue &callee = REG(calleeReg);

						if (callee.isNative()) {
							auto native = callee.asNative();
							if (native->function == nullptr) {
								runtimeError("Native function '%s' is missing implementation (NULL pointer).", native->name.c_str());
								goto trigger_panic;
							}
							RyValue result = native->function(argCount, &REG(calleeReg + 1), globals);
							REG(calleeReg) = result;
						} else if (callee.isClosure()) {
							auto closure = callee.asClosure();
							if (argCount != closure->function->arity) {
								runtimeError("ArgumentError: Function '%s' expected %d arguments but got %d.",
														 closure->function->name.c_str(), closure->function->arity, argCount);
								goto trigger_panic;
							}

							if (closure->function->isAbstract) {
								runtimeError("TypeError: Cannot call abstract method '%s'.", closure->function->name.c_str());
								goto trigger_panic;
							}

							int caller_base = FRAME.reg_base;
							CallFrame *frame = &frames[frameCount++];
							frame->closure = closure;
							frame->ip = closure->function->chunk.code.data();
							frame->reg_base = caller_base + calleeReg; // Arguments follow callee
						} else if (callee.isFunction()) {
							// This path is less common as functions are usually wrapped in closures
							auto func = callee.asFunction();
							if (argCount != func->arity) {
								runtimeError("ArgumentError: Function '%s' expected %d arguments but got %d.", func->name.c_str(),
														 func->arity, argCount);
								goto trigger_panic;
							}
							auto closure = std::make_shared<RyClosure>(func);
							int caller_base = FRAME.reg_base;
							CallFrame *frame = &frames[frameCount++];
							frame->closure = closure;
							frame->ip = func->chunk.code.data();
							frame->reg_base = caller_base + calleeReg;
						} else if (callee.isClass()) {
							auto klass = callee.asClass();
							auto instance = std::make_shared<Frontend::RyInstance>(klass);
							instance->fields = klass->fields;
							if (klass->isAbstract) { // This check is also in the compiler, but good to have at runtime too
								runtimeError("TypeError: Cannot create an instance of abstract class '%s'.", klass->name.c_str());
								goto trigger_panic;
							}
							REG(calleeReg) = RyValue(instance); // Put instance in callee's spot

							// Find initializer, walking up the inheritance chain
							auto current_class = klass;
							std::shared_ptr<RyClosure> initializer = nullptr;
							while (current_class != nullptr) {
								auto it = current_class->methods.find("init");
								if (it != current_class->methods.end()) {
									initializer = it->second;
									break;
								}
								current_class = current_class->superclass;
							}

							if (initializer != nullptr) {
								int caller_base = FRAME.reg_base;
								CallFrame *frame = &frames[frameCount++];
								frame->closure = initializer;
								frame->ip = frame->closure->function->chunk.code.data();
								frame->reg_base = caller_base + calleeReg;

								if (argCount != frame->closure->function->arity) {
									runtimeError("ArgumentError: Constructor for '%s' expected %d arguments but got %d.",
															 klass->name.c_str(), frame->closure->function->arity, argCount);
									goto trigger_panic;
								}
							} else if (argCount != 0) {
								runtimeError("ArgumentError: Constructor for '%s' expected 0 arguments but got %d.",
														 klass->name.c_str(), argCount);
								goto trigger_panic;
							}
						} else if (callee.isBoundMethod()) {
							auto bound = callee.asBoundMethod();
							if (argCount != bound->method->function->arity) {
								runtimeError("ArgumentError: Method '%s' expected %d arguments but got %d.",
														 bound->method->function->name.c_str(), bound->method->function->arity, argCount);
								goto trigger_panic;
							}
							REG(calleeReg) = bound->receiver;

							int caller_base = FRAME.reg_base;
							CallFrame *frame = &frames[frameCount++];
							frame->closure = bound->method;
							frame->ip = bound->method->function->chunk.code.data();
							frame->reg_base = caller_base + calleeReg;
						} else {
							runtimeError("TypeError: Value of type '%s' is not callable.", callee.typeName().c_str());
							goto trigger_panic;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(return) : {
					{
						RyValue result = REG(instruction.p1);
						if (FRAME.closure->function->name == "init") {
							result = registers[FRAME.reg_base];
						}
						closeUpvalues(FRAME.reg_base);

						int destinationReg = FRAME.reg_base;

						frameCount--;

						if (frameCount == 0) {
							return INTERPRET_OK;
						}

						registers[destinationReg] = result;
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(attempt) : {
					{
						uint16_t offset = instruction.p2p3();
						ControlBlock block;
						block.frameDepth = frameCount;
						block.stackDepth = 0;
						block.handlerIP = (int) (FRAME.ip - FRAME.closure->function->chunk.code.data()) + offset;
						panicStack.emplace_back(block);
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(end_attempt) : {
					if (!panicStack.empty()) {
						panicStack.pop_back();
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(load_exception) : {
					REG(instruction.p1) = lastException;
					READ_AND_DISPATCH();
				}
				START_OPCODE(close_upvalue) : {
					closeUpvalues(FRAME.reg_base + instruction.p1);
					READ_AND_DISPATCH();
				}
				START_OPCODE(assign_local) : {
					{
						uint8_t destRegIdx = instruction.p1;
						uint8_t srcRegIdx = instruction.p2;
						int fullDestReg = FRAME.reg_base + destRegIdx;
						RyValue &newVal = REG(srcRegIdx);

						if (registerTypeLocked[fullDestReg]) {
							std::string &lockedType = registerTypeNames[fullDestReg];
							std::string newType = newVal.typeName();
							if (lockedType != newType && !newVal.isNil()) {
								runtimeError("TypeError: This local variable has a locked type of '%s' and cannot be reassigned to a "
														 "value of type '%s'.",
														 lockedType.c_str(), newType.c_str());
								goto trigger_panic;
							}
						} else {
							// First assignment, lock the type.
							if (!newVal.isNil()) {
								registerTypeNames[fullDestReg] = newVal.typeName();
								registerTypeLocked[fullDestReg] = true;
							}
						}

						REG(destRegIdx) = newVal;
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(type_lock) : {
					{
						int regIdx = FRAME.reg_base + instruction.p1;
						RyValue &val = registers[regIdx];
						if (!val.isNil()) {
							registerTypeNames[regIdx] = val.typeName();
							registerTypeLocked[regIdx] = true;
						}
					}
					READ_AND_DISPATCH();
				}
				START_OPCODE(type_unlock) : {
					int regIdx = FRAME.reg_base + instruction.p1;
					if (regIdx < 256) {
						registerTypeLocked[regIdx] = false;
						registerTypeNames[regIdx] = "";
					}
					READ_AND_DISPATCH();
				}

#if !DIRECT_THREADING
				default:
					printf("Unknown opcode %d\n", instruction.opcode);
					return INTERPRET_COMPILE_ERROR;
			}
		}
#endif
		// This part is reached only if direct threading is enabled and something goes wrong,
		// or if the loop in non-direct threading is broken.
		return INTERPRET_RUNTIME_ERROR;
	}

} // namespace RyRuntime
