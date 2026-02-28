//
// Created by ryzon on 1/30/26.
//

#pragma once
#include <set>
#include <vector>
#include "expr.h"
#include "stmt.h"
#include "token.h"

namespace Backend {

	class Parser {
	public:
		Parser(const std::vector<Token> &tokens, std::set<std::string> &aliases, std::string sc) :
				tokens(tokens), externalTypeAliases(aliases), sourceCode(std::move(sc)) {}
		~Parser() = default;
		std::set<std::string> &externalTypeAliases;
		std::vector<std::shared_ptr<Stmt>> parse();

	private:
		int loopDepth = 0;
		std::string sourceCode;
		std::vector<Token> tokens;

		// Position
		int current = 0;
		std::set<std::string> typeAliases;
		Token peek();
		Token next();
		bool isTypeAlias(std::string const &name);
		bool isTypeAlias(std::shared_ptr<Expr> expr);
		Token previous();
		Token consume(TokenType type, const std::string &message);
		std::string currentNamespace = "";
		static std::set<std::string> namespaces;
		bool check(TokenType type);
		[[nodiscard]] bool checkNext(TokenType type) const;
		[[nodiscard]] bool isAtEnd() const;
		bool match(std::initializer_list<TokenType> types);
		void error(const Token &token, const std::string &message);

		// Expression
		std::shared_ptr<Expr> expression();
		std::shared_ptr<Expr> assignment();
		std::shared_ptr<Expr> logicalOr();
		std::shared_ptr<Expr> logicalAnd();
		std::shared_ptr<Expr> equality();
		std::shared_ptr<Expr> comparison();
		std::shared_ptr<Expr> range();
		std::shared_ptr<Expr> shift();
		std::shared_ptr<Expr> bitwiseOr();
		std::shared_ptr<Expr> bitwiseXor();
		std::shared_ptr<Expr> bitwiseAnd();
		std::shared_ptr<Expr> addition();
		std::shared_ptr<Expr> multiplication();
		std::shared_ptr<Expr> baseValue();
		std::shared_ptr<Expr> prefixed();
		std::shared_ptr<Expr> postfixed();
		std::shared_ptr<Expr> finishCall(std::shared_ptr<Expr> callee);

		// Statements
		std::shared_ptr<Stmt> forStatement();
		std::shared_ptr<Stmt> eachStatement();
		std::shared_ptr<Stmt> statement();
		std::shared_ptr<Stmt> declaration();
		std::shared_ptr<Stmt> whileStatement();
		std::shared_ptr<FunctionStmt> functionDeclaration(const std::string &kind);
		std::shared_ptr<Stmt> ImportDeclaration();
		std::shared_ptr<Stmt> AliasDeclaration();
		std::shared_ptr<VarStmt> typeDeclaration(std::optional<Token> prefix = std::nullopt, bool isPrivate = false);
		std::shared_ptr<Stmt> expressionStatement();
		std::shared_ptr<Stmt> returnStatement();
		std::shared_ptr<Stmt> ifStatement();
		std::shared_ptr<Stmt> unlessStatement();
		std::shared_ptr<Stmt> untilStatement();
		std::shared_ptr<Stmt> namespaceStatement();
		std::shared_ptr<Stmt> classStatement();
		std::shared_ptr<Stmt> attemptStatement();
		std::shared_ptr<Stmt> panicStatement();

		std::vector<std::shared_ptr<Stmt>> block();
	};

} // namespace Backend
