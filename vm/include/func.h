/*
 * Author: Johnryzon Z. Abejero
 * Date: February 16, 2026
 * Description: Implements RyFunctions
 */

#pragma once // Include Guard
#include <string>
// Contains Chunk that is essential for making bytecode
#include "chunk.h"

namespace Frontend {
	/*
	 * Contains the data for functions
	 */
	class RyFunction {
	public:
		int arity; // Holds how many parameters a function needs
		RyRuntime::Chunk chunk; // The data for the function
		std::string name; // The name of the function
		int upvalueCount = 0;
		bool isStatic = false;
		bool isOpen = false;
		bool isAbstract = false;
		bool isPrivate = false;
		std::string ownerClassName;

		RyFunction() :
				arity(0), name(""), isStatic(false), isOpen(false), isAbstract(false), isPrivate(false) {
		} // Default Constructor for main

		// Constructor for user made functions
		RyFunction(RyRuntime::Chunk c, std::string n, int a) : chunk(std::move(c)), name(n), arity(a) {}
	}; // class RyFunction

	/*
	 * Contains the data for native functions
	 */
	class RyNative {
	public:
		NativeFn function = nullptr; // Contains native function (core or module)
		std::string name; // Contains the name
		int arity; // Constains how much parameters it needs

		RyNative() : name(""), arity(0) {} // Default Constructor

		// Constructor for building native functions
		RyNative(NativeFn fn, std::string n, int a) : function(fn), name(n), arity(a) {}
	}; // class RyNative
} // namespace Frontend
