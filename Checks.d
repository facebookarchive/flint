// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.algorithm, std.array, std.ascii, std.conv, std.exception, std.path,
  std.range, std.stdio, std.string;
import Tokenizer, FileCategories;

bool c_mode;

enum explicitThrowSpec = "/* may throw */";

/*
 * Errors vs. Warnings vs. Advice:
 *
 *   Lint errors will be raised regardless of whether the line was
 *   edited in the change.  Warnings will be ignored by Arcanist
 *   unless the change actually modifies the line the warning occurs
 *   on.  Advice is even weaker than a warning.
 *
 *   Please select errors vs. warnings intelligently.  Too much spam
 *   on lines you don't touch reduces the value of lint output.
 *
 */

void lintError(CppLexer.Token tok, const string error) {
  stderr.writef("%.*s:%u: %s",
                cast(uint) tok.file_.length, tok.file_,
                cast(uint) tok.line_,
                error);
}

immutable string warningPrefix = "Warning: ";

void lintWarning(CppLexer.Token tok, const string warning) {
  // The FbcodeCppLinter just looks for the text "Warning" in the
  // message.
  lintError(tok, warningPrefix ~ warning);
}

void lintAdvice(CppLexer.Token tok, string advice) {
  // The FbcodeCppLinter just looks for the text "Advice" in the
  // message.
  lintError(tok, "Advice: " ~ advice);
}

bool atSequence(Range)(Range r, const CppLexer.TokenType2[] list...) {
  foreach (t; list) {
    if (r.empty || t != r.front.type_) {
      return false;
    }
    r.popFront;
  }
  return true;
}

string getSucceedingWhitespace(Token[] v) {
  if (v.length < 2) return "";
  return v[1].precedingWhitespace_;
}

bool isInMacro(Token[] v, const long idx) {
  // Walk backwards through the tokens, continuing until a newline
  // that is not preceded by a backslash is encountered (i.e. a
  // true line break).
  for (long i = idx; i >= 0; --i) {
    if (v[i..$].atSequence(tk!"#", tk!"identifier") &&
        v[i + 1].value == "define") return true;
    if (i == 0) return false;
    string pws = v[i].precedingWhitespace_;
    auto pos = lastIndexOf(pws, '\n');
    if (pos == -1) continue;
    if (pos != 0) return false;
    if (v[i - 1].type_ != tk!"\\") return false;
  }
  return false;
}

struct IncludedPath {
  string path;
  bool angleBrackets;
  bool nolint;
  bool precompiled;
}

bool getIncludedPath(R)(ref R r, out IncludedPath ipath) {
  if (!r.atSequence(tk!"#", tk!"identifier") || r[1].value != "include") {
    return false;
  }

  r.popFrontN(2);
  bool found = false;
  if (r.front.value_ == "PRECOMPILED") {
    ipath.precompiled = true;
    found = true;
  }

  if (r.front.type_ == tk!"string_literal") {
    string val = r.front.value;
    ipath.path = val[1 .. val.length - 1];
    found = true;
  } else if (r.front.type_ == tk!"<") {
    r.popFront;
    ipath.path = "";
    for (; !r.empty; r.popFront) {
      if (r.front.type_ == tk!">") {
        break;
      }
      ipath.path ~= r.front.value;
    }
    ipath.angleBrackets = true;
    found = true;
  }

  if (!found) {
    return false;
  }

  ipath.nolint = getSucceedingWhitespace(r).canFind("nolint");
  return true;
}

/*
 * Skips a template parameter list or argument list, somewhat
 * heuristically.  Basically, scans forward tracking nesting of <>
 * brackets and parenthesis to find the end of the list. This function
 * takes as input an iterator as well as an optional parameter containsArray
 * that is set to true if the outermost nest contains an array
 *
 * Known unsupported case: TK_RSHIFT can end a template instantiation
 * in C++0x as if it were two TK_GREATERs.
 * (E.g. vector<vector<int>>.)
 *
 * Returns: iterator pointing to the final TK_GREATER (or TK_EOF if it
 * didn't finish).
 */
R skipTemplateSpec(R)(R r, bool* containsArray = null) {
  assert(r.front.type_ == tk!"<");

  uint angleNest = 1;
  uint parenNest = 0;

  if (containsArray) {
    *containsArray = false;
  }

  r.popFront;
  for (; r.front.type_ != tk!"\0"; r.popFront) {
    if (r.front.type_ == tk!"(") {
      ++parenNest;
      continue;
    }
    if (r.front.type_ == tk!")") {
      --parenNest;
      continue;
    }

    // Ignore angles inside of parens.  This avoids confusion due to
    // integral template parameters that use < and > as comparison
    // operators.
    if (parenNest > 0) {
      continue;
    }

    if (r.front.type_ == tk!"[") {
      if (angleNest == 1 && containsArray) {
        *containsArray = true;
      }
      continue;
    }

    if (r.front.type_ == tk!"<") {
      ++angleNest;
      continue;
    }
    if (r.front.type_ == tk!">") {
      if (!--angleNest) {
        break;
      }
      continue;
    }
    if (r.front.type_ == tk!">>") {
      // it is possible to munch a template from within a template, so we
      // only want to consume one of the '>>'
      angleNest -= 2;
      if (angleNest <= 0) break;
      continue;
    }
  }

  return r;
}

/*
 * Returns whether `it' points to a token that is a reserved word for
 * a built in type.
 */
bool atBuiltinType(R)(R it) {
  return it.front.type_.among(tk!"double", tk!"float", tk!"int", tk!"short",
      tk!"unsigned", tk!"long", tk!"signed", tk!"void", tk!"bool", tk!"wchar_t",
      tk!"char") != 0;
}

/*
 * heuristically read a potentially namespace-qualified identifier,
 * advancing `it' in the process.
 *
 * Returns: a vector of all the identifier values involved, or an
 * empty vector if no identifier was detected.
 */
string[] readQualifiedIdentifier(R)(ref R it) {
  string[] result;
  for (; it.front.type_.among(tk!"identifier", tk!"::"); it.popFront) {
    if (it.front.type_ == tk!"identifier") {
      result ~= it.front.value_;
    }
  }
  return result;
}

/*
 * starting from a left curly brace, skips until it finds the matching
 * (balanced) right curly brace. Does not care about whether other characters
 * within are balanced.
 *
 * Returns: iterator pointing to the final TK_RCURL (or TK_EOF if it
 * didn't finish).
 */
R skipBlock(R)(R r) {
  enforce(r.front.type_ == tk!"{");

  uint openBraces = 1;

  r.popFront;
  for (; r.front.type_ != tk!"\0"; r.popFront) {
    if (r.front.type_ == tk!"{") {
      ++openBraces;
      continue;
    }
    if (r.front.type_ == tk!"}") {
      if (!--openBraces) {
        break;
      }
      continue;
    }
  }

  return r;
}

/*
 * Iterates through to find all class declarations and calls the callback
 * with an iterator pointing to the first token in the class declaration.
 * The iterator is guaranteed to have type TK_CLASS, TK_STRUCT, or TK_UNION.
 *
 * Note: The callback function is responsible for the scope of its search, as
 * the vector of tokens passed may (likely will) extend past the end of the
 * class block.
 *
 * Returns the sum of the results from calling the callback on
 * each declaration.
 */
uint iterateClasses(alias callback)(Token[] v) {
  uint result = 0;

  for (auto it = v; !it.empty; it.popFront) {
    if (it.atSequence(tk!"template", tk!"<")) {
      it.popFront;
      it = skipTemplateSpec(it);
      continue;
    }

    if (it.front.type_.among(tk!"class", tk!"struct", tk!"union")) {
      result += callback(it, v);
    }
  }

  return result;
}

/*
 * Starting from a function name or one of its arguments, skips the entire
 * function prototype or function declaration (including function body).
 *
 * Implementation is simple: stop at the first semicolon, unless an opening
 * curly brace is found, in which case we stop at the matching closing brace.
 *
 * Returns: iterator pointing to the final TK_RCURL or TK_SEMICOLON, or TK_EOF
 * if it didn't finish.
 */
R skipFunctionDeclaration(R)(R r) {
  r.popFront;
  for (; r.front.type_ != tk!"\0"; r.popFront) {
    if (r.front.type_ == tk!";") { // prototype
      break;
    } else if (r.front.type_ == tk!"{") { // full declaration
      r = skipBlock(r);
      break;
    }
  }
  return r;
}

/**
 * Represent an argument or the name of a function.
 * first is an iterator that points to the start of the argument.
 * last is an iterator that points to the token right after the end of the
 * argument.
 */
struct Argument {
  Token[] tox;
  ref Token first() { return tox.front; }
  ref Token last() { return tox.back; }
}

struct FunctionSpec {
  Argument functionName;
  Argument[] args;
  bool isNoexcept;
  bool explicitThrows;
  bool isConst;
  bool isDeleted;
  bool isDefault;
};

string formatArg(Argument arg) {
  string result;
  foreach (i, a; arg.tox) {
    if (i != 0 && !a.precedingWhitespace_.empty) {
      result ~= ' ';
    }
    result ~= a.value;
  }
  return result;
}

string formatFunction(FunctionSpec spec) {
  auto result = formatArg(spec.functionName) ~ "(";
  foreach (i; 0 .. spec.args.length) {
    if (i > 0) {
      result ~= ", ";
    }
    result ~= formatArg(spec.args[i]);
  }
  result ~= ")";
  return result;
}

/**
 * Get the list of arguments of a function, assuming that the current
 * iterator is at the open parenthesis of the function call. After the
 * this method is called, the iterator will be moved to after the end
 * of the function call.
 * @param i: the current iterator, must be at the open parenthesis of the
 * function call.
 * @param args: the arguments of the function would be push to the back of args
 * @return true if (we believe) that there was no problem during the run and
 * false if we believe that something was wrong (most probably with skipping
 * template specs.)
 */
bool getRealArguments(ref Token[] r, ref Argument[] args) {
  assert(r.front.type_ == tk!"(", text(r));
  // the first argument starts after the open parenthesis
  auto argStart = r[1 .. $];
  int parenCount = 1;
  do {
    if (r.front.type_ == tk!"\0") {
      // if we meet EOF before the closing parenthesis, something must be wrong
      // with the template spec skipping
      return false;
    }
    r.popFront;
    switch (r.front.type_.id) {
    case tk!"(".id: parenCount++;
                    break;
    case tk!")".id: parenCount--;
                    break;
    case tk!"<".id:   // This is a heuristic which would fail when < is used with
                    // the traditional meaning in an argument, e.g.
                    //  memset(&foo, a < b ? c : d, sizeof(foo));
                    // but currently we have no way to distinguish that use of
                    // '<' and
                    //  memset(&foo, something<A,B>(a), sizeof(foo));
                    // We include this heuristic in the hope that the second
                    // use of '<' is more common than the first.
                    r = skipTemplateSpec(r);
                    break;
    case tk!",".id:  if (parenCount == 1) {
                      // end an argument of the function we are looking at
                      args ~=
                        Argument(argStart[0 .. argStart.length - r.length]);
                      argStart = r[1 .. $];
                    }// otherwise we are in an inner function, so do nothing
                    break;
    default:        break;
    }
  } while (parenCount != 0);
  if (argStart !is r) {
    args ~= Argument(argStart[0 .. argStart.length - r.length]);
  }
  return true;
}

/**
 * Get the argument list of a function, with the first argument being the
 * function name plus the template spec.
 * @param i: the current iterator, must be at the function name. At the end of
 * the method, i will be pointing at the close parenthesis of the function call
 * @param functionName: the function name will be stored here
 * @param args: the arguments of the function would be push to the back of args
 * @return true if (we believe) that there was no problem during the run and
 * false if we believe that something was wrong (most probably with skipping
 * template specs.)
 */
bool getFunctionSpec(ref Token[] r, ref FunctionSpec spec) {
  auto r1 = r;
  r.popFront;
  if (r.front.type_ == tk!"<") {
    r = skipTemplateSpec(r);
    if (r.front.type_ == tk!"\0") {
      return false;
    }
    r.popFront;
  }
  assert(r1.length >= r.length);
  spec.functionName.tox = r1[0 .. r1.length - r.length];
  if (!getRealArguments(r, spec.args)) {
    return false;
  }

  auto r2 = r;
  assert(r2.front.type_ == tk!")");
  r2.popFront;
  while (true) {
    if (r2.front.precedingWhitespace_.canFind(explicitThrowSpec)) {
      spec.explicitThrows = true;
    }

    if (r2.front.type_ == tk!"noexcept") {
      spec.isNoexcept = true;
      r2.popFront;
      continue;
    }
    if (r2.front.type_ == tk!"const") {
      spec.isConst = true;
      r2.popFront;
      continue;
    }
    if (r2.front.type_ == tk!"=") {
      r2.popFront;
      if (r2.front.type_ == tk!"delete") {
        spec.isDeleted = true;
        r2.popFront;
        continue;
      }
      if (r2.front.type_ == tk!"default") {
        spec.isDefault = true;
        r2.popFront;
        continue;
      }
    }
    break;
  }

  return true;
}

uint checkInitializeFromItself(string fpath, Token[] tokens) {
  auto firstInitializer = [
    tk!":", tk!"identifier", tk!"(", tk!"identifier", tk!")"
  ];
  auto nthInitialier = [
    tk!",", tk!"identifier", tk!"(", tk!"identifier", tk!")"
  ];

  uint result = 0;
  for (auto it = tokens; !it.empty; it.popFront) {
    if (it.atSequence(firstInitializer) || it.atSequence(nthInitialier)) {
      it.popFront;
      auto outerIdentifier = it.front;
      it.popFrontN(2);
      auto innerIdentifier = it.front;
      bool isMember = outerIdentifier.value_.back == '_'
        || outerIdentifier.value_.startsWith("m_");
      if (isMember && outerIdentifier.value_ == innerIdentifier.value_) {
        lintError(outerIdentifier, text(
          "Looks like you're initializing class member [",
          outerIdentifier.value_, "] with itself.\n")
        );
        ++result;
      }
    }
  }
  return result;
}

/**
 * Lint check: check for blacklisted sequences of tokens.
 */
uint checkBlacklistedSequences(string fpath, CppLexer.Token[] v) {
  struct BlacklistEntry {
    CppLexer.TokenType2[] tokens;
    string descr;
    bool cpponly;
  }

  const static BlacklistEntry[] blacklist = [
    BlacklistEntry([tk!"volatile"],
      "'volatile' does not make your code thread-safe. If multiple threads are " ~
      "sharing data, use std::atomic or locks. In addition, 'volatile' may " ~
      "force the compiler to generate worse code than it could otherwise. " ~
      "For more about why 'volatile' doesn't do what you think it does, see " ~
      "http://www.kernel.org/doc/Documentation/" ~
      "volatile-considered-harmful.txt.\n",
                   true), // C++ only.
  ];

  const static CppLexer.TokenType2[][] exceptions = [
    [CppLexer.tk!"asm", CppLexer.tk!"volatile"],
  ];

  uint result = 0;
  bool isException = false;

  foreach (i; 0 .. v.length) {
    foreach (e; exceptions) {
      if (atSequence(v[i .. $], e)) { isException = true; break; }
    }
    foreach (ref entry; blacklist) {
      if (!atSequence(v[i .. $], entry.tokens)) { continue; }
      if (isException) { isException = false; continue; }
      if (c_mode && entry.cpponly == true) { continue; }
      if (v[i].precedingWhitespace_.canFind("nolint")) { continue; }
      lintWarning(v[i], entry.descr);
      ++result;
    }
  }

  return result;
}

uint checkBlacklistedIdentifiers(string fpath, CppLexer.Token[] v) {
  uint result = 0;

  string[string] banned = [
    "strtok" :
      "strtok() is not thread safe, and has safer alternatives.  Consider " ~
      "folly::split or strtok_r as appropriate.\n",
    "strncpy" :
      "strncpy is very often used in error; see " ~
      "http://meyering.net/crusade-to-eliminate-strncpy/\n"
  ];

  foreach (ref t; v) {
    if (t.type_ != tk!"identifier") continue;
    auto mapIt = t.value_ in banned;
    if (!mapIt) continue;
    lintError(t, *mapIt);
    ++result;
  }

  return result;
}

/**
 * Lint check: no #defined names use an identifier reserved to the
 * implementation.
 *
 * These are enforcing rules that actually apply to all identifiers,
 * but we're only raising warnings for #define'd ones right now.
 */
uint checkDefinedNames(string fpath, Token[] v) {
  // Define a set of exception to rules
  static bool[string] okNames;
  if (okNames.length == 0) {
    static string[] okNamesInit = [
      "__STDC_LIMIT_MACROS",
      "__STDC_FORMAT_MACROS",
      "_GNU_SOURCE",
      "_XOPEN_SOURCE",
    ];

    foreach (i; 0 .. okNamesInit.length) {
      okNames[okNamesInit[i]] = true;
    }
  }

  uint result = 0;
  foreach (i, ref t; v) {
    if (i == 0 || v[i - 1].type_ != tk!"#" || t.type_ != tk!"identifier"
        || t.value != "define") continue;
    if (v[i - 1].precedingWhitespace_.canFind("nolint")) continue;
    const t1 = v[i + 1];
    auto const sym = t1.value_;
    if (t1.type_ != tk!"identifier") {
      // This actually happens because people #define private public
      //   for unittest reasons
      lintWarning(t1, text("you're not supposed to #define ", sym, "\n"));
      continue;
    }
    if (sym.length >= 2 && sym[0] == '_' && isUpper(sym[1])) {
      if (sym in okNames) {
        continue;
      }
      lintWarning(t, text("Symbol ", sym, " invalid." ~
        "  A symbol may not start with an underscore followed by a " ~
        "capital letter.\n"));
      ++result;
    } else if (sym.length >= 2 && sym[0] == '_' && sym[1] == '_') {
      if (sym in okNames) {
        continue;
      }
      lintWarning(t, text("Symbol ", sym, " invalid." ~
        "  A symbol may not begin with two adjacent underscores.\n"));
      ++result;
    } else if (!c_mode /* C is less restrictive about this */ &&
        sym.canFind("__")) {
      if (sym in okNames) {
        continue;
      }
      lintWarning(t, text("Symbol ", sym, " invalid." ~
        "  A symbol may not contain two adjacent underscores.\n"));
      ++result;
    }
  }
  return result;
}

/**
 * Lint check: only the following forms of catch are allowed:
 *
 * catch (Type &)
 * catch (const Type &)
 * catch (Type const &)
 * catch (Type & e)
 * catch (const Type & e)
 * catch (Type const & e)
 *
 * Type cannot be built-in; this function enforces that it's
 * user-defined.
 */
uint checkCatchByReference(string fpath, Token[] v) {
  uint result = 0;
  foreach (i, ref e; v) {
    if (e.type_ != tk!"catch") continue;
    if (getSucceedingWhitespace(v[i..$]).canFind("nolint")) continue;
    size_t focal = 1;
    enforce(v[i + focal].type_ == tk!"(", // a "(" comes always after catch
        text(v[i + focal].file_, ":", v[i + focal].line_,
            ": Invalid C++ source code, please compile before lint."));
    ++focal;
    if (v[i + focal].type_ == tk!"...") {
      // catch (...
      continue;
    }
    if (v[i + focal].type_ == tk!"const") {
      // catch (const
      ++focal;
    }
    if (v[i + focal].type_ == tk!"typename") {
      // catch ([const] typename
      ++focal;
    }
    if (v[i + focal].type_ == tk!"::") {
      // catch ([const] [typename] ::
      ++focal;
    }
    // At this position we must have an identifier - the type caught,
    // e.g. FBException, or the first identifier in an elaborate type
    // specifier, such as facebook::FancyException<int, string>.
    if (v[i + focal].type_ != tk!"identifier") {
      const t = v[i + focal];
      lintWarning(t, "Symbol " ~ t.value_ ~ " invalid in " ~
              "catch clause.  You may only catch user-defined types.\n");
      ++result;
      continue;
    }
    ++focal;
    // We move the focus to the closing paren to detect the "&". We're
    // balancing parens because there are weird corner cases like
    // catch (Ex<(1 + 1)> & e).
    for (size_t parens = 0; ; ++focal) {
      enforce(focal < v.length,
          text(v[i + focal].file_, ":", v[i + focal].line_,
              ": Invalid C++ source code, please compile before lint."));
      if (v[i + focal].type_ == tk!")") {
        if (parens == 0) break;
        --parens;
      } else if (v[i + focal].type_ == tk!"(") {
        ++parens;
      }
    }
    // At this point we're straight on the closing ")". Backing off
    // from there we should find either "& identifier" or "&" meaning
    // anonymous identifier.
    if (v[i + focal - 1].type_ == tk!"&") {
      // check! catch (whatever &)
      continue;
    }
    if (v[i + focal - 1].type_ == tk!"identifier" &&
        v[i + focal - 2].type_ == tk!"&") {
      // check! catch (whatever & ident)
      continue;
    }
    // Oopsies times
    const t = v[i + focal - 1];
    // Get the type string
    string theType;
    foreach (j; 2 .. focal - 1) {
      if (j > 2) theType ~= " ";
      theType ~= v[i + j].value;
    }
    lintError(t, text("Symbol ", t.value_, " of type ", theType,
      " caught by value.  Use catch by (preferably const) reference " ~
      "throughout.\n"));
    ++result;
  }
  return result;
}

/**
 * Lint check: any usage of throw specifications is a lint error.
 *
 * We track whether we are at either namespace or class scope by
 * looking for class/namespace tokens and tracking nesting level.  Any
 * time we go into a { } block that's not a class or namespace, we
 * disable the lint checks (this is to avoid false positives for throw
 * expressions).
 */
uint checkThrowSpecification(string, Token[] v) {
  uint result = 0;

  // Check for throw specifications inside classes
  result += v.iterateClasses!(
     function uint(Token[] it, Token[] v) {
      uint result = 0;

      it = it.find!(a => a.type_ == tk!"{");
      if (it.empty) {
        return result;
      }

      it.popFront;

      const destructorSequence =
        [tk!"~", tk!"identifier", tk!"(", tk!")",
         tk!"throw", tk!"(", tk!")"];

      for (; !it.empty && it.front.type_ != tk!"\0"; it.popFront) {
        // Skip warnings for empty throw specifications on destructors,
        // because sometimes it is necessary to put a throw() clause on
        // classes deriving from std::exception.
        if (it.atSequence(destructorSequence)) {
          it.popFrontN(destructorSequence.length - 1);
          continue;
        }

        // This avoids warning if the function is named "what", to allow
        // inheriting from std::exception without upsetting lint.
        if (it.front.type_ == tk!"identifier" && it.front.value_ == "what") {
          it.popFront;
          auto sequence = [tk!"(", tk!")", tk!"const",
                           tk!"throw", tk!"(", tk!")"];
          if (it.atSequence(sequence)) {
            it.popFrontN(sequence.length - 1);
          }
          continue;
        }

        if (it.front.type_ == tk!"{") {
          it = skipBlock(it);
          continue;
        }

        if (it.front.type_ == tk!"}") {
          break;
        }

        if (it.front.type_ == tk!"throw" && it[1].type_ == tk!"(") {
          lintWarning(it.front, "Throw specifications on functions are " ~
              "deprecated.\n");
          ++result;
        }
      }

      return result;
    }
  );

  // Check for throw specifications in functional style code
  for (auto it = v; !it.empty; it.popFront) {
    // Don't accidentally identify a using statement as a namespace
    if (it.front.type_ == tk!"using") {
      if (it[1].type_ == tk!"namespace") {
        it.popFront;
      }
      continue;
    }

    // Skip namespaces, classes, and blocks
    if (it.front.type_.among(tk!"namespace", tk!"class", tk!"struct",
            tk!"union", tk!"{")) {
      auto term = it.find!(x => x.type_ == tk!"{");
      if (term.empty) {
        break;
      }
      it = skipBlock(term);
      continue;
    }

    if (it.front.type_ == tk!"throw" && it[1].type_ == tk!"(") {
      lintWarning(it.front, "Throw specifications on functions are " ~
        "deprecated.\n");
      ++result;
    }
  }

  return result;
}

/**
 * Lint check: balance of #if(#ifdef, #ifndef)/#endif.
 */
uint checkIfEndifBalance(string fpath, Token[] v) {
  int openIf = 0;

  // Return after the first found error, because otherwise
  // even one missed #if can be cause of a lot of errors.
  foreach (i, ref e; v) {
    if (v[i .. $].atSequence(tk!"#", tk!"if")
        || (v[i .. $].atSequence(tk!"#", tk!"identifier")
            && (v[i + 1].value_ == "ifndef" || v[i + 1].value_ == "ifdef"))) {
      ++openIf;
    } else if (v[i .. $].atSequence(tk!"#", tk!"identifier")
        && v[i + 1].value_ == "endif") {
      --openIf;
      if (openIf < 0) {
        lintError(e, "Unmatched #endif.\n");
        return 1;
      }
    } else if (v[i .. $].atSequence(tk!"#", tk!"else")) {
      if (openIf == 0) {
        lintError(e, "Unmatched #else.\n");
        return 1;
      }
    }
  }

  if (openIf != 0) {
    lintError(v.back, "Unbalanced #if/#endif.\n");
    return 1;
  }

  return 0;
}

/*
 * Lint check: warn about common errors with constructors, such as:
 *  - single-argument constructors that aren't marked as explicit, to avoid them
 *    being used for implicit type conversion (C++ only)
 *  - Non-const copy constructors, or useless const move constructors.
 *  - Move constructors that aren't marked noexcept
 */
uint checkConstructors(string fpath, Token[] tokensV) {
  if (getFileCategory(fpath) == FileCategory.source_c) {
    return 0;
  }

  uint result = 0;
  string[] nestedClasses;

  const string explicitOverride = "/"~"* implicit *"~"/";
  const CppLexer.TokenType2[] stdInitializerSequence =
    [tk!"identifier", tk!"::", tk!"identifier", tk!"<"];
  const CppLexer.TokenType2[] voidConstructorSequence =
    [tk!"identifier", tk!"(", tk!"void", tk!")"];

  for (auto tox = tokensV; tox.length; tox.popFront) {
    // Avoid mis-identifying a class context due to use of the "class"
    // keyword inside a template parameter list.
    if (tox.atSequence(tk!"template", tk!"<")) {
      tox = skipTemplateSpec(tox[1 .. $]);
      continue;
    }

    // Parse within namespace blocks, but don't do top-level constructor checks.
    // To do this, we treat namespaces like unnamed classes so any later
    // function name checks will not match against an empty string.
    if (tox.front.type_ == tk!"namespace") {
      tox.popFront;
      for (; tox.front.type_ != tk!"\0"; tox.popFront) {
        if (tox.front.type_ == tk!";") {
          break;
        } else if (tox.front.type_ == tk!"{") {
          nestedClasses ~= "";
          break;
        }
      }
      continue;
    }

    // Extract the class name if a class/struct definition is found
    if (tox.front.type_ == tk!"class" || tox.front.type_ == tk!"struct") {
      tox.popFront;
      // If we hit any C-style structs, we'll handle them like we do namespaces:
      // continue to parse within the block but don't show any lint errors.
      if (tox.front.type_ == tk!"{") {
        nestedClasses ~= "";
      } else if (tox.front.type_ == tk!"identifier") {
        auto classCandidate = tox.front.value_;
        for (; tox.front.type_ != tk!"\0"; tox.popFront) {
          if (tox.front.type_ == tk!";") {
            break;
          } else if (tox.front.type_ == tk!"{") {
            nestedClasses ~= classCandidate;
            break;
          }
        }
      }
      continue;
    }

    // Closing curly braces end the current scope, and should always be balanced
    if (tox.front.type_ == tk!"}") {
      if (nestedClasses.empty) { // parse fail
        return result;
      }
      nestedClasses.popBack;
      continue;
    }

    // Skip unrecognized blocks. We only want to parse top-level class blocks.
    if (tox.front.type_ == tk!"{") {
      tox = skipBlock(tox);
      continue;
    }

    // Only check for constructors if we've previously entered a class block
    if (nestedClasses.empty) {
      continue;
    }

    // Handle an "explicit" keyword, with or without "constexpr"
    bool checkImplicit = true;
    if (tox.atSequence(tk!"explicit", tk!"constexpr") ||
        tox.atSequence(tk!"constexpr", tk!"explicit")) {
      checkImplicit = false;
      tox.popFront;
      tox.popFront;
    }
    if (tox.front.type_ == tk!"explicit") {
      checkImplicit = false;
      tox.popFront;
    }

    // Skip anything that doesn't look like a constructor
    if (!tox.atSequence(tk!"identifier", tk!"(")) {
      continue;
    } else if (tox.front.value_ != nestedClasses.back) {
      tox = skipFunctionDeclaration(tox);
      continue;
    }

    // Suppress error and skip past functions clearly marked as implicit
    if (tox.front.precedingWhitespace_.canFind(explicitOverride)) {
      checkImplicit = false;
    }

    // Allow zero-argument void constructors
    if (tox.atSequence(voidConstructorSequence)) {
      tox = skipFunctionDeclaration(tox);
      continue;
    }

    FunctionSpec spec;
    spec.functionName = Argument(tox[0 .. 1]);
    if (tox.front.precedingWhitespace_.canFind(explicitThrowSpec)) {
      spec.explicitThrows = true;
    }

    if (!tox.getFunctionSpec(spec)) {
      // Parse fail can be due to limitations in skipTemplateSpec, such as with:
      // fn(std::vector<boost::shared_ptr<ProjectionOperator>> children);)
      return result;
    }

    // Allow zero-argument constructors
    if (spec.args.empty) {
      tox = skipFunctionDeclaration(tox);
      continue;
    }

    auto argIt = spec.args[0].tox;
    bool foundConversionCtor = false;
    bool isConstArgument = false;
    if (argIt.front.type_ == tk!"const") {
      isConstArgument = true;
      argIt.popFront;
    }

    // Allow implicit std::initializer_list constructors
    if (argIt.atSequence(stdInitializerSequence)
        && argIt.front.value_ == "std"
        && argIt[2].value_ == "initializer_list") {
      checkImplicit = false;
    }

    bool isMoveConstructor = false;
    // Copy/move constructors may have const (but not type conversion) issues
    // Note: we skip some complicated cases (e.g. template arguments) here
    if (argIt.front.value_ == nestedClasses.back) {
      auto nextType = argIt.length ? argIt[1].type_ : tk!"\0";
      if (nextType != tk!"*") {
        if (nextType == tk!"&" && !isConstArgument) {
          ++result;
          lintError(tox.front, text(
            "Copy constructors should take a const argument: ",
            formatFunction(spec), "\n"
            ));
        } else if (nextType == tk!"&&" && isConstArgument) {
          ++result;
          lintError(tox.front, text(
            "Move constructors should not take a const argument: ",
            formatFunction(spec), "\n"
            ));
        }

        if (nextType == tk!"&&") {
          isMoveConstructor = true;
        }

        // Copy constructors and move constructors are allowed to be implict
        checkImplicit = false;
      }
    }

    // Warn about constructors that may be invoked implicitly with a single
    // argument, unless they were marked with a comment saying that allowing
    // implicit construction is intentional.
    if (checkImplicit) {
      if (spec.args.length == 1) {
        foundConversionCtor = true;
      } else if (spec.args.length >= 2) {
        // 2+ will only be an issue if the trailing arguments have defaults
        for (argIt = spec.args[1].tox; !argIt.empty; argIt.popFront) {
          if (argIt.front.type_ == tk!"=") {
            foundConversionCtor = true;
            break;
          }
        }
      }

      if (foundConversionCtor) {
        ++result;
        lintError(tox.front, text(
          "Single-argument constructor '",
          formatFunction(spec),
          "' may inadvertently be used as a type conversion constructor. " ~
          "Prefix the function with the 'explicit' keyword to avoid this, or " ~
          "add an /* implicit *"~"/ comment to suppress this warning.\n"
          ));
      }
    }

    // Warn about move constructors that are not noexcept
    if (isMoveConstructor &&
        !spec.isNoexcept && !spec.explicitThrows && !spec.isDeleted &&
        !spec.isDefault) {
      ++result;
      lintError(tox.front, text(
        "Move constructor '", formatFunction(spec),
        "' should be declared noexcept.  " ~
        "Use a trailing or leading " ~ explicitThrowSpec ~
        " comment to suppress this warning\n"
        ));
    }

    tox = skipFunctionDeclaration(tox);
  }

  return result;
}

/*
 * Lint check: warn about implicit casts
 *
 * Implicit casts not marked as explicit can be dangerous if not used carefully
 */
uint checkImplicitCast(string fpath, Token[] tokensV) {
  if (c_mode || getFileCategory(fpath) == FileCategory.source_c) {
    return 0;
  }

  uint result = 0;

  const string lintOverride = "/"~"* implicit *"~"/";

  TokenType[TokenType] includeGuardDelimiters = [
    tk!"\"": tk!"\"",
    tk!"<" : tk!">"
  ];

  for (auto tox = tokensV; !tox.empty; tox.popFront) {
    // Skip operator in file name being included
    if (tox.atSequence(tk!"#", tk!"identifier") && tox[1].value == "include") {
      if (tox.length > 3 && tox[2].type_ in includeGuardDelimiters) {
        auto endDelimiterType = includeGuardDelimiters[tox[2].type_];
        tox.popFrontN(3);

        while (tox.front.type_ != endDelimiterType) {
          tox.popFront;
        }

        continue;
      }
    }

    // Skip past any functions that begin with an "explicit" keyword
    if (tox.atSequence(tk!"explicit", tk!"constexpr", tk!"operator")) {
      tox.popFrontN(2);
      continue;
    }
    if (tox.atSequence(tk!"explicit", tk!"operator") ||
        tox.atSequence(tk!"::", tk!"operator")) {
      tox.popFront;
      continue;
    }

    // Only want to process operators which do not have the overide
    if (tox.front.type_ != tk!"operator" ||
        tox.front.precedingWhitespace_.canFind(lintOverride)) {
      continue;
    }

    if (tox.atSequence(tk!"operator", tk!"bool", tk!"(", tk!")")) {
      if (tox[4 .. $].atSequence(tk!"=", tk!"delete") ||
          tox[4 .. $].atSequence(tk!"const", tk!"=", tk!"delete")) {
        // Deleted implicit operators are ok.
        continue;
      }

      ++result;
      lintError(tox.front, "operator bool() is dangerous. " ~
        "In C++11 use explicit conversion (explicit operator bool()), " ~
        "otherwise use something like the safe-bool idiom if the syntactic " ~
        "convenience is justified in this case, or consider defining a " ~
        "function (see http://www.artima.com/cppsource/safebool.html for more " ~
        "details).\n"
      );
      continue;
    }

    // Assume it is an implicit conversion unless proven otherwise
    bool isImplicitConversion = false;
    string typeString;
    for (auto typeIt = tox[1 .. $]; !typeIt.empty; typeIt.popFront) {
      if (typeIt.front.type_ == tk!"(") {
        break;
      }

      switch (typeIt.front.type_.id) {
      case tk!"double".id:
      case tk!"float".id:
      case tk!"int".id:
      case tk!"short".id:
      case tk!"unsigned".id:
      case tk!"long".id:
      case tk!"signed".id:
      case tk!"void".id:
      case tk!"bool".id:
      case tk!"wchar_t".id:
      case tk!"char".id:
      case tk!"identifier".id: isImplicitConversion = true; break;
      default:            break;
      }

      if (!typeString.empty()) {
        typeString ~= ' ';
      }
      typeString ~= typeIt.front.value;
    }

    // The operator my not have been an implicit conversion
    if (!isImplicitConversion) {
      continue;
    }

    ++result;
    lintWarning(tox.front, text(
      "Implicit conversion to '",
      typeString,
      "' may inadvertently be used. Prefix the function with the 'explicit'" ~
      " keyword to avoid this, or add an /* implicit *"~"/ comment to" ~
      " suppress this warning.\n"
      ));
  }

  return result;
}

enum AccessRestriction {
  PRIVATE,
  PUBLIC,
  PROTECTED
}

struct ClassParseState {
  string name_;
  AccessRestriction access_;
  Token token_;
  bool has_virt_function_;
  bool ignore_ = true;

  this(string n, AccessRestriction a, Token t) {
    name_ = n;
    access_ = a;
    token_ = t;
    ignore_ = false;
  }
}

/**
 * Lint check: warn about non-virtual destructors in base classes
 */
uint checkVirtualDestructors(string fpath, Token[] v) {
  if (getFileCategory(fpath) == FileCategory.source_c) {
    return 0;
  }

  uint result = 0;
  ClassParseState[] nestedClasses;

  for (auto it = v; !it.empty; it.popFront) {
    // Avoid mis-identifying a class context due to use of the "class"
    // keyword inside a template parameter list.
    if (it.atSequence(tk!"template", tk!"<")) {
      it.popFront;
      it = skipTemplateSpec(it);
      continue;
    }

    // Treat namespaces like unnamed classes
    if (it.front.type_ == tk!"namespace") {
      it.popFront;
      for (; it.front.type_ != tk!"\0"; it.popFront) {
        if (it.front.type_ == tk!";") {
          break;
        } else if (it.front.type_ == tk!"{") {
          nestedClasses ~= ClassParseState();
          break;
        }
      }
      continue;
    }

    if (it.front.type_ == tk!"class" || it.front.type_ == tk!"struct") {
      auto access = it.front.type_ == tk!"class" ?
          AccessRestriction.PRIVATE : AccessRestriction.PUBLIC;
      auto token = it.front;
      it.popFront;

      // If we hit any C-style structs or non-base classes,
      // we'll handle them like we do namespaces:
      // continue to parse within the block but don't show any lint errors.
      if(it.front.type_ == tk!"{") {
        nestedClasses ~= ClassParseState();
      } else if (it.front.type_ == tk!"identifier") {
        auto classCandidate = it.front.value_;

        for (; it.front.type_ != tk!"\0"; it.popFront) {
          if (it.front.type_ == tk!":") {
            // Skip to the class block if we have a derived class
            for (; it.front.type_ != tk!"\0"; it.popFront) {
              if (it.front.type_ == tk!"{") { // full declaration
                break;
              }
            }
            // Ignore non-base classes
            nestedClasses ~= ClassParseState();
            break;
          } else if (it.front.type_ == tk!"identifier") {
            classCandidate = it.front.value_;
          } else if (it.front.type_ == tk!"{") {
            nestedClasses ~=
              ClassParseState(classCandidate, access, token);
            break;
          }
        }
      }
      continue;
    }

    // Skip unrecognized blocks. We only want to parse top-level class blocks.
    if (it.front.type_ == tk!"{") {
      it = skipBlock(it);
      continue;
    }

    // Only check for virtual methods if we've previously entered a class block
    if (nestedClasses.empty) {
      continue;
    }

    auto c = &(nestedClasses.back);

    // Closing curly braces end the current scope, and should always be balanced
    if (it.front.type_ == tk!"}") {
      if (nestedClasses.empty) { // parse fail
        return result;
      }
      if (!c.ignore_ && c.has_virt_function_) {
        ++result;
        lintWarning(c.token_, text("Base class ", c.name_,
          " has virtual functions but a public non-virtual destructor.\n"));
      }
      nestedClasses.popBack;
      continue;
    }

    // Virtual function
    if (it.front.type_ == tk!"virtual") {
      if (it[1].type_ == tk!"~") {
        // Has virtual destructor
        c.ignore_ = true;
        it = skipFunctionDeclaration(it);
        continue;
      }
      c.has_virt_function_ = true;
      it = skipFunctionDeclaration(it);
      continue;
    }

    // Non-virtual destructor
    if (it.atSequence(tk!"~", tk!"identifier")) {
      if (c.access_ != AccessRestriction.PUBLIC) {
        c.ignore_ = true;
      }
      it = skipFunctionDeclaration(it);
      continue;
    }

    if (it.front.type_ == tk!"public") {
      c.access_ = AccessRestriction.PUBLIC;
    } else if (it.front.type_ == tk!"protected") {
      c.access_ = AccessRestriction.PROTECTED;
    } else if (it.front.type_ == tk!"private") {
      c.access_ = AccessRestriction.PRIVATE;
    }
  }
  return result;
}

/**
 * Lint check: if header file contains include guard.
 */
uint checkIncludeGuard(string fpath, Token[] v) {
  if (getFileCategory(fpath) != FileCategory.header) {
    return 0;
  }

  // Allow override-include-guard
  enum override_string = "override-include-guard";
  if (!v.empty && v.front().precedingWhitespace_.canFind(override_string)) {
    return 0;
  }

  // Allow #pragma once
  if (v.atSequence(tk!"#", tk!"identifier", tk!"identifier")
      && v[1].value_ == "pragma" && v[2].value_ == "once") {
    return 0;
  }

  // Looking for the include guard:
  //   #ifndef [name]
  //   #define [name]
  if (!v.atSequence(tk!"#", tk!"identifier", tk!"identifier",
          tk!"#", tk!"identifier", tk!"identifier")
      || v[1].value_ != "ifndef" || v[4].value_ != "define") {
    // There is no include guard in this file.
    lintError(v.front(),
        text("Missing include guard. If you are ABSOLUTELY sure that you " ~
             "don't want an include guard, then include a comment " ~
             "containing ", override_string, " in lieu of an include " ~
             "guard.\n"));
    return 1;
  }

  uint result = 0;

  // Check if there is a typo in guard name.
  if (v[2].value_ != v[5].value_) {
    lintError(v[3], text("Include guard name mismatch; expected ",
      v[2].value_, ", saw ", v[5].value_, ".\n"));
    ++result;
  }

  int openIf = 0;
  for (size_t i = 0; i != v.length; ++i) {
    if (v[i].type_ == tk!"\0") break;

    // Check if we have something after the guard block.
    if (openIf == 0 && i != 0) {
      lintError(v[i], "Include guard doesn't cover the entire file.\n");
      ++result;
      break;
    }

    if (v[i .. $].atSequence(tk!"#", tk!"if")
        || (v[i .. $].atSequence(tk!"#", tk!"identifier")
            && v[i + 1].value_.among("ifndef", "ifdef"))) {
      ++openIf;
    } else if (v[i .. $].atSequence(tk!"#", tk!"identifier")
        && v[i + 1].value_ == "endif") {
      ++i; // hop over the else
      --openIf;
    }
  }

  return result;
}

uint among(T, U...)(auto ref T t, auto ref U options) if (U.length >= 1) {
  foreach (i, unused; U) {
    if (t == options[i]) return i + 1;
  }
  return 0;
}

/**
 * Lint check: inside a header file, namespace facebook must be introduced
 * at top level only, using namespace directives are not allowed, unless
 * they are scoped to an inline function or function template.
 */
uint checkUsingDirectives(string fpath, Token[] v) {
  if (!isHeader(fpath)) {
    // This check only looks at headers. Inside .cpp files, knock
    // yourself out.
    return 0;
  }
  uint result = 0;
  uint openBraces = 0;
  string[] namespaceStack; // keep track of which namespace we are in
  immutable string lintOverride = "using override";
  immutable string lintOverrideMessage =
    "Override this lint warning with a /* " ~ lintOverride ~ " */ comment\n";

  for (auto i = v; !i.empty; i.popFront) {
    if (i.front.type_ == tk!"{") {
      ++openBraces;
      continue;
    }
    if (i.front.type_ == tk!"}") {
      if (openBraces == 0) {
        // Closed more braces than we had.  Syntax error.
        return 0;
      }
      if (openBraces == namespaceStack.length) {
        // This brace closes namespace.
        namespaceStack.length--;
      }
      --openBraces;
      continue;
    }
    if (i.front.type_ == tk!"namespace") {
      // Namespace alias doesn't open a scope.
      if (i[1 .. $].atSequence(tk!"identifier", tk!"=")) {
        continue;
      }

      // If we have more open braces than namespace, someone is trying
      // to nest namespaces inside of functions or classes here
      // (invalid C++), so we have an invalid parse and should give
      // up.
      if (openBraces != namespaceStack.length) {
        return result;
      }

      // Introducing an actual namespace.
      if (i[1].type_ == tk!"{") {
        // Anonymous namespace, let it be. Next iteration will hit the '{'.
        namespaceStack ~= "anonymous";
        continue;
      }

      i.popFront;
      if (i.front.type_ != tk!"identifier") {
        // Parse error or something.  Give up on everything.
        return result;
      }
      if (i.front.value_ == "facebook" && i[1].type_ == tk!"{") {
        // Entering facebook namespace
        if (openBraces > 0) {
          lintError(i.front, "Namespace facebook must be introduced " ~
            "at top level only.\n");
          ++result;
        }
      }
      if (i[1].type_ != tk!"{" && i[1].type_ != tk!"::") {
        // Invalid parse for us.
        return result;
      }
      namespaceStack ~= i.front.value_;
      // Continue analyzing, next iteration will hit the '{' or the '::'
      continue;
    }
    if (i.front.type_ == tk!"using") {

      // We're on a "using" keyword
      auto usingTok = i.front;
      i.popFront;

      if (usingTok.precedingWhitespace_.canFind(lintOverride)) {
        continue;
      }

      // look for 'using namespace' or 'using x::y'
      bool usingCompound = i.atSequence(tk!"identifier", tk!"::",
                                        tk!"identifier");

      if (!usingCompound && (i.front.type_ != tk!"namespace")) {
        continue;
      }
      else {
        string errorPrefix = usingCompound ? warningPrefix : "";
        if (openBraces == 0) {
          lintError(i.front, errorPrefix ~ "Using directive not allowed at top " ~
                    "level or inside namespace facebook. "
                    ~ lintOverrideMessage);
          ++result;
        } else if (openBraces == namespaceStack.length) {
          // It is only an error to pollute the facebook or global namespaces,
          // otherwise it is a warning
          if (namespaceStack.length >= 1 && namespaceStack[$-1] != "facebook") {
            errorPrefix = warningPrefix;
          }

          // We are directly inside the namespace.
          lintError(i.front, errorPrefix ~ "Using directive not allowed in " ~
                    "header file, unless it is scoped to an inline function " ~
                    "or function template. "
                    ~ lintOverrideMessage);
          ++result;
        }
      }
    }
  }
  return result;
}

/**
 * Lint check: don't allow certain "using namespace" directives to occur
 * together, e.g. if "using namespace std;" and "using namespace boost;"
 * occur, we should warn because names like "shared_ptr" are ambiguous and
 * could refer to either "std::shared_ptr" or "boost::shared_ptr".
 */
uint checkUsingNamespaceDirectives(string fpath, Token[] v) {
  bool[string][] MUTUALLY_EXCLUSIVE_NAMESPACES = [
    [ "std":1, "std::tr1":1, "boost":1, "::std":1, "::std::tr1":1, "::boost":1 ],
    // { "list", "of", "namespaces", "that", "should::not::appear", "together" }
  ];

  uint result = 0;
  // (namespace => line number) for all visible namespaces
  // we can probably simplify the implementation by getting rid of this and
  // performing a "nested scope lookup" by looking up symbols in the current
  // scope, then the enclosing scope etc.
  size_t[string] allNamespaces;
  bool[string][] nestedNamespaces;
  int[] namespaceGroupCounts = new int[MUTUALLY_EXCLUSIVE_NAMESPACES.length];

  ++nestedNamespaces.length;
  assert(!nestedNamespaces.back.length);
  for (auto i = v; !i.empty; i.popFront) {
    if (i.front.type_ == tk!"{") {
      // create a new set for all namespaces in this scope
      ++nestedNamespaces.length;
    } else if (i.front.type_ == tk!"}") {
      if (nestedNamespaces.length == 1) {
        // closed more braces than we had.  Syntax error.
        return 0;
      }
      // delete all namespaces that fell out of scope
      foreach (iNs, unused; nestedNamespaces.back) {
        allNamespaces.remove(iNs);
        foreach (ii, iGroup; MUTUALLY_EXCLUSIVE_NAMESPACES) {
          if (iNs in iGroup) {
            --namespaceGroupCounts[ii];
          }
        }
      }
      nestedNamespaces.popBack;
    } else if (i.atSequence(tk!"using", tk!"namespace")) {
      i.popFrontN(2);
      // crude method for getting the namespace name; this assumes the
      // programmer puts a semicolon at the end of the line and doesn't do
      // anything else invalid
      string ns;
      while (i.front.type_ != tk!";") {
        ns ~= i.front.value;
        i.popFront;
      }
      auto there = ns in allNamespaces;
      if (there) {
        // duplicate using namespace directive
        size_t line = *there;
        string error = format("Duplicate using directive for " ~
            "namespace \"%s\" (line %s).\n", ns, line);
        lintError(i.front, error);
        ++result;
        continue;
      } else {
        allNamespaces[ns] = i.front.line_;
      }
      nestedNamespaces.back[ns] = true;
      // check every namespace group for this namespace
      foreach (ii, ref iGroup; MUTUALLY_EXCLUSIVE_NAMESPACES) {
        if (ns !in iGroup) {
          continue;
        }
        if (namespaceGroupCounts[ii] >= 1) {
          // mutual exclusivity violated
          // find the first conflicting namespace in the file
          string conflict;
          size_t conflictLine = size_t.max;
          foreach (iNs, unused; iGroup) {
            if (iNs == ns) {
              continue;
            }
            auto it = iNs in allNamespaces;
            if (it && *it < conflictLine) {
              conflict = iNs;
              conflictLine = *it;
            }
          }
          string error = format("Using namespace conflict: \"%s\" " ~
              "and \"%s\" (line %s).\n",
              ns, conflict, conflictLine);
          lintError(i.front, error);
          ++result;
        }
        ++namespaceGroupCounts[ii];
      }
    }
  }

  return result;
}

/**
 * Lint check: don't allow heap allocated exception, i.e. throw new Class()
 *
 * A simple check for two consecutive tokens "throw new"
 *
 */
uint checkThrowsHeapException(string fpath, Token[] v) {
  uint result = 0;
  for (; !v.empty; v.popFront) {
    if (v.atSequence(tk!"throw", tk!"new")) {
      size_t focal = 2;
      string msg;

      if (v[focal].type_ == tk!"identifier") {
        msg = text("Heap-allocated exception: throw new ", v[focal].value_,
            "();");
      } else if (v[focal .. $].atSequence(tk!"(", tk!"identifier", tk!")")) {
        // Alternate syntax throw new (Class)()
        ++focal;
        msg = text("Heap-allocated exception: throw new (",
                       v[focal].value_, ")();");
      } else {
        // Some other usage of throw new Class().
        msg = "Heap-allocated exception: throw new was used.";
      }
      lintError(v[focal], text(msg, "\n  This is usually a mistake in C++. " ~
        "Please refer to the C++ Primer (https://www.intern.facebook.com/" ~
        "intern/wiki/images/b/b2/C%2B%2B--C%2B%2B_Primer.pdf) for FB exception " ~
        "guidelines.\n"));
      ++result;
    }
  }
  return result;
}

/**
 * Lint check: if source has explicit references to HPHP namespace, ensure
 * there is at least a call to f_require_module('file') for some file.
 *
 *  {
 *  }
 *
 *  using namespace HPHP;
 *  using namespace ::HPHP;
 *
 *  [using] HPHP::c_className
 *  [using] ::HPHP::c_className
 *  HPHP::f_someFunction();
 *  HPHP::c_className.mf_memberFunc();
 *  ::HPHP::f_someFunction();
 *  ::HPHP::c_className.mf_memberFunc();
 *
 *  Also, once namespace is opened, it can be used bare, so we have to
 *  blacklist known HPHP class and function exports f_XXXX and c_XXXX.  It
 *  should be noted that function and class references are not the only
 *  potentially dangerous references, however they are by far the most common.
 *  A few FXL functions use constants as well.  Unfortunately, the HPHP
 *  prefixes are particularly weak and may clash with common variable
 *  names, so we try to be as careful as possible to make sure the full
 *  scope on all identifiers is resolved.  Specifically excluded are
 *
 *  c_str (outside using namespace declaration)
 *  ::f_0
 *  OtherScope::f_func
 *  ::OtherScope::c_class
 *  somecomplex::nameorclass::reference
 *
 */
uint checkHPHPNamespace(string fpath, Token[] v) {
  uint result = 0;
  uint openBraces = 0;
  uint useBraceLevel = 0;
  bool usingHPHPNamespace = false;
  bool gotRequireModule = false;
  static const blacklist =
    ["c_", "f_", "k_", "ft_"];
  bool isSigmaFXLCode = false;
  Token sigmaCode;
  for (auto i = v; !i.empty; i.popFront) {
    auto s = toLower(i.front.value);
    bool boundGlobal = false;

    // Track nesting level to determine when HPHP namespace opens / closes
    if (i.front.type_ == tk!"{") {
      ++openBraces;
      continue;
    }
    if (i.front.type_ == tk!"}") {
      if (openBraces) {
        --openBraces;
      }
      if (openBraces < useBraceLevel) {
        usingHPHPNamespace = false;
        gotRequireModule = false;
      }
      continue;
    }

    // Find using namespace declarations
    if (i.atSequence(tk!"using", tk!"namespace")) {
      i.popFrontN(2);
      if (i.front.type_ == tk!"::") {
        // optional syntax
        i.popFront;
      }
      if (i.front.type_ != tk!"identifier") {
        lintError(i.front, text("Symbol ", i.front.value_,
                " not valid in using namespace declaration.\n"));
        ++result;
        continue;
      }
      if (i.front.value == "HPHP" && !usingHPHPNamespace) {
        usingHPHPNamespace = true;
        useBraceLevel = openBraces;
        continue;
      }
    }

    // Find identifiers, but make sure we start from top level name scope
    if (i.atSequence(tk!"::", tk!"identifier")) {
      i.popFront;
      boundGlobal = true;
    }
    if (i.front.type_ == tk!"identifier") {
      bool inHPHPScope = usingHPHPNamespace && !boundGlobal;
      bool boundHPHP = false;
      if (i[1 .. $].atSequence(tk!"::", tk!"identifier")) {
        if (i.front.value == "HPHP") {
          inHPHPScope = true;
          boundHPHP = true;
          i.popFrontN(2);
        }
      }
      if (inHPHPScope) {
        if (i.front.value_ == "f_require_module") {
          gotRequireModule = true;
        }
        // exempt std::string.c_str
        if (!gotRequireModule && !(i.front.value_ == "c_str" && !boundHPHP)) {
          foreach (l; blacklist) {
            if (i.front.value.length > l.length) {
              auto substr = i.front.value[0 .. l.length];
              if (substr == l) {
                lintError(i.front, text("Missing f_require_module before " ~
                  "suspected HPHP namespace reference ", i.front.value_, "\n"));
                ++result;
              }
            }
          }
        }
      }
      // strip remaining sub-scoped tokens
      while (i.atSequence(tk!"identifier", tk!"::")) {
        i.popFrontN(2);
      }
    }
  }
  return result;
}


/**
 * Lint checks:
 * 1) Warn if any file includes a deprecated include.
 * 2) Warns about include certain "expensive" headers in other headers
 */
uint checkQuestionableIncludes(string fpath, Token[] v) {
  // Set storing the deprecated includes. Add new headers here if you'd like
  // to deprecate them
  const bool[string] deprecatedIncludes = [
    "common/base/Base.h":1,
    "common/base/StlTypes.h":1,
    "common/base/StringUtil.h":1,
    "common/base/Types.h":1,
  ];

  // Set storing the expensive includes. Add new headers here if you'd like
  // to mark them as expensive
  const bool[string] expensiveIncludes = [
    "multifeed/aggregator/gen-cpp/aggregator_types.h":1,
    "multifeed/shared/gen-cpp/multifeed_types.h":1,
    "admarket/adfinder/if/gen-cpp/adfinder_types.h":1,
  ];

  bool includingFileIsHeader = (getFileCategory(fpath) == FileCategory.header);
  uint result = 0;
  for (; !v.empty; v.popFront) {
    IncludedPath ipath;
    if (!getIncludedPath(v, ipath)) {
      continue;
    }

    string includedFile = ipath.path;

    if (includedFile in deprecatedIncludes) {
      lintWarning(v.front, text("Including deprecated header ",
              includedFile, "\n"));
      ++result;
    }
    if (includingFileIsHeader && includedFile in expensiveIncludes) {
      lintWarning(v.front,
                  text("Including expensive header ",
                       includedFile, " in another header, prefer to forward ",
                       "declare symbols instead if possible\n"));
      ++result;
    }
  }
  return result;
}

/**
 * Lint check: Ensures .cpp files include their associated header first
 * (this catches #include-time dependency bugs where .h files don't
 * include things they depend on)
 */
uint checkIncludeAssociatedHeader(string fpath, Token[] v) {
  if (!isSource(fpath)) {
    return 0;
  }

  auto fileName = fpath.baseName;
  auto fileNameBase = getFileNameBase(fileName);
  auto parentPath = fpath.absolutePath.dirName.buildNormalizedPath;
  uint totalIncludesFound = 0;

  for (; !v.empty; v.popFront) {
    IncludedPath ipath;
    if (!getIncludedPath(v, ipath)) {
      continue;
    }

    // Skip PRECOMPILED #includes, or #includes followed by a 'nolint' comment
    if (ipath.precompiled || ipath.nolint) {
      continue;
    }

    ++totalIncludesFound;

    string includedFile = ipath.path.baseName;
    string includedParentPath = ipath.path.dirName;
    if (includedParentPath == ".") includedParentPath = null;

    if (getFileNameBase(includedFile) == fileNameBase &&
        (includedParentPath.empty ||
         parentPath.endsWith('/' ~ includedParentPath))) {
      if (totalIncludesFound > 1) {
        lintError(v.front, text("The associated header file of .cpp files " ~
                "should be included before any other includes.\n(This " ~
                "helps catch missing header file dependencies in the .h)\n"));
        return 1;
      }
      return 0;
    }
  }
  return 0;
}

/**
 * Lint check: if encounter memset(foo, sizeof(foo), 0), we warn that the order
 * of the arguments is wrong.
 * Known unsupported case: calling memset inside another memset. The inner
 * call will not be checked.
 */
uint checkMemset(string fpath, Token[] v) {
  uint result = 0;

  for (; !v.empty; v.popFront) {
    if (!v.atSequence(tk!"identifier", tk!"(") || v.front.value_ != "memset") {
      continue;
    }
    FunctionSpec spec;
    if (!getFunctionSpec(v, spec)) {
      return result;
    }

    // If there are more than 3 arguments, then there might be something wrong
    // with skipTemplateSpec but the iterator didn't reach the EOF (because of
    // a '>' somewhere later in the code). So we only deal with the case where
    // the number of arguments is correct.
    if (spec.args.length == 3) {
      // wrong calls include memset(..., ..., 0) and memset(..., sizeof..., 1)
      bool error =
        (spec.args[2].tox.length == 1) &&
        (
          (spec.args[2].first.value_ == "0") ||
          (spec.args[2].first.value_ == "1" &&
           spec.args[1].first.type_ == tk!"sizeof")
        );
      if (!error) {
        continue;
      }
      swap(spec.args[1], spec.args[2]);
      lintError(spec.functionName.first,
        "Did you mean " ~ formatFunction(spec) ~ "?\n");
      result++;
    }
  }
  return result;
}

uint checkInlHeaderInclusions(string fpath, Token[] v) {
  uint result = 0;

  auto fileName = fpath.baseName;
  auto fileNameBase = getFileNameBase(fileName);

  for (; !v.empty; v.popFront) {
    IncludedPath ipath;
    if (!getIncludedPath(v, ipath)) {
      continue;
    }

    auto includedPath = ipath.path;
    if (getFileCategory(includedPath) != FileCategory.inl_header) {
      continue;
    }

    if (includedPath.baseName.getFileNameBase == fileNameBase) {
      continue;
    }

    lintError(v.front, text("A -inl file (", includedPath, ") was " ~
      "included even though this is not its associated header.  " ~
      "Usually files like Foo-inl.h are implementation details and should " ~
      "not be included outside of Foo.h.\n"));
    ++result;
  }

  return result;
}

uint checkFollyDetail(string fpath, Token[] v) {
  if (fpath.canFind("folly")) return 0;

  uint result = 0;
  for (; !v.empty; v.popFront) {
    if (!v.atSequence(tk!"identifier", tk!"::", tk!"identifier", tk!"::")) {
      continue;
    }
    if (v.front.value_ == "folly" && v[2].value_ == "detail") {
      lintError(v.front, text("Code from folly::detail is logically " ~
                              "private, please avoid use outside of " ~
                              "folly.\n"));
      ++result;
    }
  }

  return result;
}

uint checkFollyStringPieceByValue(string fpath, Token[] v) {
  uint result = 0;
  for (; !v.empty; v.popFront) {
    if ((v.atSequence(tk!"const", tk!"identifier", tk!"&") &&
         v[1].value_ == "StringPiece") ||
        (v.atSequence(tk!"const", tk!"identifier", tk!"::",
                      tk!"identifier", tk!"&") &&
         v[1].value_ == "folly" &&
         v[3].value_ == "StringPiece")) {
      lintWarning(v.front, text("Pass folly::StringPiece by value " ~
                                "instead of as a const reference.\n"));
      ++result;
    }
  }

  return result;
}

/**
 * Lint check: classes should not have protected inheritance.
 */
uint checkProtectedInheritance(string fpath, Token[] v) {

  uint result = v.iterateClasses!(
    (Token[] it, Token[] v) {
      uint result = 0;
      auto term = it.find!((t) => t.type_.among(tk!":", tk!"{"));

      if (term.empty) {
        return result;
      }

      for (; it.front.type_ != tk!"\0"; it.popFront) {
        if (it.front.type_.among(tk!"{", tk!";")) {
          break;
        }

        // Detect a member access specifier.
        if (it.atSequence(tk!"protected", tk!"identifier")) {
          lintWarning(it.front, "Protected inheritance is sometimes not a good " ~
              "idea. Read http://stackoverflow.com/questions/" ~
              "6484306/effective-c-discouraging-protected-inheritance " ~
              "for more information.\n");
          ++result;
        }
      }

      return result;
    }
  );

  return result;
}

uint checkUpcaseNull(string fpath, Token[] v) {
  uint ret = 0;
  foreach (ref t; v) {
    if (t.type_ == tk!"identifier" && t.value_ == "NULL") {
      lintAdvice(t,
        "Prefer `nullptr' to `NULL' in new C++ code.  Unlike `NULL', " ~
        "`nullptr' can't accidentally be used in arithmetic or as an " ~
        "integer. See " ~
        "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2431.pdf" ~
        " for details.\n");
      ++ret;
    }
  }
  return ret;
}

static bool endsClass(CppLexer.TokenType2 tkt) {
  return tkt.among(tk!"\0", tk!"{", tk!";") != 0;
}

static bool isAccessSpecifier(CppLexer.TokenType2 tkt) {
  return tkt.among(tk!"private", tk!"public", tk!"protected") != 0;
}

static bool checkExceptionAndSkip(ref Token[] it) {
  if (it.atSequence(tk!"identifier", tk!"::")) {
    if (it.front.value_ != "std") {
      it.popFrontN(2);
      return false;
    }
    it.popFrontN(2);
  }

  return it.front.type_ == tk!"identifier" && it.front.value_ == "exception";
}

static bool badExceptionInheritance(TokenType classType, TokenType access) {
  return (classType == tk!"class" && access != tk!"public") ||
    (classType == tk!"struct" && access == tk!"private");
}

/**
 * Check for non-public std::exception inheritance.
 *
 * Enforces the following:
 *  1. "class foo: <access-spec> std::inheritance"
 *    is bad if "<access-spec>" is not "public"
 *  2. struct foo: <access-spec> std::inheritance"
 *    is bad if "<access-spec>" is "private"
 *  Handles multiple inheritance.
 *
 *  Assumptions:
 *  1. If "exception" is not prefixed with a
 *  namespace, it is in the "std" namespace.
 */
uint checkExceptionInheritance(string fpath, Token[] v) {
  uint result = v.iterateClasses!(
    (Token[] it, Token[] v) {
      auto classType = it.front.type_; // struct, union or class

      if (classType == tk!"union") return 0;

      while (!endsClass(it.front.type_) && it.front.type_ != tk!":") {
        it.popFront;
      }
      if (it.front.type_ != tk!":") {
        return 0;
      }

      it.popFront;

      auto access = tk!"protected"; // not really, just a safe initializer
      bool warn = false;
      while (!endsClass(it.front.type_)) {
        if (isAccessSpecifier(it.front.type_)) {
          access = it.front.type_;
        } else if (it.front.type_ == tk!",") {
          access = tk!"protected"; // reset
        } else if (checkExceptionAndSkip(it)) {
          warn = badExceptionInheritance(classType, access);
        }
        if (warn) {
          lintWarning(it.front, "std::exception should not be inherited " ~
            "non-publicly, as this base class will not be accessible in " ~
            "try..catch(const std::exception& e) outside the derived class. " ~
            "See C++ standard section 11.2 [class.access.base] / 4.\n");
          return 1;
        }
        it.popFront;
      }

      return 0;
    }
  );

  return result;
}

/**
 * Lint check: Identifies incorrect usage of unique_ptr() with arrays. In other
 * words the unique_ptr is used with an array allocation, but not declared as
 * an array. The canonical example is: unique_ptr<Foo> Bar(new Foo[8]), which
 * compiles fine but should be unique_ptr<Foo[]> Bar(new Foo[8]).
 */
uint checkUniquePtrUsage(string fpath, Token[] v) {
  uint result = 0;

  for (auto iter = v; !iter.empty; iter.popFront) {
    const ident = readQualifiedIdentifier(iter);
    bool ofInterest =
      (ident.length == 1 && ident[0] == "unique_ptr") ||
      (ident.length == 2 && ident[0] == "std" && ident[1] == "unique_ptr");
    if (!ofInterest) continue;

    // Keep the outer loop separate from the lookahead from here out.
    // We want this after the detection of {std::}?unique_ptr above or
    // we'd give errors on qualified unique_ptr's twice.
    auto i = iter;

    // Save the unique_ptr iterator because we'll raise any warnings
    // on that line.
    const uniquePtrIt = i;

    // Determine if the template parameter is an array type.
    if (i.front.type_ != tk!"<") continue;
    bool uniquePtrHasArray = false;
    i = skipTemplateSpec(i, &uniquePtrHasArray);
    if (i.front.type_ == tk!"\0") {
      return result;
    }
    assert(i.front.type_ == tk!">" || i.front.type_ == tk!">>");
    i.popFront;

    /*
     * We should see an optional identifier, then an open paren, or
     * something is weird so bail instead of giving false positives.
     *
     * Note that we could be looking at a function declaration and its
     * return type right now---we're assuming we won't see a
     * new-expression in the argument declarations.
     */
    if (i.front.type_ == tk!"identifier") i.popFront;
    if (i.front.type_ != tk!"(") continue;
    i.popFront;

    uint parenNest = 1;
    for (; i.front.type_ != tk!"\0"; i.popFront) {
      if (i.front.type_ == tk!"(") {
        ++parenNest;
        continue;
      }

      if (i.front.type_ == tk!")") {
        if (--parenNest == 0) break;
        continue;
      }

      if (i.front.type_ != tk!"new" || parenNest != 1) continue;
      i.popFront;

      // We're looking at the new expression we care about.  Try to
      // ensure it has array brackets only if the unique_ptr type did.
      while (i.front.type_.among(tk!"const", tk!"volatile")) {
        i.popFront;
      }
      while (i.front.type_ == tk!"identifier" || i.front.type_ == tk!"::") {
        i.popFront;
      }
      if (i.front.type_ == tk!"<") {
        i = skipTemplateSpec(i);
        if (i.front.type_ == tk!"\0") return result;
        i.popFront;
      } else {
        while (atBuiltinType(i)) i.popFront;
      }
      while (i.front.type_.among(tk!"*", tk!"const", tk!"volatile")) {
        i.popFront;
      }

      bool newHasArray = i.front.type_ == tk!"[";
      if (newHasArray != uniquePtrHasArray) {
        lintError(uniquePtrIt.front,
          uniquePtrHasArray
            ? text("unique_ptr<T[]> should be used with an array type\n")
            : text("unique_ptr<T> should be unique_ptr<T[]> when " ~
                       "used with an array\n")
        );
        ++result;
      }
      break;
    }
  }

  return result;
}

/**
 * Lint check: Identifies usage of shared_ptr() and suggests replacing with
 * make_shared(). When shared_ptr takes 3 arguments a custom allocator is used
 * and allocate_shared() is suggested.
 * The suggested replacements perform less memory allocations.
 *
 * Overall, matches usages of <namespace>::shared_ptr<T> id(new Ctor(),...);
 * where <namespace> is one of "std", "boost" or "facebook". It also matches
 * unqualified usages.
 * Requires the first argument of the call to be a "new expression" starting
 * with the "new" keyword.
 * That is not inclusive of all usages of that construct but it allows
 * to easily distinguish function calls vs. function declarations.
 * Essentially this function matches the following
 * <namespace>::shared_ptr TemplateSpc identifier Arguments
 * where the first argument starts with "new" and <namespace> is optional
 * and, when present, one of the values described above.
 */
uint checkSmartPtrUsage(string fpath, Token[] v) {
  uint result = 0;

  for (auto i = v; !i.empty; i.popFront) {
    // look for unqualified 'shared_ptr<' or 'namespace::shared_ptr<' where
    // namespace is one of 'std', 'boost' or 'facebook'
    if (i.front.type_ != tk!"identifier") continue;
    const startLine = i;
    const ns = i.front.value;
    if (i[1].type_ == tk!"::") {
      i.popFrontN(2);
      if (!i.atSequence(tk!"identifier", tk!"<")) continue;
    } else if (i[1].type_ != tk!"<") {
      continue;
    }
    const fn = i.front.value;
    // check that we have the functions and namespaces we care about
    if (fn != "shared_ptr") continue;
    if (fn != ns && ns != "std" && ns != "boost" && ns != "facebook") {
      continue;
    }

    // skip over the template specification
    i.popFront;
    i = skipTemplateSpec(i);
    if (i.front.type_ == tk!"\0") {
      return result;
    }
    i.popFront;
    // look for a possible function call
    if (!i.atSequence(tk!"identifier", tk!"(")) continue;

    i.popFront;
    Argument[] args;
    // ensure the function call first argument is a new expression
    if (!getRealArguments(i, args)) continue;

    if (i.front.type_ == tk!")" && i[1].type_ == tk!";"
        && args.length > 0 && args[0].first.type_ == tk!"new") {
      // identifies what to suggest:
      // shared_ptr should be  make_shared unless there are 3 args in which
      // case an allocator is used and thus suggests allocate_shared.
      string newFn = args.length == 3 ? "allocate_shared" :  "make_shared";
      string qFn = ns;
      string qNewFn = newFn;
      if (ns != fn) {
        qFn ~= "::" ~ fn;
        // qNewFn.insert(0, "::").insert(0, ns.str());
        qNewFn = ns ~ "::" ~ qNewFn;
      }
      lintWarning(startLine.front,
          text(qFn, " should be replaced by ", qNewFn, ". ", newFn,
          " performs better with less allocations. Consider changing '", qFn,
          "<Foo> p(new Foo(w));' with 'auto p = ", qNewFn, "<Foo>(w);'\n"));
      ++result;
    }
  }

  return result;
}

/*
 * Lint check: some identifiers are warned on because there are better
 * alternatives to whatever they are.
 */
uint checkBannedIdentifiers(string fpath, Token[] v) {
  uint result = 0;

  // Map from identifier to the rationale.
  string[string] warnings = [
    // https://svn.boost.org/trac/boost/ticket/5699
    //
    // Also: deleting a thread_specific_ptr to an object that contains
    // another thread_specific_ptr can lead to corrupting an internal
    // map.
    "thread_specific_ptr" :
    "There are known bugs and performance downsides to the use of " ~
    "this class. Use folly::ThreadLocal instead.\n",
  ];

  foreach (ref t; v) {
    if (t.type_ != tk!"identifier") continue;
    auto warnIt = t.value_ in warnings;
    if (!warnIt) continue;
    lintError(t, *warnIt);
    ++result;
  }

  return result;
}

/*
 * Lint check: disallow namespace-level static specifiers in C++ headers
 * since it is either redundant (such as for simple integral constants)
 * harmful (generates unnecessary code in each TU). Find more information
 * here: https://our.intern.facebook.com/intern/tasks/?t=2435344
*/
uint checkNamespaceScopedStatics(string fpath, Token[] w) {
  auto v = w;

  if (!isHeader(fpath)) {
    // This check only looks at headers. Inside .cpp files, knock
    // yourself out.
    return 0;
  }

  uint result = 0;
  for (; !v.empty; v.popFront) {
    if (v.atSequence(tk!"namespace", tk!"identifier", tk!"{")) {
      // namespace declaration. Reposition the iterator on TK_LCURL
      v.popFrontN(2);
    } else if (v.atSequence(tk!"namespace", tk!"{")) {
      // nameless namespace declaration. Reposition the iterator on TK_LCURL.
      v.popFront;
    } else if (v.front.type_ == tk!"{") {
      // Found a '{' not belonging to a namespace declaration. Skip the block,
      // as it can only be an aggregate type, function or enum, none of
      // which are interesting for this rule.
      v = skipBlock(v);
    } else if (v.front.type_ == tk!"static") {
      if (!isInMacro(w, w.length - v.length)) {
        lintWarning(v.front,
                    "Avoid using static at global or namespace scope " ~
                    "in C++ header files.\n");
        ++result;
      }
    }
  }

  return result;
}

/*
 * Lint check: disallow the declaration of mutex holders
 * with no name, since that causes the destructor to be called
 * on the same line, releasing the lock immediately.
*/
uint checkMutexHolderHasName(string fpath, Token[] v) {
  if (getFileCategory(fpath) == FileCategory.source_c) {
    return 0;
  }

  bool[string] mutexHolderNames = ["lock_guard":1/*, "shared_lock":1, "unique_lock":1*/];
  uint result = 0;

  for (; !v.empty; v.popFront) {
    if (v.atSequence(tk!"identifier", tk!"<")) {
      if (v.front.value_ in mutexHolderNames) {
        v.popFront;
        v = skipTemplateSpec(v);
        if (v.atSequence(tk!">", tk!"(")) {
          lintError(v.front, "Mutex holder variable declared without a name, " ~
              "causing the lock to be released immediately.\n");
          ++result;
        }
      }
    }
  }

  return result;
}

/*
 * Util method that checks ppath only includes files from `allowed` projects.
 */
uint checkIncludes(
    string ppath,
    Token[] v,
    const string[] allowedPrefixes,
    void function(CppLexer.Token, const string) fn) {
  uint result = 0;

  // Find all occurrences of '#include "..."'. Ignore '#include <...>', since
  // <...> is not used for fbcode includes except to include other OSS
  // projects.
  for (auto it = v; !it.empty; it.popFront) {
    IncludedPath ipath;
    if (!getIncludedPath(it, ipath) || ipath.angleBrackets || ipath.nolint) {
      continue;
    }

    auto includePath = ipath.path;

    // Includes from other projects always contain a '/'.
    auto slash = includePath.findSplitBefore("/");
    if (slash[1].empty) continue;

    // If this prefix is allowed, continue
    if (allowedPrefixes.any!(x => includePath.startsWith(x))) continue;

    // Finally, the lint error.
    fn(it.front, "Open Source Software may not include files from " ~
        "other fbcode projects (except what's already open-sourced). " ~
        "If this is not an fbcode include, please use " ~
        "'#include <...>' instead of '#include \"...\"'. " ~
        "You may suppress this warning by including the " ~
        "comment 'nolint' after the #include \"...\".\n");
    ++result;
  }
  return result;
}

/*
 * Lint check: prevent multiple includes of the same file
 */
uint checkMultipleIncludes(string fpath, Token[] v) {
  uint result = 0;
  bool[string] pathSet;
  for (auto it = v; !it.empty; it.popFront) {
    IncludedPath ipath;
    if (!getIncludedPath(it, ipath) || ipath.nolint) {
      continue;
    }

    auto includePath = ipath.path;
    if (includePath !in pathSet) {
      pathSet[includePath] = true;
    } else {
      lintError(it.front, text("\"", includePath, "\" is included multiple ",
          "times. Please remove one of the #includes.\nTo suppress this ",
          "lint error, add the comment 'nolint' at the end of the include ",
          "line.\n"));
      ++result;
    }
  }
  return result;
}

/*
 * Lint check: prevent OSS-fbcode projects from including other projects
 * from fbcode.
 */
uint checkOSSIncludes(string fpath, Token[] v) {
  // strip fpath of '.../fbcode/', if present
  auto ppath = fpath.findSplitAfter("/fbcode/")[1];

  alias void function(CppLexer.Token, const string) Fn;

  import std.typecons;
  Tuple!(string, string[], Fn)[] projects = [
    tuple("folly/", ["folly/"], &lintError),
    tuple("mcrouter/", ["mcrouter/", "folly/"], &lintError),
    tuple("hphp/",
          ["hphp/", "folly/", "thrift/", "proxygen/lib/",
           "mcrouter/", "squangle/"],
          &lintError),
    tuple("thrift/", ["thrift/", "folly/"], &lintError),
    tuple("squangle/", ["squangle/", "folly/", "thrift/"], &lintError),
    tuple("proxygen/lib/",
          ["proxygen/lib/", "folly/"],
          &lintError),
    tuple("proxygen/httpserver/",
          ["proxygen/httpserver/", "proxygen/lib/", "folly/"],
          &lintError),
  ];

  foreach (ref p; projects) {
    // Only check for OSS projects
    if (!ppath.startsWith(p[0])) continue;

    // <OSS>/facebook is allowed to include non-OSS code
    if (ppath.startsWith(p[0] ~ "facebook/")) return 0;

    return checkIncludes(ppath, v, p[1], p[2]);
  }

  return 0;
}

/*
 * Starting from a left parent brace, skips until it finds the matching
 * balanced right parent brace.
 * Returns: iterator pointing to the final TK_RPAREN
 */
R skipParens(R)(R r) {
  enforce(r.front.type_ == tk!"(");

  uint openParens = 1;
  r.popFront;
  for (; r.front.type_ != tk!"\0"; r.popFront) {
    if (r.front.type_ == tk!"(") {
      ++openParens;
      continue;
    }
    if (r.front.type_ == tk!")") {
      if (!--openParens) {
        break;
      }
      continue;
    }
  }
  return r;
}

/*
 * Lint check: disable use of "break"/"continue" inside
 * SYNCHRONIZED pseudo-statements
*/
uint checkBreakInSynchronized(string fpath, Token[] v) {
  /*
   * structure about the current statement block
   * @name: the name of statement
   * @openBraces: the number of open braces in this block
  */
  struct StatementBlockInfo {
    string name;
    uint openBraces;
  }

  uint result = 0;
  StatementBlockInfo[] nestedStatements;

  for (Token[] tox = v; !tox.empty; tox.popFront) {
    if (tox.front.type_.among(tk!"while", tk!"switch", tk!"do", tk!"for")
        || (tox.front.type_ == tk!"identifier"
        && tox.front.value_.among("FOR_EACH_KV", "FOR_EACH", "FOR_EACH_R",
                                  "FOR_EACH_ENUMERATE"))) {
      StatementBlockInfo s;
      s.name = tox.front.value_;
      s.openBraces = 0;
      nestedStatements ~= s;

      //skip the block in "(" and ")" following "for" statement
      if (tox.front.type_ == tk!"for") {
        tox.popFront;
        tox = skipParens(tox);
      }
      continue;
    }

    if (tox.front.type_ == tk!"{") {
      if (!nestedStatements.empty)
        nestedStatements.back.openBraces++;
      continue;
    }

    if (tox.front.type_ == tk!"}") {
      if (!nestedStatements.empty) {
        nestedStatements.back.openBraces--;
        if(nestedStatements.back.openBraces == 0)
          nestedStatements.popBack;
      }
      continue;
    }

    //incase there is no "{"/"}" in for/while statements
    if (tox.front.type_ == tk!";") {
      if (!nestedStatements.empty &&
          nestedStatements.back.openBraces == 0)
        nestedStatements.popBack;
      continue;
    }

    if (tox.front.type_ == tk!"identifier") {
      string strID = tox.front.value_;
      if (strID.among("SYNCHRONIZED", "UNSYNCHRONIZED", "TIMED_SYNCHRONIZED",
            "SYNCHRONIZED_CONST", "TIMED_SYNCHRONIZED_CONST")) {
        StatementBlockInfo s;
        s.name = "SYNCHRONIZED";
        s.openBraces = 0;
        nestedStatements ~= s;
        continue;
      }
    }

    if (tox.front.type_.among(tk!"break", tk!"continue")) {
      if (!nestedStatements.empty &&
        nestedStatements.back.name == "SYNCHRONIZED") {
        lintError(tox.front, "Cannot use break/continue inside " ~
          "SYNCHRONIZED pseudo-statement\n"
        );
        ++result;
      }
      continue;
    }
  }
  return result;
}

/*
 * Lint check: using C rand(), random_device or
 * RandomInt32/RandomInt64 (under common/base/Random.h)
 * to generate random number is not a good way, use rand32() or rand64() in
 * folly/Random.h instead
 */
uint checkRandomUsage(string fpath, Token[] v) {
  uint result = 0;

  string[string] random_banned = [
    "random_device" :
      "random_device uses /dev/urandom, which is expensive. " ~
      "Use folly::Random::rand32 or other methods in folly/Random.h.\n", 
    "RandomInt32" :
      "using RandomInt32 (in common/base/Random.h) to generate random number " ~
      "is discouraged, please consider folly::Random::rand32().\n",
    "RandomInt64" :
      "using RandomInt64 (in common/base/Random.h) to generate random number " ~
      "is discouraged, please consider folly::Random::rand64().\n",
    "random_shuffle" :
      "std::random_shuffle is bankrupt (see http://fburl.com/evilrand) and is " ~
      "scheduled for removal from C++17. Please consider the overload of" ~
      "std::shuffle that takes a random # generator."
  ];

  for (; !v.empty; v.popFront) {
    auto t = v.front;
    if (t.type_ != tk!"identifier") continue;
    auto mapIt = t.value_ in random_banned;
    if (!mapIt) {
      if (v.atSequence(tk!"identifier", tk!"(", tk!")")
            && t.value_ == "rand") {
          lintWarning(
            t,
            "using C rand() to generate random number causes lock contention, " ~
            "please consider folly::Random::rand32().\n");
          ++result;
      }
      continue;
    }
    lintWarning(t, *mapIt);
    ++result;
  }

  return result;
}

/*
 * Lint check: sleep and sleep-like functions.
 * Sleep calls are rarely actually the solution to your problems
 * and accordingly, warrant explanation.
 */
uint checkSleepUsage(string fpath, Token[] v) {
  uint result = 0;

  immutable string lintOverride = "sleep override";
  immutable string message = "Most sleep calls are inappropriate. " ~
    "Sleep calls are especially harmful in test cases, whereby they make the " ~
    "tests, and by extension, contbuild, flakey. In general, the correctness " ~
    "of a program should not depend on its execution speed.\n" ~
    "Consider condition variables and/or futures as a replacement " ~
    "(see http://fburl.com/SleepsToFuturesDex)." ~
    "\n\nOverride lint rule by preceding the call with a /* sleep override */" ~
    "comment.";
  byte[string] sleepBanned = [
    "sleep" : true,
    "usleep" : true,
  ];
  immutable sequence = [ tk!"identifier", tk!"::", tk!"identifier" ];
  immutable sequenceWithStd = [ tk!"identifier", tk!"::" ].idup ~ sequence;
  bool hasBannedSequenceIdentifiers(const(Token)[] v) {
    return v.length >= 4 && v[0].value_ == "this_thread"
        && (v[2].value_ == "sleep_for" || v[2].value_ == "sleep_until")
        && v[3].type_ == tk!"(";
  }

  for (; !v.empty; v.popFront) {
    auto t = v.front;
    if (t.type_ != tk!"identifier") {
      continue;
    }
    auto matched = t.value_ in sleepBanned
                   && v.length >= 2 && v[1].type_ == tk!"(";
    if (!matched) {
      if (!v.atSequence(sequence)) {
        continue;
      }
      auto sequenceStart = v;
      if (v.atSequence(sequenceWithStd)) {
        sequenceStart = v[2 .. $];
      }
      if (!hasBannedSequenceIdentifiers(sequenceStart)) {
        continue;
      }
      //No double jeopardy when we look past std::
      v = sequenceStart;
    }
    stderr.writeln("----------------", t, " Whitespace: ", t.precedingWhitespace_);
    if (t.precedingWhitespace_.canFind(lintOverride)) {
      continue;
    }

    lintWarning(t, message);
    ++result;
  }

  return result;
}

/**
 * Checks that the proper C++11 headers are directly (i.e. non-transitively)
 * included for instances of std::identifier.
 *
 * This is a first naive grep and needs to be extended but is supposed to be a
 * reasonable first approximation of the types available in the std namespace.
 *
 * There are inaccuracies/mistakes in the stdHeader2ClassesAndStructs
 * resulting from the crudeness of the grep (e.g.
 *   "stdexcept" : ["for"],
 *   "algorithm" : ["uniform_int_distribution"],
 *   "random"    : ["uniform_int_distribution"],
 *   ...
 * ).
 * If this is inacceptable we should use the linter itself and not rely on
 * a grep.
 *
 * For future reference, the associative array initializer to include file
 * map was generated using the following commands:
 *
 * cd $SOME_DIR
 *
 * git -c http.proxy=fwdproxy.any:8080 clone \
 * https://github.com/llvm-mirror/libcxx.git libcxx
 *
 * find $SOME_DIR/libcxx/include/ -name "*" -type f  | grep -v "\.h" \
 * | grep -v "\.tcc" | xargs egrep "class |struct " | grep -v "\*" \
 * | grep -v " _" | grep -v "#include" | grep -v ";" | grep -v // \
 * | grep -v "<" | grep -v "\.\.\." | sed "s:/usr/include/c++/4.4.6/::g" \
 * | sed "s/: / /g" | grep -v "std::" | grep -v "()" | grep -v "#" \
 * | grep -v enum
 * | grep -v __ | egrep -v "<|>|=" | sed "s:/data/users/ntv/libcxx/include/::g"\
 * | sed "s/:/ /g" | sed "s/class//g" | sed s/struct//g
 * | awk {'printf("\"%s\"  : \"%s\",\n", $1, $2)'} | sort | uniq > /tmp/foo
 *
 * cat /tmp/foo
 *
 * Manual tweaks and regexp replaces in emacs
 */
uint checkDirectStdInclude(string fpath, Token[] toks) {
  immutable auto stdHeader2ClassesAndStructs = [
    "algorithm" : [
      "param_type", "uniform_int_distribution"
    ],
    "array" : [
      "array"
    ],
    "atomic" : [
      "atomic", "typedef"
    ],
    "bitset" : [
      "bitset", "reference"
    ],
    "chrono" : [
      "duration", "duration_values", "steady_clock", "system_clock",
      "time_point"
    ],
    "codecvt" : [
      "codecvt_utf16", "codecvt_utf8", "codecvt_utf8_utf16"
    ],
    "complex" : [
      "complex"
    ],
    "condition_variable" : [
      "condition_variable", "condition_variable_any"
    ],
    "deque" : [
      "deque"
    ],
    "exception" : [
      "bad_exception", "exception", "nested_exception"
    ],
    "experimental/dynarray" : [
      "dynarray", "bad_optional_access", "nullopt_t", "optional"
    ],
    "ext/hash_map" : [
      "hash_map", "hash_multimap"
    ],
    "ext/hash_set" : [
      "hash_multiset", "hash_set"
    ],
    "forward_list" : [
      "forward_list"
    ],
    "fstream" : [
      "basic_filebuf", "basic_fstream", "basic_ifstream", "basic_ofstream"
    ],
    "functional" : [
      "bad_function_call", "binary_function", "binary_negate", "binder1st",
      "binder2nd", "reference_wrapper", "unary_function", "unary_negate"
    ],
    "future" : [
      "future", "future_error", "promise", "shared_future"
    ],
    // Do not include initializer_list
    // "initializer_list" : [
    //   "initializer_list"
    // ],
    "ios" : [
      "basic_ios", "ios_base"
    ],
    "istream" : [
      "basic_iostream", "basic_istream"
    ],
    "iterator" : [
      "back_insert_iterator", "front_insert_iterator", "insert_iterator",
      "istreambuf_iterator", "istream_iterator", "iterator",
      "iterator_traits", "ostreambuf_iterator", "ostream_iterator",
      "reverse_iterator"
    ],
    "limits" : [
      "numeric_limits"
    ],
    "list" : [
      "list"
    ],
    "locale" : [
      "locale", "wbuffer_convert", "wstring_convert"
    ],
    "map" : [
      "map", "multimap", "value_compare"
    ],
    "memory" : [
      "allocator", "allocator_traits", "auto_ptr", "auto_ptr_ref",
      "bad_weak_ptr", "default_delete", "enable_shared_from_this",
      "pointer_traits", "raw_storage_iterator", "shared_ptr", "unique_ptr",
      "weak_ptr"
    ],
    "mutex" : [
      "lock_guard", "mutex", "once_flag", "recursive_mutex",
      "recursive_timed_mutex", "timed_mutex", "unique_lock"
    ],
    "new" : [
      "bad_alloc", "bad_array_new_length"
    ],
    "ostream" : [
      "basic_ostream"
    ],
    "queue" : [
      "priority_queue", "queue"
    ],
    "random" : [
      "bernoulli_distribution", "binomial_distribution",
      "cauchy_distribution", "chi_squared_distribution",
      "discard_block_engine", "discrete_distribution",
      "exponential_distribution", "extreme_value_distribution",
      "fisher_f_distribution", "gamma_distribution", "geometric_distribution",
      "independent_bits_engine", "linear_congruential_engine",
      "lognormal_distribution", "mersenne_twister_engine",
      "negative_binomial_distribution", "normal_distribution", "param_type",
      "piecewise_constant_distribution", "piecewise_linear_distribution",
      "poisson_distribution", "random_device", "seed_seq",
      "shuffle_order_engine", "student_t_distribution",
      "subtract_with_carry_engine", "UIntType,", "uniform_int_distribution",
      "uniform_real_distribution", "weibull_distribution"
    ],
    "ratio" : [
      "ratio"
    ],
    "regex" : [
      "basic_regex", "match_results", "regex_error", "regex_iterator",
      "regex_token_iterator", "regex_traits", "sub_match"
    ],
    "scoped_allocator" : [
      "rebind", "scoped_allocator_adaptor"
    ],
    "set" : [
      "multiset", "set"
    ],
    "shared_mutex" : [
      "shared_lock", "shared_mutex"
    ],
    "sstream" : [
      "basic_istringstream", "basic_ostringstream", "basic_stringbuf",
      "basic_stringstream"
    ],
    "stack" : [
      "stack"
    ],
    // grep bug
    // "stdexcept" : [
    //   "for"
    // ],
    "streambuf" : [
      "basic_streambuf"
    ],
    "string" : [
      "basic_string", "char_traits", "fpos"
    ],
    "strstream" : [
      "istrstream", "ostrstream", "strstream", "strstreambuf"
    ],
    "system_error" : [
      "error_category", "error_code", "error_condition", "system_error"
    ],
    "thread" : [
      "thread"
    ],
    "tuple" : [
      "tuple"
    ],
    "typeindex" : [
      "type_index"
    ],
    "typeinfo" : [
      "bad_cast", "bad_typeid", "type_info"
    ],
    "type_traits" : [
      "aligned_union", "is_assignable", "is_deible",
      "is_trivially_assignable", "underlying_type"
    ],
    "unordered_map" : [
      "unordered_map", "unordered_multimap"
    ],
    "unordered_set" : [
      "unordered_multiset", "unordered_set"
    ],
    "utility" : [
      "integer_sequence", "pair"
    ],
    "valarray" : [
      "gslice", "gslice_array", "indirect_array", "mask_array", "slice",
      "slice_array", "valarray"
    ],
    "vector" : ["vector"]
  ];

  // Results of a second grep after missing a few things:
  // find /data/users/ntv/libcxx/include/ -name "*" -type f  | grep -v "\.h" |
  // grep -v "\.tcc" | xargs egrep "class |struct " | grep -v "\*" | grep -v
  // "#include" | grep -v ";" | grep -v // | grep -v "<" | grep -v "\.\.\." |
  // sed "s:/usr/include/c++/4.4.6/::g" | sed "s/: / /g" | grep -v "std::" |
  // grep -v "()" | grep -v "#" | grep -v enum | grep -v __ | egrep -v "<|>|=" |
  // sed "s:/data/users/ntv/libcxx/include/::g" | sed "s/_LIBCPP_TYPE_VIS//g" |
  // sed "s/_ONLY//g" | sed "s/:/ /g" | sed "s/  / /g" | grep -v " _" |sort |
  // uniq | more | sed "s/  / /g" | sed "s/class//g" | sed s/struct//g | awk
  // {'printf("\"%s\"  : \"%s\",\n", $1, $2)'} | sort | uniq > /tmp/aaa
  immutable auto stdHeader2ClassesAndStructsGrep2 = [
    "cstddef"  : [
      "nullptr_t"
    ],
    "functional"  : [
      "bad_function_call", "binary_function", "binary_negate",
      "binder1st", "binder2nd", "const_mem_fun1_ref_t", "hash",
      "pointer_to_binary_function", "pointer_to_unary_function",
      "reference_wrapper", "unary_function", "unary_negate"
    ],
    "locale"  : [
      "locale", "messages", "messages_base", "messages_byname",
      "money_base", "money_get", "moneypunct", "moneypunct_byname",
      "money_put", "num_get", "num_put", "time_base", "time_get",
      "time_get_byname", "time_put", "time_put_byname",
      "wbuffer_convert", "wstring_convert"
    ],
    "memory"  : [
      "pointer_safety"
    ],
    "ratio"  : [
      "ratio", "ratio_add", "ratio_divide", "ratio_equal",
      "ratio_greater", "ratio_greater_equal", "ratio_less",
      "ratio_less_equal", "ratio_multiply", "ratio_not_equal",
      "ratio_scoped"
    ],
    "subtract_allocator"  : [
      "rebind", "scoped_allocator_adaptor"
    ],
    "type_traits"  : [
      "aligned_storage", "aligned_union", "common_type",
      "decay", "integral_constant", "is_assignable",
      "is_base_of", "is_conible", "is_copy_conible",
      "is_default_conible", "is_deible", "is_empty",
      "is_move_conible", "is_nothrow_assignable",
      "is_nothrow_conible", "is_nothrow_deible",
      "is_polymorphic", "is_trivially_assignable",
      "is_trivially_conible", "make_signed", "make_unsigned",
      "underlying_type"
    ]
  ];

  // Manual entries for things that may still be missing
  immutable auto stdHeader2ClassesAndStructsManual = [
    "string" : [
      "string"
    ]
  ];

  // These were created by hand
  immutable auto methods = [
    "algorithm" : [
      "all_of", "any_of", "none_of", "for_each", "find", "find_if",
      "find_if_not", "find_end", "find_first_of", "adjacent_find", "count",
      "count_if", "mismatch", "equal", "is_permutation", "search", "search_n",
      "copy", "copy_n", "copy_if", "copy_backward", "move", "move_backward",
      "swap", "swap_ranges", "iter_swap", "transform", "replace", "replace_if",
      "replace_copy", "replace_copy_if", "fill", "fill_n", "generate",
      "generate_n", "remove", "remove_if", "remove_copy", "remove_copy_if",
      "unique", "unique_copy", "reverse", "reverse_copy", "rotate",
      "rotate_copy", "random_shuffle", "shuffle", "is_partitioned", "partition",
      "stable_partition", "partition_copy", "partition_point", "sort",
      "stable_sort", "partial_sort", "partial_sort_copy", "is_sorted",
      "is_sorted_until", "nth_element", "lower_bound", "upper_bound",
      "equal_range", "binary_search", "merge", "inplace_merge", "includes",
      "set_union", "set_intersection", "set_difference",
      "set_symmetric_difference", "push_heap", "pop_heap", "make_heap",
      "sort_heap", "is_heap", "is_heap_until", "min", "max", "minmax",
      "min_element", "max_element", "minmax_element", "lexicographical_compare",
      "next_permutation", "prev_permutation"
    ]
  ];

  auto includeMap = ["" : ""];
  foreach (k, v; stdHeader2ClassesAndStructs) {
    for (size_t i = 0; i < v.length; i++) {
      includeMap[v[i]] = k;
    }
  }
  foreach (k, v; stdHeader2ClassesAndStructsGrep2) {
    for (size_t i = 0; i < v.length; i++) {
      includeMap[v[i]] = k;
    }
  }
  foreach (k, v; stdHeader2ClassesAndStructsManual) {
    for (size_t i = 0; i < v.length; i++) {
      includeMap[v[i]] = k;
    }
  }
  foreach (k, v; methods) {
    for (size_t i = 0; i < v.length; i++) {
      includeMap[v[i]] = k;
    }
  }

  int result;
  string[] parsedIncludes;

  // Also Tokenize the corresponding include's contents
  import std.file;
  Token[] tokens;
  string includePath1 = tr(fpath, ".cpp", ".h", "s");
  string includePath2 = tr(fpath, ".cpp", ".hpp");
  string includePath3 = tr(fpath, ".cpp", ".hxx");
  string includePath  = "";
  if (includePath1.exists &&
      getFileCategory(includePath1) == FileCategory.header) {
    includePath = includePath1;
  } else if (includePath2.exists &&
             getFileCategory(includePath2) == FileCategory.header) {
    includePath = includePath2;
  } else if (includePath3.exists &&
             getFileCategory(includePath3) == FileCategory.header) {
    includePath = includePath3;
  }

  if (includePath.length >= 1) {
    string file = includePath.readText;
    tokens = tokenize(file, includePath) ~ toks;
  } else {
    tokens = toks;
  }

  Token[][string] warningMap;
  for (; !tokens.empty; tokens.popFront) {
    if (tokens.atSequence(tk!"#", tk!"identifier") &&
        tokens[1].value == "include") {
      // Skip relative include paths atm.
      if (tokens[2].value == "<") {
        parsedIncludes ~= tokens[3].value;
      }
      continue;
    }
    if (!tokens.atSequence(tk!"identifier", tk!"::") ||
        tokens[0].value != "std")
      continue;

    // Advance token to xxx in std::xxx
    tokens.popFrontN(2);
    string typeName = tokens[0].value;
    auto p = (typeName in includeMap);
    if (p is null) {
      // This would print a lot of warnings in this first implementation...
      // lintWarning(tokens.front,
      //             text("No entry std::", typeName,
      //                  "found in Linter's standard library include map. ",
      //                  "Please report omission."));
      continue;
    }

    // This fails for occurrences of type used before the proper include is
    // defined. For example, forward declarations would fail. On the other
    // hand, foward declaration of std::xxx results in undefined behavior.
    auto pp = find(parsedIncludes, includeMap[typeName]);
    if (pp.empty) {
      string includeStr = includeMap[typeName];
      warningMap[includeStr] ~= tokens[0];
      result++;
    }
  }

  if (result > 0) {
    foreach (key; warningMap.byKey()) {
      string occurrences = "";
      foreach (ref elem; warningMap[key]) {
        occurrences ~= " std::" ~ elem.value;
      }
      lintWarning(warningMap[key][0],
                  text("Direct include ",
                       "not found in either cpp or include file",
                       " for", occurrences, ", prefer to use direct",
                       " #include <", key, "> (found ", warningMap[key].length,
                       " total occurrence(s) for this missing include)\n"));
    }
  }


  return result;
}


/*
 * Get the right-hand-side expression starting from a comparison operator.
*/
string getRhsExpr(const Token[] tox, const bool[TokenType] exprTokens) {
  string rhs = "";
  int rhsLParenCount = 0;
  int toxIdx = 1;

  while (toxIdx < tox.length &&
         (0 < rhsLParenCount ||
          (tox[toxIdx].type_ in exprTokens && tox[toxIdx].type_ != tk!")"))) {
    rhs ~= tox[toxIdx].value ~ " ";

    if (tox[toxIdx].type_ == tk!"(") {
      ++rhsLParenCount;
    } else if (tox[toxIdx].type_ == tk!")") {
      --rhsLParenCount;
    }

    ++toxIdx;
  }

    return rhs;
}

/*
 * Get bogus comparisons for the tokens in comparisonTokens.
*/
uint getBogusComparisons(Token[] v,
                         const bool[TokenType] comparisonTokens,
                         const bool[TokenType] exprTokens) {
  uint result = 0;
  string lhs = "";
  ulong[] specialTokenHead = [];
  ulong[] lhsExprHead = [0];
  ulong lhsLParenCount = 0;

  for (auto tox = v; !tox.empty; tox.popFront) {
    lhs ~= tox.front.value ~ " ";

    if (tox.front.type_ in exprTokens) {
      if (tox.front.type_ == tk!"(") {
        lhsExprHead ~= lhs.length;
        ++lhsLParenCount;
      } else if (tox.front.type_ == tk!")") {
        while (lhsExprHead.back != 0 && lhs[lhsExprHead.back - 1 - 1] != '(') {
          lhsExprHead.popBack;
        }

        if (lhsExprHead.back != 0) {
          lhsExprHead.popBack;
          --lhsLParenCount;
        }
      } else if (tox.front.type_.among(tk!"identifier",
                                       tk!"number",
                                       tk!"string_literal",
                                       tk!"char_literal")) {
        specialTokenHead ~= lhs.length;
      }

      continue;
    }

    if (tox.front.type_ !in comparisonTokens) {
      if (0 < lhsLParenCount) {
        lhsExprHead ~= lhs.length;
      } else {
        specialTokenHead.destroy();
        lhs.destroy();
        lhsExprHead = [0];
        lhsLParenCount = 0;
      }

      continue;
    }

    string rhs = getRhsExpr(tox, exprTokens);

    if (// Ignore less than and greater than, due to the large number of
        // false positives
        !tox.front.type_.among(tk!"<", tk!">") &&
        // Ignore expressions without special tokens
        !specialTokenHead.empty && lhsExprHead.back < specialTokenHead.back &&
        lhs[lhsExprHead.back .. lhs.length - tox.front.value.length - 1] ==
          rhs) {
      lintWarning(tox.front,
                  text("A comparison between identical expressions, ",
                       rhs.stripRight,
                       ".\n"));
      ++result;
    }
  }

  return result;
}

/*
 * Lint check: detect bogus comparisons, e.g., EXPR == EXPR.
*/
uint checkBogusComparisons(string fpath, Token[] v) {
  bool[TokenType] exprTokens = [
    tk!"::":1,
    tk!"++":1,
    tk!"--":1,
    tk!"(":1,
    tk!")":1,
    tk!"[":1,
    tk!"]":1,
    tk!".":1,
    tk!"->":1,
    tk!"typeid":1,
    tk!"const_cast":1,
    tk!"dynamic_cast":1,
    tk!"reinterpret_cast":1,
    tk!"static_cast":1,
    tk!"+":1,
    tk!"-":1,
    tk!"!":1,
    tk!"not":1,
    tk!"~":1,
    tk!"compl":1,
    tk!"&":1,
    tk!"sizeof":1,
    tk!"new":1,
    tk!"delete":1,
    tk!".*":1,
    tk!"->*":1,
    tk!"*":1,
    tk!"/":1,
    tk!"%":1,
    tk!"<<":1,
    tk!">>":1,
    tk!"#":1,
    tk!"##":1,
    tk!"identifier":1,
    tk!"number":1,
    tk!"string_literal":1,
    tk!"char_literal":1,
    tk!"char":1,
    tk!"bool":1,
    tk!"short":1,
    tk!"int":1,
    tk!"long":1,
    tk!"float":1,
    tk!"double":1,
    tk!"wchar_t":1,
    tk!"signed":1,
    tk!"unsigned":1,
  ];

  const bool[TokenType] inequalityTokens = [
    tk!"<":1,
    tk!"<=":1,
    tk!">":1,
    tk!">=":1,
  ];

  // We need to execute the following statements in order.
  // ... Inequality Tokens have higher precedence than equality tokens,
  // ... so we need to check inequality bogus comparisons first.
  uint inequalityBogusComparisons =
    getBogusComparisons(v, inequalityTokens, exprTokens);

  // ... Then, we merge inequality tokens into expression tokens.
  foreach (t; inequalityTokens.keys) {
    exprTokens[t] = true;
  }

  const bool[TokenType] equalityTokens = [
    tk!"==":1,
    tk!"!=":1,
    tk!"not_eq":1,
  ];

  // ... Finally, we check equality bogus comparisons.
  uint equalityBogusComparisons =
    getBogusComparisons(v, equalityTokens, exprTokens);

  return inequalityBogusComparisons + equalityBogusComparisons;
}


version(facebook) {
  immutable string[] angleBracketErrorDirs = [
    "folly/", "thrift/", "proxygen/lib", "proxygen/httpserver"
  ];
  immutable string[] angleBracketRequiredPrefixes = [
    "folly/", "thrift/lib/", "proxygen/lib", "proxygen/httpserver"
  ];

  uint checkAngleBracketIncludes(string fpath, Token[] v) {
    // strip fpath of '.../fbcode/', if present
    auto ppath = fpath.findSplitAfter("/fbcode/")[1];
    bool isError = angleBracketErrorDirs.any!(x => ppath.startsWith(x));
    auto errorFunc = isError ? &lintError : &lintWarning;

    uint errorCount = 0;
    for (; !v.empty; v.popFront) {
      IncludedPath ipath;
      if (!getIncludedPath(v, ipath)) {
        continue;
      }
      if (ipath.angleBrackets) {
        continue;
      }

      if (angleBracketRequiredPrefixes.any!(x => ipath.path.startsWith(x))) {
        errorFunc(v.front, text(
            "#include \"", ipath.path, "\" must use angle brackets\n"));
        if (isError) {
          errorCount += 1;
        }
        break;
      }
    }

    return errorCount;
  }
}

/**
  * Lint check: exit(-1) (or any other negative exit code) makes no sense
  * We should at least use exit(EXIT_FAILURE) instead
  */
uint checkExitStatus(string fpath, Token[] v) {
  uint result = 0;
  bool[string] exitFunctions = ["exit":true, "_exit":true];
  for (auto tox = v; !tox.empty; tox.popFront) {
    if (tox.atSequence(tk!"identifier", tk!"(", tk!"-", tk!"number", tk!")")) {
      if (tox[0].value !in exitFunctions) {
        continue;
      }
      lintWarning(tox[3], text(
          "exit(-",
          tox[3].value_,
          ") is not well-defined; use exit(EXIT_FAILURE) instead.\n"));
      ++result;
    }
  }

  return result;
}

/**
  * Lint check: the argument(s) of __attribute__((...)) should have the
  * form __KEYWORD__, i.e., with leading and trailing "__".
  * Otherwise, especially in header files, the unadorned keyword
  * impinges on the user/application-code namespace.
  */
uint checkAttributeArgumentUnderscores(string fpath, Token[] v) {
  uint result = 0;
  for (auto tok = v; !tok.empty; tok.popFront) {
    /* First, detect "__attribute__((T", where T does not start with "__". */
    if (!tok.atSequence(tk!"identifier", tk!"(", tk!"(", tk!"identifier")
        || tok[0].value != "__attribute__") {
      continue;
    }
    auto kw = tok[3];
    if (!kw.value.startsWith("__")) {
      lintWarning(kw, format("__attribute__ type \"%s\"" ~
                             " should be written as \"__%s__\"\n",
                             kw.value_, kw.value_));
      ++result;
    }

    /* Pop off the 4 tokens we've just recognized. */
    tok.popFrontN(4);

    if (kw.value != "__format__") {
      continue;
    }

    /* Also detect when the T in "__format__(T" does not start with "__". */
    if (tok.atSequence(tk!"(", tk!"identifier")
        && !tok[1].value.startsWith("__")) {
      lintWarning(tok[1], format("__attribute__ format archetype \"%s\"" ~
                                 " should be written as \"__%s__\"\n",
                                 tok[1].value_, tok[1].value_));
      ++result;
    }
  }

  return result;
}
