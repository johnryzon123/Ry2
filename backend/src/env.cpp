#include "env.h"
#include "tools.h"
#include "value.h"

using namespace Backend;
using namespace RyTools;

RyValue Environment::get(const std::string &name, const Token &errorToken) {
	if (values.contains(name))
		return values[name].value;

	if (auto parent = enclosing.lock()) {
		return parent->get(name, errorToken);
	}
	throw RyRuntimeError(errorToken, "Undefined variable '" + name + "'.");
}

void Environment::assign(Token name, RyVariable value) {
	if (values.find(name.lexeme) != values.end()) {
		values[name.lexeme] = std::move(value);
		return;
	}
	if (auto parent = enclosing.lock()) {
		parent->assign(name, value);
		return;
	}
	throw RyRuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
}

bool Environment::has(const std::string &name, const Token &errorToken) {
	if (values.contains(name))
		return true;
	if (auto parent = enclosing.lock())
		return parent->has(name, errorToken);
	return false;
}

RyVariable &Environment::getVariable(Token name) {
	if (values.find(name.lexeme) != values.end()) {
		return values[name.lexeme];
	}
	if (auto parent = enclosing.lock()) {
		return parent->getVariable(name);
	}
	throw RyRuntimeError(name, "Undefined variable.");
}
void Environment::define(const std::string &name, RyVariable value) { values[name] = std::move(value); }
void Environment::define(const std::string &name, RyValue value, bool isPrivate) {
	values[name] = RyVariable{std::move(value), isPrivate, std::nullopt};
}
RyVariable &Environment::getAt(int distance, const std::string &name) {
	Environment *ancestor = this;
	for (int i = 0; i < distance; i++) {
		if (auto parent = ancestor->enclosing.lock()) {
			ancestor = parent.get();
		}
	}
	return ancestor->values[name];
}
std::string Environment::resolveType(const std::string &typeName) {
	if (typeAliases.contains(typeName))
		return typeAliases[typeName];
	if (auto parent = enclosing.lock())
		return parent->resolveType(typeName);
	return typeName;
}
bool Environment::isTypeAlias(const std::string &name) {
	if (typeAliases.contains(name))
		return true;
	if (auto parent = enclosing.lock())
		return parent->isTypeAlias(name);
	return false;
}
std::string Environment::getTypeAlias(const std::string &name) {
	if (typeAliases.contains(name))
		return typeAliases[name];
	if (auto parent = enclosing.lock())
		return parent->getTypeAlias(name);
	return name;
}
