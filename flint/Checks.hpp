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

	// Deprecated due to too many false positives
	//X(Incrementers);
	// Merged into banned identifiers
	//X(UpcaseNull);

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
	X(ExceptionInheritance);
	X(SmartPtrUsage);
	X(UniquePtrUsage);
	X(BannedIdentifiers);
	X(NamespaceScopedStatics);
	X(MutexHolderHasName);
	X(OSSIncludes);
	X(BreakInSynchronized);
};