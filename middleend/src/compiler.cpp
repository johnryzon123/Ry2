#include "compiler.h"
#include <iostream>
#include <vector>
#include "chunk.h"
#include "func.h"
#include "stmt.h"
#include "token.h"

using namespace Backend;

namespace RyRuntime {
	bool Compiler::compile(const std::vector<std::shared_ptr<Backend::Stmt>> &statements, Chunk *chunk) {
		this->compilingChunk = chunk;
		this->locals.clear();
		this->scopeDepth = 0;

		for (const auto &stmt: statements) {
			compileStatement(stmt);
		}

		emitByte(OP_RETURN);
		return true; // Return false if there's a compilation error
	}

	void Compiler::compileStatement(std::shared_ptr<Backend::Stmt> stmt) {
		if (stmt)
			stmt->accept(*this);
	}

	void Compiler::compileExpression(std::shared_ptr<Backend::Expr> expr) {
		if (expr)
			expr->accept(*this);
	}

	// --- Bytecode Helpers ---

	void Compiler::emitByte(uint8_t byte) { compilingChunk->write(byte, currentLine, currentColumn); }

	void Compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
		emitByte(byte1);
		emitByte(byte2);
	}

	void Compiler::emitConstant(RyValue value) { emitBytes(OP_CONSTANT, (uint8_t) makeConstant(value)); }

	int Compiler::makeConstant(RyValue value) {
		int constant = compilingChunk->addConstant(value);
		if (constant > 255) {
			std::cerr << "Too many constants in one chunk!" << std::endl;
			return 0;
		}
		return constant;
	}

	int Compiler::emitJump(uint8_t instruction) {
		emitByte(instruction);
		emitByte(0xff);
		emitByte(0xff);
		return compilingChunk->code.size() - 2;
	}

	void Compiler::patchJump(int offset) {
		// -2 to adjust for the jump offset itself
		int jump = compilingChunk->code.size() - offset - 2;

		if (jump > UINT16_MAX) {
			std::cerr << "Too much code to jump over!" << std::endl;
		}

		compilingChunk->code[offset] = (jump >> 8) & 0xff;
		compilingChunk->code[offset + 1] = jump & 0xff;
	}

	void Compiler::emitLoop(int loopStart) {
		emitByte(OP_LOOP);

		int offset = compilingChunk->code.size() - loopStart + 2;
		if (offset > UINT16_MAX)
			std::cerr << "Loop body too large!" << std::endl;

		emitByte((offset >> 8) & 0xff);
		emitByte(offset & 0xff);
	}

	// --- Scope Helpers ---

	void Compiler::beginScope() { scopeDepth++; }

	void Compiler::endScope() {
		scopeDepth--;
		// Pop locals that were in this scope
		while (!locals.empty() && locals.back().depth > scopeDepth) {
			emitByte(OP_POP);
			locals.pop_back();
		}
	}

	void Compiler::addLocal(Token name) {
		Local local = {name, scopeDepth, false};
		locals.push_back(local);
	}

	int Compiler::resolveLocal(Token &name) {
		for (int i = locals.size() - 1; i >= 0; i--) {
			Local &local = locals[i];
			if (name.lexeme == local.name.lexeme) {
				return i;
			}
		}
		return -1;
	}

	// --- Visitors ---

	void Compiler::visitMath(MathExpr &expr) {
		currentColumn = expr.op_t.column;
		currentLine = expr.op_t.line;

		compileExpression(expr.left);
		compileExpression(expr.right);

		switch (expr.op_t.type) {
			case Backend::TokenType::PLUS:
				emitByte(OP_ADD);
				break;
			case Backend::TokenType::MINUS:
				emitByte(OP_SUBTRACT);
				break;
			case TokenType::STAR:
				emitByte(OP_MULTIPLY);
				break;
			case TokenType::DIVIDE:
				emitByte(OP_DIVIDE);
				break;
			case TokenType::PERCENT:
				emitByte(OP_MODULO);
				break;
			case TokenType::EQUAL_EQUAL:
				emitByte(OP_EQUAL);
				break;
			case TokenType::BANG_EQUAL:
				emitBytes(OP_EQUAL, OP_NOT);
				break;
			case TokenType::GREATER:
				emitByte(OP_GREATER);
				break;
			case TokenType::GREATER_EQUAL:
				emitBytes(OP_LESS, OP_NOT);
				break;
			case TokenType::LESS:
				emitByte(OP_LESS);
				break;
			case TokenType::LESS_EQUAL:
				emitBytes(OP_GREATER, OP_NOT);
				break;
			default:
				break;
		}
	}

	void Compiler::visitGroup(GroupExpr &expr) { compileExpression(expr.expression); }

	void Compiler::visitVariable(VariableExpr &expr) {
		currentColumn = expr.name.column;
		currentLine = expr.name.line;

		int arg = resolveLocal(expr.name);
		if (arg != -1) {
			emitBytes(OP_GET_LOCAL, (uint8_t) arg);
		} else {
			emitBytes(OP_GET_GLOBAL, (uint8_t) makeConstant(RyValue(expr.name.lexeme)));
		}
	}

	void Compiler::visitValue(ValueExpr &expr) {
		currentColumn = expr.value.column;
		currentLine = expr.value.line;

		if (expr.value.type == TokenType::TRUE) {
			emitByte(OP_TRUE);
		} else if (expr.value.type == TokenType::FALSE) {
			emitByte(OP_FALSE);
		} else if (expr.value.type == TokenType::NULL_TOKEN) {
			emitByte(OP_NULL);
		} else if (expr.value.type == TokenType::NUMBER) {
			double val = std::stod(expr.value.lexeme);
			emitConstant(RyValue(val));
		} else if (expr.value.type == TokenType::STRING) {
			emitConstant(RyValue(expr.value.lexeme));
		}
	}

	void Compiler::visitLogical(LogicalExpr &expr) {
		currentColumn = expr.op_t.column;
		currentLine = expr.op_t.line;

		compileExpression(expr.left);
		if (expr.op_t.type == TokenType::AND) {
			int endJump = emitJump(OP_JUMP_IF_FALSE);
			emitByte(OP_POP);
			compileExpression(expr.right);
			patchJump(endJump);
		} else { // OR
			int elseJump = emitJump(OP_JUMP_IF_FALSE);
			int endJump = emitJump(OP_JUMP);
			patchJump(elseJump);
			emitByte(OP_POP);
			compileExpression(expr.right);
			patchJump(endJump);
		}
	}
	void Compiler::visitRange(RangeExpr &expr) {
		currentColumn = expr.op_t.column;
		currentLine = expr.op_t.line;

		// Compile the start (e.g., 1)
		compileExpression(expr.leftBound);
		// Compile the end (e.g., 10)
		compileExpression(expr.rightBound);

		// Ry Specialty: Tell the VM to build a list from this range
		emitByte(OP_BUILD_RANGE_LIST);
	}
	void Compiler::visitList(ListExpr &expr) {
		for (const auto &element: expr.elements) {
			compileExpression(element);
		}
		// Emit an instruction that knows how many elements to grab from the stack
		emitBytes(OP_BUILD_LIST, (uint8_t) expr.elements.size());
	}

	void Compiler::visitAssign(AssignExpr &expr) {
		compileExpression(expr.value);
		int arg = resolveLocal(expr.name);
		if (arg != -1) {
			emitBytes(OP_SET_LOCAL, (uint8_t) arg);
		} else {
			emitBytes(OP_SET_GLOBAL, (uint8_t) makeConstant(RyValue(expr.name.lexeme)));
		}
	}

	void Compiler::visitCall(CallExpr &expr) {
		compileExpression(expr.callee);
		for (const auto &arg: expr.arguments) {
			compileExpression(arg);
		}
		emitBytes(OP_CALL, (uint8_t) expr.arguments.size());
	}

	void Compiler::visitExpressionStmt(ExpressionStmt &stmt) {
		compileExpression(stmt.expression);
		emitByte(OP_POP);
	}

	void Compiler::visitBlockStmt(BlockStmt &stmt) {
		beginScope();
		for (const auto &s: stmt.statements) {
			compileStatement(s);
		}
		endScope();
	}

	void Compiler::visitIfStmt(IfStmt &stmt) {
		compileExpression(stmt.condition);
		int thenJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);

		compileStatement(stmt.thenBranch);

		int elseJump = emitJump(OP_JUMP);
		patchJump(thenJump);
		emitByte(OP_POP);

		if (stmt.elseBranch) {
			compileStatement(stmt.elseBranch);
		}
		patchJump(elseJump);
	}

	void Compiler::visitWhileStmt(WhileStmt &stmt) {
		int loopStart = compilingChunk->code.size();
		compileExpression(stmt.condition);

		int exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);

		compileStatement(stmt.body);
		emitLoop(loopStart);

		patchJump(exitJump);
		emitByte(OP_POP);
	}

	void Compiler::visitForStmt(ForStmt &stmt) {
		beginScope();
		if (stmt.init)
			compileStatement(stmt.init);

		int loopStart = compilingChunk->code.size();

		int exitJump = -1;
		if (stmt.condition) {
			compileExpression(stmt.condition);
			exitJump = emitJump(OP_JUMP_IF_FALSE);
			emitByte(OP_POP);
		}

		compileStatement(stmt.body);

		if (stmt.increment) {
			compileExpression(stmt.increment);
			emitByte(OP_POP);
		}

		emitLoop(loopStart);

		if (exitJump != -1) {
			patchJump(exitJump);
			emitByte(OP_POP);
		}
		endScope();
	}

	void Compiler::visitVarStmt(VarStmt &stmt) {
		if (stmt.initializer) {
			compileExpression(stmt.initializer);
		} else {
			emitByte(OP_NULL);
		}

		if (scopeDepth > 0) {
			addLocal(stmt.name);
		} else {
			emitBytes(OP_DEFINE_GLOBAL, (uint8_t) makeConstant(RyValue(stmt.name.lexeme)));
		}
	}

	void Compiler::visitReturnStmt(ReturnStmt &stmt) {
		if (stmt.value)
			compileExpression(stmt.value);
		else
			emitByte(OP_NULL);
		emitByte(OP_RETURN);
	}

	void Compiler::visitPrefix(PrefixExpr &expr) {
		compileExpression(expr.right);
		if (expr.prefix.type == TokenType::MINUS)
			emitByte(OP_NEGATE);
		else if (expr.prefix.type == TokenType::BANG)
			emitByte(OP_NOT);
	}

	void Compiler::visitPanicStmt(PanicStmt &stmt) {
		if (stmt.message)
			compileExpression(stmt.message);
		else
			emitByte(OP_NULL);
		emitByte(OP_PANIC);
	}

	void Compiler::visitClassStmt(ClassStmt &stmt) {
		emitBytes(OP_CLASS, (uint8_t) makeConstant(RyValue(stmt.name.lexeme)));
		emitBytes(OP_DEFINE_GLOBAL, (uint8_t) makeConstant(RyValue(stmt.name.lexeme)));
	}

	// --- Placeholders for Linker Satisfaction ---
	void Compiler::visitThis(ThisExpr &expr) { emitBytes(OP_GET_LOCAL, 0); }
	void Compiler::visitGet(GetExpr &expr) {
		compileExpression(expr.object);
		emitBytes(OP_GET_PROPERTY, (uint8_t) makeConstant(RyValue(expr.name.lexeme)));
	}
	void Compiler::visitSet(SetExpr &expr) {
		compileExpression(expr.object);
		compileExpression(expr.value);
		emitBytes(OP_SET_PROPERTY, (uint8_t) makeConstant(RyValue(expr.name.lexeme)));
	}
	void Compiler::visitFunctionStmt(FunctionStmt &stmt) {
		// Setup the new function object
		auto function = std::make_shared<Frontend::RyFunction>();
		function->name = stmt.name.lexeme;
		function->arity = stmt.parameters.size();

		Chunk *mainChunk = compilingChunk;
		compilingChunk = &function->chunk;

		// Compile the function's internal world
		beginScope();
		locals.push_back({Token{TokenType::IDENTIFIER, "", nullptr, 0, 0}, 0, false});

		for (const auto &param: stmt.parameters) {
			addLocal(param.name);
		}
		for (const auto &bodyStmt: stmt.body) {
			compileStatement(bodyStmt);
		}

		// Functions MUST end with their own return (implicitly null if not stated)
		emitByte(OP_NULL);
		emitByte(OP_RETURN);
		endScope();

		// Switch back to Main
		compilingChunk = mainChunk;

		// 1. Push the function as a constant
		emitConstant(RyValue(function));

		// 2. Define it as a global with a NAME OPERAND
		emitBytes(OP_DEFINE_GLOBAL, (uint8_t) makeConstant(RyValue(stmt.name.lexeme)));
	}
	void Compiler::visitMap(MapExpr &expr) {}
	void Compiler::visitIndexSet(IndexSetExpr &expr) {}
	void Compiler::visitIndex(IndexExpr &expr) {}
	void Compiler::visitBitwiseOr(BitwiseOrExpr &expr) {}
	void Compiler::visitBitwiseXor(BitwiseXorExpr &expr) {}
	void Compiler::visitBitwiseAnd(BitwiseAndExpr &expr) {}
	void Compiler::visitPostfix(PostfixExpr &expr) {}
	void Compiler::visitShift(ShiftExpr &expr) {}
	void Compiler::visitStopStmt(StopStmt &stmt) {}
	void Compiler::visitSkipStmt(SkipStmt &stmt) {}
	void Compiler::visitImportStmt(ImportStmt &stmt) {}
	void Compiler::visitAliasStmt(AliasStmt &stmt) {}
	void Compiler::visitNamespaceStmt(NamespaceStmt &stmt) {}
	void Compiler::visitEachStmt(EachStmt &stmt) {
		// Push Collection
		compileExpression(stmt.collection);
		// Push initial Index
		emitConstant(RyValue(0.0));

		Token dummyToken = {TokenType::Nothing_Here, "", nullptr, 0, 0};
		addLocal(dummyToken); // Represents the Collection slot
		addLocal(dummyToken); // Represents the Index slot
		addLocal(dummyToken); // Represents the data

		int loopStart = compilingChunk->code.size();
		int exitJump = emitJump(OP_FOR_EACH_NEXT);

		// Now, when it adds the loop variable, it will be at the
		// stack top where OP_FOR_EACH_NEXT just pushed 'current'!
		beginScope();
		addLocal(stmt.id);

		compileStatement(stmt.body);
		endScope();

		emitLoop(loopStart);
		patchJump(exitJump);

		// Pop the 3 things it pushed (Item, Index, Collection)
		emitBytes(OP_POP, OP_POP);
		emitByte(OP_POP);

		// Remove our two phantom locals from the compiler's tracking
		locals.pop_back();
		locals.pop_back();
	}
	void Compiler::visitAttemptStmt(AttemptStmt &stmt) {
		// Emit OP_ATTEMPT and a placeholder for the jump to the 'fail' block
		int jumpToFail = emitJump(OP_ATTEMPT);

		// Compile the 'attempt' body
		for (const auto &s: stmt.attemptBody) {
			compileStatement(s);
		}

		// If we get here, no panic happened. Remove the safety net.
		emitByte(OP_END_ATTEMPT);

		// Jump over the 'fail' block
		int skipFail = emitJump(OP_JUMP);

		// Patch the OP_ATTEMPT jump so it lands HERE if a panic occurs
		patchJump(jumpToFail);

		// Handle the error variable
		beginScope();
		addLocal(stmt.error); // The VM pushes the error message; track it here

		for (const auto &s: stmt.failBody) {
			compileStatement(s);
		}

		endScope(); // Pops the error variable

		// 7. Patch the skipFail jump so the 'attempt' block finishes here
		patchJump(skipFail);
	}
} // namespace RyRuntime
