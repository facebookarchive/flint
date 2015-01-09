// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.conv, std.file, std.getopt, std.stdio, std.string;
import Checks, FileCategories, Ignored, Tokenizer;

bool recursive = true;
bool include_what_you_use = false;
string[] exclude_checks;

uint function(string, Token[])[string] checks;
uint function(string, Token[])[string] cppChecks;

/**
 * Entry point. Reads in turn and verifies each file passed onto the
 * command line.
 */
int main(string[] args) {
  try {
    getopt(args,
           "recursive", &recursive,
           "c_mode", &c_mode,
           "include_what_you_use", &include_what_you_use,
           "exclude", &exclude_checks);
  } catch (Exception e) {
    stderr.writeln(e.msg);
    stderr.writeln("usage: flint "
                   "[--recursive] [--c_mode], [--include_what_you_use]"
                   "[--exclude=<rule>,...]");
  }

checks = mixin(
    makeHashtable!(
      checkBlacklistedSequences,
      checkBlacklistedIdentifiers,
      checkDefinedNames,
      checkIfEndifBalance,
      checkIncludeGuard,
      checkMemset,
      checkQuestionableIncludes,
      checkInlHeaderInclusions,
      checkInitializeFromItself,
      checkSmartPtrUsage,
      checkUniquePtrUsage,
      checkBannedIdentifiers,
      checkOSSIncludes,
      checkMultipleIncludes,
      checkBreakInSynchronized,
      checkBogusComparisons,
      checkExitStatus,
      checkAttributeArgumentUnderscores
    )
  );

 version(facebook) {
   checks["checkToDoFollowedByTaskNumber"] = &checkToDoFollowedByTaskNumber;
   checks["checkAngleBracketIncludes"] = &checkAngleBracketIncludes;
 }

 cppChecks = mixin(
    makeHashtable!(
      checkNamespaceScopedStatics,
      checkIncludeAssociatedHeader,
      checkCatchByReference,
      checkConstructors,
      checkVirtualDestructors,
      checkThrowSpecification,
      checkThrowsHeapException,
      checkUsingNamespaceDirectives,
      checkUsingDirectives,
      checkFollyDetail,
      checkFollyStringPieceByValue,
      checkProtectedInheritance,
      checkImplicitCast,
      checkUpcaseNull,
      checkExceptionInheritance,
      checkMutexHolderHasName)
  );

 if (include_what_you_use) {
   cppChecks["checkDirectStdInclude"] = &checkDirectStdInclude;
 }

 foreach (string s ; exclude_checks) {
   string ss = strip(s);
   checks.remove(ss);
   cppChecks.remove(ss);
 }

 // Check each file
 uint errors = 0;
 foreach (arg; args) {
   errors += checkEntry(arg);
 }

  return errors == 0 ? 0 : 1;
}

auto makeHashtable(T...)() {
  string result = `[`;
  foreach (t; T) {
    string name = __traits(identifier, t);
    result ~= `"` ~ name ~ `" : &` ~ name ~ ", ";
  }
  return result ~= `]`;
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
    string file = to!string(path.read);
    Token[] tokens;
    // Remove code that occurs in pairs of
    // "// %flint: pause" & "// %flint: resume"
    file = removeIgnoredCode(file, path);
    // Tokenize the file's contents
    tokens = tokenize(file, path);

    // *** Checks each lint rule
    foreach (uint function(string, Token[]) check ; checks.byValue()) {
      errors += check(path, tokens);
    }
    if (!c_mode) {
      foreach (uint function(string, Token[]) check ; cppChecks.byValue()) {
        errors += check(path, tokens);
      }
    }
  } catch (Exception e) {
    stderr.writef("Flint was unable to lint %s\n", path);
    stderr.writeln(e.toString());
  }
  return errors;
}
