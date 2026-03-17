#include "optimizer.h"

using namespace Backend;

void Optimizer::visitMath(MathExpr &expr) {
	// Dig deeper first
	auto left = fold(expr.left);
	auto right = fold(expr.right);

	// Try to case to see if they are constants
	auto lVal = std::dynamic_pointer_cast<ValueExpr>(left);
	auto rVal = std::dynamic_pointer_cast<ValueExpr>(right);
	// Right-hand side identity check
	if (rVal && rVal->value.type == TokenType::NUMBER) {
		double val = std::stod(rVal->value.lexeme);
		if ((expr.op_t.type == TokenType::PLUS || expr.op_t.type == TokenType::MINUS) && val == 0) {
			lastFolded = left;
			return;
		}

		if ((expr.op_t.type == TokenType::STAR || expr.op_t.type == TokenType::DIVIDE) && val == 1) {
			lastFolded = left;
			return;
		}
	}

	// If both are numbers, precompute
	if (lVal && rVal && lVal->value.type == TokenType::NUMBER && rVal->value.type == TokenType::NUMBER) {
		double ld = std::stod(lVal->value.lexeme);
		double rd = std::stod(rVal->value.lexeme);
		double result = 0;

		switch (expr.op_t.type) {
			case TokenType::PLUS:
				result = ld + rd;
				break;
			case TokenType::MINUS:
				result = ld - rd;
				break;
			case TokenType::STAR:
				result = ld * rd;
				break;
			case TokenType::DIVIDE:
				if (rd == 0) {
					lastFolded = std::make_shared<MathExpr>(left, expr.op_t, right);
					return;
				}
				result = ld / rd;
				break;
			default:
				// If it's a comparison (> < ==), return the original tree
				lastFolded = std::make_shared<MathExpr>(left, expr.op_t, right);
				return;
		}

		// Replace the whole branch with a single number
		Token resultToken = expr.op_t;
		resultToken.type = TokenType::NUMBER;
		resultToken.lexeme = std::to_string(result);
		lastFolded = std::make_shared<ValueExpr>(resultToken);
		return;
	}

	// If we can't fold, return the tree but with optimized children
	lastFolded = std::make_shared<MathExpr>(left, expr.op_t, right);
}
void Optimizer::visitGroup(GroupExpr &expr) {
	// Just return the folded inner expression, throwing away the ( )
	lastFolded = fold(expr.expression);
}
void Optimizer::visitVariable(VariableExpr &expr) { lastFolded = std::make_shared<VariableExpr>(expr.name); }

void Optimizer::visitValue(ValueExpr &expr) { lastFolded = std::make_shared<ValueExpr>(expr.value); }

void Optimizer::visitBitwiseOr(BitwiseOrExpr &expr) {
	auto left = fold(expr.left);
	auto right = fold(expr.right);
	auto lVal = std::dynamic_pointer_cast<ValueExpr>(left);
	auto rVal = std::dynamic_pointer_cast<ValueExpr>(right);

	if (lVal && rVal && lVal->value.type == TokenType::NUMBER && rVal->value.type == TokenType::NUMBER) {
		long l = static_cast<long>(std::stod(lVal->value.lexeme));
		long r = static_cast<long>(std::stod(rVal->value.lexeme));
		Token t = expr.op_t;
		t.type = TokenType::NUMBER;
		t.lexeme = std::to_string(static_cast<double>(l | r));
		lastFolded = std::make_shared<ValueExpr>(t);
		return;
	}
	lastFolded = std::make_shared<BitwiseOrExpr>(left, expr.op_t, right);
}

void Optimizer::visitBitwiseXor(BitwiseXorExpr &expr) {
	auto left = fold(expr.left);
	auto right = fold(expr.right);
	auto lVal = std::dynamic_pointer_cast<ValueExpr>(left);
	auto rVal = std::dynamic_pointer_cast<ValueExpr>(right);

	if (lVal && rVal && lVal->value.type == TokenType::NUMBER && rVal->value.type == TokenType::NUMBER) {
		long l = static_cast<long>(std::stod(lVal->value.lexeme));
		long r = static_cast<long>(std::stod(rVal->value.lexeme));
		Token t = expr.op_t;
		t.type = TokenType::NUMBER;
		t.lexeme = std::to_string(static_cast<double>(l ^ r));
		lastFolded = std::make_shared<ValueExpr>(t);
		return;
	}
	lastFolded = std::make_shared<BitwiseXorExpr>(left, expr.op_t, right);
}

void Optimizer::visitBitwiseAnd(BitwiseAndExpr &expr) {
	auto left = fold(expr.left);
	auto right = fold(expr.right);
	auto lVal = std::dynamic_pointer_cast<ValueExpr>(left);
	auto rVal = std::dynamic_pointer_cast<ValueExpr>(right);

	if (lVal && rVal && lVal->value.type == TokenType::NUMBER && rVal->value.type == TokenType::NUMBER) {
		long l = static_cast<long>(std::stod(lVal->value.lexeme));
		long r = static_cast<long>(std::stod(rVal->value.lexeme));
		Token t = expr.op_t;
		t.type = TokenType::NUMBER;
		t.lexeme = std::to_string(static_cast<double>(l & r));
		lastFolded = std::make_shared<ValueExpr>(t);
		return;
	}
	lastFolded = std::make_shared<BitwiseAndExpr>(left, expr.op_t, right);
}

void Optimizer::visitShift(ShiftExpr &expr) {
	auto left = fold(expr.left);
	auto right = fold(expr.right);
	auto lVal = std::dynamic_pointer_cast<ValueExpr>(left);
	auto rVal = std::dynamic_pointer_cast<ValueExpr>(right);

	if (lVal && rVal && lVal->value.type == TokenType::NUMBER && rVal->value.type == TokenType::NUMBER) {
		long l = static_cast<long>(std::stod(lVal->value.lexeme));
		long r = static_cast<long>(std::stod(rVal->value.lexeme));
		double result = 0;
		if (expr.op_t.type == TokenType::LESS_LESS) {
			result = static_cast<double>(l << r);
		} else {
			result = static_cast<double>(l >> r);
		}
		Token t = expr.op_t;
		t.type = TokenType::NUMBER;
		t.lexeme = std::to_string(result);
		lastFolded = std::make_shared<ValueExpr>(t);
		return;
	}
	lastFolded = std::make_shared<ShiftExpr>(left, expr.op_t, right);
}

void Optimizer::visitPrefix(PrefixExpr &expr) {
	auto right = fold(expr.right);
	auto rVal = std::dynamic_pointer_cast<ValueExpr>(right);

	if (rVal) {
		if (expr.prefix.type == TokenType::MINUS && rVal->value.type == TokenType::NUMBER) {
			double d = std::stod(rVal->value.lexeme);
			Token t = rVal->value;
			t.lexeme = std::to_string(-d);
			lastFolded = std::make_shared<ValueExpr>(t);
			return;
		}
		if (expr.prefix.type == TokenType::BANG) {
			bool truthy = true;
			if (rVal->value.type == TokenType::FALSE || rVal->value.type == TokenType::NULL_TOKEN)
				truthy = false;
			Token t = expr.prefix;
			t.type = (!truthy) ? TokenType::TRUE : TokenType::FALSE;
			t.lexeme = (!truthy) ? "true" : "false";
			lastFolded = std::make_shared<ValueExpr>(t);
			return;
		}
		if (expr.prefix.type == TokenType::TILDE && rVal->value.type == TokenType::NUMBER) {
			long l = static_cast<long>(std::stod(rVal->value.lexeme));
			Token t = rVal->value;
			t.lexeme = std::to_string(static_cast<double>(~l));
			lastFolded = std::make_shared<ValueExpr>(t);
			return;
		}
	}
	lastFolded = std::make_shared<PrefixExpr>(expr.prefix, right);
}

void Optimizer::visitPostfix(PostfixExpr &expr) {
	auto left = fold(expr.left);
	lastFolded = std::make_shared<PostfixExpr>(expr.postfix, left);
}

void Optimizer::visitLogical(LogicalExpr &expr) {
	auto left = fold(expr.left);
	auto lVal = std::dynamic_pointer_cast<ValueExpr>(left);

	if (lVal) {
		bool truthy = true;
		if (lVal->value.type == TokenType::FALSE || lVal->value.type == TokenType::NULL_TOKEN)
			truthy = false;

		if (expr.op_t.type == TokenType::OR) {
			if (truthy) {
				lastFolded = left;
				return;
			}
		} else if (expr.op_t.type == TokenType::AND) {
			if (!truthy) {
				lastFolded = left;
				return;
			}
		}
	}

	auto right = fold(expr.right);
	lastFolded = std::make_shared<LogicalExpr>(left, expr.op_t, right);
}

void Optimizer::visitAssign(AssignExpr &expr) {
	auto value = fold(expr.value);
	lastFolded = std::make_shared<AssignExpr>(expr.name, value);
}

void Optimizer::visitCall(CallExpr &expr) {
	auto callee = fold(expr.callee);
	std::vector<std::shared_ptr<Expr>> args;
	for (auto &arg: expr.arguments) {
		args.emplace_back(fold(arg));
	}
	lastFolded = std::make_shared<CallExpr>(callee, args, expr.Paren);
}

void Optimizer::visitThis(ThisExpr &expr) { lastFolded = std::make_shared<ThisExpr>(expr.keyword); }

void Optimizer::visitGet(GetExpr &expr) {
	auto object = fold(expr.object);
	lastFolded = std::make_shared<GetExpr>(object, expr.name);
}

void Optimizer::visitMap(MapExpr &expr) {
	std::vector<std::pair<std::shared_ptr<Expr>, std::shared_ptr<Expr>>> items;
	for (auto &pair: expr.items) {
		items.emplace_back(fold(pair.first), fold(pair.second));
	}
	lastFolded = std::make_shared<MapExpr>(expr.braceToken, items);
}

void Optimizer::visitRange(RangeExpr &expr) {
	auto left = fold(expr.leftBound);
	auto right = fold(expr.rightBound);
	lastFolded = std::make_shared<RangeExpr>(left, expr.op_t, right);
}

void Optimizer::visitSet(SetExpr &expr) {
	auto object = fold(expr.object);
	auto value = fold(expr.value);
	lastFolded = std::make_shared<SetExpr>(object, expr.name, value);
}

void Optimizer::visitIndexSet(IndexSetExpr &expr) {
	auto object = fold(expr.object);
	auto index = fold(expr.index);
	auto value = fold(expr.value);
	lastFolded = std::make_shared<IndexSetExpr>(object, expr.bracket, index, value);
}

void Optimizer::visitIndex(IndexExpr &expr) {
	auto object = fold(expr.object);
	auto index = fold(expr.index);
	lastFolded = std::make_shared<IndexExpr>(object, index, expr.bracket);
}

void Optimizer::visitList(ListExpr &expr) {
	std::vector<std::shared_ptr<Expr>> elements;
	for (auto &el: expr.elements) {
		elements.emplace_back(fold(el));
	}
	lastFolded = std::make_shared<ListExpr>(elements);
}
