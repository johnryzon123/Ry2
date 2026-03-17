#include "compiler.h"
#include <iostream>
#include <vector>
#include "chunk.h"
#include "class.h"
#include "func.h"
#include "parser.h"
#include "stmt.h"
#include "token.h"
#include "tools.h"

using namespace Backend;

namespace RyRuntime {
	// Register allocation
	int Compiler::allocReg() {
		int reg;
		// Reuse a freed register if available
		if (!freeList.empty()) {
			reg = freeList.back();
			freeList.pop_back();
		} else {
			if (nextReg >= 255) {
				error(Token(), "Too many registers used in function.");
				return 255;
			}
			reg = nextReg++;
		}
		allocationStack.push_back(reg);
		return reg;
	}

	void Compiler::freeRegs(int count) {
		for (int i = 0; i < count; i++) {
			if (allocationStack.empty())
				break;
			int reg = allocationStack.back();
			allocationStack.pop_back();

			// If we are freeing the top-most register, just lower the watermark
			if (reg == nextReg - 1) {
				nextReg--;
			} else {
				// Otherwise, add it to the free list for reuse
				freeList.push_back(reg);
			}
		}
	}

	bool Compiler::compile(const std::vector<std::shared_ptr<Backend::Stmt>> &statements, Chunk *chunk) {
		this->compilingChunk = chunk;
		this->nextReg = 0;
		this->freeList.clear();
		this->allocationStack.clear();
		this->locals.clear();
		this->scopeDepth = 0;
		Token internal;
		internal.lexeme = "(script)";
		addLocal(internal); // R0 is reserved for the script's function object

		for (const auto &stmt: statements) {
			compileStatement(stmt);
		}

		Instruction load_null;
		load_null.opcode = OP_LOAD_NULL;
		load_null.p1 = 0;
		emitInstruction(load_null);

		Instruction ret;
		ret.opcode = OP_RETURN;
		ret.p1 = 0;
		emitInstruction(ret);

		return !RyTools::hadError;
	}

	void Compiler::compileStatement(std::shared_ptr<Backend::Stmt> stmt) {
		if (stmt)
			stmt->accept(*this);
	}

	int Compiler::compileExpression(std::shared_ptr<Backend::Expr> expr) {
		int reg = allocReg();
		compileExpression(expr, reg);
		return reg;
	}

	void Compiler::compileExpression(std::shared_ptr<Backend::Expr> expr, int destReg) {
		this->targetReg = destReg;
		if (expr) {
			expr->accept(*this);
		}
	}
	void Compiler::compileMethod(std::shared_ptr<Backend::FunctionStmt> stmt, const std::string &ownerClassName) {
		track(stmt->name);

		Compiler subCompiler(this, this->sourceCode);
		subCompiler.currentClass = this->currentClass;
		auto function = std::make_shared<Frontend::RyFunction>();
		function->name = stmt->name.lexeme;
		function->ownerClassName = ownerClassName;
		function->isPrivate = stmt->isPrivate;
		function->arity = stmt->parameters.size();
		function->isStatic = (stmt->name.type == TokenType::STATIC);
		function->isOpen = (stmt->name.literal.isBool() && stmt->name.literal.asBool());
		function->isAbstract = stmt->isAbstract;

		subCompiler.compilingChunk = &function->chunk;
		subCompiler.beginScope();

		if (stmt->name.type != TokenType::STATIC) {
			// Slot 0 is "this" for methods!
			Token thisToken;
			thisToken.lexeme = "this";
			subCompiler.addLocal(thisToken);
		} else {
			subCompiler.addLocal(Token());
		}

		for (const auto &param: stmt->parameters) {
			subCompiler.addLocal(param.name);
		}

		for (const auto &bodyStmt: stmt->body) {
			subCompiler.compileStatement(bodyStmt);
		}

		Instruction load_null;
		load_null.opcode = OP_LOAD_NULL;
		load_null.p1 = 0; // Return value in R0
		subCompiler.emitInstruction(load_null);

		Instruction ret;
		ret.opcode = OP_RETURN;
		ret.p1 = 0;
		subCompiler.emitInstruction(ret);

		subCompiler.endScope();

		function->upvalueCount = subCompiler.upvalues.size();
		Instruction closure_instr;
		closure_instr.opcode = OP_CLOSURE;
		closure_instr.p1 = targetReg;
		closure_instr.set_p2p3(makeConstant(RyValue(function)));
		emitInstruction(closure_instr);

		// Emit upvalue data
		for (int i = 0; i < subCompiler.upvalues.size(); i++) {
			// The VM will need to be aware that OP_CLOSURE is followed by
			// this many "dummy" instructions carrying upvalue data.
			Instruction upvalue_instr;
			upvalue_instr.opcode = OP_MOVE; // Opcode is ignored by VM here
			upvalue_instr.p1 = subCompiler.upvalues[i].isLocal ? 1 : 0;
			upvalue_instr.p2 = subCompiler.upvalues[i].index;
			emitInstruction(upvalue_instr);
		}
	}

	// --- Bytecode Helpers ---

	void Compiler::emitInstruction(Instruction instr) { compilingChunk->write(instr, currentLine, currentColumn); }

	int Compiler::makeConstant(RyValue value) {
		int constant = compilingChunk->addConstant(value);
		if (constant > UINT16_MAX) {
			std::cerr << "Too many constants in one chunk!" << std::endl;
			return 0;
		}
		return constant;
	}

	int Compiler::emitJump(OpCode op) {
		Instruction instr;
		instr.opcode = op;
		instr.set_p2p3(0xFFFF);
		emitInstruction(instr);
		return compilingChunk->code.size() - 1;
	}

	void Compiler::patchJump(int offset) {
		int jump = compilingChunk->code.size() - offset - 1;

		if (jump > UINT16_MAX) {
			std::cerr << "Too much code to jump over!" << std::endl;
		}

		compilingChunk->code[offset].set_p2p3((uint16_t) jump);
	}

	void Compiler::emitLoop(int loopStart) {
		Instruction instr;
		instr.opcode = OP_LOOP;
		int offset = compilingChunk->code.size() - loopStart + 1;
		if (offset > UINT16_MAX)
			std::cerr << "Loop body too large!" << std::endl;

		instr.set_p2p3(offset);
		emitInstruction(instr);
	}

	// --- Scope Helpers ---

	void Compiler::beginScope() { scopeDepth++; }

	void Compiler::endScope() {
		scopeDepth--;
		// Pop locals that were in this scope
		while (!locals.empty() && locals.back().depth > scopeDepth) {
			if (locals.back().isCaptured) {
				Instruction instr;
				instr.opcode = OP_CLOSE_UPVALUE;
				instr.p1 = locals.size() - 1; // The register index to close
				emitInstruction(instr);
			}

			// Unlock the type guard for the register
			Instruction unlockInstr;
			unlockInstr.opcode = OP_TYPE_UNLOCK;
			unlockInstr.p1 = locals.size() - 1;
			emitInstruction(unlockInstr);
			locals.pop_back();
		}
		nextReg = locals.size();
	}

	void Compiler::addLocal(Token name) {
		if (locals.size() >= 255) {
			error(name, "Too many local variables in function.");
			return;
		}
		Local local = Local(name, scopeDepth, false);
		locals.emplace_back(local);
		nextReg = locals.size();
	}

	auto Compiler::resolveLocal(Token &name) -> int {
		for (int i = locals.size() - 1; i >= 0; i--) {
			Local &local = locals[i];
			if (name.lexeme == local.name.lexeme) {
				if (local.depth == -1) {
					error(name, "Can't read local variable in its own initializer.");
				}
				return i;
			}
		}
		return -1;
	}
	auto Compiler::resolveUpvalue(Token &name) -> int {
		if (enclosing == nullptr)
			return -1; // We hit the top (global scope)

		int local = enclosing->resolveLocal(name);
		if (local != -1) {
			enclosing->locals[local].isCaptured = true;
			return addUpvalue((uint8_t) local, true);
		}

		int upvalue = enclosing->resolveUpvalue(name);
		if (upvalue != -1) {
			return addUpvalue((uint8_t) upvalue, false);
		}

		return -1;
	}

	auto Compiler::addUpvalue(uint8_t index, bool isLocal) -> int {
		for (int i = 0; i < upvalues.size(); i++) {
			Upvalue &upvalue = upvalues[i];
			if (upvalue.index == index && upvalue.isLocal == isLocal) {
				return i;
			}
		}

		if (upvalues.size() == 256) {
			RyTools::report(currentLine, currentColumn, "", "Too many closure variables in function.", sourceCode);
			RyTools::hadError = true;
			return 0;
		}

		Upvalue upvalue;
		upvalue.isLocal = isLocal;
		upvalue.index = index;
		upvalues.emplace_back(upvalue);
		return (int) upvalues.size() - 1;
	}

	// --- Error reporting ---
	void Compiler::error(const Backend::Token &token, const std::string &message) {
		RyTools::report(token.line, token.column, "", message, this->sourceCode);

		RyTools::hadError = true;
	}

	void Compiler::track(Token token) {
		this->currentLine = token.line;
		this->currentColumn = token.column;
	}

	// --- Visitors ---

	void Compiler::visitMath(MathExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;

		int leftReg = compileExpression(expr.left);
		int rightReg = compileExpression(expr.right);

		Instruction instr;
		instr.p1 = finalDest;
		instr.p2 = leftReg;
		instr.p3 = rightReg;

		switch (expr.op_t.type) {
			case Backend::TokenType::PLUS:
				instr.opcode = OP_ADD;
				break;
			case Backend::TokenType::MINUS:
				instr.opcode = OP_SUBTRACT;
				break;
			case TokenType::STAR:
				instr.opcode = OP_MULTIPLY;
				break;
			case TokenType::DIVIDE:
				instr.opcode = OP_DIVIDE;
				break;
			case TokenType::PERCENT:
				instr.opcode = OP_MODULO;
				break;
			case TokenType::EQUAL_EQUAL:
				instr.opcode = OP_EQUAL;
				break;
			case TokenType::BANG_EQUAL:
				instr.opcode = OP_EQUAL; // a != b is !(a == b)
				emitInstruction(instr);

				instr.opcode = OP_NOT;
				instr.p2 = finalDest;
				instr.p3 = 0;
				return;
			case TokenType::GREATER:
				instr.opcode = OP_GREATER;
				break;
			case TokenType::GREATER_EQUAL:
				instr.opcode = OP_LESS; // a >= b is !(a < b)
				emitInstruction(instr);

				instr.opcode = OP_NOT;
				instr.p2 = finalDest;
				instr.p3 = 0;
				return;
			case TokenType::LESS:
				instr.opcode = OP_LESS;
				break;
			case TokenType::LESS_EQUAL:
				instr.opcode = OP_GREATER; // a <= b is !(a > b)
				emitInstruction(instr);

				instr.opcode = OP_NOT;
				instr.p2 = finalDest;
				instr.p3 = 0;
				return;
			default:
				return; // Should not happen
		}
		emitInstruction(instr);
		freeRegs(2);
	}

	void Compiler::visitGroup(GroupExpr &expr) { compileExpression(expr.expression, targetReg); }

	void Compiler::visitVariable(VariableExpr &expr) {
		track(expr.name);
		int finalDest = targetReg;

		Instruction instr;
		instr.p1 = finalDest;

		int arg = resolveLocal(expr.name);
		if (arg != -1) {
			instr.opcode = OP_MOVE;
			instr.p2 = arg;
			emitInstruction(instr);
			return;
		}

		arg = resolveUpvalue(expr.name);
		if (arg != -1) {
			instr.opcode = OP_LOAD_UPVALUE;
			instr.p2 = arg;
			emitInstruction(instr);
			return;
		}

		// Globals
		std::string name = expr.name.lexeme;
		if (name.find("::") != std::string::npos) {
			// Already namespaced
		} else {
			bool isNative = nativeNames.count(name) > 0;
			if (!currentNamespace.empty() && !isNative && !name.starts_with("native")) {
				name = currentNamespace + "::" + name;
			}
		}

		instr.opcode = OP_GET_GLOBAL;
		instr.set_p2p3(makeConstant(RyValue(name)));
		emitInstruction(instr);
	}

	void Compiler::visitValue(ValueExpr &expr) {
		track(expr.value);
		int finalDest = targetReg;

		Instruction instr;
		instr.p1 = finalDest;

		if (expr.value.type == TokenType::TRUE) {
			instr.opcode = OP_LOAD_TRUE;
		} else if (expr.value.type == TokenType::FALSE) {
			instr.opcode = OP_LOAD_FALSE;
		} else if (expr.value.type == TokenType::NULL_TOKEN) {
			instr.opcode = OP_LOAD_NULL;
		} else if (expr.value.type == TokenType::NUMBER || expr.value.type == TokenType::STRING ||
							 expr.value.type == TokenType::CHAR) {
			instr.opcode = OP_LOAD_CONST;
			instr.set_p2p3(makeConstant(expr.value.literal));
		} else {
			return; // Should not happen
		}
		emitInstruction(instr);
	}

	void Compiler::visitLogical(LogicalExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;

		if (expr.op_t.type == TokenType::AND) {
			// Compile left, if false, result is left, jump to end.
			compileExpression(expr.left, finalDest);
			int endJump = emitJump(OP_JUMP_IF_FALSE);

			// If true, result is right.
			compileExpression(expr.right, finalDest);
			patchJump(endJump);
			compilingChunk->code[endJump].p1 = finalDest; // Set condition register
		} else { // OR
			// Compile left, if true, result is left, jump to end.
			compileExpression(expr.left, finalDest);
			int elseJump = emitJump(OP_JUMP_IF_FALSE);

			int endJump = emitJump(OP_JUMP);
			patchJump(elseJump);
			compilingChunk->code[elseJump].p1 = finalDest;

			// If false, result is right.
			compileExpression(expr.right, finalDest);
			patchJump(endJump);
		}
	}
	void Compiler::visitRange(RangeExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;

		int startReg = compileExpression(expr.leftBound);
		int endReg = compileExpression(expr.rightBound);
		Instruction instr;
		instr.opcode = OP_BUILD_RANGE_LIST;
		instr.p1 = finalDest; // Use the saved destination
		instr.p2 = startReg;
		instr.p3 = endReg;
		emitInstruction(instr);
		freeRegs(2);
	}
	void Compiler::visitList(ListExpr &expr) {
		int finalDest = targetReg;
		int startReg = nextReg;
		if (startReg + expr.elements.size() > 255) {
			error(Token(), "Too many elements in list literal.");
			return;
		}
		nextReg += expr.elements.size();

		for (size_t i = 0; i < expr.elements.size(); ++i) {
			compileExpression(expr.elements[i], startReg + i);
		}

		Instruction instr;
		instr.opcode = OP_BUILD_LIST;
		instr.p1 = finalDest;
		instr.p2 = startReg;
		instr.p3 = (uint8_t) expr.elements.size();
		emitInstruction(instr);

		freeRegs(expr.elements.size());
	}

	void Compiler::visitAssign(AssignExpr &expr) {
		track(expr.name);
		int finalDest = targetReg;
		int arg = resolveLocal(expr.name);
		if (arg != -1) {
			int valReg = compileExpression(expr.value);
			Instruction assignInstr;
			assignInstr.opcode = OP_ASSIGN_LOCAL;
			assignInstr.p1 = arg; // dest
			assignInstr.p2 = valReg; // src
			emitInstruction(assignInstr);
			Instruction moveInstr;
			moveInstr.opcode = OP_MOVE;
			moveInstr.p1 = finalDest;
			moveInstr.p2 = arg;
			emitInstruction(moveInstr);
			freeRegs(1);
			return;
		}

		arg = resolveUpvalue(expr.name);
		if (arg != -1) {
			int valReg = compileExpression(expr.value);
			Instruction instr;
			instr.opcode = OP_SET_UPVALUE;
			instr.p1 = valReg;
			instr.p2 = arg;
			emitInstruction(instr);

			Instruction move_instr;
			move_instr.opcode = OP_MOVE;
			move_instr.p1 = finalDest;
			move_instr.p2 = valReg;
			emitInstruction(move_instr);
			freeRegs(1);
			return;
		}

		// Globals
		int valReg = compileExpression(expr.value);
		std::string name = expr.name.lexeme;
		if (expr.name.lexeme.find("::") != std::string::npos) {
			// Is namespaced
		} else {
			if (name.find("::") == std::string::npos && !currentNamespace.empty()) {
				name = currentNamespace + "::" + name;
			}
		}

		Instruction instr;
		instr.opcode = OP_SET_GLOBAL;
		instr.p1 = valReg;
		instr.set_p2p3(makeConstant(RyValue(name)));
		emitInstruction(instr);

		Instruction move_instr;
		move_instr.opcode = OP_MOVE;
		move_instr.p1 = finalDest;
		move_instr.p2 = valReg;
		emitInstruction(move_instr);
		freeRegs(1);
	}

	void Compiler::visitCall(CallExpr &expr) {
		track(expr.Paren);
		int finalDest = targetReg;

		int calleeReg = compileExpression(expr.callee);
		int argStartReg = nextReg;

		for (const auto &arg: expr.arguments) {
			compileExpression(arg, allocReg());
		}

		Instruction instr;
		instr.opcode = OP_CALL;
		instr.p1 = calleeReg;
		instr.p2 = expr.arguments.size();
		emitInstruction(instr);

		// Result of call is in calleeReg, move it to target
		if (calleeReg != finalDest) {
			Instruction move_instr;
			move_instr.opcode = OP_MOVE;
			move_instr.p1 = finalDest;
			move_instr.p2 = calleeReg;
			emitInstruction(move_instr);
		}

		// Free registers used for callee and args
		freeRegs(1 + expr.arguments.size());
	}

	void Compiler::visitExpressionStmt(ExpressionStmt &stmt) {
		int reg = compileExpression(stmt.expression);
		freeRegs(1); // Result is not used, free the register.
	}

	void Compiler::visitBlockStmt(BlockStmt &stmt) {
		beginScope();
		if (stmt.statements.empty()) {
			// Emit a no-op for empty blocks. This prevents an OP_LOOP from immediately
			// following a conditional jump like OP_FOR_EACH_NEXT, which can cause
			// subtle issues with jump offset calculations in the VM for empty loop bodies.
			Instruction pop;
			pop.opcode = OP_POP;
			emitInstruction(pop);
		} else {
			for (const auto &s: stmt.statements) {
				compileStatement(s);
			}
		}
		endScope();
	}

	void Compiler::visitIfStmt(IfStmt &stmt) {
		int condReg = compileExpression(stmt.condition);

		int thenJump = emitJump(OP_JUMP_IF_FALSE);
		freeRegs(1);

		compileStatement(stmt.thenBranch);

		int elseJump = emitJump(OP_JUMP);

		patchJump(thenJump);
		compilingChunk->code[thenJump].p1 = condReg;

		if (stmt.elseBranch) {
			compileStatement(stmt.elseBranch);
		}
		patchJump(elseJump);
	}

	void Compiler::visitWhileStmt(WhileStmt &stmt) {
		int loopStart = compilingChunk->code.size();


		LoopContext context = LoopContext();
		context.startIP = loopStart;
		context.scopeDepth = this->scopeDepth;
		context.type = LOOP_WHILE;
		loopStack.emplace_back(context);

		int condReg = compileExpression(stmt.condition);

		int exitJump = emitJump(OP_JUMP_IF_FALSE);
		freeRegs(1);

		compileStatement(stmt.body);
		emitLoop(loopStart);

		patchJump(exitJump);
		compilingChunk->code[exitJump].p1 = condReg;
		for (int location: context.breakJumps) {
			patchJump(location);
		}
		loopStack.pop_back();
	}

	void Compiler::visitForStmt(ForStmt &stmt) {
		beginScope();
		if (stmt.init)
			compileStatement(stmt.init);

		int loopStart = compilingChunk->code.size();
		LoopContext context = LoopContext();
		context.startIP = loopStart;
		context.scopeDepth = this->scopeDepth;
		context.type = LOOP_FOR;
		loopStack.emplace_back(context);

		int exitJump = -1;
		int condReg = -1;
		if (stmt.condition) {
			condReg = compileExpression(stmt.condition);
			exitJump = emitJump(OP_JUMP_IF_FALSE);
			freeRegs(1);
		}

		compileStatement(stmt.body);

		if (stmt.increment) {
			int reg = compileExpression(stmt.increment); // NOLINT
			freeRegs(1);
		}

		emitLoop(loopStart);

		if (exitJump != -1) {
			patchJump(exitJump);
			compilingChunk->code[exitJump].p1 = condReg;
		}

		for (int location: context.breakJumps) {
			patchJump(location);
		}
		loopStack.pop_back();
		endScope();
	}

	void Compiler::visitVarStmt(VarStmt &stmt) {
		track(stmt.name);
		int finalDest = targetReg;

		if (scopeDepth > 0) {
			Token localName = stmt.name;
			// Parser incorrectly adds namespace to locals. Strip it.
			size_t pos = localName.lexeme.rfind("::");
			if (pos != std::string::npos) {
				localName.lexeme = localName.lexeme.substr(pos + 2);
			}
			addLocal(localName);
			int reg = locals.size() - 1;
			if (stmt.initializer) {
				compileExpression(stmt.initializer, reg);
				Instruction lockInstr;
				lockInstr.opcode = OP_TYPE_LOCK;
				lockInstr.p1 = reg;
				emitInstruction(lockInstr);
			} else {
				Instruction instr;
				instr.opcode = OP_LOAD_NULL;
				instr.p1 = reg;
				emitInstruction(instr);
			}
		} else {
			int reg = -1;
			if (stmt.initializer) {
				reg = compileExpression(stmt.initializer);
			} else {
				reg = allocReg();
				Instruction instr;
				instr.opcode = OP_LOAD_NULL;
				instr.p1 = reg;
				emitInstruction(instr);
			}

			std::string name = stmt.name.lexeme;
			if (!currentNamespace.empty() && name.find("::") == std::string::npos) {
				name = currentNamespace + "::" + name;
			}

			Instruction instr;
			instr.opcode = OP_DEFINE_GLOBAL;
			instr.p1 = reg;
			instr.set_p2p3(makeConstant(RyValue(name)));
			emitInstruction(instr);
			freeRegs(1);
		}
	}

	void Compiler::visitReturnStmt(ReturnStmt &stmt) {
		track(stmt.keyword);
		int finalDest = targetReg;
		if (stmt.value) {
			int resultReg = compileExpression(stmt.value);

			Instruction instr;
			instr.opcode = OP_RETURN;
			instr.p1 = resultReg; // Return the actual result register
			emitInstruction(instr);

		} else {
			// Return null when no value is provided
			Instruction instr;
			instr.opcode = OP_LOAD_NULL;
			instr.p1 = 0; // Return null in R0
			emitInstruction(instr);

			Instruction retInstr;
			retInstr.opcode = OP_RETURN;
			retInstr.p1 = 0;
			emitInstruction(retInstr);
		}
	}

	void Compiler::visitPanicStmt(PanicStmt &stmt) {
		track(stmt.keyword);
		int finalDest = targetReg;
		Instruction instr;
		instr.opcode = OP_PANIC;
		if (stmt.message) {
			instr.p1 = compileExpression(stmt.message);
		} else {
			instr.p1 = 0; // Or a register with null
		}
		emitInstruction(instr);
	}

	void Compiler::visitClassStmt(ClassStmt &stmt) {
		track(stmt.name);
		int finalDest = targetReg;

		// Create the class object
		int classReg = allocReg();
		Instruction classInstr;
		classInstr.opcode = OP_CLASS;
		classInstr.p1 = classReg;
		classInstr.set_p2p3(makeConstant(RyValue(stmt.name.lexeme)));
		emitInstruction(classInstr);

		if (stmt.isAbstract) {
			Instruction verify;
			verify.opcode = OP_VERIFY_ABSTRACT;
			verify.p1 = classReg;
			emitInstruction(verify);
		}

		// Inheritance
		if (stmt.superclass) {
			int superReg = compileExpression(stmt.superclass);
			Instruction inheritInstr;
			inheritInstr.opcode = OP_INHERIT;
			inheritInstr.p1 = classReg;
			inheritInstr.p2 = superReg;
			emitInstruction(inheritInstr);
			freeRegs(1);
		}

		// Methods
		auto enclosingClass = currentClass;
		currentClass = std::make_shared<Frontend::ClassCompiler>();
		currentClass->enclosing = enclosingClass;
		if (stmt.superclass)
			currentClass->hasSuperclass = true;

		for (auto &method: stmt.methods) {
			int methodReg = allocReg();
			targetReg = methodReg;
			compileMethod(method, stmt.name.lexeme);

			Instruction methodInstr;
			methodInstr.opcode = OP_METHOD;
			methodInstr.p1 = classReg;
			methodInstr.p2 = methodReg;
			methodInstr.p3 = (uint8_t) makeConstant(RyValue(method->name.lexeme));
			emitInstruction(methodInstr);

			freeRegs(1);
		}

		// Fields
		for (auto &field: stmt.fields) {
			if (field->isPrivate) {
				Instruction fieldInstr;
				fieldInstr.opcode = OP_DEFINE_PRIVATE_FIELD;
				fieldInstr.p1 = classReg;
				fieldInstr.set_p2p3(makeConstant(RyValue(field->name.lexeme)));
				emitInstruction(fieldInstr);
			}

			int valReg = -1;
			if (field->initializer) {
				valReg = compileExpression(field->initializer);
			} else {
				valReg = allocReg();
				Instruction nullInstr;
				nullInstr.opcode = OP_LOAD_NULL;
				nullInstr.p1 = valReg;
				emitInstruction(nullInstr);
			}

			Instruction setProp;
			setProp.opcode = OP_SET_PROPERTY;
			setProp.p1 = classReg;
			setProp.p2 = (uint8_t) makeConstant(RyValue(field->name.lexeme));
			setProp.p3 = valReg;
			emitInstruction(setProp);

			freeRegs(1);
		}

		currentClass = enclosingClass;

		// Define the class variable
		if (scopeDepth > 0) {
			addLocal(stmt.name);
			int localReg = locals.size() - 1;
			if (classReg != localReg) {
				Instruction move;
				move.opcode = OP_MOVE;
				move.p1 = localReg;
				move.p2 = classReg;
				emitInstruction(move);
			}
		} else {
			std::string name = stmt.name.lexeme;
			if (!currentNamespace.empty() && name.find("::") == std::string::npos) {
				name = currentNamespace + "::" + name;
			}
			Instruction def;
			def.opcode = OP_DEFINE_GLOBAL;
			def.p1 = classReg;
			def.set_p2p3(makeConstant(RyValue(name)));
			emitInstruction(def);
			freeRegs(1);
		}
	}

	void Compiler::visitThis(ThisExpr &expr) {
		if (currentClass == nullptr) {
			error(expr.keyword, "Cannot use 'this' outside of a class.");
			return;
		}
		track(expr.keyword);
		int finalDest = targetReg;
		Token thisToken;
		thisToken.lexeme = "this";
		int reg = resolveLocal(thisToken);
		if (reg != -1) {
			Instruction instr;
			instr.opcode = OP_MOVE;
			instr.p1 = finalDest;
			instr.p2 = reg;
			emitInstruction(instr);
		} else {
			error(expr.keyword, "Could not resolve 'this'.");
		}
	}
	void Compiler::visitGet(GetExpr &expr) {
		track(expr.name);
		int finalDest = targetReg;
		if (auto var = std::dynamic_pointer_cast<VariableExpr>(expr.object)) {
			if (var->name.type == TokenType::PARENT) { // 'parent' is the keyword for super
				if (currentClass == nullptr) {
					error(var->name, "Cannot use 'parent' outside of a class method.");
					return;
				}
				if (!currentClass->hasSuperclass) {
					error(var->name, "Cannot use 'parent' in a class with no superclass.");
					return;
				}

				Token thisToken;
				thisToken.lexeme = "this";
				int thisReg = resolveLocal(thisToken);

				Instruction getSuper;
				getSuper.opcode = OP_GET_SUPER;
				getSuper.p1 = finalDest;
				getSuper.p2 = thisReg;
				getSuper.p3 = (uint8_t) makeConstant(RyValue(expr.name.lexeme));
				emitInstruction(getSuper);
				return;
			}
			if (resolveLocal(var->name) == -1) {
				std::string baseName = var->name.lexeme;

				// If it's in our known namespaces
				if (Backend::Parser::getNamespaces().count(baseName) > 0) {
					std::string mangledName = baseName + "::" + expr.name.lexeme;
					std::cout << "[DEBUG] Compiling Namespace Access: " << mangledName << std::endl;
					Instruction instr;
					instr.opcode = OP_GET_GLOBAL;
					instr.p1 = finalDest;
					instr.set_p2p3(makeConstant(RyValue(mangledName)));
					emitInstruction(instr);
					return;
				}
			}
		}
		int objReg = compileExpression(expr.object);
		Instruction instr;
		instr.opcode = OP_GET_PROPERTY;
		instr.p1 = finalDest;
		instr.p2 = objReg;
		instr.p3 = (uint8_t) makeConstant(RyValue(expr.name.lexeme));
		emitInstruction(instr);
		freeRegs(1);
	}
	void Compiler::visitSet(SetExpr &expr) {
		track(expr.name);
		int finalDest = targetReg;
		int objReg = compileExpression(expr.object);
		int valReg = compileExpression(expr.value);
		Instruction instr;
		instr.opcode = OP_SET_PROPERTY;
		instr.p1 = objReg;
		instr.p2 = (uint8_t) makeConstant(RyValue(expr.name.lexeme));
		instr.p3 = valReg;
		emitInstruction(instr);

		Instruction move;
		move.opcode = OP_MOVE;
		move.p1 = finalDest;
		move.p2 = valReg;
		emitInstruction(move);

		freeRegs(2);
	}
	void Compiler::visitFunctionStmt(FunctionStmt &stmt) {
		track(stmt.name);
		int finalDest = targetReg;

		int funcReg = allocReg();
		targetReg = funcReg;

		Compiler subCompiler(this, this->sourceCode);

		auto function = std::make_shared<Frontend::RyFunction>();
		function->name = stmt.name.lexeme;
		function->arity = stmt.parameters.size();

		subCompiler.compilingChunk = &function->chunk;

		subCompiler.beginScope();
		subCompiler.addLocal(Token());

		for (const auto &param: stmt.parameters) {
			subCompiler.addLocal(param.name);
		}

		for (const auto &bodyStmt: stmt.body) {
			subCompiler.compileStatement(bodyStmt);
		}

		Instruction load_null;
		load_null.opcode = OP_LOAD_NULL;
		load_null.p1 = 0;
		subCompiler.emitInstruction(load_null);

		Instruction ret;
		ret.opcode = OP_RETURN;
		ret.p1 = 0;
		subCompiler.emitInstruction(ret);

		subCompiler.endScope();

		function->upvalueCount = subCompiler.upvalues.size();
		Instruction closure_instr;
		closure_instr.opcode = OP_CLOSURE;
		closure_instr.p1 = funcReg;
		closure_instr.set_p2p3(makeConstant(RyValue(function)));
		emitInstruction(closure_instr);

		for (auto &upvalue: subCompiler.upvalues) {
			Instruction upvalue_instr;
			upvalue_instr.opcode = OP_MOVE; // Opcode is ignored
			upvalue_instr.p1 = upvalue.isLocal ? 1 : 0;
			upvalue_instr.p2 = upvalue.index;
			emitInstruction(upvalue_instr);
		}

		std::string name = stmt.name.lexeme;
		if (!currentNamespace.empty() && name.find("::") == std::string::npos) {
			name = currentNamespace + "::" + name;
		}

		Instruction def;
		def.opcode = OP_DEFINE_GLOBAL;
		def.p1 = funcReg;
		def.set_p2p3(makeConstant(RyValue(name)));
		emitInstruction(def);
		freeRegs(1);
	}
	void Compiler::visitMap(MapExpr &expr) {
		track(expr.braceToken);
		int finalDest = targetReg;

		int size = expr.items.size();
		int startReg = nextReg;
		if (startReg + size * 2 > 255) {
			error(Token(), "Too many elements in map literal.");
			return;
		}
		nextReg += size * 2;

		for (size_t i = 0; i < size; ++i) {
			compileExpression(expr.items[i].first, startReg + i * 2);
			compileExpression(expr.items[i].second, startReg + i * 2 + 1);
		}

		Instruction instr;
		instr.opcode = OP_BUILD_MAP;
		instr.p1 = finalDest;
		instr.p2 = startReg;
		instr.p3 = (uint8_t) size;
		emitInstruction(instr);
		freeRegs(size * 2);
	}
	void Compiler::visitIndexSet(IndexSetExpr &expr) {
		track(expr.bracket);
		int finalDest = targetReg;
		int objReg = compileExpression(expr.object);
		int idxReg = compileExpression(expr.index);
		int valReg = compileExpression(expr.value);

		Instruction instr;
		instr.opcode = OP_SET_INDEX;
		instr.p1 = objReg;
		instr.p2 = idxReg;
		instr.p3 = valReg;
		emitInstruction(instr);

		Instruction move;
		move.opcode = OP_MOVE;
		move.p1 = finalDest;
		move.p2 = valReg;
		emitInstruction(move);

		freeRegs(3);
	}
	void Compiler::visitIndex(IndexExpr &expr) {
		track(expr.bracket);
		int finalDest = targetReg;
		int objReg = compileExpression(expr.object);
		int idxReg = compileExpression(expr.index);

		Instruction instr;
		instr.opcode = OP_GET_INDEX;
		instr.p1 = finalDest;
		instr.p2 = objReg;
		instr.p3 = idxReg;
		emitInstruction(instr);

		freeRegs(2);
	}
	void Compiler::visitBitwiseOr(BitwiseOrExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;
		int leftReg = compileExpression(expr.left);
		int rightReg = compileExpression(expr.right);
		Instruction instr;
		instr.opcode = OP_BITWISE_OR;
		instr.p1 = finalDest;
		instr.p2 = leftReg;
		instr.p3 = rightReg;
		emitInstruction(instr);
		freeRegs(2);
	}
	void Compiler::visitBitwiseXor(BitwiseXorExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;
		int leftReg = compileExpression(expr.left);
		int rightReg = compileExpression(expr.right);
		Instruction instr;
		instr.opcode = OP_BITWISE_XOR;
		instr.p1 = finalDest;
		instr.p2 = leftReg;
		instr.p3 = rightReg;
		emitInstruction(instr);
		freeRegs(2);
	}
	void Compiler::visitBitwiseAnd(BitwiseAndExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;
		int leftReg = compileExpression(expr.left);
		int rightReg = compileExpression(expr.right);
		Instruction instr;
		instr.opcode = OP_BITWISE_AND;
		instr.p1 = finalDest;
		instr.p2 = leftReg;
		instr.p3 = rightReg;
		emitInstruction(instr);
		freeRegs(2);
	}
	void Compiler::visitPrefix(PrefixExpr &expr) {
		track(expr.prefix);
		int finalDest = targetReg;

		// Handle prefix increment/decrement (++x, --x)
		if (expr.prefix.type == TokenType::PLUS_PLUS || expr.prefix.type == TokenType::MINUS_MINUS) {
			// Only supports variables for now
			auto var = std::dynamic_pointer_cast<VariableExpr>(expr.right);
			if (var) {
				int arg = resolveLocal(var->name);
				if (arg != -1) {
					// Load 1 into a register
					int oneReg = allocReg();
					Instruction loadOne;
					loadOne.opcode = OP_LOAD_CONST;
					loadOne.p1 = oneReg;
					loadOne.set_p2p3(makeConstant(RyValue(1.0)));
					emitInstruction(loadOne);

					// Increment/Decrement the local variable
					Instruction op;
					op.opcode = (expr.prefix.type == TokenType::PLUS_PLUS) ? OP_ADD : OP_SUBTRACT;
					op.p1 = arg; // Update local directly
					op.p2 = arg;
					op.p3 = oneReg;
					emitInstruction(op);

					// Move the result to target (prefix returns the new value)
					Instruction move;
					move.opcode = OP_MOVE;
					move.p1 = finalDest;
					move.p2 = arg;
					emitInstruction(move);

					freeRegs(1); // Free the oneReg
					return;
				}

				// Handle upvalues
				int upvalueIdx = resolveUpvalue(var->name);
				if (upvalueIdx != -1) {
					// Load the upvalue value first
					int valReg = allocReg();
					Instruction loadUpvalue;
					loadUpvalue.opcode = OP_LOAD_UPVALUE;
					loadUpvalue.p1 = valReg;
					loadUpvalue.p2 = upvalueIdx;
					emitInstruction(loadUpvalue);

					// Load 1 into another register
					int oneReg = allocReg();
					Instruction loadOne;
					loadOne.opcode = OP_LOAD_CONST;
					loadOne.p1 = oneReg;
					loadOne.set_p2p3(makeConstant(RyValue(1.0)));
					emitInstruction(loadOne);

					// Increment/Decrement
					int resultReg = allocReg();
					Instruction op;
					op.opcode = (expr.prefix.type == TokenType::PLUS_PLUS) ? OP_ADD : OP_SUBTRACT;
					op.p1 = resultReg;
					op.p2 = valReg;
					op.p3 = oneReg;
					emitInstruction(op);

					// Set the upvalue
					Instruction setUpvalue;
					setUpvalue.opcode = OP_SET_UPVALUE;
					setUpvalue.p1 = resultReg;
					setUpvalue.p2 = upvalueIdx;
					emitInstruction(setUpvalue);

					// Move result to target
					Instruction move;
					move.opcode = OP_MOVE;
					move.p1 = finalDest;
					move.p2 = resultReg;
					emitInstruction(move);

					freeRegs(3); // Free valReg, oneReg, resultReg
					return;
				}

				// Handle globals
				std::string name = var->name.lexeme;
				if (name.find("::") == std::string::npos && !currentNamespace.empty()) {
					name = currentNamespace + "::" + name;
				}

				// Get current global value
				int valReg = allocReg();
				Instruction getGlobal;
				getGlobal.opcode = OP_GET_GLOBAL;
				getGlobal.p1 = valReg;
				getGlobal.set_p2p3(makeConstant(RyValue(name)));
				emitInstruction(getGlobal);

				// Load 1
				int oneReg = allocReg();
				Instruction loadOne;
				loadOne.opcode = OP_LOAD_CONST;
				loadOne.p1 = oneReg;
				loadOne.set_p2p3(makeConstant(RyValue(1.0)));
				emitInstruction(loadOne);

				// Increment/Decrement
				int resultReg = allocReg();
				Instruction op;
				op.opcode = (expr.prefix.type == TokenType::PLUS_PLUS) ? OP_ADD : OP_SUBTRACT;
				op.p1 = resultReg;
				op.p2 = valReg;
				op.p3 = oneReg;
				emitInstruction(op);

				// Set global
				Instruction setGlobal;
				setGlobal.opcode = OP_SET_GLOBAL;
				setGlobal.p1 = resultReg;
				setGlobal.set_p2p3(makeConstant(RyValue(name)));
				emitInstruction(setGlobal);

				// Move result to target
				Instruction move;
				move.opcode = OP_MOVE;
				move.p1 = finalDest;
				move.p2 = resultReg;
				emitInstruction(move);

				freeRegs(3); // Free valReg, oneReg, resultReg
				return;
			}
			// If not a variable, error
			error(expr.prefix, "Prefix increment/decrement requires a variable.");
			return;
		}

		// Handle negation and logical not
		int rightReg = compileExpression(expr.right);
		Instruction instr;
		instr.p1 = finalDest;
		instr.p2 = rightReg;
		if (expr.prefix.type == TokenType::MINUS) {
			instr.opcode = OP_NEGATE;
		} else if (expr.prefix.type == TokenType::BANG) {
			instr.opcode = OP_NOT;
		} else {
			// Unknown operator
			freeRegs(1);
			return;
		}
		emitInstruction(instr);
		freeRegs(1);
	}
	void Compiler::visitPostfix(PostfixExpr &expr) {
		track(expr.postfix);
		int finalDest = targetReg;
		// Only supports variables for now
		auto var = std::dynamic_pointer_cast<VariableExpr>(expr.left);
		if (var) {
			int arg = resolveLocal(var->name);
			if (arg != -1) {
				// Load current value to target (result of postfix is old value)
				Instruction move;
				move.opcode = OP_MOVE;
				move.p1 = finalDest;
				move.p2 = arg;
				emitInstruction(move);

				// Increment/Decrement
				int oneReg = allocReg();
				Instruction loadOne;
				loadOne.opcode = OP_LOAD_CONST;
				loadOne.p1 = oneReg;
				loadOne.set_p2p3(makeConstant(RyValue(1.0)));
				emitInstruction(loadOne);

				Instruction op;
				op.opcode = (expr.postfix.type == TokenType::PLUS_PLUS) ? OP_ADD : OP_SUBTRACT;
				op.p1 = arg; // Update local directly
				op.p2 = arg;
				op.p3 = oneReg;
				emitInstruction(op);
				freeRegs(1);
			}
		}
	}
	void Compiler::visitShift(ShiftExpr &expr) {
		track(expr.op_t);
		int finalDest = targetReg;
		int leftReg = compileExpression(expr.left);
		int rightReg = compileExpression(expr.right);
		Instruction instr;
		if (expr.op_t.type == TokenType::LESS_LESS) {
			instr.opcode = OP_LEFT_SHIFT;
		} else {
			instr.opcode = OP_RIGHT_SHIFT;
		}
		instr.p1 = finalDest;
		instr.p2 = leftReg;
		instr.p3 = rightReg;
		emitInstruction(instr);
		freeRegs(2);
	}
	void Compiler::visitStopStmt(StopStmt &stmt) {
		track(stmt.keyword);
		int finalDest = targetReg;
		if (loopStack.empty()) {
			error(stmt.keyword, "Cannot use 'stop' outside of a loop.");
			return;
		}

		// In a register VM, we don't need to pop locals from the stack when jumping out
		// because registers are random access. However, if we were supporting
		// upvalue closing upon scope exit, we would need to handle that here.
		// For now, we just jump.


		Instruction instr;
		instr.opcode = OP_JUMP;
		instr.set_p2p3(0xFFFF);
		emitInstruction(instr);
		loopStack.back().breakJumps.emplace_back(compilingChunk->code.size() - 1);
	}
	void Compiler::visitSkipStmt(SkipStmt &stmt) {
		track(stmt.keyword);
		int finalDest = targetReg;
		if (loopStack.empty()) {
			error(stmt.keyword, "Cannot use 'skip' outside of a loop.");
			return;
		}


		emitLoop(loopStack.back().startIP);
	}
	void Compiler::visitImportStmt(ImportStmt &stmt) {
		// 1. Compile the path expression
		int pathReg = compileExpression(stmt.module);

		// 2. Emit OP_IMPORT to get the module closure.
		// The result (the closure) will be placed in a new register.
		int closureReg = allocReg();
		Instruction import_instr;
		import_instr.opcode = OP_IMPORT;
		import_instr.p1 = closureReg; // Destination
		import_instr.p2 = pathReg; // Source (path)
		emitInstruction(import_instr);

		// 3. Call the module closure to execute its body.
		Instruction call_instr;
		call_instr.opcode = OP_CALL;
		call_instr.p1 = closureReg; // Callee register
		call_instr.p2 = 0; // Arg count
		emitInstruction(call_instr);
		freeRegs(2); // Free the path and closure registers.
	}
	void Compiler::visitAliasStmt(AliasStmt &stmt) {
		track(stmt.name);
		int finalDest = targetReg;
		// Evaluate the expression we are aliasing (e.g., Math.sqrt)
		int valReg = compileExpression(stmt.aliasExpr);

		// Define it in the global map under the NEW name
		Instruction instr;
		instr.opcode = OP_DEFINE_GLOBAL;
		instr.p1 = valReg;
		instr.set_p2p3(makeConstant(RyValue(stmt.name.lexeme)));
		emitInstruction(instr);
		freeRegs(1);
	}
	void Compiler::visitNamespaceStmt(NamespaceStmt &stmt) {
		track(stmt.name);
		int finalDest = targetReg;
		std::string lastNamespace = currentNamespace;
		currentNamespace = stmt.name.lexeme;
		// compile the body
		for (const auto &s: stmt.body) {
			compileStatement(s);
		}
		currentNamespace = lastNamespace;
	}
	void Compiler::visitEachStmt(EachStmt &stmt) {
		track(stmt.id);

		beginScope(); // Single scope for the whole loop construct

		// 1. Compile collection into a hidden local
		Token hiddenCol;
		hiddenCol.lexeme = "(iterator_collection)";
		addLocal(hiddenCol);
		int hiddenColReg = locals.size() - 1;
		compileExpression(stmt.collection, hiddenColReg);

		// 2. Create hidden index local and initialize to 0
		Token hiddenIter;
		hiddenIter.lexeme = "(iterator_index)";
		addLocal(hiddenIter);
		int hiddenIterReg = locals.size() - 1;

		Instruction loadZero;
		loadZero.opcode = OP_LOAD_CONST;
		loadZero.p1 = hiddenIterReg;
		loadZero.set_p2p3(makeConstant(RyValue(0.0)));
		emitInstruction(loadZero);

		// 3. Create the user-visible loop variable local. The VM will populate this.
		addLocal(stmt.id);

		// --- Loop setup ---
		int loopStart = compilingChunk->code.size();

		LoopContext context;
		context.startIP = loopStart;
		context.scopeDepth = scopeDepth;
		context.type = LOOP_EACH;
		loopStack.emplace_back(context);

		// --- Loop condition/iterator ---
		int foreachNextIP = compilingChunk->code.size();
		Instruction nextOp;
		nextOp.opcode = OP_FOR_EACH_NEXT;
		nextOp.p1 = hiddenIterReg; // The VM uses this to find collection (p1-1) and user var (p1+1)
		nextOp.set_p2p3(0xFFFF); // Placeholder jump to exit
		emitInstruction(nextOp);

		// --- Loop body ---
		compileStatement(stmt.body);

		// --- Loop back-jump ---
		emitLoop(loopStart);

		// --- Patch exit and break jumps ---
		patchJump(foreachNextIP);
		for (int jumpLoc: loopStack.back().breakJumps) {
			patchJump(jumpLoc);
		}
		loopStack.pop_back();

		endScope(); // Pops all three locals (collection, index, user var)
	}
	/* void Compiler::visitWhileStmt(WhileStmt &stmt) {
		int loopStart = compilingChunk->code.size();


		LoopContext context = LoopContext();
		context.startIP = loopStart;
		context.scopeDepth = this->scopeDepth;
		context.type = LOOP_WHILE;
		loopStack.emplace_back(context);

		int condReg = compileExpression(stmt.condition);

		int exitJump = emitJump(OP_JUMP_IF_FALSE);
		freeRegs(1);

		compileStatement(stmt.body);
		emitLoop(loopStart);

		patchJump(exitJump);
		compilingChunk->code[exitJump].p1 = condReg;
		for (int location: context.breakJumps) {
			patchJump(location);
		}
		loopStack.pop_back();
	} */
	void Compiler::visitAttemptStmt(AttemptStmt &stmt) {
		// Emit OP_ATTEMPT and a placeholder for the jump to the 'fail' block
		Instruction attempt;
		attempt.opcode = OP_ATTEMPT;
		attempt.set_p2p3(0xFFFF);
		emitInstruction(attempt);
		int jumpToFail = compilingChunk->code.size() - 1;

		// Compile the 'attempt' body
		beginScope();
		for (const auto &s: stmt.attemptBody) {
			compileStatement(s);
		}
		endScope();

		// If we get here, no panic happened. Remove the safety net.
		Instruction endAttempt;
		endAttempt.opcode = OP_END_ATTEMPT;
		emitInstruction(endAttempt);

		// Jump over the 'fail' block
		int skipFail = emitJump(OP_JUMP);

		// Patch the OP_ATTEMPT jump so it lands HERE if a panic occurs
		patchJump(jumpToFail);

		// Handle the error variable
		beginScope();
		addLocal(stmt.error); // The VM pushes the error message; track it here

		// Emit instruction to load the exception into the error variable's register
		Instruction loadErr;
		loadErr.opcode = OP_LOAD_EXCEPTION;
		loadErr.p1 = locals.size() - 1; // The register assigned to stmt.error
		emitInstruction(loadErr);

		for (const auto &s: stmt.failBody) {
			compileStatement(s);
		}

		endScope(); // Pops the error variable
		patchJump(skipFail);
	}
} // namespace RyRuntime
