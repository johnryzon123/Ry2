#pragma once
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "common.h"

namespace RyRuntime {
	class RyClosure;
}
namespace Frontend {
	class RyClass;
	class RyBoundMethod;
} // namespace Frontend

struct RyRange {
	double start;
	double end;

	bool operator==(const RyRange &other) const { return start == other.start && end == other.end; }
};

struct RyValue;

struct RyValueHasher {
	size_t operator()(const RyValue &v) const;
};


struct RyValue {
	using List = std::shared_ptr<std::vector<RyValue>>;
	using Map = std::shared_ptr<std::map<RyValue, RyValue>>;
	using Func = std::shared_ptr<Frontend::RyFunction>;
	using Instance = std::shared_ptr<Frontend::RyInstance>;
	using Native = std::shared_ptr<Frontend::RyNative>;
	using Closure = std::shared_ptr<RyRuntime::RyClosure>;
	using Class = std::shared_ptr<Frontend::RyClass>;
	using BoundMethod = std::shared_ptr<Frontend::RyBoundMethod>;

	using Variant = std::variant<std::monostate, Native, Func, Closure, double, bool, char, std::string, List, RyRange,
															 Map, Instance, Class, BoundMethod>;

	Variant val;

	RyValue() : val(std::monostate{}) {}
	RyValue(double d) : val(d) {}
	RyValue(bool b) : val(b) {}
	RyValue(char c) : val(c) {}
	RyValue(std::string s) : val(s) {}
	RyValue(const char *s) : val(std::string(s)) {}
	RyValue(List l) : val(l) {}
	RyValue(Map m) : val(m) {}
	RyValue(Func f) : val(f) {}
	RyValue(Closure c) : val(c) {}
	RyValue(Instance i) : val(i) {}
	RyValue(std::nullptr_t) : val(std::monostate{}) {}
	RyValue(Native n) : val(n) {}
	RyValue(RyRange r) : val(r) {}
	RyValue(Class c) : val(c) {}
	RyValue(BoundMethod b) : val(b) {}


	bool isNil() const { return std::holds_alternative<std::monostate>(val); }
	bool isNumber() const { return std::holds_alternative<double>(val); }
	bool isBool() const { return std::holds_alternative<bool>(val); }
	bool isChar() const { return std::holds_alternative<char>(val); }
	bool isString() const { return std::holds_alternative<std::string>(val); }
	bool isList() const { return std::holds_alternative<List>(val); }
	bool isMap() const { return std::holds_alternative<Map>(val); }
	bool isFunction() const { return std::holds_alternative<Func>(val); }
	bool isInstance() const { return std::holds_alternative<Instance>(val); }
	bool isNative() const { return std::holds_alternative<Native>(val); };
	bool isClass() const { return std::holds_alternative<Class>(val); }
	bool isRange() const { return std::holds_alternative<RyRange>(val); }
	bool isClosure() const { return std::holds_alternative<Closure>(val); }
	bool isBoundMethod() const { return std::holds_alternative<BoundMethod>(val); }

	double asNumber() const {
		if (const double *b = std::get_if<double>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a number\n";
		return 0;
	}
	Closure asClosure() const {
		if (const Closure *b = std::get_if<Closure>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a closure\n";
		return nullptr;
	}
	bool asBool() const {
		if (const bool *b = std::get_if<bool>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a bool\n";
		return false;
	}
	char asChar() const {
		if (const char *c = std::get_if<char>(&val)) {
			return *c;
		}
		std::cerr << "Value is not a char\n";
		return '\0';
	}
	std::string asString() const {
		if (const std::string *b = std::get_if<std::string>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a string\n";
		return "";
	}
	List asList() const {
		if (const List *b = std::get_if<List>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a list\n";
		return nullptr;
	}
	Map asMap() const {
		if (const Map *b = std::get_if<Map>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a map\n";
		return nullptr;
	}
	Func asFunction() const {
		if (const Func *b = std::get_if<Func>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a function\n";
		return nullptr;
	}
	Instance asInstance() const {
		if (const Instance *b = std::get_if<Instance>(&val)) {
			return *b;
		}
		std::cerr << "Value is not an instance\n";
		return nullptr;
	}
	Native asNative() const {
		if (const Native *b = std::get_if<Native>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a native function\n";
		return nullptr;
	}
	RyRange asRange() const {
		if (const RyRange *b = std::get_if<RyRange>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a range\n";
		return {};
	}
	Class asClass() const {
		if (const Class *b = std::get_if<Class>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a class\n";
		return nullptr;
	}
	BoundMethod asBoundMethod() const {
		if (const BoundMethod *b = std::get_if<BoundMethod>(&val)) {
			return *b;
		}
		std::cerr << "Value is not a bound method" << std::endl;
		return nullptr;
	}


	bool operator==(const RyValue &other) const { return val == other.val; }

	bool operator!=(const RyValue &other) const { return val != other.val; }

	std::string to_string() const;
	std::string typeName() const;

	RyValue operator+(const RyValue &other) const;
	RyValue operator-(const RyValue &other) const;
	RyValue operator*(const RyValue &other) const;
	RyValue operator/(const RyValue &other) const;
	RyValue operator%(const RyValue &other) const;
	RyValue operator-() const;
	RyValue operator!() const;
	RyValue operator>(const RyValue &other) const;
	bool operator<(const RyValue &other) const;
	RyValue operator>=(const RyValue &other) const;
};
typedef RyValue (*NativeFn)(int argCount, RyValue *args, std::map<std::string, RyValue> &globals);
typedef RyValue (*SimpleNativeFn)(int argCount, RyValue *args);
