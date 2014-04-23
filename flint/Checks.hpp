#pragma once

#include <string>
#include <vector>
#include "Polyfill.hpp"
#include "Tokenizer.hpp"

using namespace std;

namespace flint {

	struct Errors {
		uint errors;
		uint warnings;
		uint advice;

		Errors() : errors(0), warnings(0), advice(0) {};
	};

#define X(func)																\
	void check##func(Errors &errors, const string &path, const vector<Token> &tokens)

	X(Iterators);
	X(DefinedNames);
	X(CatchByReference);
	X(BlacklistedSequences);
	X(BlacklistedIdentifiers);
	X(InitializeFromItself);
	X(ThrowSpecification);
	X(IfEndifBalance);
	X(IncludeGuard);
	X(UsingDirectives);
	X(UsingNamespaceDirectives);
	X(ThrowsHeapException);
	X(HPHPNamespace);
	X(DeprecatedIncludes);
	X(IncludeAssociatedHeader);
	X(Memset);
	X(InlHeaderInclusions);
	X(Constructors);
	X(VirtualDestructors);
	X(FollyDetail);
	X(ProtectedInheritance);
	X(ImplicitCast);
	X(UpcaseNull);
	X(ExceptionInheritance);
	X(SmartPtrUsage);
	X(UniquePtrUsage);
	X(BannedIdentifiers);
	X(NamespaceScopedStatics);
	X(MutexHolderHasName);
	X(OSSIncludes);
	X(BreakInSynchronized);
};