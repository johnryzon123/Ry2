#pragma once
#include <map>
#include <optional>
#include <string>
#include "value.h"
#include "token.h"

namespace Backend {
	struct RyVariable {
		RyValue value;
		bool isPrivate = false;
		std::optional<std::string> typeConstraint;
		RyVariable() : value(RyValue()), isPrivate(false), typeConstraint(std::nullopt) {}
		RyVariable(RyValue val, bool priv = false, std::optional<std::string> constraint = std::nullopt) :
				value(val), isPrivate(priv), typeConstraint(constraint) {
		}
	};

	class Environment {
		std::weak_ptr<Environment> enclosing;
		std::map<std::string, RyVariable> values;

	public:
		Environment() : enclosing() {}
		Environment(std::shared_ptr<Environment> enclosing) : enclosing(enclosing) {}
		RyValue get(const Token &name) { return get(name.lexeme, name); }
		RyValue get(const std::string &name, const Token &errorToken);
		void define(const std::string &name, RyVariable value);
		void define(const std::string &name, RyValue value, bool isPrivate = false);
		void assign(Token name, RyVariable value);
		bool has(const std::string &name, const Token &errorToken);
		RyVariable &getVariable(Token name);
		RyVariable &getAt(int distance, const std::string &name);
		std::map<std::string, std::string> typeAliases;
		std::string resolveType(const std::string &typeName);
		void defineTypeAlias(const std::string &alias, const std::string &original) { typeAliases[alias] = original; }
		bool isTypeAlias(const std::string &name);
		std::string getTypeAlias(const std::string &name);

		int size() { return values.size(); }
	};
} // namespace Backend
