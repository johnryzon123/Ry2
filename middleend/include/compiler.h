#ifndef ry_compiler_h
#define ry_compiler_h

#include <unordered_set>
#include "chunk.h"
#include "expr.h"
#include "native.hpp"
#include "stmt.h"
#include "token.h"
#include "tools.h"

namespace Frontend {
	class ClassCompiler;
}

namespace RyRuntime {
	struct Local {
		Backend::Token name; // The name of the local
		int depth;
		bool isCaptured = false;

		Local(Backend::Token n, int d, bool c = false) : name(n), depth(d), isCaptured(c) {}
	};
	struct Upvalue {
		uint8_t index;
		bool isLocal;
	};
	enum LoopType { LOOP_WHILE, LOOP_FOR, LOOP_EACH };
	struct LoopContext {
		int startIP;
		std::vector<int> breakJumps;
		int scopeDepth;
		LoopType type;
	};

	class Compiler : public Backend::ExprVisitor, public Backend::StmtVisitor {
	public:
		Compiler *enclosing = nullptr;
		Compiler(Compiler *enclosing, const std::string &source) : enclosing(enclosing), sourceCode(source) {
			RyTools::hadError = false;
			for (const auto &name: getNativeNames()) {
				nativeNames.insert(name);
			}
		}
		// Main entry point: takes source and returns a compiled chunk
		bool compile(const std::vector<std::shared_ptr<Backend::Stmt>> &statements, Chunk *chunk);

	private:
		// Error reporting
		int currentLine;
		int currentColumn;
		void error(const Backend::Token &token, const std::string &message);
		std::string sourceCode;
		void track(Backend::Token token);

		// --- Visitors ---
		void visitMath(Backend::MathExpr &expr);
		void visitGroup(Backend::GroupExpr &expr);
		void visitVariable(Backend::VariableExpr &expr);
		void visitValue(Backend::ValueExpr &expr);
		void visitLogical(Backend::LogicalExpr &expr);
		void visitAssign(Backend::AssignExpr &expr);
		void visitCall(Backend::CallExpr &expr);
		void visitThis(Backend::ThisExpr &expr);
		void visitGet(Backend::GetExpr &expr);
		void visitMap(Backend::MapExpr &expr);
		void visitRange(Backend::RangeExpr &expr);
		void visitSet(Backend::SetExpr &expr);
		void visitIndexSet(Backend::IndexSetExpr &expr);
		void visitIndex(Backend::IndexExpr &expr);
		void visitList(Backend::ListExpr &expr);
		void visitBitwiseOr(Backend::BitwiseOrExpr &expr);
		void visitBitwiseXor(Backend::BitwiseXorExpr &expr);
		void visitBitwiseAnd(Backend::BitwiseAndExpr &expr);
		void visitPrefix(Backend::PrefixExpr &expr);
		void visitPostfix(Backend::PostfixExpr &expr);
		void visitShift(Backend::ShiftExpr &expr);
		void visitExpressionStmt(Backend::ExpressionStmt &stmt);
		void visitStopStmt(Backend::StopStmt &stmt);
		void visitSkipStmt(Backend::SkipStmt &stmt);
		void visitImportStmt(Backend::ImportStmt &stmt);
		void visitAliasStmt(Backend::AliasStmt &stmt);
		void visitNamespaceStmt(Backend::NamespaceStmt &stmt);
		void visitEachStmt(Backend::EachStmt &stmt);
		void visitBlockStmt(Backend::BlockStmt &stmt);
		void visitReturnStmt(Backend::ReturnStmt &stmt);
		void visitForStmt(Backend::ForStmt &stmt);
		void visitAttemptStmt(Backend::AttemptStmt &stmt);
		void visitPanicStmt(Backend::PanicStmt &stmt);
		void visitIfStmt(Backend::IfStmt &stmt);
		void visitWhileStmt(Backend::WhileStmt &stmt);
		void visitClassStmt(Backend::ClassStmt &stmt);
		void visitFunctionStmt(Backend::FunctionStmt &stmt);
		void visitVarStmt(Backend::VarStmt &stmt);

		// Instruction helpers
		void emitInstruction(Instruction instr);
		int makeConstant(RyValue value);

		// Jump helpers
		int emitJump(OpCode op);
		void patchJump(int offset);
		void emitLoop(int loopStart);

		Chunk *compilingChunk;
		std::shared_ptr<Frontend::ClassCompiler> currentClass = nullptr;
		void compileStatement(std::shared_ptr<Backend::Stmt> stmt);
		// compileExpression now returns the register where the result is stored.
		// A value of -1 indicates the expression does not produce a value that needs a register.
		int compileExpression(std::shared_ptr<Backend::Expr> expr);
		void compileExpression(std::shared_ptr<Backend::Expr> expr, int destReg); // Compile into a specific register
		void compileMethod(std::shared_ptr<Backend::FunctionStmt> stmt, const std::string &ownerClassName);


		// Scope & Locals
		std::vector<Local> locals;
		std::string currentNamespace;
		int scopeDepth = 0;
		void beginScope();
		void endScope();
		int resolveLocal(Backend::Token &name);
		int resolveUpvalue(Backend::Token &name);
		void addLocal(Backend::Token name);

		// Register allocation
		int targetReg = 0;
		int nextReg = 0;
		std::vector<int> freeList;
		std::vector<int> allocationStack;
		int allocReg();
		void freeRegs(int count);
		int addUpvalue(uint8_t index, bool isLocal);
		std::unordered_set<std::string> nativeNames;
		std::vector<Upvalue> upvalues;


		// Stack helpers
		std::vector<LoopContext> loopStack;
	};
} // namespace RyRuntime

#endif
