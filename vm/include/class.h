#include <memory>
#include <set>
#include "map"
#include "vm.h"

namespace Frontend {
	struct ClassCompiler {
		std::shared_ptr<ClassCompiler> enclosing;
		bool hasSuperclass = false;
	};

	class RyClass {
	public:
		std::string name;
		std::shared_ptr<RyClass> superclass = nullptr;
		std::map<std::string, std::shared_ptr<RyRuntime::RyClosure>> methods;
		std::set<std::string> privateFields;
		std::map<std::string, RyValue> fields;
		bool isAbstract;

		RyClass(std::string n, bool isAbstract = false) : name(std::move(n)), isAbstract(isAbstract) {}
	};

	class RyInstance {
	public:
		std::shared_ptr<RyClass> klass;
		std::map<std::string, RyValue> fields;
		RyInstance(std::shared_ptr<RyClass> k) : klass(k) {}
	};

	class RyBoundMethod {
	public:
		RyValue receiver;
		std::shared_ptr<RyRuntime::RyClosure> method;
		RyBoundMethod(RyValue r, std::shared_ptr<RyRuntime::RyClosure> m) : receiver(r), method(m) {}
	};
} // namespace Frontend
