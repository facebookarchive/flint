// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.file, std.getopt, std.stdio;
import Checks, FileCategories, Ignored, Tokenizer;

bool recursive = true;

/**
 * Entry point. Reads in turn and verifies each file passed onto the
 * command line.
 */
int main(string[] args) {
  getopt(args,
    "recursive", &recursive,
    "c_mode", &c_mode);

  // Check each file
  uint errors = 0;
  foreach (arg; args) {
    errors += checkEntry(arg);
  }

  return 0;
}

bool dontLintPath(string path) {
  return std.path.buildPath(path, ".nolint").exists;
}

uint checkEntry(string path) {
  if (!path.exists) {
    return 0;
  }

  uint errors = 0;

  if (path.isDir) {
    if (!recursive || dontLintPath(path)) {
      return 0;
    }
    foreach (entry; dirEntries(path, SpanMode.shallow)) {
      if (!dontLintPath(entry)) {
        errors += checkEntry(entry);
      }
    }
    return errors;
  }

  if (getFileCategory(path) == FileCategory.unknown) {
    return 0;
  }

  try {
    // Get file intro memory
    string file = path.readText;
    Token[] tokens;
    // Remove code that occurs in pairs of
    // "// %flint: pause" & "// %flint: resume"
    file = removeIgnoredCode(file, path);
    // Tokenize the file's contents
    tokens = tokenize(file, path);

    // *** Checks begin
    errors += checkBlacklistedSequences(path, tokens);
    errors += checkBlacklistedIdentifiers(path, tokens);
    errors += checkDefinedNames(path, tokens);
    errors += checkIfEndifBalance(path, tokens);
    errors += checkIncludeGuard(path, tokens);
    errors += checkMemset(path, tokens);
    errors += checkQuestionableIncludes(path, tokens);
    errors += checkInlHeaderInclusions(path, tokens);
    errors += checkInitializeFromItself(path, tokens);
    errors += checkSmartPtrUsage(path, tokens);
    errors += checkUniquePtrUsage(path, tokens);
    errors += checkBannedIdentifiers(path, tokens);
    errors += checkOSSIncludes(path, tokens);
    errors += checkBreakInSynchronized(path, tokens);
    if (!c_mode) {
      errors += checkNamespaceScopedStatics(path, tokens);
      errors += checkIncludeAssociatedHeader(path, tokens);
      errors += checkCatchByReference(path, tokens);
      errors += checkConstructors(path, tokens);
      errors += checkVirtualDestructors(path, tokens);
      errors += checkThrowSpecification(path, tokens);
      errors += checkThrowsHeapException(path, tokens);
      errors += checkUsingNamespaceDirectives(path, tokens);
      errors += checkUsingDirectives(path, tokens);
      errors += checkFollyDetail(path, tokens);
      errors += checkFollyStringPieceByValue(path, tokens);
      errors += checkProtectedInheritance(path, tokens);
      errors += checkImplicitCast(path, tokens);
      errors += checkUpcaseNull(path, tokens);
      errors += checkExceptionInheritance(path, tokens);
      errors += checkMutexHolderHasName(path, tokens);
    }
    // *** Checks end
  } catch (Exception e) {
    stderr.writef("Exception thrown during checks on %s.\n%s",
        path, e.toString());
  }
  return errors;
}
