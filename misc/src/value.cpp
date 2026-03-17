#include "value.h"
#include "class.h"

RyValue RyValue::operator!() const {
	if (isBool()) {
		return RyValue(!asBool());
	}
	return RyValue(std::nullptr_t{});
}

RyValue RyValue::operator>(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() > other.asNumber());
	}
	return RyValue(std::nullptr_t{});
}

bool RyValue::operator<(const RyValue &other) const {
	if (val.index() != other.val.index()) {
		return val.index() < other.val.index();
	}

	if (isNumber())
		return asNumber() < other.asNumber();
	if (isChar())
		return asChar() < other.asChar();
	if (isString())
		return asString() < other.asString();
	if (isBool())
		return asBool() < other.asBool();

	if (isList())
		return asList().get() < other.asList().get();
	if (isMap())
		return asMap().get() < other.asMap().get();

	return false;
}

RyValue RyValue::operator>=(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() >= other.asNumber());
	}
	return RyValue(std::nullptr_t{});
}

size_t RyValueHasher::operator()(const RyValue &v) const {
	if (v.isNumber())
		return std::hash<double>{}(v.asNumber());
	if (v.isBool())
		return std::hash<bool>{}(v.asBool());
	if (v.isString())
		return std::hash<std::string>{}(v.to_string());
	if (v.isList())
		return std::hash<RyValue::List>{}(v.asList());
	if (v.isMap())
		return std::hash<RyValue::Map>{}(v.asMap());
	return 0;
};

std::string RyValue::to_string() const {
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
	if (isChar()) {
		std::string s(1, asChar());
		return s;
	}
	if (isNil())
		return "null";
	if (isList()) {
		std::string result = "[";
		auto list = asList();
		for (size_t i = 0; i < list->size(); i++) {
			result += (*list)[i].to_string();
			if (i < list->size() - 1)
				result += ", ";
		}
		result += "]";
		return result;
	}
	if (isMap()) {
		std::string result = "{";
		auto ryMap = asMap();
		int i = 0;
		for (auto const &[key, val]: *ryMap) {
			result += key.to_string() + ": " + val.to_string();
			if (++i < ryMap->size())
				result += ", ";
		}
		result += "}";
		return result;
	}
	if (isFunction())
		return "<function>";
	if (isInstance())
		return asInstance()->klass->name + " instance";
	if (isRange()) {
		RyRange r = asRange();
		return std::to_string((int) r.start) + ".." + std::to_string((int) r.end);
	}
	if (isNative())
		return "<native>";
	if (isClosure())
		return "<closure>";
	if (isClass())
		return asClass()->name;
	if (isBoundMethod())
		return "<bound method>";
	return "<unknown>";
}

std::string RyValue::typeName() const {
	if (isNil())
		return "nil";
	if (isNumber())
		return "number";
	if (isBool())
		return "boolean";
	if (isChar())
		return "char";
	if (isString()) {
		return "string";
	}
	if (isList())
		return "list";
	if (isMap())
		return "map";
	if (isRange())
		return "range";
	if (isFunction())
		return "function";
	if (isNative())
		return "native";
	if (isClosure())
		return "closure";
	if (isClass())
		return "class";
	if (isInstance())
		return "instance";
	if (isBoundMethod())
		return "boundmethod";
	return "unknown";
}

RyValue RyValue::operator+(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() + other.asNumber());
	}
	return RyValue(to_string() + other.to_string());
}
RyValue RyValue::operator-(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() - other.asNumber());
	}
	return RyValue(to_string() + other.to_string());
}
RyValue RyValue::operator*(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() * other.asNumber());
	}
	return RyValue(to_string() + other.to_string());
}
RyValue RyValue::operator/(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() / other.asNumber());
	}
	return RyValue(to_string() + other.to_string());
}
RyValue RyValue::operator%(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(std::fmod(asNumber(), other.asNumber()));
	}
	return RyValue(std::nullptr_t{});
}
RyValue RyValue::operator-() const {
	if (isNumber()) {
		return RyValue(-asNumber());
	}
	return RyValue(std::nullptr_t{}); // Or throw a runtime error
}
