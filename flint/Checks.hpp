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

	// These checks get access to a list of identified structs/class/unions's
	X_struct(ThrowSpecification);
	X_struct(Constructors);
	X_struct(ProtectedInheritance);
	X_struct(ImplicitCast);

	X(DefinedNames);
	X(CatchByReference);
	X(BlacklistedSequences);
	X(BlacklistedIdentifiers);
	X(InitializeFromItself);
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
	X(VirtualDestructors);
	X(FollyDetail);	
	X(ExceptionInheritance);
	X(SmartPtrUsage);
	X(UniquePtrUsage);
	X(BannedIdentifiers);
	X(NamespaceScopedStatics);
	X(MutexHolderHasName);
	X(OSSIncludes);
	X(BreakInSynchronized);
};