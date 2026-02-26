//
// Created by ryzon on 1/30/26.
//


#include "../include/parser.h"
#include <memory>
#include <optional>
#include "../include/tools.h"
#include "common.h"
#include "expr.h"
#include "optimizer.h"
#include "stmt.h"
#include "token.h"

using namespace Backend;

std::vector<std::shared_ptr<Stmt>> Parser::parse() {
	std::vector<std::shared_ptr<Stmt>> statements;

	try {
		while (!isAtEnd()) {
			statements.push_back(declaration());
		}
	} catch (const RyTools::ParseError &error) {
		loopDepth = 0;
		return {};
	} catch (const std::exception &error) {
		loopDepth = 0;
		std::cout << "System Failure: " << error.what() << std::endl;
		return {};
	}

	return statements;
}

Token Parser::peek() { return tokens[current]; }

Token Parser::next() {
	if (!isAtEnd())
		current++;
	return previous();
}

Token Parser::previous() { return tokens[current - 1]; }

bool Parser::isAtEnd() const { return tokens[current].type == TokenType::EOF_TOKEN; }

bool Parser::match(const std::initializer_list<TokenType> types) {
	for (const TokenType type: types) {
		if (check(type)) {
			next();
			return true;
		}
	}
	return false;
}
bool Parser::isTypeAlias(const std::string &name) { return externalTypeAliases.count(name) > 0; }

bool Parser::isTypeAlias(std::shared_ptr<Expr> expr) {
	// Check if the expression is actually a VariableExpr
	auto var = dynamic_cast<VariableExpr *>(expr.get());
	return var != nullptr;
}
bool Parser::check(const TokenType type) {
	if (isAtEnd())
		return false;
	return peek().type == type;
}

bool Parser::checkNext(const TokenType type) const {
	if (isAtEnd())
		return false;
	if (tokens[current + 1].type == TokenType::EOF_TOKEN)
		return false;
	return tokens[current + 1].type == type;
}

Token Parser::consume(const TokenType type, const std::string &message) {
	if (check(type))
		return next();

	error(peek(), message);
	return tokens[current];
}

std::shared_ptr<Expr> Parser::equality() {
	auto expr = comparison();

	while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
		Token op = previous();
		auto right = comparison();
		expr = std::make_shared<MathExpr>(std::move(expr), op, std::move(right));
	}

	return expr;
}

std::shared_ptr<Expr> Parser::logicalAnd() {
	auto expr = equality();
	while (match({TokenType::AND})) {
		Token op = previous();
		auto right = equality();
		expr = std::make_shared<LogicalExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}

std::shared_ptr<Expr> Parser::logicalOr() {
	auto expr = logicalAnd();
	while (match({TokenType::OR})) {
		Token op = previous();
		auto right = logicalAnd();
		expr = std::make_shared<LogicalExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}

std::shared_ptr<Expr> Parser::comparison() {
	auto expr = bitwiseOr();

	while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL})) {
		Token op = previous();
		auto right = bitwiseOr();
		expr = std::make_shared<MathExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}

std::shared_ptr<Expr> Parser::shift() {
	auto expr = addition();

	while (match({TokenType::LESS_LESS, TokenType::GREATER_GREATER})) {
		Token op = previous();
		auto right = addition();
		expr = std::make_shared<ShiftExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}


std::shared_ptr<Expr> Parser::range() {
	auto expr = shift();

	while (match({TokenType::TO})) {
		Token op = previous();
		auto right = shift();
		expr = std::make_shared<RangeExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}

std::shared_ptr<Expr> Parser::multiplication() {
	auto expr = prefixed();

	while (match({TokenType::STAR, TokenType::DIVIDE, TokenType::PERCENT})) {
		Token op = previous();
		auto right = prefixed();

		// Wrap into our MathExpr struct
		expr = std::make_shared<MathExpr>(std::move(expr), op, std::move(right));
	}

	return expr;
}

std::shared_ptr<Expr> Parser::bitwiseOr() {
	auto expr = bitwiseXor();

	while (match({TokenType::PIPE})) {
		Token op = previous();
		auto right = bitwiseXor();
		expr = std::make_shared<BitwiseOrExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}

std::shared_ptr<Expr> Parser::bitwiseXor() {
	auto expr = bitwiseAnd();

	while (match({TokenType::CARET})) {
		Token op = previous();
		auto right = bitwiseAnd();
		expr = std::make_shared<BitwiseXorExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}


std::shared_ptr<Expr> Parser::bitwiseAnd() {
	auto expr = range();

	while (match({TokenType::AMPERSAND})) {
		Token op = previous();
		auto right = range();
		expr = std::make_shared<BitwiseAndExpr>(std::move(expr), op, std::move(right));
	}
	return expr;
}

void Parser::error(const Token &token, const std::string &message) {
	RyTools::report(token.line, token.column, "", message, sourceCode);
	throw RyTools::ParseError();
}
std::shared_ptr<Expr> Parser::expression() {
	auto expr = assignment();

	// Ry's Precalculator
	Optimizer opt;
	return opt.fold(expr);
}

std::shared_ptr<Expr> Parser::assignment() {
	auto expr = logicalOr();

	if (match({TokenType::EQUAL})) {
		Token equals = previous();
		auto value = assignment();

		// Check left side
		if (auto var = dynamic_cast<VariableExpr *>(expr.get())) {
			Token name = var->name;
			return std::make_shared<AssignExpr>(name, std::move(value));
		} else if (auto get = dynamic_cast<GetExpr *>(expr.get())) {
			return std::make_shared<SetExpr>(get->object, get->name, std::move(value));
		} else if (auto index = dynamic_cast<IndexExpr *>(expr.get())) {
			return std::make_shared<IndexSetExpr>(index->object, index->bracket, index->index, value);
		}


		error(equals, "Invalid assignment target.");
	}

	return expr;
}

std::shared_ptr<Expr> Parser::addition() {
	auto expr = multiplication();

	while (match({TokenType::PLUS, TokenType::MINUS})) {
		Token op = previous();
		auto right = multiplication();
		expr = std::make_shared<MathExpr>(std::move(expr), op, std::move(right));
	}

	return expr;
}

std::shared_ptr<Expr> Parser::baseValue() {
	// Handle Literals (Data)
	if (match({TokenType::NUMBER, TokenType::STRING})) {
		return std::make_shared<ValueExpr>(previous());
	}

	// Handle Identifiers
	if (match({TokenType::IDENTIFIER})) {
		return std::make_shared<VariableExpr>(previous());
	}

	// Handle Booleans
	if (match({TokenType::TRUE, TokenType::FALSE, TokenType::NULL_TOKEN})) {
		return std::make_shared<ValueExpr>(previous());
	}

	if (match({TokenType::LBRACKET})) {
		std::vector<std::shared_ptr<Expr>> elements;
		if (!check(TokenType::RBRACKET)) {
			do {
				// Recursion: lists can contain any expression (even other lists!)
				elements.push_back(expression());
			} while (match({TokenType::COMMA}));
		}
		consume(TokenType::RBRACKET, "Expected ']' after list elements.");
		return std::make_shared<ListExpr>(std::move(elements));
	}

	// Handle Grouping (Parentheses)
	if (match({TokenType::LPAREN})) {
		auto expr = expression(); // Jump back to the top of the ladder
		consume(TokenType::RPAREN, "Expected ')' after expression.");
		return std::make_shared<GroupExpr>(std::move(expr));
	}

	// Handle '{'
	if (match({TokenType::LBRACE})) {
		// A vector to hold pairs of expressions
		std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Expr>>> items;

		if (!check(TokenType::RBRACE)) {
			do {
				auto key = expression();
				consume(TokenType::COLON, "Expected ':' after map key.");
				auto value = expression();
				items.push_back({key, value});
			} while (match({TokenType::COMMA})); // Separated by commas!
		}

		Token brace = consume(TokenType::RBRACE, "Expected '}' after map elements.");
		return std::make_shared<MapExpr>(brace, std::move(items));
	}

	if (match({TokenType::THIS})) {
		return std::make_shared<ThisExpr>(previous());
	}

	// Fallback: Report the error instead of crashing
	error(peek(), "Expected a value or '('");
	if (!isAtEnd())
		next();
	return nullptr;
}

std::shared_ptr<Expr> Parser::prefixed() {
	if (match({TokenType::BANG, TokenType::MINUS, TokenType::TILDE, TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
		Token op = previous();
		// We call prefixed() recursively to handle multiple prefixes like "!!true"
		auto right = prefixed();
		return std::make_shared<PrefixExpr>(op, std::move(right));
	}

	return postfixed();
}

std::shared_ptr<Expr> Parser::postfixed() {
	auto expr = baseValue();

	while (true) {
		if (match({TokenType::LPAREN})) {
			expr = finishCall(expr);
		} else if (match({TokenType::LBRACKET})) {
			auto index = expression();
			Token bracket = consume(TokenType::RBRACKET, "Expect ']' after index.");
			expr = std::make_shared<IndexExpr>(std::move(expr), std::move(index), bracket);
		} else if (match({TokenType::DOT})) {
			Token name = consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
			expr = std::make_shared<GetExpr>(expr, name);
		} else if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
			Token op = previous();
			expr = std::make_shared<PostfixExpr>(op, std::move(expr));
		} else {
			break;
		}
	}

	return expr;
}

std::shared_ptr<Expr> Parser::finishCall(std::shared_ptr<Expr> callee) {
	std::vector<std::shared_ptr<Expr>> arguments;

	if (!check(TokenType::RPAREN)) {
		do {
			arguments.push_back(expression());
		} while (match({TokenType::COMMA}));
	}

	Token paren = consume(TokenType::RPAREN, "Expect ')' after arguments.");

	return std::make_shared<CallExpr>(std::move(callee), std::move(arguments), paren);
}

std::shared_ptr<Stmt> Parser::statement() {
	if (match({TokenType::DO}))
		return untilStatement();
	if (match({TokenType::WHILE}))
		return whileStatement();
	if (match({TokenType::FOR}))
		return forStatement();
	if (match({TokenType::IF}))
		return ifStatement();
	if (match({TokenType::RETURN}))
		return returnStatement();
	if (match({TokenType::NAMESPACE}))
		return namespaceStatement();
	if (match({TokenType::STOP})) {
		if (loopDepth == 0) {
			error(previous(), "Cannot use 'stop' outside of a loop.");
		}
		return std::make_shared<StopStmt>(previous());
	}
	if (match({TokenType::SKIP})) {
		if (loopDepth == 0) {
			error(previous(), "Cannot use 'skip' outside of a loop.");
		}
		return std::make_shared<SkipStmt>(previous());
	}
	if (match({TokenType::UNLESS}))
		return unlessStatement();
	if (match({TokenType::LBRACE}))
		return std::make_shared<BlockStmt>(block());
	if (match({TokenType::EACH}))
		return eachStatement();
	if (match({TokenType::CLASS}))
		return classStatement();
	if (match({TokenType::ATTEMPT}))
		return attemptStatement();
	if (match({TokenType::PANIC}))
		return panicStatement();
	return expressionStatement();
}


std::shared_ptr<Stmt> Parser::declaration() {
	if (match({TokenType::IMPORT}))
		return ImportDeclaration();
	if (match({TokenType::FUNC}))
		return functionDeclaration("function");
	if (match({TokenType::ALIAS}))
		return AliasDeclaration();

	// Check for the alias before anything else
	if (check(TokenType::IDENTIFIER) && checkNext(TokenType::DOT)) {
		if (tokens[current + 2].type == TokenType::IDENTIFIER && tokens[current + 3].type == TokenType::IDENTIFIER) {
			Token namespaceToken = next(); // 'Math'
			next(); // '.'

			return typeDeclaration(namespaceToken);
		}
	}
	if (check(TokenType::IDENTIFIER) && isTypeAlias(peek().lexeme)) {
		next(); // Consume the alias name (e.g., 'num')
		return typeDeclaration();
	}

	if (match({TokenType::DATA}))
		return typeDeclaration();

	return statement();
}

std::shared_ptr<FunctionStmt> Parser::functionDeclaration(const std::string &kind) {
	Token name = consume(TokenType::IDENTIFIER, "Expect " + kind + " name.");
	consume(TokenType::LPAREN, "Expect '(' before parameters");

	std::vector<Parameter> parameters;

	if (!check(TokenType::RPAREN)) {
		do {
			Token typeToken(TokenType::DATA, "data", RyValue(), peek().line, peek().column);

			// Check for 'data' or a type alias
			if (match({TokenType::DATA})) {
				typeToken = previous();
			} else if (isTypeAlias(peek().lexeme)) {
				typeToken = next();
			}

			// Check for optional '::type'
			if (match({TokenType::DOUBLE_COLON})) {
				typeToken = consume(TokenType::IDENTIFIER, "Expect type after '::'.");
			}

			// The actual variable name
			Token paramName = consume(TokenType::IDENTIFIER, "Expect parameter name.");
			std::shared_ptr<Expr> defaultVal = nullptr;

			// Default values (Optional)
			if (match({TokenType::EQUAL})) {
				defaultVal = expression();
			}
			parameters.push_back({paramName, typeToken, defaultVal});
		} while (match({TokenType::COMMA}));
	}

	consume(TokenType::RPAREN, "Expect ')' after parameters.");
	std::optional<Token> returnTypeNamespace = std::nullopt;
	std::optional<Token> returnTypeAlias = std::nullopt;

	if (match({TokenType::LARROW})) {
		if (check(TokenType::IDENTIFIER) && checkNext(TokenType::DOT)) {
			returnTypeNamespace = next(); // namespace name
			next(); // '.'
			returnTypeAlias = consume(TokenType::IDENTIFIER, "Expect return type after '.'.");
		} else {
			returnTypeAlias = consume(TokenType::IDENTIFIER, "Expect return type after '->'.");
		}
	}
	consume(TokenType::LBRACE, "Expect '{' before " + kind + " body.");

	// Use std::move for the body to ensure the vector is passed correctly
	std::vector<std::shared_ptr<Stmt>> body = block();

	return std::make_shared<FunctionStmt>(name, parameters, std::move(body), std::move(returnTypeNamespace),
																				std::move(returnTypeAlias));
}

std::shared_ptr<Stmt> Parser::ImportDeclaration() {
	consume(TokenType::LPAREN, "Expect '(' after import.");
	std::shared_ptr<Expr> module = expression();
	consume(TokenType::RPAREN, "Expect ')' after import.");
	return std::make_shared<ImportStmt>(module);
}

std::shared_ptr<Stmt> Parser::whileStatement() {
	loopDepth++;
	if (check(TokenType::LBRACE)) {
		error(previous(), "Expect condition before '{'.");
	}
	auto condition = expression();

	auto body = statement();
	loopDepth--;
	return std::make_shared<WhileStmt>(std::move(condition), body);
}

std::shared_ptr<Stmt> Parser::forStatement() {
	loopDepth++;

	std::shared_ptr<Stmt> initializer = nullptr;
	if (check(TokenType::LBRACE)) {
		error(previous(), "Expect condition before '{'.");
	}
	if (match({TokenType::DATA})) {
		initializer = typeDeclaration();
	} else if (!check(TokenType::COMMA)) {
		initializer = expressionStatement();
	}

	consume(TokenType::COMMA, "Expect ',' after loop initializer.");

	std::shared_ptr<Expr> condition = nullptr;
	if (!check(TokenType::COMMA)) {
		condition = expression();
	}
	consume(TokenType::COMMA, "Expect ',' after loop condition.");

	std::shared_ptr<Expr> increment = nullptr;
	if (!check(TokenType::RBRACE)) {
		increment = expression();
	}

	std::shared_ptr<Stmt> body = statement();

	loopDepth--;
	return std::make_shared<ForStmt>(std::move(initializer), std::move(condition), std::move(increment), body);
}

std::shared_ptr<Stmt> Parser::eachStatement() {
	loopDepth++;
	Token typeToken(TokenType::Nothing_Here, "", RyValue(), 0, 0); // Rename to avoid confusion

	consume(TokenType::DATA, "Expect 'data' in each loop.");

	if (match({TokenType::DOUBLE_COLON})) {
		typeToken = consume(TokenType::IDENTIFIER, "Expect type name after '::'.");
	}

	Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");
	consume(TokenType::IN, "Expect 'in' after variable name.");

	auto iterable = expression();

	auto body = statement();
	loopDepth--;

	if (typeToken.type == TokenType::Nothing_Here) {
		return std::make_shared<EachStmt>(name, iterable, body);
	} else {
		return std::make_shared<EachStmt>(name, iterable, body, typeToken);
	}
}

std::shared_ptr<Stmt> Parser::AliasDeclaration() {
	std::shared_ptr<Expr> aliasExpr;

	// Check if we are aliasing a raw data type (data::num)
	if (match({TokenType::DATA})) {
		consume(TokenType::DOUBLE_COLON, "Expect '::' after data");
		Token type = consume(TokenType::IDENTIFIER, "Expect type name");
		aliasExpr = std::make_shared<VariableExpr>(type); // Wrap the type name
	}
	// Check if we are aliasing an EXISTING type alias (num as integer)
	else if (check(TokenType::IDENTIFIER) && isTypeAlias(peek().lexeme)) {
		aliasExpr = std::make_shared<VariableExpr>(next());
	}
	// Otherwise, it's a normal variable/function alias
	else {
		aliasExpr = expression();
	}

	consume(TokenType::AS, "Expect 'as' after target.");
	Token name = consume(TokenType::IDENTIFIER, "Expect alias name.");

	if (isTypeAlias(aliasExpr)) {
		typeAliases.insert(name.lexeme);
	}

	return std::make_shared<AliasStmt>(aliasExpr, name);
}

std::shared_ptr<VarStmt> Parser::typeDeclaration(std::optional<Token> prefix, bool isPrivate) {
	Token typeToken = prefix.has_value() ? prefix.value() : previous();
	if (prefix.has_value()) {
		typeToken = prefix.value();
	} else if (check(TokenType::DATA) || isTypeAlias(peek().lexeme)) {
		typeToken = next();
	} else {
		typeToken = previous();
	}


	std::shared_ptr<Expr> initializer = nullptr;
	std::optional<Token> innerTypeToken = std::nullopt;
	Token name = Token(TokenType::Nothing_Here, "", RyValue(), 0, 0);

	if (match({TokenType::DOUBLE_COLON})) {
		innerTypeToken = consume(TokenType::IDENTIFIER, "Expect type after '::'.");
	}

	name = consume(TokenType::IDENTIFIER, "Expect variable name.");


	if (match({TokenType::EQUAL})) {
		initializer = expression();
	}


	return std::make_shared<VarStmt>(typeToken, innerTypeToken, name, initializer, isPrivate);
}
std::shared_ptr<Stmt> Parser::expressionStatement() {
	auto expr = expression();
	return std::make_shared<ExpressionStmt>(std::move(expr));
}

std::shared_ptr<Stmt> Parser::returnStatement() {
	Token keyword = previous(); // This is the 'return' token
	std::shared_ptr<Expr> value = nullptr;

	value = expression();
	return std::make_shared<ReturnStmt>(keyword, std::move(value));
}

std::shared_ptr<Stmt> Parser::ifStatement() {
	if (check(TokenType::LBRACE)) {
		error(previous(), "Expect condition before '{'.");
	}
	auto condition = expression();
	if (!check(TokenType::LBRACE)) {
		error(previous(), "Expect '{' after if condition.");
	}
	auto thenBranch = statement();
	std::shared_ptr<Stmt> elseBranch = nullptr;

	if (match({TokenType::ELSE})) {
		elseBranch = statement();
	}

	return std::make_shared<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::shared_ptr<Stmt> Parser::unlessStatement() {
	Token op = previous();
	op.type = TokenType::BANG;
	op.lexeme = "!";

	if (check(TokenType::LBRACE)) {
		error(previous(), "Expect condition before '{'.");
	}
	auto condition = expression();
	auto flippedCondition = std::make_shared<PrefixExpr>(op, std::move(condition));

	if (!check(TokenType::LBRACE)) {
		error(previous(), "Expect '{' after unless condition.");
	}

	auto thenBranch = statement();
	std::shared_ptr<Stmt> elseBranch = nullptr;

	if (match({TokenType::ELSE})) {
		elseBranch = statement();
	}

	return std::make_shared<IfStmt>(std::move(flippedCondition), std::move(thenBranch), std::move(elseBranch));
}

std::shared_ptr<Stmt> Parser::untilStatement() {
	loopDepth++;

	// Parse the body of the 'do' block
	auto body = statement();

	consume(TokenType::UNTIL, "Expect 'until' after do block.");

	// Flip the condition
	Token op = previous();
	op.type = TokenType::BANG;
	op.lexeme = "!";

	if (isAtEnd() || peek().type == TokenType::EOF_TOKEN) {
		error(previous(), "Expect condition after 'until'.");
	}
	auto condition = expression();
	auto flippedCondition = std::make_shared<PrefixExpr>(op, std::move(condition));

	loopDepth--;

	// Create a while loop that uses the SAME body
	auto whileLoop = std::make_shared<WhileStmt>(std::move(flippedCondition), body);

	// Wrap them in a list of statements
	std::vector<std::shared_ptr<Stmt>> statements;
	statements.push_back(body); // Run once first
	statements.push_back(whileLoop); // Then check the loop

	// Return them as a single Block statement
	return std::make_shared<BlockStmt>(std::move(statements));
}


std::shared_ptr<Stmt> Parser::namespaceStatement() {
	Token name = consume(TokenType::IDENTIFIER, "Expect namespace name.");
	consume(TokenType::LBRACE, "Expect '{' after namespace body.");
	std::vector<std::shared_ptr<Stmt>> body = block();
	return std::make_shared<NamespaceStmt>(name, body);
}

std::shared_ptr<Stmt> Parser::classStatement() {
	std::vector<std::shared_ptr<FunctionStmt>> methods;
	std::vector<std::shared_ptr<VarStmt>> fields;
	bool isPrivate = false;
	std::shared_ptr<VariableExpr> superclass = nullptr;

	Token name = consume(TokenType::IDENTIFIER, "Expect class name.");
	if (match({TokenType::CHILDOF})) {
		consume(TokenType::IDENTIFIER, "Expect superclass name after 'childof'.");
		superclass = std::make_shared<VariableExpr>(previous());
	}
	consume(TokenType::LBRACE, "Expect '{' before class body.");


	while (!check(TokenType::RBRACE) && !isAtEnd()) {
		bool memberIsPrivate = match({TokenType::PRIVATE});

		if (match({TokenType::FUNC})) {
			auto method = functionDeclaration("method");
			method->isPrivate = memberIsPrivate;
			methods.push_back(method);
		} else if (check(TokenType::DATA) || (check(TokenType::IDENTIFIER) && isTypeAlias(peek().lexeme))) {
			auto field = typeDeclaration(std::nullopt, memberIsPrivate);
			fields.push_back(field);
		} else {
			error(peek(), "Expect 'func' or 'data' inside class body.");
		}
	}

	consume(TokenType::RBRACE, "Expect '}' after class body.");
	return std::make_shared<ClassStmt>(name, std::move(methods), std::move(fields), isPrivate, superclass);
}

std::shared_ptr<Stmt> Parser::attemptStatement() {
	std::vector<std::shared_ptr<Stmt>> attemptBody;
	std::vector<std::shared_ptr<Stmt>> failBody;
	Token error = Token(TokenType::Nothing_Here, "", RyValue(), 0, 0);
	std::vector<std::shared_ptr<Stmt>> finallyBody;
	Token errorType = Token(TokenType::Nothing_Here, "", RyValue(), 0, 0);

	consume(TokenType::LBRACE, "Expect '{' before attempt block.");
	attemptBody = block();
	if (match({TokenType::FAIL})) {
		error = consume(TokenType::IDENTIFIER, "Expect error name after 'fail'");
		if (match({TokenType::DOUBLE_COLON})) {
			errorType = consume(TokenType::IDENTIFIER, "Expect error type after '::'.");
		}
		consume(TokenType::LBRACE, "Expect '{' before fail block");
		failBody = block();
	}
	if (match({TokenType::FINALLY})) {
		consume(TokenType::LBRACE, "Expect '{' before finally block.");
		finallyBody = block();
	}
	return std::make_shared<AttemptStmt>(std::move(attemptBody), std::move(failBody), error, finallyBody, errorType);
}

std::shared_ptr<Stmt> Parser::panicStatement() {
	Token keyword = previous();
	std::shared_ptr<Expr> value = nullptr;
	if (!check(TokenType::RBRACE) && !isAtEnd())
		value = expression();
	return std::make_shared<PanicStmt>(keyword, value);
}

std::vector<std::shared_ptr<Stmt>> Parser::block() {
	std::vector<std::shared_ptr<Stmt>> statements;
	while (!check(TokenType::RBRACE) && !isAtEnd()) {
		statements.push_back(declaration());
	}
	consume(TokenType::RBRACE, "Expect '}' after block.");
	return statements;
}
