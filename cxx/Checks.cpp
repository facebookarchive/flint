// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt

#include "Checks.h"

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <gflags/gflags.h>

#include <map>
#include <unordered_map>
#include <set>
#include <stack>

#include "folly/Range.h"
#include "folly/String.h"
#include "folly/Exception.h"

DEFINE_bool(c_mode, false, "Only check for C code");

namespace facebook { namespace flint {

using namespace folly;
namespace fs = boost::filesystem;

using std::string;
using std::vector;

namespace {

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

void lintError(const Token& tok, const string& error) {
  checkUnixError(fprintf(stderr, "%.*s(%u): %s",
    static_cast<uint32_t>(tok.file_.size()), tok.file_.data(),
    static_cast<uint32_t>(tok.line_),
                         error.c_str()));
}

void lintWarning(const Token& tok, const std::string& warning) {
  // The FbcodeCppLinter just looks for the text "Warning" in the
  // message.
  lintError(tok, "Warning: " + warning);
}

void lintAdvice(const Token& tok, const std::string& advice) {
  // The FbcodeCppLinter just looks for the text "Advice" in the
  // message.
  lintError(tok, "Advice: " + advice);
}

template<class Iterator>
bool atSequence(Iterator it, const vector<TokenType>& list) {
  FOR_EACH (l, list) {
    if (*l != it->type_) {
      return false;
    }
    ++it;
  }
  return true;
}

// Remove the double quotes or <'s from an included path.
fs::path getIncludedPath(const folly::StringPiece& p) {
  return fs::path(p.subpiece(1, p.size() - 2).toString());
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
template<class Iterator>
Iterator skipTemplateSpec(Iterator it, bool* containsArray = nullptr) {
  CHECK_EQ(it->type_, TK_LESS);

  uint angleNest = 1;
  uint parenNest = 0;

  if (containsArray) {
    *containsArray = false;
  }

  ++it;
  for (; it->type_ != TK_EOF; ++it) {
    if (it->type_ == TK_LPAREN) {
      ++parenNest;
      continue;
    }
    if (it->type_ == TK_RPAREN) {
      --parenNest;
      continue;
    }

    // Ignore angles inside of parens.  This avoids confusion due to
    // integral template parameters that use < and > as comparison
    // operators.
    if (parenNest > 0) {
      continue;
    }

    if (it->type_ == TK_LSQUARE) {
      if (angleNest == 1 && containsArray) {
        *containsArray = true;
      }
      continue;
    }

    if (it->type_ == TK_LESS) {
      ++angleNest;
      continue;
    }
    if (it->type_ == TK_GREATER) {
      if (!--angleNest) {
        break;
      }
      continue;
    }
  }

  return it;
}

/*
 * Returns whether `it' points to a token that is a reserved word for
 * a built in type.
 */
template<class Iterator>
bool atBuiltinType(Iterator it) {
  switch (it->type_) {
  case TK_DOUBLE:
  case TK_FLOAT:
  case TK_INT:
  case TK_SHORT:
  case TK_UNSIGNED:
  case TK_LONG:
  case TK_SIGNED:
  case TK_VOID:
  case TK_BOOL:
  case TK_WCHAR_T:
  case TK_CHAR:
    return true;
  default:
    return false;
  }
}

/*
 * Heuristically read a potentially namespace-qualified identifier,
 * advancing `it' in the process.
 *
 * Returns: a vector of all the identifier values involved, or an
 * empty vector if no identifier was detected.
 */
template<class Iterator>
std::vector<StringPiece> readQualifiedIdentifier(Iterator& it) {
  std::vector<StringPiece> ret;
  for (; it->type_ == TK_IDENTIFIER || it->type_ == TK_DOUBLE_COLON; ++it) {
    if (it->type_ == TK_IDENTIFIER) {
      ret.push_back(it->value_);
    }
  }
  return ret;
}

/*
 * Starting from a left curly brace, skips until it finds the matching
 * (balanced) right curly brace. Does not care about whether other characters
 * within are balanced.
 *
 * Returns: iterator pointing to the final TK_RCURL (or TK_EOF if it
 * didn't finish).
 */
template<class Iterator>
Iterator skipBlock(Iterator it) {
  CHECK_EQ(it->type_, TK_LCURL);

  uint openBraces = 1;

  ++it;
  for (; it->type_ != TK_EOF; ++it) {
    if (it->type_ == TK_LCURL) {
      ++openBraces;
      continue;
    }
    if (it->type_ == TK_RCURL) {
      if (!--openBraces) {
        break;
      }
      continue;
    }
  }

  return it;
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
template<class Callback>
uint iterateClasses(const vector<Token>& v, const Callback& callback) {
    //const std::function<uint (Iterator, const vector<Token>&)>& callback) {
  uint result = 0;

  FOR_EACH (it, v) {
    if (atSequence(it, {TK_TEMPLATE, TK_LESS})) {
      it = skipTemplateSpec(++it);
      continue;
    }

    if (it->type_ == TK_CLASS
        || it->type_ == TK_STRUCT
        || it->type_ == TK_UNION) {
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
template<class Iterator>
Iterator skipFunctionDeclaration(Iterator it) {
  ++it;
  for (; it->type_ != TK_EOF; ++it) {
    if (it->type_ == TK_SEMICOLON) { // prototype
      break;
    } else if (it->type_ == TK_LCURL) { // full declaration
      it = skipBlock(it);
      break;
    }
  }
  return it;
}

/**
 * Represent an argument or the name of a function.
 * first is an iterator that points to the start of the argument.
 * last is an iterator that points to the token right after the end of the
 * argument.
 */
struct Argument {
  vector<Token>::const_iterator first;
  vector<Token>::const_iterator last;

  Argument(vector<Token>::const_iterator first,
           vector<Token>::const_iterator last)
    : first(first)
    , last(last)
  {}
};

std::string formatArg(const Argument& arg) {
  std::string result;
  FOR_EACH_RANGE (it, arg.first, arg.last) {
    if (it != arg.first && !it->precedingWhitespace_.empty()) {
      result.push_back(' ');
    }
    result += it->value_.toString();
  }
  return result;
}

std::string formatFunction(
    const Argument& functionName,
    const vector<Argument>& args) {
  std::string result = formatArg(functionName) + "(";
  for (int i = 0; i < args.size(); ++i) {
    if (i > 0) {
      result += ", ";
    }
    result += formatArg(args[i]);
  }
  result += ")";
  return result;
}

/**
 * Get the list of arguments of a function, assuming that the current
 * iterator is at the open parenthesis of the function call. After the this
 * method is call, the iterator will be moved to after the end of the function
 * call.
 * @param i: the current iterator, must be at the open parenthesis of the
 * function call.
 * @param args: the arguments of the function would be push to the back of args
 * @return true if (we believe) that there was no problem during the run and
 * false if we believe that something was wrong (most probably with skipping
 * template specs.)
 */
bool getRealArguments(
    vector<Token>::const_iterator& i,
    vector<Argument>& args) {
  assert(i->type_ == TK_LPAREN);
  // the first argument starts after the open parenthesis
  auto argStart = i + 1;
  int parenCount = 1;
  do {
    if (i->type_ == TK_EOF) {
      // if we meet EOF before the closing parenthesis, something must be wrong
      // with the template spec skipping
      return false;
    }
    ++i;
    switch (i->type_) {
    case TK_LPAREN: parenCount++;
                    break;
    case TK_RPAREN: parenCount--;
                    break;
    case TK_LESS:   // This is a heuristic which would fail when < is used with
                    // the traditional meaning in an argument, e.g.
                    //  memset(&foo, a < b ? c : d, sizeof(foo));
                    // but currently we have no way to distinguish that use of
                    // '<' and
                    //  memset(&foo, something<A,B>(a), sizeof(foo));
                    // We include this heuristic in the hope that the second
                    // use of '<' is more common than the first.
                    i = skipTemplateSpec(i);
                    break;
    case TK_COMMA:  if (parenCount == 1) {
                      // end an argument of the function we are looking at
                      args.push_back(Argument(argStart, i));
                      argStart = i + 1;
                    }// otherwise we are in an inner function, so do nothing
                    break;
    default:        break;
    }
  } while (parenCount != 0);
  if (argStart != i) {
    args.push_back(Argument(argStart, i));
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
bool getFunctionNameAndArguments(
    vector<Token>::const_iterator& i,
    Argument& functionName,
    vector<Argument>& args) {
  functionName.first = i;
  ++i;
  if (i->type_ == TK_LESS) {
    i = skipTemplateSpec(i);
    if (i->type_ == TK_EOF) {
      return false;
    }
    ++i;
  }
  functionName.last = i;
  return getRealArguments(i, args);
}

} //unamed namespace

uint checkInitializeFromItself(
    const string& fpath, const vector<Token>& tokens) {
  vector<TokenType> firstInitializer{
    TK_COLON, TK_IDENTIFIER, TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
  };
  vector<TokenType> nthInitialier{
    TK_COMMA, TK_IDENTIFIER, TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
  };

  uint result = 0;
  FOR_EACH(it, tokens) {
    if (atSequence(it, firstInitializer) || atSequence(it, nthInitialier)) {
      auto outerIdentifier = ++it;
      auto innerIdentifier = ++(++it);
      bool isMember = outerIdentifier->value_.back() == '_' ||
                      outerIdentifier->value_.startsWith("m_");
      if (isMember && outerIdentifier->value_ == innerIdentifier->value_) {
        lintError(*outerIdentifier, to<std::string>(
          "Looks like you're initializing class member [",
          outerIdentifier->value_, "] with itself.\n")
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
uint checkBlacklistedSequences(const string& fpath, const vector<Token>& v) {
  struct BlacklistEntry {
    vector<TokenType> tokens;
    string descr;
    bool cpponly;
    BlacklistEntry(const vector<TokenType>& t, const string& d, bool cpponly)
      : tokens(t), descr(d), cpponly(cpponly) { }
  };

  const static vector<BlacklistEntry> blacklist = {
    { {TK_VOLATILE},
      "'volatile' does not make your code thread-safe. If multiple threads are "
      "sharing data, use std::atomic or locks. In addition, 'volatile' may "
      "force the compiler to generate worse code than it could otherwise. "
      "For more about why 'volatile' doesn't do what you think it does, see "
      "http://fburl.com/volatile or http://www.kernel.org/doc/Documentation/"
      "volatile-considered-harmful.txt.\n",
      true, // C++ only.
    },
  };

  const static vector<vector<TokenType> > exceptions = {
    {TK_ASM, TK_VOLATILE},
  };

  uint result = 0;
  bool isException = false;

  FOR_EACH(i, v) {
    for (const auto& e : exceptions) {
      if (atSequence(i, e)) { isException = true; break; }
    }
    for (const BlacklistEntry& entry : blacklist) {
      if (!atSequence(i, entry.tokens)) { continue; }
      if (isException) { isException = false; continue; }
      if (FLAGS_c_mode && entry.cpponly == true) { continue; }
      lintWarning(*i, entry.descr);
      ++result;
    }
  }

  return result;
}

uint checkBlacklistedIdentifiers(const string& fpath, const vector<Token>& v) {
  uint result = 0;

  static const std::map<StringPiece,StringPiece> banned {
    { "strtok",
      "strtok() is not thread safe, and has safer alternatives.  Consider "
      "folly::split or strtok_r as appropriate.\n" }
  };

  FOR_EACH (i, v) {
    if (i->type_ != TK_IDENTIFIER) continue;
    auto mapIt = banned.find(i->value_);
    if (mapIt == end(banned)) continue;
    lintError(*i, mapIt->second.toString());
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
uint checkDefinedNames(const string& fpath, const vector<Token>& v) {

  // Define a set of exception to rules
  static std::set<StringPiece> okNames;
  if (okNames.empty()) {

    static const char* okNamesInit[] = {
      "__STDC_LIMIT_MACROS",
      "__STDC_FORMAT_MACROS",
      "_GNU_SOURCE",
      "_XOPEN_SOURCE",
    };

    FOR_EACH_RANGE (i, 0, sizeof(okNamesInit)/sizeof(*okNamesInit)) {
      okNames.insert(okNamesInit[i]);
    }
  }

  uint result = 0;
  FOR_EACH (i, v) {
    if (i->type_ != TK_DEFINE) continue;
    auto const & t = i[1];
    auto const sym = t.value_;
    if (t.type_ != TK_IDENTIFIER) {
      // This actually happens because people #define private public
      //   for unittest reasons
      lintWarning(t, to<std::string>("you're not supposed to #define ",
                                     sym, "\n"));
      continue;
    }
    if (sym.size() >= 2 && sym[0] == '_' && isupper(sym[1])) {
      if (okNames.find(sym) != okNames.end()) {
        continue;
      }
      lintWarning(t, to<std::string>("Symbol ", sym, " invalid."
        "  A symbol may not start with an underscore followed by a "
        "capital letter.\n"));
      ++result;
    } else if (sym.size() >= 2 && sym[0] == '_' && sym[1] == '_') {
      if (okNames.find(sym) != okNames.end()) {
        continue;
      }
      lintWarning(t, to<std::string>("Symbol ", sym, " invalid."
        "  A symbol may not begin with two adjacent underscores.\n"));
      ++result;
    } else if (!FLAGS_c_mode /* C is less restrictive about this */ &&
               sym.find("__") != string::npos) {
      if (okNames.find(sym) != okNames.end()) {
        continue;
      }
      lintWarning(t, to<std::string>("Symbol ", sym, " invalid."
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
uint checkCatchByReference(const string& fpath, const vector<Token>& v) {
  uint result = 0;
  FOR_EACH (i, v) {
    if (i->type_ != TK_CATCH) continue;
    size_t focal = 1;
    if (i[focal].type_ != TK_LPAREN) { // a "(" comes always after catch
      throw std::runtime_error(to<std::string>(
        i[focal].file_, ":", i[focal].line_,
        ": Invalid C++ source code, please compile before lint."));
    }
    ++focal;
    if (i[focal].type_ == TK_ELLIPSIS) {
      // catch (...
      continue;
    }
    if (i[focal].type_ == TK_CONST) {
      // catch (const
      ++focal;
    }
    if (i[focal].type_ == TK_TYPENAME) {
      // catch ([const] typename
      ++focal;
    }
    if (i[focal].type_ == TK_DOUBLE_COLON) {
      // catch ([const] [typename] ::
      ++focal;
    }
    // At this position we must have an identifier - the type caught,
    // e.g. FBException, or the first identifier in an elaborate type
    // specifier, such as facebook::FancyException<int, string>.
    if (i[focal].type_ != TK_IDENTIFIER) {
      auto const & t = i[focal];
      lintWarning(t, to<std::string>("Symbol ", t.value_, " invalid in "
        "catch clause.  You may only catch user-defined types.\n"));
      ++result;
      continue;
    }
    ++focal;
    // We move the focus to the closing paren to detect the "&". We're
    // balancing parens because there are weird corner cases like
    // catch (Ex<(1 + 1)> & e).
    for (size_t parens = 0; ; ++focal) {
      if (focal >= v.size()) {
        throw std::runtime_error(
          to<std::string>(
            i[focal].file_, ":", i[focal].line_,
            ": Invalid C++ source code, please compile before lint."));
      }
      if (i[focal].type_ == TK_RPAREN) {
        if (parens == 0) break;
        --parens;
      } else if (i[focal].type_ == TK_LPAREN) {
        ++parens;
      }
    }
    // At this point we're straight on the closing ")". Backing off
    // from there we should find either "& identifier" or "&" meaning
    // anonymous identifier.
    if (i[focal - 1].type_ == TK_AMPERSAND) {
      // check! catch (whatever &)
      continue;
    }
    if (i[focal - 1].type_ == TK_IDENTIFIER &&
        i[focal - 2].type_ == TK_AMPERSAND) {
      // check! catch (whatever & ident)
      continue;
    }
    // Oopsies times
    auto const & t = i[focal - 1];
    // Get the type string
    string theType;
    FOR_EACH_RANGE (j, 2, focal - 1) {
      if (j > 2) theType += " ";
      theType += i[j].value_.toString();
    }
    lintError(t, to<std::string>("Symbol ", t.value_, " of type ", theType,
      " caught by value.  Use catch by (preferably const) reference "
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
uint checkThrowSpecification(const string&, const vector<Token>& v) {
  uint result = 0;

  // Check for throw specifications inside classes
  result += iterateClasses(v,
    [&](vector<Token>::const_iterator it, const vector<Token>& v) -> uint {

      uint result = 0;

      auto term = std::find_if(it, v.end(),
                     boost::bind(&Token::type_, _1) == TK_LCURL);

      if (term == v.end()) {
        return result;
      }

      it = ++term;

      const vector<TokenType> destructorSequence =
        {TK_TILDE, TK_IDENTIFIER, TK_LPAREN, TK_RPAREN,
         TK_THROW, TK_LPAREN, TK_RPAREN};

      for (; it->type_ != TK_EOF; ++it) {
        // Skip warnings for empty throw specifications on destructors,
        // because sometimes it is necessary to put a throw() clause on
        // classes deriving from std::exception.
        if (atSequence(it, destructorSequence)) {
          it += destructorSequence.size() - 1;
          continue;
        }

        // This avoids warning if the function is named "what", to allow
        // inheriting from std::exception without upsetting lint.
        if (it->type_ == TK_IDENTIFIER && it->value_ == "what") {
          ++it;
          auto sequence = {TK_LPAREN, TK_RPAREN, TK_CONST,
                           TK_THROW, TK_LPAREN, TK_RPAREN};
          if (atSequence(it, sequence)) {
            it += sequence.size() - 1;
          }
          continue;
        }

        if (it->type_ == TK_LCURL) {
          it = skipBlock(it);
          continue;
        }

        if (it->type_ == TK_RCURL) {
          break;
        }

        if (it->type_ == TK_THROW && it[1].type_ == TK_LPAREN) {
          lintWarning(*it, "Throw specifications on functions are "
            "deprecated.\n");
          ++result;
        }
      }

      return result;
    }
  );

  // Check for throw specifications in functional style code
  FOR_EACH (it, v) {
    // Don't accidentally identify a using statement as a namespace
    if (it->type_ == TK_USING) {
      if (it[1].type_ == TK_NAMESPACE) {
        ++it;
      }
      continue;
    }

    // Skip namespaces, classes, and blocks
    if (it->type_ == TK_NAMESPACE
        || it->type_ == TK_CLASS
        || it->type_ == TK_STRUCT
        || it->type_ == TK_UNION
        || it->type_ == TK_LCURL) {
      auto term = std::find_if(it, v.end(),
                     boost::bind(&Token::type_, _1) == TK_LCURL);

      if (term == v.end()) {
        break;
      }

      it = skipBlock(term);
      continue;
    }

    if (it->type_ == TK_THROW && it[1].type_ == TK_LPAREN) {
      lintWarning(*it, "Throw specifications on functions are "
        "deprecated.\n");
      ++result;
    }
  }

  return result;
}

/**
 * Lint check: balance of #if(#ifdef, #ifndef)/#endif.
 */
uint checkIfEndifBalance(const string& fpath, const vector<Token>& v) {
  int openIf = 0;

  // Return after the first found error, because otherwise
  // even one missed #if can be cause of a lot of errors.
  FOR_EACH (i, v) {
    if (i->type_ == TK_IFNDEF || i->type_ == TK_IFDEF ||
        i->type_ == TK_POUNDIF) {
      ++openIf;
    } else if (i->type_ == TK_ENDIF) {
      --openIf;
      if (openIf < 0) {
        lintError(*i, "Unmatched #endif.\n");
        return 1;
      }
    } else if (i->type_ == TK_POUNDELSE) {
      if (openIf == 0) {
        lintError(*i, "Unmatched #else.\n");
        return 1;
      }
    }
  }

  if (openIf != 0) {
    lintError(v.back(), "Unbalanced #if/#endif.\n");
    return 1;
  }

  return 0;
}

/*
 * Lint check: warn about common errors with constructors, such as:
 *  - single-argument constructors that aren't marked as explicit, to avoid them
 *    being used for implicit type conversion (C++ only)
 *  - Non-const copy constructors, or useless const move constructors.
 */
uint checkConstructors(const string& fpath, const std::vector<Token>& tokensV) {
  if (getFileCategory(fpath) == FileCategory::SOURCE_C) {
    return 0;
  }

  uint result = 0;
  std::stack<StringPiece> nestedClasses;

  const string lintOverride = "/""* implicit *""/";
  const vector<TokenType> stdInitializerSequence =
    {TK_IDENTIFIER, TK_DOUBLE_COLON, TK_IDENTIFIER, TK_LESS};
  const vector<TokenType> voidConstructorSequence =
    {TK_IDENTIFIER, TK_LPAREN, TK_VOID, TK_RPAREN};

  FOR_EACH (it, tokensV) {
    // Avoid mis-identifying a class context due to use of the "class"
    // keyword inside a template parameter list.
    if (atSequence(it, {TK_TEMPLATE, TK_LESS})) {
      it = skipTemplateSpec(++it);
      continue;
    }

    // Parse within namespace blocks, but don't do top-level constructor checks.
    // To do this, we treat namespaces like unnamed classes so any later
    // function name checks will not match against an empty string.
    if (it->type_ == TK_NAMESPACE) {
      ++it;
      for (; it->type_ != TK_EOF; ++it) {
        if (it->type_ == TK_SEMICOLON) {
          break;
        } else if (it->type_ == TK_LCURL) {
          nestedClasses.push("");
          break;
        }
      }
      continue;
    }

    // Extract the class name if a class/struct definition is found
    if (it->type_ == TK_CLASS || it->type_ == TK_STRUCT) {
      ++it;

      // If we hit any C-style structs, we'll handle them like we do namespaces:
      // continue to parse within the block but don't show any lint errors.
      if(it->type_ == TK_LCURL) {
        nestedClasses.push("");
      } else if (it->type_ == TK_IDENTIFIER) {
        StringPiece classCandidate = it->value_;
        for (; it->type_ != TK_EOF; ++it) {
          if (it->type_ == TK_SEMICOLON) {
            break;
          } else if (it->type_ == TK_LCURL) {
            nestedClasses.push(classCandidate);
            break;
          }
        }
      }

      continue;
    }

    // Closing curly braces end the current scope, and should always be balanced
    if (it->type_ == TK_RCURL) {
      if (nestedClasses.empty()) { // parse fail
        return result;
      }
      nestedClasses.pop();
      continue;
    }

    // Skip unrecognized blocks. We only want to parse top-level class blocks.
    if (it->type_ == TK_LCURL) {
      it = skipBlock(it);
      continue;
    }

    // Only check for constructors if we've previously entered a class block
    if (nestedClasses.empty()) {
      continue;
    }

    // Skip past any functions that begin with an "explicit" keyword
    if (it->type_ == TK_EXPLICIT) {
      it = skipFunctionDeclaration(it);
      continue;
    }

    // Skip anything that doesn't look like a constructor
    if (!atSequence(it, {TK_IDENTIFIER, TK_LPAREN})) {
      continue;
    } else if (it->value_ != nestedClasses.top()) {
      it = skipFunctionDeclaration(it);
      continue;
    }

    // Suppress error and skip past functions clearly marked as implicit
    if (it->precedingWhitespace_.find(lintOverride) != string::npos) {
      it = skipFunctionDeclaration(it);
      continue;
    }

    // Allow zero-argument void constructors
    if (atSequence(it, voidConstructorSequence)) {
      it = skipFunctionDeclaration(it);
      continue;
    }

    vector<Argument> args;
    Argument functionName(it, it);
    if (!getFunctionNameAndArguments(it, functionName, args)) {
      // Parse fail can be due to limitations in skipTemplateSpec, such as with:
      // fn(std::vector<boost::shared_ptr<ProjectionOperator>> children);)
      return result;
    }

    // Allow zero-argument constructors
    if (args.empty()) {
      it = skipFunctionDeclaration(it);
      continue;
    }

    auto argIt = args[0].first;
    bool foundConversionCtor = false;
    bool isConstArgument = false;
    if (argIt->type_ == TK_CONST) {
      isConstArgument = true;
      ++argIt;
    }

    // Copy/move constructors may have const (but not type conversion) issues
    // Note: we skip some complicated cases (e.g. template arguments) here
    if (argIt->value_ == nestedClasses.top()) {
      auto nextType = (argIt + 1 != args[0].last) ? argIt[1].type_ : TK_EOF;
      if(nextType != TK_STAR) {
        if(nextType == TK_AMPERSAND && !isConstArgument) {
          ++result;
          lintError(*it, to<std::string>(
            "Copy constructors should take a const argument: ",
            formatFunction(functionName, args), "\n"
            ));
        } else if (nextType == TK_LOGICAL_AND && isConstArgument) {
          ++result;
          lintError(*it, to<std::string>(
            "Move constructors should not take a const argument: ",
            formatFunction(functionName, args), "\n"
            ));
        }
        it = skipFunctionDeclaration(it);
        continue;
      }
    }

    // Allow std::initializer_list constructors
    if (atSequence(argIt, stdInitializerSequence)
          && argIt->value_ == "std"
          && argIt[2].value_ == "initializer_list") {
        it = skipFunctionDeclaration(it);
        continue;
    }

    if (args.size() == 1) {
      foundConversionCtor = true;
    } else if (args.size() >= 2) {
      // 2+ will only be an issue if the second argument is a default argument
      for (argIt = args[1].first; argIt != args[1].last; ++argIt) {
        if (argIt->type_ == TK_ASSIGN) {
          foundConversionCtor = true;
          break;
        }
      }
    }

    if (foundConversionCtor) {
      ++result;
      lintError(*it, to<std::string>(
        "Single-argument constructor '",
        formatFunction(functionName, args),
        "' may inadvertently be used as a type conversion constructor. Prefix"
        " the function with the 'explicit' keyword to avoid this, or add an /"
        "* implicit *""/ comment to suppress this warning.\n"
        ));
    }

    it = skipFunctionDeclaration(it);
  }

  return result;
}

/*
 * Lint check: warn about implicit casts
 *
 * Implicit casts not marked as explicit can be dangerous if not used carefully
 */
uint checkImplicitCast(const string& fpath, const std::vector<Token>& tokensV) {
  if (FLAGS_c_mode ||
      getFileCategory(fpath) == FileCategory::SOURCE_C) {
    return 0;
  }

  uint result = 0;

  const string lintOverride = "/""* implicit *""/";

  FOR_EACH (it, tokensV) {
    // Skip past any functions that begin with an "explicit" keyword
    if (atSequence(it, {TK_EXPLICIT, TK_CONSTEXPR, TK_OPERATOR})) {
      ++it;
      ++it;
      continue;
    }
    if (atSequence(it, {TK_EXPLICIT, TK_OPERATOR}) ||
        atSequence(it, {TK_DOUBLE_COLON, TK_OPERATOR})) {
      ++it;
      continue;
    }

    // Special case operator bool(), we don't want to allow over-riding
    if (atSequence(it, {TK_OPERATOR, TK_BOOL, TK_LPAREN, TK_RPAREN})) {
      if (atSequence(it + 4, {TK_ASSIGN, TK_DELETE}) ||
          atSequence(it + 4, {TK_CONST, TK_ASSIGN, TK_DELETE})) {
        // Deleted implicit operators are ok.
        continue;
      }

      ++result;
      lintError(*it, "operator bool() is dangerous. "
        "In C++11 use explicit conversion (explicit operator bool()), "
        "otherwise use something like the safe-bool idiom if the syntactic "
        "convenience is justified in this case, or consider defining a "
        "function (see http://www.artima.com/cppsource/safebool.html for more "
        "details).\n"
      );
      continue;
    }

    // Only want to process operators which do not have the overide
    if (it->type_ != TK_OPERATOR ||
        it->precedingWhitespace_.find(lintOverride) != string::npos) {
      continue;
    }

    // Assume it is an implicit conversion unless proven otherwise
    bool isImplicitConversion = false;
    std::string typeString;
    for (auto typeIt = it+1; typeIt != tokensV.end(); ++typeIt) {
      if (typeIt->type_ == TK_LPAREN) {
        break;
      }

      switch (typeIt->type_) {
      case TK_DOUBLE:
      case TK_FLOAT:
      case TK_INT:
      case TK_SHORT:
      case TK_UNSIGNED:
      case TK_LONG:
      case TK_SIGNED:
      case TK_VOID:
      case TK_BOOL:
      case TK_WCHAR_T:
      case TK_CHAR:
      case TK_IDENTIFIER: isImplicitConversion = true;
                          break;
      default:            break;
      }

      if (!typeString.empty()) {
        typeString += ' ';
      }
      typeString += typeIt->value_.toString();
    }

    // The operator my not have been an implicit conversion
    if (!isImplicitConversion) {
      continue;
    }

    ++result;
    lintWarning(*it, to<std::string>(
      "Implicit conversion to '",
      typeString,
      "' may inadvertently be used. Prefix the function with the 'explicit'"
      " keyword to avoid this, or add an /* implicit *""/ comment to"
      " suppress this warning.\n"
      ));
  }

  return result;
}

enum class AccessRestriction {
  PRIVATE,
  PUBLIC,
  PROTECTED
};

struct ClassParseState {
  string name_;
  AccessRestriction access_;
  Token token_;
  bool has_virt_function_;
  bool ignore_;

  ClassParseState(const string& n,
                  const AccessRestriction& a,
                  const Token& t)
    : name_(n), access_(a), token_(t), has_virt_function_(false),
      ignore_(false)
  {}

  ClassParseState() : ignore_(true) {}
};

/**
 * Lint check: warn about non-virtual destructors in base classes
 */
uint checkVirtualDestructors(const string& fpath, const vector<Token>& v) {
  if (getFileCategory(fpath) == FileCategory::SOURCE_C) {
    return 0;
  }

  uint result = 0;
  std::stack<ClassParseState> nestedClasses;

  FOR_EACH (it, v) {
    // Avoid mis-identifying a class context due to use of the "class"
    // keyword inside a template parameter list.
    if (atSequence(it, {TK_TEMPLATE, TK_LESS})) {
      it = skipTemplateSpec(++it);
      continue;
    }

    // Treat namespaces like unnamed classes
    if (it->type_ == TK_NAMESPACE) {
      ++it;
      for (; it->type_ != TK_EOF; ++it) {
        if (it->type_ == TK_SEMICOLON) {
          break;
        } else if (it->type_ == TK_LCURL) {
          nestedClasses.push(ClassParseState());
          break;
        }
      }
      continue;
    }

    if (it->type_ == TK_CLASS || it->type_ == TK_STRUCT) {
      auto const& v = it->type_ == TK_CLASS ?
          AccessRestriction::PRIVATE : AccessRestriction::PUBLIC;
      auto const& token = *it;
      ++it;

      // If we hit any C-style structs or non-base classes,
      // we'll handle them like we do namespaces:
      // continue to parse within the block but don't show any lint errors.
      if(it->type_ == TK_LCURL) {
        nestedClasses.push(ClassParseState());
      } else if (it->type_ == TK_IDENTIFIER) {
        StringPiece classCandidate = it->value_;

        for (; it->type_ != TK_EOF; ++it) {
          if (it->type_ == TK_COLON) {
            // Skip to the class block if we have a derived class
            for (; it->type_ != TK_EOF; ++it) {
              if (it->type_ == TK_LCURL) { // full declaration
                break;
              }
            }
            // Ignore non-base classes
            nestedClasses.push(ClassParseState());
            break;
          } else if (it->type_ == TK_IDENTIFIER) {
            classCandidate = it->value_;
          } else if (it->type_ == TK_LCURL) {
            nestedClasses.push(
              ClassParseState(classCandidate.toString(), v, token));
            break;
          }
        }
      }
      continue;
    }

    // Skip unrecognized blocks. We only want to parse top-level class blocks.
    if (it->type_ == TK_LCURL) {
      it = skipBlock(it);
      continue;
    }

    // Only check for virtual methods if we've previously entered a class block
    if (nestedClasses.empty()) {
      continue;
    }

    auto& c = nestedClasses.top();

    // Closing curly braces end the current scope, and should always be balanced
    if (it->type_ == TK_RCURL) {
      if (nestedClasses.empty()) { // parse fail
        return result;
      }
      if (!c.ignore_ && c.has_virt_function_) {
        ++result;
        lintWarning(c.token_, to<std::string>("Base class ", c.name_,
          " has virtual functions but a public non-virtual destructor.\n"));
      }
      nestedClasses.pop();
      continue;
    }

    // Virtual function
    if (it->type_ == TK_VIRTUAL) {
      if (it[1].type_ == TK_TILDE) {
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
    if (atSequence(it, {TK_TILDE, TK_IDENTIFIER})) {
      if (c.access_ != AccessRestriction::PUBLIC) {
        c.ignore_ = true;
      }
      it = skipFunctionDeclaration(it);
      continue;
    }

    if (it->type_ == TK_PUBLIC) {
      c.access_ = AccessRestriction::PUBLIC;
    } else if (it->type_ == TK_PROTECTED) {
      c.access_ = AccessRestriction::PROTECTED;
    } else if (it->type_ == TK_PRIVATE) {
      c.access_ = AccessRestriction::PRIVATE;
    }
  }
  return result;
}

/**
 * Lint check: if header file contains include guard.
 */
uint checkIncludeGuard(const string& fpath, const vector<Token>& v) {
  if (getFileCategory(fpath) != FileCategory::HEADER) {
    return 0;
  }

  // Allow #pragma once
  if (v[0].type_ == TK_PRAGMA && v[1].value_ == "once") {
    return 0;
  }

  // Looking for the include guard:
  //   #ifndef [name]
  //   #define [name]
  if (!atSequence(v.begin(), {TK_IFNDEF, TK_IDENTIFIER,
                              TK_DEFINE, TK_IDENTIFIER})) {
    // There is no include guard in this file.
    lintError(v.front(), "Missing include guard.\n");
    return 1;
  }

  uint result = 0;

  // Check if there is a typo in guard name.
  if (v[1].value_ != v[3].value_) {
    lintError(v[3], to<std::string>("Include guard name mismatch; expected ",
      v[1].value_, ", saw ", v[3].value_, ".\n"));
    ++result;
  }

  int openIf = 0;
  FOR_EACH (i, v) {
    if (i->type_ == TK_EOF) break;

    // Check if we have something after the guard block.
    if (openIf == 0 && i != v.begin()) {
      lintError(*i, "Include guard doesn't cover the entire file.\n");
      ++result;
      break;
    }

    if (i->type_ == TK_IFNDEF || i->type_ == TK_IFDEF ||
        i->type_ == TK_POUNDIF) {
      ++openIf;
    } else if (i->type_ == TK_ENDIF) {
      --openIf;
    }
  }

  return result;
}

/**
 * Lint check: inside a header file, namespace facebook must be introduced
 * at top level only, using namespace directives are not allowed, unless
 * they are scoped to an inline function or function template.
 */
uint checkUsingDirectives(const string& fpath, const vector<Token>& v) {
  if (!isHeader(fpath)) {
    // This check only looks at headers. Inside .cpp files, knock
    // yourself out.
    return 0;
  }
  uint result = 0;
  uint openBraces = 0;
  uint openNamespaces = 0;

  FOR_EACH (i, v) {
    if (i->type_ == TK_LCURL) {
      ++openBraces;
      continue;
    }
    if (i->type_ == TK_RCURL) {
      if (openBraces == 0) {
        // Closed more braces than we had.  Syntax error.
        return 0;
      }
      if (openBraces == openNamespaces) {
        // This brace closes namespace.
        --openNamespaces;
      }
      --openBraces;
      continue;
    }
    if (i->type_ == TK_NAMESPACE) {
      // Namespace alias doesn't open a scope.
      if (atSequence(i + 1, { TK_IDENTIFIER, TK_ASSIGN })) {
        continue;
      }

      // If we have more open braces than namespace, someone is trying
      // to nest namespaces inside of functions or classes here
      // (invalid C++), so we have an invalid parse and should give
      // up.
      if (openBraces != openNamespaces) {
        return result;
      }

      // Introducing an actual namespace.
      if (i[1].type_ == TK_LCURL) {
        // Anonymous namespace, let it be. Next iteration will hit the '{'.
        ++openNamespaces;
        continue;
      }

      ++i;
      if (i->type_ != TK_IDENTIFIER) {
        // Parse error or something.  Give up on everything.
        return result;
      }
      if (i->value_ == "facebook" && i[1].type_ == TK_LCURL) {
        // Entering facebook namespace
        if (openBraces > 0) {
          lintError(*i, "Namespace facebook must be introduced "
            "at top level only.\n");
          ++result;
        }
      }
      if (i[1].type_ != TK_LCURL && i[1].type_ != TK_DOUBLE_COLON) {
        // Invalid parse for us.
        return result;
      }
      ++openNamespaces;
      // Continue analyzing, next iteration will hit the '{' or the '::'
      continue;
    }
    if (i->type_ == TK_USING) {
      // We're on a "using" keyword
      ++i;
      if (i->type_ != TK_NAMESPACE) {
        // we only care about "using namespace"
        continue;
      }
      if (openBraces == 0) {
        lintError(*i, "Using directive not allowed at top level"
          " or inside namespace facebook.\n");
        ++result;
      } else if (openBraces == openNamespaces) {
        // We are directly inside the namespace.
        lintError(*i, "Using directive not allowed in header file, "
          "unless it is scoped to an inline function or function template.\n");
        ++result;
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
static const vector<std::set<string> > MUTUALLY_EXCLUSIVE_NAMESPACES {
  { "std", "std::tr1", "boost", "::std", "::std::tr1", "::boost" },
  // { "list", "of", "namespaces", "that", "should::not::appear", "together" }
};
uint checkUsingNamespaceDirectives(const string& fpath,
                                   const vector<Token>& v) {
  using std::map;
  using std::set;
  using std::stack;
  uint result = 0;
  // (namespace => line number) for all visible namespaces
  // we can probably simplify the implementation by getting rid of this and
  // performing a "nested scope lookup" by looking up symbols in the current
  // scope, then the enclosing scope etc.
  map<string, size_t> allNamespaces;
  stack<set<string> > nestedNamespaces;
  vector<int> namespaceGroupCounts(MUTUALLY_EXCLUSIVE_NAMESPACES.size(), 0);

  nestedNamespaces.push(set<string>());
  FOR_EACH (i, v) {
    if (i->type_ == TK_LCURL) {
      // create a new set for all namespaces in this scope
      nestedNamespaces.push(set<string>());
    } else if (i->type_ == TK_RCURL) {
      if (nestedNamespaces.size() == 1) {
        // closed more braces than we had.  Syntax error.
        return 0;
      }
      // delete all namespaces that fell out of scope
      FOR_EACH (iNs, nestedNamespaces.top()) {
        allNamespaces.erase(*iNs);
        FOR_EACH_ENUMERATE (ii, iGroup, MUTUALLY_EXCLUSIVE_NAMESPACES) {
          if (iGroup->find(*iNs) != iGroup->end()) {
            --namespaceGroupCounts[ii];
          }
        }
      }
      nestedNamespaces.pop();
    } else if (atSequence(i, { TK_USING, TK_NAMESPACE })) {
      i += 2;
      // crude method for getting the namespace name; this assumes the
      // programmer puts a semicolon at the end of the line and doesn't do
      // anything else invalid
      string ns;
      while (i->type_ != TK_SEMICOLON) {
        ns += i->value_.toString();
        ++i;
      }
      auto insertResult = allNamespaces.insert(make_pair(ns, i->line_));
      if (!insertResult.second) {
        // duplicate using namespace directive
        size_t line = insertResult.first->second;
        string error = to<std::string>(
          "Duplicate using directive for "
          "namespace \"", ns, "\" (line ", line, ").\n");
        lintError(*i, error);
        ++result;
        continue;
      }
      nestedNamespaces.top().insert(ns);
      // check every namespace group for this namespace
      FOR_EACH_ENUMERATE (ii, iGroup, MUTUALLY_EXCLUSIVE_NAMESPACES) {
        if (iGroup->find(ns) != iGroup->end()) {
          if (namespaceGroupCounts[ii] >= 1) {
            // mutual exclusivity violated
            // find the first conflicting namespace in the file
            string conflict;
            size_t conflictLine = std::numeric_limits<size_t>::max();
            FOR_EACH(iNs, *iGroup) {
              if (*iNs == ns) {
                continue;
              }
              map<string, size_t>::iterator it = allNamespaces.find(*iNs);
              if (it != allNamespaces.end() && it->second < conflictLine) {
                conflict = it->first;
                conflictLine = it->second;
              }
            }
            string error = to<std::string>(
              "Using namespace conflict: \"", ns, "\" "
              "and \"", conflict, "\" (line ", conflictLine, ").\n");
            lintError(*i, error);
            ++result;
          }
          ++namespaceGroupCounts[ii];
        }
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
uint checkThrowsHeapException(const string& fpath, const vector<Token> & v) {
  uint result = 0;
  FOR_EACH (i, v) {
    if (atSequence(i, {TK_THROW, TK_NEW})) {
      size_t focal = 2;
      string msg;

      if (i[focal].type_ == TK_IDENTIFIER) {
        msg = to<std::string>("Heap-allocated exception: throw new ",
                              i[focal].value_, "();");
      } else if (atSequence(&i[focal], {TK_LPAREN, TK_IDENTIFIER,
                                        TK_RPAREN})) {
        // Alternate syntax throw new (Class)()
        ++focal;
        msg = to<std::string>("Heap-allocated exception: throw new (",
                       i[focal].value_, ")();");
      } else {
        // Some other usage of throw new Class().
        msg = "Heap-allocated exception: throw new was used.";
      }
      lintError(
        i[focal],
        to<std::string>(
          msg,
          "\n  This is usually a mistake in c++. "
          "Please refer to the C++ Primer (https://www.intern.facebook.com/"
          "intern/wiki/images/b/b2/C%2B%2B--C%2B%2B_Primer.pdf) for FB "
          "exception guidelines.\n"));
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
uint checkHPHPNamespace(const string& fpath, const vector<Token> & v) {
  uint result = 0;
  uint openBraces = 0;
  uint useBraceLevel = 0;
  bool usingHPHPNamespace = false;
  bool gotRequireModule = false;
  static const vector<StringPiece> blacklist =
    {"c_", "f_", "k_", "ft_"};
  bool isSigmaFXLCode = false;
  Token sigmaCode;
  FOR_EACH (i, v) {
    std::string s = i->value_.toString();
    boost::to_lower(s);
    bool boundGlobal = false;

    // Track nesting level to determine when HPHP namespace opens / closes
    if (i->type_ == TK_LCURL) {
      ++openBraces;
      continue;
    }
    if (i->type_ == TK_RCURL) {
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
    if (atSequence(i, {TK_USING, TK_NAMESPACE})) {
      i += 2;
      if (i->type_ == TK_DOUBLE_COLON) {
        // optional syntax
        ++i;
      }
      if (i->type_ != TK_IDENTIFIER) {
        lintError(*i, to<std::string>(
                    "Symbol ", i->value_,
                    " not valid in using namespace declaration.\n"));
        ++result;
        continue;
      }
      if (i->value_ == "HPHP" && !usingHPHPNamespace) {
        usingHPHPNamespace = true;
        useBraceLevel = openBraces;
        continue;
      }
    }

    // Find identifiers, but make sure we start from top level name scope
    if (atSequence(i, {TK_DOUBLE_COLON, TK_IDENTIFIER})) {
      ++i;
      boundGlobal = true;
    }
    if (i->type_ == TK_IDENTIFIER) {
      bool inHPHPScope = usingHPHPNamespace && !boundGlobal;
      bool boundHPHP = false;
      if (atSequence(&i[1], {TK_DOUBLE_COLON, TK_IDENTIFIER})) {
        if (i->value_ == "HPHP") {
          inHPHPScope = true;
          boundHPHP = true;
          i+=2;
        }
      }
      if (inHPHPScope) {
        if (i->value_ == "f_require_module") {
          gotRequireModule = true;
        }
        // exempt std::string.c_str
        if (!gotRequireModule && !(i->value_ == "c_str" && !boundHPHP)) {
          FOR_EACH(l, blacklist) {
            if (i->value_.size() > l->size()) {
              StringPiece substr = i->value_.subpiece(0, l->size());
              if (substr.compare(*l) == 0) {
                lintError(*i, to<std::string>("Missing f_require_module before "
                  "suspected HPHP namespace reference ", i->value_, "\n"));
                ++result;
              }
            }
          }
        }
      }
      // strip remaining sub-scoped tokens
      while (atSequence(i, {TK_IDENTIFIER, TK_DOUBLE_COLON})) {
        i += 2;
      }
    }
  }
  return result;
}


/**
 * Lint check: Ensures that no files contain deprecated includes.
 */
uint checkDeprecatedIncludes(const string& fpath, const vector<Token>& v) {
  // Set storing the deprecated includes. Add new headers here if you'd like
  // to deprecate them
  static const std::set<string> deprecatedIncludes = {
    "common/base/Base.h",
    "common/base/StringUtil.h",
  };

  uint result = 0;
  FOR_EACH (i, v) {
    if (i->type_ != TK_INCLUDE) continue;
    if (i[1].type_ != TK_STRING_LITERAL ||
        i[1].value_ == "PRECOMPILED") continue;

    string includedFile = getIncludedPath(i[1].value_).string();

    if (deprecatedIncludes.find(includedFile) !=
        deprecatedIncludes.end()) {
      lintWarning(*i, to<std::string>("Including deprecated header ",
                               includedFile, "\n"));
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
uint checkIncludeAssociatedHeader(
    const string& fpath,
    const vector<Token>& v) {
  if (!isSource(fpath)) {
    return 0;
  }

  auto fileName = fs::path(fpath).filename().string();
  auto fileNameBase = getFileNameBase(fileName);
  auto parentPath = fs::absolute(fpath).parent_path().normalize().string();
  uint totalIncludesFound = 0;

  FOR_EACH (i, v) {
    if (i->type_ != TK_INCLUDE) continue;
    if (i[1].value_ == "PRECOMPILED") continue;

    ++totalIncludesFound;

    if (i[1].type_ != TK_STRING_LITERAL) continue;

    string includedFile = getIncludedPath(i[1].value_).filename().string();
    string includedParentPath =
      getIncludedPath(i[1].value_).parent_path().string();

    if (getFileNameBase(includedFile) == fileNameBase &&
        (includedParentPath.empty() ||
         StringPiece(parentPath).endsWith('/' + includedParentPath))) {
      if (totalIncludesFound > 1) {
        lintError(*i, to<std::string>("The associated header file of .cpp "
          "files should be included before any other includes.\n(This "
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
uint checkMemset(const string& fpath, const vector<Token>& v) {
  uint result = 0;

  FOR_EACH (i, v) {
    if (!atSequence(i, {TK_IDENTIFIER, TK_LPAREN}) || i->value_ != "memset") {
      continue;
    }
    vector<Argument> args;
    Argument functionName(i,i);
    if (!getFunctionNameAndArguments(i, functionName, args)) {
      return result;
    }

    // If there are more than 3 arguments, then there might be something wrong
    // with skipTemplateSpec but the iterator didn't reach the EOF (because of
    // a '>' somewhere later in the code). So we only deal with the case where
    // the number of arguments is correct.
    if (args.size() == 3) {
      // wrong calls include memset(..., ..., 0) and memset(..., sizeof..., 1)
      bool error =
        ((args[2].last - args[2].first) == 1) &&
        (
          (args[2].first->value_ == "0") ||
          (args[2].first->value_ == "1" && args[1].first->value_ == "sizeof")
        );
      if (!error) {
        continue;
      }
      std::swap(args[1], args[2]);
      lintError(*functionName.first,
        "Did you mean " + formatFunction(functionName, args) + "?\n");
      result++;
    }
  }
  return result;
}

uint checkInlHeaderInclusions(const std::string& fpath,
                              const std::vector<Token>& v) {
  uint result = 0;

  auto fileName = fs::path(fpath).filename().string();
  auto fileNameBase = getFileNameBase(fileName);

  FOR_EACH (it, v) {
    if (!atSequence(it, { TK_INCLUDE, TK_STRING_LITERAL })) continue;
    ++it;

    auto includedPath = getIncludedPath(it->value_);
    if (getFileCategory(includedPath.string()) != FileCategory::INL_HEADER) {
      continue;
    }

    if (getFileNameBase(includedPath.filename().string()) == fileNameBase) {
      continue;
    }

    lintError(*it, to<std::string>("A -inl file (", includedPath.string(),
      ") was included even though this is not its associated header. "
      "Usually files like Foo-inl.h are implementation details and should "
      "not be included outside of Foo.h.\n"));
    ++result;
  }

  return result;
}

uint checkFollyDetail(const std::string& fpath,
                      const std::vector<Token>& v) {
  if (boost::contains(fpath, "folly")) return 0;

  uint result = 0;
  FOR_EACH (it, v) {
    if (!atSequence(it, { TK_IDENTIFIER, TK_DOUBLE_COLON,
                          TK_IDENTIFIER, TK_DOUBLE_COLON })) {
      continue;
    }
    if (it->value_ == "folly" && it[2].value_ == "detail") {
      lintError(*it, to<std::string>("Code from folly::detail is logically "
                              "private, please avoid use outside of "
                              "folly.\n"));
      ++result;
    }
  }

  return result;
}

/**
 * Lint check: classes should not have protected inheritance.
 */
uint checkProtectedInheritance(const std::string& fpath,
                                    const std::vector<Token>& v) {

  uint result = iterateClasses(v,
    [&](vector<Token>::const_iterator it, const vector<Token>& v) -> uint {

      uint result = 0;

      auto term = std::find_if(it, v.end(),
                     boost::bind(&Token::type_, _1) == TK_COLON ||
                     boost::bind(&Token::type_, _1) == TK_LCURL);

      if (term == v.end()) {
        return result;
      }

      for (; it->type_ != TK_EOF; ++it) {
        if (it->type_ == TK_LCURL) {
          break;
        }

        // Detect a member access specifier.
        if (it->type_ == TK_PROTECTED
            && it[1].type_ == TK_IDENTIFIER) {
          lintWarning(*it, "Protected inheritance is sometimes not a good "
              "idea. Read "
              "http://stackoverflow.com/questions/6484306/effective-c-discouraging-protected-inheritance "
              "for more information.\n");
          ++result;
        }
      }

      return result;
    }
  );

  return result;
}

uint checkUpcaseNull(const std::string& fpath,
                     const std::vector<Token>& v) {
  uint ret = 0;
  FOR_EACH (it, v) {
    if (it->type_ == TK_IDENTIFIER && it->value_ == "NULL") {
      lintAdvice(*it,
        "Prefer `nullptr' to `NULL' in new C++ code.  Unlike `NULL', "
        "`nullptr' can't accidentally be used in arithmetic or as an "
        "integer. See "
        "http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2431.pdf"
        " for details.\n");
      ++ret;
    }
  }
  return ret;
}

static bool endsClass(TokenType tkt) {
  return tkt == TK_EOF || tkt == TK_LCURL || tkt == TK_SEMICOLON;
}

static bool isAccessSpecifier(TokenType tkt) {
  return (tkt == TK_PRIVATE || tkt == TK_PUBLIC || tkt == TK_PROTECTED);
}

static bool checkExceptionAndSkip(vector<Token>::const_iterator& it) {
  if (atSequence(it, {TK_IDENTIFIER, TK_DOUBLE_COLON})) {
    if (it->value_ != "std") {
      it += 2;
      return false;
    }
    it += 2;
  }

  return (it->type_ == TK_IDENTIFIER && it->value_ == "exception");
}

static bool badExceptionInheritance(TokenType classType,
                                            TokenType access) {
  return ((classType == TK_CLASS && access != TK_PUBLIC) ||
          (classType == TK_STRUCT && access == TK_PRIVATE)
  );
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
uint checkExceptionInheritance(const std::string& fpath,
                                    const std::vector<Token>& v) {
  uint result = iterateClasses(v,
    [&](vector<Token>::const_iterator it, const vector<Token>& v) -> uint {
      TokenType classType = it->type_; // struct, union or class

      if (classType == TK_UNION) return 0;

      while (!endsClass(it->type_) && it->type_ != TK_COLON) {
        ++it;
      }
      if (it->type_ != TK_COLON) {
        return 0;
      }

      ++it;

      TokenType access = TK_PROTECTED; // not really, just a safe initializer
      bool warn = false;
      while (!endsClass(it->type_)) {
        if (isAccessSpecifier(it->type_)) {
          access = it->type_;
        } else if (it->type_ == TK_COMMA) {
          access = TK_PROTECTED; // reset
        } else if (checkExceptionAndSkip(it)) {
          warn = badExceptionInheritance(classType, access);
        }
        if (warn) {
          lintWarning(*it, "std::exception should not be inherited "
            "non-publicly, as this base class will not be accessible in "
            "try..catch(const std::exception& e) outside the derived class. "
            "See C++ standard section 11.2 [class.access.base] / 4.\n");
          return 1;
        }
        ++it;
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
uint checkUniquePtrUsage(const string& fpath, const std::vector<Token>& v) {
  uint result = 0;

  FOR_EACH (iter, v) {
    auto const ident = readQualifiedIdentifier(iter);
    bool ofInterest =
      (ident.size() == 1 && ident[0] == "unique_ptr") ||
      (ident.size() == 2 && ident[0] == "std" && ident[1] == "unique_ptr");
    if (!ofInterest) continue;

    // Keep the outer loop separate from the lookahead from here out.
    // We want this after the detection of {std::}?unique_ptr above or
    // we'd give errors on qualified unique_ptr's twice.
    auto i = iter;

    // Save the unique_ptr iterator because we'll raise any warnings
    // on that line.
    auto const uniquePtrIt = i;

    // Determine if the template parameter is an array type.
    if (i->type_ != TK_LESS) continue;
    bool uniquePtrHasArray = false;
    i = skipTemplateSpec(i, &uniquePtrHasArray);
    if (i->type_ == TK_EOF) {
      return result;
    }
    assert(i->type_ == TK_GREATER);
    ++i;

    /*
     * We should see an optional identifier, then an open paren, or
     * something is weird so bail instead of giving false positives.
     *
     * Note that we could be looking at a function declaration and its
     * return type right now---we're assuming we won't see a
     * new-expression in the argument declarations.
     */
    if (i->type_ == TK_IDENTIFIER) ++i;
    if (i->type_ != TK_LPAREN) continue;
    ++i;

    unsigned parenNest = 1;
    for (; i->type_ != TK_EOF; ++i) {
      if (i->type_ == TK_LPAREN) {
        ++parenNest;
        continue;
      }

      if (i->type_ == TK_RPAREN) {
        if (--parenNest == 0) break;
        continue;
      }

      if (i->type_ != TK_NEW || parenNest != 1) continue;
      ++i;

      // We're looking at the new expression we care about.  Try to
      // ensure it has array brackets only if the unique_ptr type did.
      while (i->type_ == TK_IDENTIFIER || i->type_ == TK_DOUBLE_COLON) {
        ++i;
      }
      if (i->type_ == TK_LESS) {
        i = skipTemplateSpec(i);
        if (i->type_ == TK_EOF) return result;
        ++i;
      } else {
        while (atBuiltinType(i)) ++i;
      }
      while (i->type_ == TK_STAR || i->type_ == TK_CONST ||
          i->type_ == TK_VOLATILE) {
        ++i;
      }

      bool newHasArray = i->type_ == TK_LSQUARE;
      if (newHasArray != uniquePtrHasArray) {
        lintError(
          *uniquePtrIt,
          to<std::string>(
            uniquePtrHasArray
            ? "unique_ptr<T[]> should be used with an array type\n"
            : "unique_ptr<T> should be unique_ptr<T[]> when "
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
uint checkSmartPtrUsage(const string& fpath, const std::vector<Token>& v) {
  uint result = 0;

  FOR_EACH (i, v) {
    // look for unqualified 'shared_ptr<' or 'namespace::shared_ptr<' where
    // namespace is one of 'std', 'boost' or 'facebook'
    if (i->type_ != TK_IDENTIFIER) continue;
    auto const startLine = i;
    auto const ns = i->value_;
    if (i[1].type_ == TK_DOUBLE_COLON) {
      i += 2;
      if (!atSequence(i, {TK_IDENTIFIER, TK_LESS})) continue;
    } else if (i[1].type_ != TK_LESS) {
      continue;
    }
    auto const fn = i->value_;
    // check that we have the functions and namespaces we care about
    if (fn != "shared_ptr") continue;
    if (fn != ns && ns != "std" && ns != "boost" && ns != "facebook") {
      continue;
    }

    // skip over the template specification
    ++i;
    i = skipTemplateSpec(i);
    if (i->type_ == TK_EOF) {
      return result;
    }
    i++;
    // look for a possible function call
    if (!atSequence(i, {TK_IDENTIFIER, TK_LPAREN})) continue;

    ++i;
    vector<Argument> args;
    // ensure the function call first argument is a new expression
    if (!getRealArguments(i, args)) continue;

    if (i->type_ == TK_RPAREN && i[1].type_ == TK_SEMICOLON
        && args.size() > 0 && args[0].first->type_ == TK_NEW) {
      // identifies what to suggest:
      // shared_ptr should be  make_shared unless there are 3 args in which
      // case an allocator is used and thus suggests allocate_shared.
      string newFn = args.size() == 3 ? "allocate_shared" :  "make_shared";
      string qFn = ns.str();
      string qNewFn = newFn;
      if (ns != fn) {
        qFn.append("::").append(fn.str());
        qNewFn.insert(0, "::").insert(0, ns.str());
      }
      lintWarning(*startLine,
          to<std::string>(qFn, " should be replaced by ", qNewFn, ". ", newFn,
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
uint checkBannedIdentifiers(const std::string& fpath,
                            const std::vector<Token>& v) {
  uint result = 0;

  // Map from identifier to the rationale.
  std::unordered_map<std::string, std::string> warnings = {
    // https://svn.boost.org/trac/boost/ticket/5699
    //
    // Also: deleting a thread_specific_ptr to an object that contains
    // another thread_specific_ptr can lead to corrupting an internal
    // std::map.
    { "thread_specific_ptr",
      "There are known bugs and performance downsides to the use of "
      "this class.  Use folly::ThreadLocal instead."
    },
  };

  FOR_EACH (i, v) {
    if (i->type_ != TK_IDENTIFIER) continue;
    auto warnIt = warnings.find(i->value_.toString());
    if (warnIt == warnings.end()) continue;
    lintError(*i, warnIt->second);
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
uint checkNamespaceScopedStatics(const std::string& fpath,
                                 const std::vector<Token>& v) {
  if (!isHeader(fpath)) {
    // This check only looks at headers. Inside .cpp files, knock
    // yourself out.
    return 0;
  }

  uint result = 0;
  FOR_EACH (it, v) {
    if (atSequence(it, {TK_NAMESPACE, TK_IDENTIFIER, TK_LCURL})) {
      // namespace declaration. Reposition the iterator on TK_LCURL
      it += 2;
    } else if (atSequence(it, {TK_NAMESPACE, TK_LCURL})) {
      // nameless namespace declaration. Reposition the iterator on TK_LCURL.
      ++it;
    } else if (it->type_ == TK_LCURL) {
      // Found a '{' not belonging to a namespace declaration. Skip the block,
      // as it can only be an aggregate type, function or enum, none of
      // which are interesting for this rule.
      it = skipBlock(it);
    } else if (it->type_ == TK_STATIC) {
      lintWarning(*it,
                  "Avoid using static at global or namespace scope "
                  "in C++ header files.\n");
      ++result;
    }
  }

  return result;
}

/*
 * Lint check: disallow the declaration of mutex holders
 * with no name, since that causes the destructor to be called
 * on the same line, releasing the lock immediately.
*/
uint checkMutexHolderHasName(const std::string& fpath,
                             const std::vector<Token>& v) {
  if (getFileCategory(fpath) == FileCategory::SOURCE_C) {
    return 0;
  }

  std::set<std::string> mutexHolderNames = {"lock_guard",
                                            "unique_lock"};
  uint result = 0;

  FOR_EACH (it, v) {
    if (atSequence(it, {TK_IDENTIFIER, TK_LESS})) {
      if (mutexHolderNames.count(it->value_.str()) > 0) {
        it = skipTemplateSpec(++it);
        if (atSequence(it, {TK_GREATER, TK_LPAREN})) {
          lintError(*it, "Mutex holder variable declared without a name, "
                         "causing the lock to be released immediately.\n");
          ++result;
        }
      }
    }
  }

  return result;
}

/*
 * Lint check: prevent OSS-fbcode projects from including other projects
 * from fbcode.
 */
uint checkOSSIncludes(const std::string& fpath, const std::vector<Token>& v) {
  uint result = 0;

  // strip fpath of '.../fbcode/', if present
  auto pos = fpath.find("/fbcode/");
  if (pos == std::string::npos) pos = -8;
  auto ppath = fpath.substr(pos + 8);

  // Only perform this check in projects which are open sourced.
  if (!boost::starts_with(ppath, "folly/")
      && (!boost::starts_with(ppath, "hphp/")
           || boost::starts_with(ppath, "hphp/facebook/"))) return 0;

  auto project = ppath.substr(0, ppath.find('/'));

  // Find all occurrences of '#include "..."'. Ignore '#include <...>', since
  // <...> is not used for fbcode includes.
  FOR_EACH (it, v) {
    if (atSequence(it, {TK_INCLUDE, TK_STRING_LITERAL})) {
      ++it;
      auto includePath = it->value_;

      // Includes from other projects always contain a '/'.
      auto slash = includePath.find('/');
      if (slash == std::string::npos) continue;

      auto includeProject = includePath.subpiece(1, slash-1);

      // If the included file is from the same project, or from folly,
      // then it is ok.
      if (includeProject == project || includeProject == "folly") continue;

      // If the include is followed by the comment 'nolint' then it is ok.
      auto nit = it + 1; // Do not increment 'it' - increment a copy.
      if (nit == v.end() || nit->precedingWhitespace_.find("nolint")
                              != std::string::npos) continue;

      // Finally, the lint error.
      lintError(*it, "Open Source Software may not include files from "
                     "other fbcode projects (except folly). "
                     "If this is not an fbcode include, please use "
                     "'#include <...>' instead of '#include \"...\"'. "
                     "You may suppress this warning by including the "
                     "comment 'nolint' after the #include \"...\".\n");
      ++result;
    }
  }
  return result;
}

/**
 * structure about the current statement block
 * @name: the name of statement
 * @openBraces: the number of open braces in this block
 */
struct StatementBlockInfo {
  StringPiece name;
  uint openBraces;
};

/*
 * Starting from a left parent brace, skips until it finds the matching
 * balanced right parent brace.
 * Returns: iterator pointing to the final TK_RPAREN
 */
template <class Iterator>
Iterator skipParens(Iterator it) {
  CHECK_EQ(it->type_, TK_LPAREN);

  uint openParens = 1;
  ++it;
  for (; it->type_ != TK_EOF; ++it) {
    if (it->type_ == TK_LPAREN) {
      ++openParens;
      continue;
    }
    if (it->type_ == TK_RPAREN) {
      if (!--openParens) {
        break;
      }
      continue;
    }
  }
  return it;
}

/*
 * Lint check: disable use of "break"/"continue" inside
 * SYNCHRONIZED pseudo-statements
*/
uint checkBreakInSynchronized(const std::string& fpath,
                             const std::vector<Token>& v) {
  uint result = 0;
  std::stack<StatementBlockInfo> nestedStatements;

  FOR_EACH (it, v) {
    if (it->type_ == TK_WHILE || it->type_ == TK_SWITCH
        || it->type_ == TK_DO || it->type_ == TK_FOR) {
      StatementBlockInfo s;
      s.name = it->value_;
      s.openBraces = 0;
      nestedStatements.push(s);

      //skip the block in "(" and ")" following "for" statement
      if (it->type_ == TK_FOR)
        it = skipParens(++it);
      continue;
    }

    if (it->type_ == TK_LCURL) {
      if (!nestedStatements.empty())
        nestedStatements.top().openBraces++;
      continue;
    }

    if (it->type_ == TK_RCURL) {
      if (!nestedStatements.empty()) {
        nestedStatements.top().openBraces--;
        if(nestedStatements.top().openBraces == 0)
          nestedStatements.pop();
      }
      continue;
    }

    //incase there is no "{"/"}" in for/while statements
    if (it->type_ == TK_SEMICOLON) {
      if (!nestedStatements.empty() &&
          nestedStatements.top().openBraces == 0)
        nestedStatements.pop();
      continue;
    }

    if (it->type_ == TK_IDENTIFIER) {
      std::string strID = it->value_.toString();
      if (strID.compare("SYNCHRONIZED") == 0 ||
          strID.compare("UNSYNCHRONIZED") == 0 ||
          strID.compare("TIMED_SYNCHRONIZED") == 0 ||
          strID.compare("SYNCHRONIZED_CONST") == 0 ||
          strID.compare("TIMED_SYNCHRONIZED_CONST") == 0) {
        StatementBlockInfo s;
        s.name = "SYNCHRONIZED";
        s.openBraces = 0;
        nestedStatements.push(s);
        continue;
      }
    }

    if (it->type_ == TK_BREAK || it->type_ == TK_CONTINUE) {
      if (!nestedStatements.empty() &&
        nestedStatements.top().name == "SYNCHRONIZED") {
        lintError(*it, "Cannot use break/continue inside "
          "SYNCHRONIZED pseudo-statement\n"
        );
        ++result;
      }
      continue;
    }
  }
  return result;
}

} } // namespace flint namespace facebook
