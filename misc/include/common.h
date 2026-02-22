#ifndef RY_COMMON_H
#define RY_COMMON_H

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

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
struct RyRange {
	double start;
	double end;

	bool operator==(const RyRange &other) const { return start == other.start && end == other.end; }
};

struct RyValue {
	using List = std::shared_ptr<std::vector<RyValue>>;
	using Map = std::shared_ptr<Backend::Environment>;
	using Func = std::shared_ptr<Frontend::RyFunction>;
	using Instance = std::shared_ptr<Frontend::RyInstance>;
	using Native = std::shared_ptr<Frontend::RyNative>;

	using Variant = std::variant<std::monostate, double, bool, std::string, List, RyRange, Map, Func, Instance, Native>;

	Variant val;

	RyValue() : val(std::monostate{}) {}
	RyValue(double d) : val(d) {}
	RyValue(bool b) : val(b) {}
	RyValue(std::string s) : val(s) {}
	RyValue(const char *s) : val(std::string(s)) {}
	RyValue(List l) : val(l) {}
	RyValue(Map m) : val(m) {}
	RyValue(Func f) : val(f) {}
	RyValue(Instance i) : val(i) {}
	RyValue(std::nullptr_t) : val(std::monostate{}) {}
	RyValue(Native n) : val(n) {}
	RyValue(RyRange r) : val(r) {}


	bool isNil() const { return std::holds_alternative<std::monostate>(val); }
	bool isNumber() const { return std::holds_alternative<double>(val); }
	bool isBool() const { return std::holds_alternative<bool>(val); }
	bool isString() const { return std::holds_alternative<std::string>(val); }
	bool isList() const { return std::holds_alternative<List>(val); }
	bool isMap() const { return std::holds_alternative<Map>(val); }
	bool isFunction() const { return std::holds_alternative<Func>(val); }
	bool isInstance() const { return std::holds_alternative<Instance>(val); }
	bool isNative() const { return std::holds_alternative<Native>(val); };
	bool isClass() const;
	bool isRange() const { return std::holds_alternative<RyRange>(val); }

	double asNumber() const { return std::get<double>(val); }
	bool asBool() const { return std::get<bool>(val); }
	std::string asString() const { return std::get<std::string>(val); }
	List asList() const { return std::get<List>(val); }
	Map asMap() const { return std::get<Map>(val); }
	Func asFunction() const { return std::get<Func>(val); }
	Instance asInstance() const { return std::get<Instance>(val); }
	Native asNative() const { return std::get<Native>(val); }
	RyRange asRange() const { return std::get<RyRange>(val); }


	bool operator==(const RyValue &other) const { return val == other.val; }

	bool operator!=(const RyValue &other) const { return val != other.val; }

	std::string to_string() const {
		if (isString())
			return asString();
		if (isNumber()) {
			std::string s = std::to_string(asNumber());
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.back() == '.')
				s.pop_back();
			return s;
		}
		if (isBool())
			return asBool() ? "true" : "false";
		if (isNil())
			return "null";
		if (isList()) {
			std::string result = "[";
			auto list = asList();
			for (int i = 0; i < list->size(); i++) {
				result += (*list)[i].to_string();
				if (i < list->size() - 1)
					result += ", ";
			}
			result += "]";
			return result;
		}
		if (isMap())
			return "<map>";
		if (isFunction())
			return "<function>";
		if (isInstance())
			return "<instance>";
		if (isRange()) {
			RyRange r = asRange();
			return std::to_string((int) r.start) + ".." + std::to_string((int) r.end);
		}
		return "<object>";
	}

	RyValue operator+(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(asNumber() + other.asNumber());
		}
		return RyValue(to_string() + other.to_string());
	}
	RyValue operator-(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(asNumber() - other.asNumber());
		}
		return RyValue(to_string() + other.to_string());
	}
	RyValue operator*(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(asNumber() * other.asNumber());
		}
		return RyValue(to_string() + other.to_string());
	}
	RyValue operator/(const RyValue &other) const;
	RyValue operator%(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(std::fmod(asNumber(), other.asNumber()));
		}
		return RyValue(std::nullptr_t{});
	}
	RyValue operator-() const {
		if (isNumber()) {
			return RyValue(-asNumber());
		}
		return RyValue(std::nullptr_t{}); // Or throw a runtime error
	}
	RyValue operator!() const {
		if (isBool()) {
			return RyValue(!asBool());
		}
		return RyValue(std::nullptr_t{});
	}
	RyValue operator>(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(asNumber() > other.asNumber());
		}
		return RyValue(std::nullptr_t{});
	}
	RyValue operator<(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(asNumber() < other.asNumber());
		}
		return RyValue(std::nullptr_t{});
	}
	RyValue operator>=(const RyValue &other) const {
		if (isNumber() && other.isNumber()) {
			return RyValue(asNumber() >= other.asNumber());
		}
		return RyValue(std::nullptr_t{});
	}
};

typedef RyValue (*NativeFn)(int argCount, RyValue *args);

#endif
