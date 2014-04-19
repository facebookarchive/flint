// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>

#include "Ignored.h"
#include "Checks.h"
#include "folly/FileUtil.h"
#include "folly/Exception.h"
#include "folly/Foreach.h"
#include <gflags/gflags.h>

DEFINE_bool(recursive, true, "Run into directories");
DECLARE_bool(c_mode);

namespace fs = boost::filesystem;
using namespace folly;
using namespace std;

uint checkEntry(fs::path path) {
  using namespace facebook;
  using namespace facebook::flint;

  if (!fs::exists(path)) {
    return 0;
  }

  uint errors = 0;

  if (fs::is_directory(path)) {
    if (!FLAGS_recursive) {
      return 0;
    }
    static const fs::directory_iterator endIter;
    for (fs::directory_iterator it(path); it != endIter ; ++it) {
      errors += checkEntry(*it);
    }
    return errors;
  }
  const auto fpath = path.string();
  if (getFileCategory(fpath) == FileCategory::UNKNOWN) {
    return 0;
  }

  try {
    std::string file;
    // Get file intro memory
    if (!folly::readFile(fpath.c_str(), file)) {
      throwSystemError();
    }
    vector<Token> tokens;
    // Remove code that occurs in pairs of
    // "// %flint: pause" & "// %flint: resume"
    file = removeIgnoredCode(file, fpath);
    // Tokenize the file's contents
    tokenize(file, fpath, tokens);

    // *** Checks begin
    errors += checkBlacklistedSequences(fpath, tokens);
    errors += checkBlacklistedIdentifiers(fpath, tokens);
    errors += checkDefinedNames(fpath, tokens);
    errors += checkIfEndifBalance(fpath, tokens);
    errors += checkIncludeGuard(fpath, tokens);
    errors += checkMemset(fpath, tokens);
    errors += checkDeprecatedIncludes(fpath, tokens);
    errors += checkInlHeaderInclusions(fpath, tokens);
    errors += checkInitializeFromItself(fpath, tokens);
    errors += checkSmartPtrUsage(fpath, tokens);
    errors += checkUniquePtrUsage(fpath, tokens);
    errors += checkBannedIdentifiers(fpath, tokens);
    errors += checkOSSIncludes(fpath, tokens);
    errors += checkBreakInSynchronized(fpath, tokens);
    if (!FLAGS_c_mode) {
      errors += checkNamespaceScopedStatics(fpath, tokens);
      errors += checkIncludeAssociatedHeader(fpath, tokens);
      errors += checkCatchByReference(fpath, tokens);
      errors += checkConstructors(fpath, tokens);
      errors += checkVirtualDestructors(fpath, tokens);
      errors += checkThrowSpecification(fpath, tokens);
      errors += checkThrowsHeapException(fpath, tokens);
      errors += checkUsingNamespaceDirectives(fpath, tokens);
      errors += checkUsingDirectives(fpath, tokens);
      errors += checkFollyDetail(fpath, tokens);
      errors += checkProtectedInheritance(fpath, tokens);
      errors += checkImplicitCast(fpath, tokens);
      errors += checkUpcaseNull(fpath, tokens);
      errors += checkExceptionInheritance(fpath, tokens);
    }
    // *** Checks end
  } catch (std::exception const& e) {
    checkUnixError(fprintf(stderr, "Exception thrown during checks on %s.\n%s",
                           fpath.c_str(), e.what()));
  }
  return errors;
}

/**
 * Entry point. Reads in turn and verifies each file passed onto the
 * command line.
 */
int main(int argc, char ** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  // Check each file
  uint errors = 0;
  FOR_EACH_RANGE (i, 1, argc) {
    errors += checkEntry(fs::path(argv[i]));
  }

  return 0;
}
