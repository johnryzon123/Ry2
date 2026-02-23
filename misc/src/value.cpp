#include "value.h"

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

RyValue RyValue::operator<(const RyValue &other) const {
	if (isNumber() && other.isNumber()) {
		return RyValue(asNumber() < other.asNumber());
	}
	return RyValue(std::nullptr_t{});
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
		return "<instance>";
	if (isRange()) {
		RyRange r = asRange();
		return std::to_string((int) r.start) + ".." + std::to_string((int) r.end);
	}
	return "<object>";
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
