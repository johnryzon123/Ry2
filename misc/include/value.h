#pragma once
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "common.h"

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
	using Map = std::shared_ptr<std::unordered_map<RyValue, RyValue, RyValueHasher>>;
	using Func = std::shared_ptr<Frontend::RyFunction>;
	using Instance = std::shared_ptr<Frontend::RyInstance>;
	using Native = std::shared_ptr<Frontend::RyNative>;

	using Variant = std::variant<std::monostate, Native, Func, double, bool, std::string, List, RyRange, Map, Instance>;

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

	std::string to_string() const;

	RyValue operator+(const RyValue &other) const;
	RyValue operator-(const RyValue &other) const;
	RyValue operator*(const RyValue &other) const;
	RyValue operator/(const RyValue &other) const;
	RyValue operator%(const RyValue &other) const;
	RyValue operator-() const;
	RyValue operator!() const;
	RyValue operator>(const RyValue &other) const;
	RyValue operator<(const RyValue &other) const;
	RyValue operator>=(const RyValue &other) const;
};

typedef RyValue (*NativeFn)(int argCount, RyValue *args);
