//
// Created by ryzon on 1/30/26.
//

#pragma once
#include <optional>
#include <utility>
#include <vector>
#include "expr.h"
#include "token.h"


namespace Backend {
	struct Parameter {
		Token name;
		Token type;
		std::shared_ptr<Expr> defaultValue;
	};

	struct ExpressionStmt;
	struct FunctionStmt;
	struct ClassStmt;
	struct ImportStmt;
	struct AliasStmt;
	struct VarStmt;
	struct ReturnStmt;
	struct IfStmt;
	struct WhileStmt;
	struct BlockStmt;
	struct NamespaceStmt;
	struct EachStmt;
	struct StopStmt;
	struct SkipStmt;
	struct ForStmt;
	struct AttemptStmt;
	struct StmtVisitor;
	struct PanicStmt;


	struct Stmt : public std::enable_shared_from_this<Stmt> {
		virtual ~Stmt() = default;
		virtual void accept(StmtVisitor &visitor) = 0;
	};
	struct StmtVisitor {
		virtual ~StmtVisitor() = default;

		virtual void visitExpressionStmt(ExpressionStmt &stmt) = 0;
		virtual void visitFunctionStmt(FunctionStmt &stmt) = 0;
		virtual void visitImportStmt(ImportStmt &stmt) = 0;
		virtual void visitAliasStmt(AliasStmt &stmt) = 0;
		virtual void visitVarStmt(VarStmt &stmt) = 0;
		virtual void visitReturnStmt(ReturnStmt &stmt) = 0;
		virtual void visitWhileStmt(WhileStmt &stmt) = 0;
		virtual void visitIfStmt(IfStmt &stmt) = 0;
		virtual void visitBlockStmt(BlockStmt &stmt) = 0;
		virtual void visitNamespaceStmt(NamespaceStmt &stmt) = 0;
		virtual void visitEachStmt(EachStmt &stmt) = 0;
		virtual void visitStopStmt(StopStmt &stmt) = 0;
		virtual void visitSkipStmt(SkipStmt &stmt) = 0;
		virtual void visitForStmt(ForStmt &stmt) = 0;
		virtual void visitClassStmt(ClassStmt &stmt) = 0;
		virtual void visitAttemptStmt(AttemptStmt &stmt) = 0;
		virtual void visitPanicStmt(PanicStmt &stmt) = 0;
	};


	struct ExpressionStmt : public Stmt {
		std::shared_ptr<Expr> expression;
		explicit ExpressionStmt(std::shared_ptr<Expr> expr) : expression(std::move(expr)) {}

		void accept(StmtVisitor &visitor) override { visitor.visitExpressionStmt(*this); }
	};

	struct FunctionStmt : public Stmt {
		Token name;

		std::vector<Parameter> parameters;
		std::vector<std::shared_ptr<Stmt>> body;
		std::optional<Token> returnTypeNamespace;
		std::optional<Token> returnTypeAlias;
		bool isPrivate = false; // member for Classes


		FunctionStmt(Token n, std::vector<Parameter> p, std::vector<std::shared_ptr<Stmt>> b, std::optional<Token> rTypeNs,
								 std::optional<Token> rTypeAlias) :
				name(std::move(n)), parameters(std::move(p)), body(std::move(b)), returnTypeNamespace(std::move(rTypeNs)),
				returnTypeAlias(std::move(rTypeAlias)) {}
		void accept(StmtVisitor &visitor) override { visitor.visitFunctionStmt(*this); }
	};

	struct ImportStmt : public Stmt {
		std::shared_ptr<Expr> module;

		explicit ImportStmt(std::shared_ptr<Expr> m) : module(std::move(m)) {}
		void accept(StmtVisitor &visitor) override { visitor.visitImportStmt(*this); }
	};

	struct AliasStmt : public Stmt {
		std::shared_ptr<Expr> aliasExpr;
		Token name;

		AliasStmt(std::shared_ptr<Expr> a, Token n) : name(n), aliasExpr(std::move(a)) {}
		void accept(StmtVisitor &visitor) override { visitor.visitAliasStmt(*this); }
	};

	struct VarStmt : public Stmt {
		Token type; // can be a type alias or 'data'
		std::optional<Token> innerType; // data type
		Token name; // variable name
		std::shared_ptr<Expr> initializer; // variable data
		bool isPrivate = false; // member for Classes


		VarStmt(Token type, std::optional<Token> innerType, Token name, std::shared_ptr<Expr> initializer,
						bool isPrivate = false) :
				type(std::move(type)), innerType(std::move(innerType)), name(std::move(name)),
				initializer(std::move(initializer)), isPrivate(isPrivate) {}

		void accept(StmtVisitor &visitor) override { visitor.visitVarStmt(*this); }
	};

	struct ReturnStmt : public Stmt {
		Token keyword;
		std::shared_ptr<Expr> value; // Can be nullptr for 'return;'

		ReturnStmt(Token keyword, std::shared_ptr<Expr> value) : keyword(std::move(keyword)), value(std::move(value)) {}

		void accept(StmtVisitor &visitor) override { visitor.visitReturnStmt(*this); }
	};

	struct IfStmt : public Stmt {
		std::shared_ptr<Expr> condition;
		std::shared_ptr<Stmt> thenBranch;
		std::shared_ptr<Stmt> elseBranch; // Can be nullptr

		IfStmt(std::shared_ptr<Expr> condition, std::shared_ptr<Stmt> thenBranch, std::shared_ptr<Stmt> elseBranch) :
				condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}

		void accept(StmtVisitor &visitor) override { visitor.visitIfStmt(*this); }
	};

	struct WhileStmt : public Stmt {
		std::shared_ptr<Expr> condition;
		std::shared_ptr<Stmt> body;

		WhileStmt(std::shared_ptr<Expr> condition, std::shared_ptr<Stmt> body) :
				condition(std::move(condition)), body(std::move(body)) {}

		void accept(StmtVisitor &visitor) override { visitor.visitWhileStmt(*this); }
	};

	struct BlockStmt : public Stmt {
		std::vector<std::shared_ptr<Stmt>> statements;

		BlockStmt(std::vector<std::shared_ptr<Stmt>> statements) : statements(std::move(statements)) {}

		void accept(StmtVisitor &visitor) override { visitor.visitBlockStmt(*this); }
	};

	struct NamespaceStmt : public Stmt {
		Token name;
		std::vector<std::shared_ptr<Stmt>> body;

		NamespaceStmt(Token n, std::vector<std::shared_ptr<Stmt>> b) : name(std::move(n)), body(std::move(b)) {}
		void accept(StmtVisitor &visitor) override { visitor.visitNamespaceStmt(*this); }
	};
	struct EachStmt : public Stmt {
		Token id;
		std::optional<Token> dataType = std::nullopt;
		std::shared_ptr<Expr> collection;
		std::shared_ptr<Stmt> body;
		EachStmt(Token id, std::shared_ptr<Expr> collection, std::shared_ptr<Stmt> body,
						 std::optional<Token> dataType = std::nullopt) :
				id(std::move(id)), collection(std::move(collection)), body(std::move(body)), dataType(std::move(dataType)) {}
		void accept(StmtVisitor &visitor) override { visitor.visitEachStmt(*this); }
	};
	struct StopStmt : public Stmt {
		Token keyword;
		StopStmt(Token keyword) : keyword(keyword) {}
		void accept(StmtVisitor &visitor) override { visitor.visitStopStmt(*this); }
	};

	struct SkipStmt : public Stmt {
		Token keyword;
		SkipStmt(Token keyword) : keyword(keyword) {}
		void accept(StmtVisitor &visitor) override { visitor.visitSkipStmt(*this); }
	};
	struct ForStmt : public Stmt {
		std::shared_ptr<Stmt> init;
		std::shared_ptr<Expr> condition;
		std::shared_ptr<Expr> increment;
		std::shared_ptr<Stmt> body;

		ForStmt(std::shared_ptr<Stmt> init, std::shared_ptr<Expr> condition, std::shared_ptr<Expr> increment,
						std::shared_ptr<Stmt> body) :
				init(std::move(init)), condition(std::move(condition)), increment(std::move(increment)), body(std::move(body)) {
		}

		void accept(StmtVisitor &visitor) override { visitor.visitForStmt(*this); }
	};
	class ClassStmt : public Stmt {
	public:
		Token name;
		std::vector<std::shared_ptr<FunctionStmt>> methods;
		std::vector<std::shared_ptr<VarStmt>> fields;
		std::shared_ptr<VariableExpr> superclass = nullptr;
		bool isPrivate = false;

		ClassStmt(Token name, std::vector<std::shared_ptr<FunctionStmt>> methods,
							std::vector<std::shared_ptr<VarStmt>> fields, bool isPrivate,
							std::shared_ptr<VariableExpr> superclass = nullptr) :
				name(name), methods(std::move(methods)), fields(std::move(fields)), isPrivate(isPrivate),
				superclass(std::move(superclass)) {}
		void accept(StmtVisitor &visitor) override { visitor.visitClassStmt(*this); }
	};
	struct AttemptStmt : public Stmt {
		std::vector<std::shared_ptr<Stmt>> attemptBody;
		std::vector<std::shared_ptr<Stmt>> failBody;
		Token errorType;
		Token error;
		std::vector<std::shared_ptr<Stmt>> finallyBody;

		AttemptStmt(std::vector<std::shared_ptr<Stmt>> aBody, std::vector<std::shared_ptr<Stmt>> fBody, Token e,
								std::vector<std::shared_ptr<Stmt>> fiBody, Token errorType) :
				attemptBody(aBody), failBody(fBody), error(e), finallyBody(std::move(fiBody)), errorType(errorType) {}
		void accept(StmtVisitor &visitor) override { visitor.visitAttemptStmt(*this); }
	};
	struct PanicStmt : public Stmt {
		Token keyword;
		std::shared_ptr<Expr> message; // The message to throw

		PanicStmt(Token keyword, std::shared_ptr<Expr> value) : keyword(keyword), message(std::move(value)) {}

		void accept(StmtVisitor &visitor) override { visitor.visitPanicStmt(*this); }
	};
} // namespace Backend
