#ifndef RY_COMMON_H
#define RY_COMMON_H

#include <cmath>

namespace Backend {
	// Forward Declarations
	class Stmt;
	class Expr;
	class Token;
	class Environment;
	class Token;
	class ExprVisitor;
	class StmtVisitor;
	class Environment;
} // namespace Backend

namespace Frontend {
	class Interpreter;
	class RyCallable;
	class RyFunction;
	class RyNative;
	class RyInstance;
	class RyClass;
} // namespace Frontend

class Resolver;
#endif