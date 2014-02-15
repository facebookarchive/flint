// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

#ifndef CPPLINT_CHECKS_H_
#define CPPLINT_CHECKS_H_

#include "Tokenizer.h"
#include "FileCategories.h"
#include <string>
#include <vector>

namespace facebook { namespace flint {

#define X(func)                                                       \
  uint check##func(const std::string& filename, const std::vector<Token>&)

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

#undef X

}}

#endif
