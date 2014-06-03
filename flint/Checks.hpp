#pragma once

#include <string>
#include <vector>
#include "ErrorReport.hpp"
#include "Polyfill.hpp"
#include "Tokenizer.hpp"

using namespace std;

namespace flint {

#define X(func)																\
	void check##func(ErrorFile &errors, const string &path, const vector<Token> &tokens)

#define X_struct(func)																\
	void check##func(ErrorFile &errors, const string &path, const vector<Token> &tokens, const vector<size_t> &structures)

	// Deprecated due to too many false positives
	//X(Incrementers);
	// Merged into banned identifiers
	//X(UpcaseNull);

	// These checks get access to a list of identified 
	// structs/class/unions's
	X_struct(ThrowSpecification);
	X_struct(Constructors);
	X_struct(ProtectedInheritance);
	X_struct(ImplicitCast);
	X_struct(ExceptionInheritance);
	X_struct(VirtualDestructors);

	// Blacklisted Terms
	X(DefinedNames);
	X(BlacklistedSequences);
	X(BlacklistedIdentifiers);

	// Common Mistakes
	X(CatchByReference);
	X(Memset);
	X(ThrowsHeapException);
	X(InitializeFromItself);
	X(IfEndifBalance);
	X(NamespaceScopedStatics);
	X(MutexHolderHasName);

	// Include Errors
	X(IncludeGuard);
	X(DeprecatedIncludes);
	X(IncludeAssociatedHeader);
	X(InlHeaderInclusions);

	// Pointer Errors
	X(SmartPtrUsage);
	X(UniquePtrUsage);
	
	// To be implemented...
	X(UsingNamespaceDirectives);	
};
