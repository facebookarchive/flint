#include "Checks.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <cassert>
#include <stdexcept>

#include "Options.hpp"
#include "Polyfill.hpp"
#include "ErrorReport.hpp"
#include "FileCategories.hpp"

namespace flint {

// Shorthand for comparing two strings
#define cmpStr(a,b) ((a).compare((b)) == 0)
#define cmpTok(a,b) cmpStr((a).value_, (b))

#define cmpToks(a,b) cmpStr((a).value_, (b).value_)

// Shorthand for comparing a Token and TokenType
#define isTok(a,b) ((a).type_ == (b))

	namespace { // Anonymous Namespace for Token stream traversal functions

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

		void lintError(ErrorFile &errors, const Token &tok, const string title, const string desc = "") {
			errors.addError(ErrorObject(Lint::ERROR, tok.line_, title, desc));
		};
		void lintWarning(ErrorFile &errors, const Token &tok, const string title, const string desc = "") {
			errors.addError(ErrorObject(Lint::WARNING, tok.line_, title, desc));
		};
		void lintAdvice(ErrorFile &errors, const Token &tok, const string title, const string desc = "") {
			errors.addError(ErrorObject(Lint::ADVICE, tok.line_, title, desc));
		};

		void lint(ErrorFile &errors, const Token &tok, const Lint level, const string title, const string desc = "") {
			errors.addError(ErrorObject(level, tok.line_, title, desc));
		};

		/**
		* Returns whether the current token is at the start of a given sequence
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param list
		*		The token list for the desired sequence
		* @return
		*		Returns true if we were at the start of a given sequence
		*/
		bool atSequence(const vector<Token> &tokens, size_t pos, const vector<TokenType> &list) {

			if ((pos + list.size() + 1) >= tokens.size()) {
				return false;
			}

      		return find_if(begin(list), end(list), [pos, &tokens](const TokenType &token) mutable { 
					return !isTok(tokens[pos++], token); 
				}) == end(list);
		};

		/**
		* Moves pos to the next position of the target token
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param target
		*		The token to match
		* @return
		*		Returns true if we are at the given token
		*/
		bool skipToToken(const vector<Token> &tokens, size_t &pos, TokenType target) {
			for (; pos < tokens.size() && !isTok(tokens[pos], target); ++pos) {}
			return (pos < tokens.size() && isTok(tokens[pos], target));
		};

		/**
		* Strips the ""'s or <>'s from an #include path
		*
		* @param path
		*		The string to trim
		* @return
		*		Returns the include path without it's wrapping quotes/brackets
		*/
		string getIncludedPath(const string &path) {
			return path.substr(1, path.size() - 2);
		};

		/**
		* Traverses the token list until the whole template sequence has been passed
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param containsArray
		*		Optional parameter to return a bool of whether an array was found inside
		*		the template list
		* @return
		*		Returns the position of the closing angle bracket
		*/
		size_t skipTemplateSpec(const vector<Token> &tokens, size_t pos, bool *containsArray = nullptr) {
			assert(isTok(tokens[pos], TK_LESS));

			uint angleNest = 1; // Because we began on the leading '<'
			uint parenNest = 0;

			if (containsArray) {
				*containsArray = false;
			}

			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				TokenType tok = tokens[pos].type_;

				if (tok == TK_LPAREN) {
					++parenNest;
					continue;
				}
				if (tok == TK_RPAREN) {
					--parenNest;
					continue;
				}

				// Ignore angles inside of parens.  This avoids confusion due to
				// integral template parameters that use < and > as comparison
				// operators.
				if (parenNest > 0) {
					continue;
				}

				if (tok == TK_LSQUARE) {
					if (angleNest == 1 && containsArray) {
						*containsArray = true;
					}
					continue;
				}

				if (tok == TK_LESS) {
					++angleNest;
					continue;
				}
				if (tok == TK_GREATER) {
					// Removed decrement/zero-check as one line
					// It's not a race guys, readability > length of code
					--angleNest;
					if (angleNest == 0) {
						break;
					}
					continue;
				}
			}

			return pos;
		};

		/**
		* Returns whether the current token is a built in type
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @return
		*		Returns true is the token as pos is a built in type
		*/
		bool atBuiltinType(const vector<Token> &tokens, size_t pos) {

			static const vector<TokenType> builtIns = {
				TK_DOUBLE,
				TK_FLOAT,
				TK_INT,
				TK_SHORT,
				TK_UNSIGNED,
				TK_LONG,
				TK_SIGNED,
				TK_VOID,
				TK_BOOL,
				TK_WCHAR_T,
				TK_CHAR
			};

			return find(begin(builtIns), end(builtIns), tokens[pos].type_) != end(builtIns);
		};

		/**
		* Heuristically read a potentially namespace-qualified identifier,
		* advancing 'pos' in the process.
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @return
		*		Returns a vector of all the identifier values involved, or an
		*		empty vector if no identifier was detected.
		*/
		vector<string> readQualifiedIdentifier(const vector<Token> &tokens, size_t &pos) {

			vector<string> ret;
			for (; isTok(tokens[pos], TK_IDENTIFIER) || isTok(tokens[pos], TK_DOUBLE_COLON); ++pos) {
				if (isTok(tokens[pos], TK_IDENTIFIER)) {
					ret.push_back(tokens[pos].value_);
				}
			}
			return ret;
		};

		/**
		* Traverses the token list until the whole code block has been passed
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @return
		*		Returns the position of the closing curly bracket
		*/
		size_t skipBlock(const vector<Token> &tokens, size_t pos) {
			assert(isTok(tokens[pos], TK_LCURL));

			uint openBraces = 1; // Because we began on the leading '{'

			++pos;
			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				Token tok = tokens[pos];

				if (isTok(tok, TK_LCURL)) {
					++openBraces;
					continue;
				}
				if (isTok(tok, TK_RCURL)) {
					// Removed decrement/zero-check as one line
					// It's not a race guys, readability > length of code
					--openBraces;
					if (openBraces == 0) {
						break;
					}
					continue;
				}
			}

			return pos;
		};

		/**
		* Traverses the token list and runs a Callback function on each
		* class/struct/union it finds
		*
		* @param errors
		*		Struct to track how many errors/warnings/advice occured
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param callback
		*		The function to run on each code object
		*/
		template<class Callback>
		void iterateClasses(ErrorFile &errors, const vector<Token> &tokens, const Callback &callback) {

			for (size_t pos = 0; pos < tokens.size() - 1; ++pos) {
				// Skip template sequence if we find ... template< ...
				if (atSequence(tokens, pos, { TK_TEMPLATE, TK_LESS })) {
					pos = skipTemplateSpec(tokens, ++pos);
					continue;
				}

				TokenType tok = tokens[pos].type_;
				if (tok == TK_CLASS || tok == TK_STRUCT || tok == TK_UNION) {
					callback(errors, tokens, pos);
				}
			}
		};

		/**
		* Starting from a function name or one of its arguments, skips the entire
		* function prototype or function declaration (including function body).
		*
		* Implementation is simple: stop at the first semicolon, unless an opening
		* curly brace is found, in which case we stop at the matching closing brace.
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @return
		*		Returns the position of the closing curly bracket or semicolon
		*/
		size_t skipFunctionDeclaration(const vector<Token> &tokens, size_t pos) {

			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				TokenType tok = tokens[pos].type_;

				if (tok == TK_SEMICOLON) { // Function Prototype
					break;
				}
				else if (tok == TK_LCURL) { // Full Declaration
					pos = skipBlock(tokens, pos);
					break;
				}
			}

			return pos;
		};

		/**
		* Represent an argument or the name of a function.
		* first is an iterator that points to the start of the argument.
		* last is an iterator that points to the token right after the end of the
		* argument.
		*/
		struct Argument {
			size_t first;
			size_t last;

			Argument(size_t a, size_t b) : first(a), last(b) {
				// Just to check the port hasn't broken Token traversal somehow
				assert(first <= last);
			};
		};

		/**
		* Take the bounds of an argument list and pretty print it to a string
		*
		* @param tokens
		*		The token list for the file
		* @param arg
		*		A struct representing the bounds of the argument list tokens
		* @return
		*		Returns a string representation of the argument token list
		*/
		string formatArg(const vector<Token> &tokens, const Argument &arg) {
			string result;

			for (size_t pos = arg.first; pos < arg.last; ++pos) {
				if (pos != arg.first && !(tokens[pos].precedingWhitespace_.empty())) {
					result.push_back(' ');
				}

				result += tokens[pos].value_;
			}
			return result;
		};

		/**
		* Pretty print a function declaration/prototype to a string
		*
		* @param tokens
		*		The token list for the file
		* @param func
		*		A reference to the name of the function
		* @param args
		*		A list of arguments for the function
		* @return
		*		Returns a string representation of the argument token list
		*/
		string formatFunction(const vector<Token> &tokens, const Argument &func, const vector<Argument> &args) {

			string result = formatArg(tokens, func) + "(";

			for (size_t i = 0; i < args.size(); ++i) {
				if (i > 0) {
					result += ", ";
				}

				result += formatArg(tokens, args[i]);
			}

			result += ")";
			return result;
		};

		/**
		* Get the list of arguments of a function, assuming that the current
		* iterator is at the open parenthesis of the function call. After the this
		* method is call, the iterator will be moved to after the end of the function
		* call.
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param args
		*		A reference to the list to fill with arguments
		* @return
		*		Returns true if we believe (sorta) that everything went okay,
		*		false if something bad happened (maybe)
		*/
		bool getRealArguments(const vector<Token> &tokens, size_t &pos, vector<Argument> &args) {
			assert(isTok(tokens[pos], TK_LPAREN));

			++pos;
			size_t argStart = pos; // First arg starts after parenthesis
			int parenCount = 1;

			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				TokenType tok = tokens[pos].type_;

				if (tok == TK_LPAREN) {
					++parenCount;
					continue;
				}
				if (tok == TK_RPAREN) {
					// Removed decrement/zero-check as one line
					// It's not a race guys, readability > length of code
					--parenCount;
					if (parenCount == 0) {
						break;
					}
					continue;
				}
				/*
				if (tok == TK_LESS) {
					// This is a heuristic which would fail when < is used with
					// the traditional meaning in an argument, e.g.
					//  memset(&foo, a < b ? c : d, sizeof(foo));
					// but currently we have no way to distinguish that use of
					// '<' and
					//  memset(&foo, something<A,B>(a), sizeof(foo));
					// We include this heuristic in the hope that the second
					// use of '<' is more common than the first.
					pos = skipTemplateSpec(tokens, pos);
					continue;
				}
				*/
				if (tok == TK_COMMA) {
					if (parenCount == 1) {
						// end an argument of the function we are looking at
						args.push_back(Argument(argStart, pos));
						argStart = pos + 1;
					}
					continue;
				}
			}

			if (pos >= tokens.size() || isTok(tokens[pos], TK_EOF)) {
				return false;
			}

			if (argStart != pos) {
				args.push_back(Argument(argStart, pos));
			}
			return true;
		};

		/**
		* Get the argument list of a function, with the first argument being the
		* function name plus the template spec.
		*
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param func
		*		A reference to the name of the function
		* @param args
		*		A reference to the list to fill with arguments
		* @return
		*		Returns true if we believe (sorta) that everything went okay,
		*		false if something bad happened (maybe)
		*/
		bool getFunctionNameAndArguments(const vector<Token> &tokens, size_t &pos
			, Argument &func, vector<Argument> &args) {

			func.first = pos;
			++pos;

			if (pos < tokens.size() && isTok(tokens[pos], TK_LESS)) {
				pos = skipTemplateSpec(tokens, pos);

				if (pos >= tokens.size() || isTok(tokens[pos], TK_EOF)) {
					return false;
				}
				++pos;
			}
			func.last = pos;
			return getRealArguments(tokens, pos, args);
		};

	}; // Anonymous Namespace

	/**
	* Check all member intializations to make sure they do not initialize on themselves
	*
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	* @return
	*		Returns the number of errors this check found in the token stream
	*/
	void checkInitializeFromItself(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		// Token Sequences for parameter initializers
		const vector<TokenType> firstInitializer = {
			TK_COLON, TK_IDENTIFIER, TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
		};
		const vector<TokenType> nthInitializer = {
			TK_COMMA, TK_IDENTIFIER, TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
		};

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (atSequence(tokens, pos, firstInitializer) || atSequence(tokens, pos, nthInitializer)) {

				size_t outerPos = ++pos;     // +1 for identifier
				size_t innerPos = ++(++pos); // +2 again for the inner identifier

				bool isMember = tokens[outerPos].value_.back() == '_' ||
								startsWith(tokens[outerPos].value_, "m_");

				if (isMember && cmpToks(tokens[outerPos], tokens[innerPos])) {
					lintError(errors, tokens[outerPos],
						"Initializing class member '" + tokens[outerPos].value_ + "' with itself.");
				}
			}
		}
	};

	/**
	* Check for blacklisted sequences of tokens
	*
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	* @return
	*		Returns the number of errors this check found in the token stream
	*/
	void checkBlacklistedSequences(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		struct BlacklistEntry {
			vector<TokenType> tokens;
			string title, descr;
			bool cpponly;
			BlacklistEntry(const vector<TokenType> &t, const string &h, const string &d, bool cpponly)
				: tokens(t), title(h), descr(d), cpponly(cpponly) {};
		};

		static const vector<BlacklistEntry> blacklist = {
			{ { TK_VOLATILE },
			"'volatile' is not thread-safe.",
			"If multiple threads are sharing data, use std::atomic or locks. In addition, 'volatile' may "
			"force the compiler to generate worse code than it could otherwise. "
			"For more about why 'volatile' doesn't do what you think it does, see "
			"http://www.kernel.org/doc/Documentation/volatile-considered-harmful.txt.",
			true, // C++ only.
			}
		};

		static const vector< vector<TokenType> > exceptions = {
			{ TK_ASM, TK_VOLATILE }
		};

		bool isException = false;

		for (size_t pos = 0; pos < tokens.size(); ++pos) {

			// Make sure we aren't at an exception to the blacklist
			for (const auto &e : exceptions) {
				if (atSequence(tokens, pos, e)) {
					isException = true;
					break;
				}
			}

			for (const BlacklistEntry &entry : blacklist) {
				if (!atSequence(tokens, pos, entry.tokens)) {
					continue;
				}
				if (isException) {
					isException = false;
					continue;
				}
				if (Options.CMODE && entry.cpponly) {
					continue;
				}

				lintWarning(errors, tokens[pos], entry.title, entry.descr);
			}
		}
	};

	/**
	* Check for blacklisted identifiers
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkBlacklistedIdentifiers(ErrorFile &errors, const string &path, const vector<Token> &tokens) {


		static const map<string, pair<Lint,string>> blacklist = {
			{ "strtok",
				{ Lint::ERROR, "'strtok' is not thread safe. Consider 'strtok_r'." }
			},

			{ "NULL",
				{ Lint::ADVICE, "Prefer `nullptr' to `NULL' in new C++ code." }
			}
		};

		for (size_t pos = 0; pos < tokens.size(); ++pos) {

			if (isTok(tokens[pos], TK_IDENTIFIER)) {
				for (const auto &entry : blacklist) {
					if (cmpTok(tokens[pos], entry.first)) {
						auto desc = entry.second;
						lint(errors, tokens[pos], desc.first, desc.second);
						continue;
					}
				}
			}
		}
	};

	/**
	* No #defined names use an identifier reserved to the
	* implementation.
	*
	* These are enforcing rules that actually apply to all identifiers,
	* but we're only raising warnings for #define'd ones right now.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkDefinedNames(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		// Exceptions to the check
		static const set<string> okNames = {
			"__STDC_LIMIT_MACROS",
			"__STDC_FORMAT_MACROS",
			"_GNU_SOURCE",
			"_XOPEN_SOURCE"
		};

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (!isTok(tokens[pos], TK_DEFINE)) {
				continue;
			}

			Token tok = tokens[pos + 1];
			string sym = tok.value_;

			if (!isTok(tok, TK_IDENTIFIER)) {
				// This actually happens because people #define private public
				//   for unittest reasons
				lintWarning(errors, tok, "You're not supposed to #define " + sym);
				continue;
			}

			if (sym.size() >= 2 && sym[0] == '_' && isupper(sym[1])) {
				if (okNames.find(sym) != okNames.end()) {
					continue;
				}
				lintWarning(errors, tok, "Symbol " + sym + " invalid.",
					"A symbol may not start with an underscore followed by a capital letter.");
			}
			else if (sym.size() >= 2 && sym[0] == '_' && sym[1] == '_') {
				if (okNames.find(sym) != okNames.end()) {
					continue;
				}
				lintWarning(errors, tok, "Symbol " + sym + " invalid.",
					"A symbol may not begin with two adjacent underscores.");
			}
			else if (!Options.CMODE && sym.find("__") != string::npos) { // !FLAGS_c_mode /* C is less restrictive about this */ &&
				if (okNames.find(sym) != okNames.end()) {
					continue;
				}
				lintWarning(errors, tok, "Symbol " + sym + " invalid. ",
					"A symbol may not contain two adjacent underscores.");
			}
		}
	};

	/**
	* Only the following forms of catch are allowed:
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
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkCatchByReference(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (!isTok(tokens[pos], TK_CATCH)) {
				continue;
			}

			size_t focal = pos + 1;
			if (!isTok(tokens[focal], TK_LPAREN)) { // a "(" comes always after catch
				throw runtime_error(path + ":" + to_string(tokens[focal].line_)
					+ ": Invalid C++ source code, please compile before lint.");
			}
			++focal;

			if (isTok(tokens[focal], TK_ELLIPSIS)) {
				// catch (...
				continue;
			}
			if (isTok(tokens[focal], TK_CONST)) {
				// catch (const
				++focal;
			}
			if (isTok(tokens[focal], TK_TYPENAME)) {
				// catch ([const] typename
				++focal;
			}
			if (isTok(tokens[focal], TK_DOUBLE_COLON)) {
				// catch ([const] [typename] ::
				++focal;
			}

			// At this position we must have an identifier - the type caught,
			// e.g. FBException, or the first identifier in an elaborate type
			// specifier, such as facebook::FancyException<int, string>.
			if (!isTok(tokens[focal], TK_IDENTIFIER)) {

				const Token &tok = tokens[focal];
				lintWarning(errors, tok, "Symbol " + tok.value_ + " invalid in catch clause.",
					"You may only catch user-defined types.");
				continue;
			}
			++focal;

			// We move the focus to the closing paren to detect the "&". We're
			// balancing parens because there are weird corner cases like
			// catch (Ex<(1 + 1)> & e).
			for (size_t parens = 0;; ++focal) {
				if (focal >= tokens.size()) {
					throw runtime_error(path + ":" + to_string(tokens[focal].line_)
						+ ": Invalid C++ source code, please compile before lint.");
				}
				if (isTok(tokens[focal], TK_RPAREN)) {
					if (parens == 0) break;
					--parens;
				}
				else if (isTok(tokens[focal], TK_LPAREN)) {
					++parens;
				}
			}

			// At this point we're straight on the closing ")". Backing off
			// from there we should find either "& identifier" or "&" meaning
			// anonymous identifier.
			if (isTok(tokens[focal - 1], TK_AMPERSAND)) {
				// check! catch (whatever &)
				continue;
			}
			if (isTok(tokens[focal - 1], TK_IDENTIFIER) &&
				isTok(tokens[focal - 2], TK_AMPERSAND)) {
				// check! catch (whatever & ident)
				continue;
			}

			// Oopsies times
			const Token &tok = tokens[focal - 1];
			// Get the type string
			string theType = "";
			for (size_t j = 2; j <= focal - 1; ++j) {
				if (j > 2) theType += " ";
				theType += tokens[j].value_;
			}
			lintError(errors, tok, "Symbol " + tok.value_ + " of type " + theType
				+ " caught by value.", "Use catch by (preferably const) reference throughout.");
		}
	};

	/**
	* Any usage of throw specifications is a lint error.
	*
	* We track whether we are at either namespace or class scope by
	* looking for class/namespace tokens and tracking nesting level.  Any
	* time we go into a { } block that's not a class or namespace, we
	* disable the lint checks (this is to avoid false positives for throw
	* expressions).
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkThrowSpecification(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		// Check for throw specifications inside classes
		iterateClasses(errors, tokens, [&](ErrorFile &errors, const vector<Token> &tokens, size_t pos) -> void {

			const vector<TokenType> destructorSequence = {
				TK_TILDE, TK_IDENTIFIER, TK_LPAREN, TK_RPAREN, TK_THROW, TK_LPAREN, TK_RPAREN
			};
			const vector<TokenType> whatSequence = {
				TK_LPAREN, TK_RPAREN, TK_CONST, TK_THROW, TK_LPAREN, TK_RPAREN
			};

			// Skip to opening '{'
			if (!skipToToken(tokens, pos, TK_LCURL)) {
				return;
			}
			++pos;

			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				Token tok = tokens[pos];

				// Skip warnings for empty throw specifications on destructors,
				// because sometimes it is necessary to put a throw() clause on
				// classes deriving from std::exception.
				if (atSequence(tokens, pos, destructorSequence)) {
					pos += destructorSequence.size();
					continue;
				}

				// This avoids warning if the function is named "what", to allow
				// inheriting from std::exception without upsetting lint.
				if (isTok(tok, TK_IDENTIFIER) && cmpTok(tok, "what")) {
					++pos;
					if (atSequence(tokens, pos, whatSequence)) {
						pos += whatSequence.size();
					}
					continue;
				}

				// Any time we find an open curly skip straight to the closing one
				if (isTok(tok, TK_LCURL)) {
					pos = skipBlock(tokens, pos);
					continue;
				}

				// If we actually find a closing one we know it's the object's closing bracket
				if (isTok(tok, TK_RCURL)) {
					break;
				}

				// Because we skip the bodies of functions the only throws we should find are function throws
				if (isTok(tok, TK_THROW) && isTok(tokens[pos + 1], TK_LPAREN)) {
					lintWarning(errors, tok, "Throw specifications on functions are deprecated.");
					continue;
				}
			}
		});

		// Check for throw specifications in functional style code
		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			Token tok = tokens[pos];

			// Don't accidentally identify a using statement as a namespace
			if (isTok(tok, TK_USING)) {
				if (isTok(tokens[pos + 1], TK_NAMESPACE)) {
					++pos;
				}
				continue;
			}

			// Skip namespaces, classes, and blocks
			if (isTok(tok, TK_NAMESPACE)
				|| isTok(tok, TK_CLASS)
				|| isTok(tok, TK_STRUCT)
				|| isTok(tok, TK_UNION)
				|| isTok(tok, TK_LCURL)) {

				// Move to opening object '{'
				for (; !isTok(tokens[pos], TK_LCURL) && !isTok(tokens[pos], TK_EOF); ++pos) {}

				// Return if we didn't find a '{'
				if (!isTok(tokens[pos], TK_LCURL)) {
					return;
				}

				// Skip to closing '}'
				pos = skipBlock(tokens, pos);
			}

			// Because we skip the bodies of functions the only throws we should find are function throws
			if (isTok(tok, TK_THROW) && isTok(tokens[pos + 1], TK_LPAREN)) {
				lintWarning(errors, tok, "Throw specifications on functions are deprecated.");
				continue;
			}
		}
	};

	// ******************************************
	// Deprecated due to too many false positives
	// ******************************************
	/**
	* Check for postfix incrementers
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	
	void checkIncrementers(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		const vector<TokenType> iteratorPlus = {
			TK_IDENTIFIER, TK_INCREMENT
		};
		const vector<TokenType> iteratorMinus = {
			TK_IDENTIFIER, TK_DECREMENT
		};

		for (size_t pos = 0; pos < tokens.size(); ++pos) {

			if (atSequence(tokens, pos, iteratorPlus) || atSequence(tokens, pos, iteratorMinus)) {
				lintAdvice(errors, tokens[pos],
					"Use prefix notation '" + tokens[pos + 1].value_ + tokens[pos].value_ + "'.",
					"Postfix incrementers inject a copy operation, almost doubling the workload.");
			}
		}
	};
	*/

	/**
	* Balance of #if(#ifdef, #ifndef)/#endif.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkIfEndifBalance(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		int openIf = 0;

		// Return after the first found error, because otherwise
		// even one missed #if can be cause of a lot of errors.
		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			Token tok = tokens[pos];

			if (isTok(tok, TK_IFNDEF)
				|| isTok(tok, TK_IFDEF)
				|| isTok(tok, TK_POUNDIF)) {

				++openIf;
			}
			else if (isTok(tok, TK_ENDIF)) {

				--openIf;
				if (openIf < 0) {
					lintError(errors, tok, "Unmatched #endif.");
				}
			}
			else if (isTok(tok, TK_POUNDELSE)) {

				if (openIf == 0) {
					lintError(errors, tok, "Unmatched #else.");
				}
			}
		}

		if (openIf != 0) {
			lintError(errors, tokens.back(), "Unmatched #if/#endif.");
		}
	};

	/**
	* Warn about common errors with constructors, such as:
	*  - single-argument constructors that aren't marked as explicit, to avoid them
	*    being used for implicit type conversion (C++ only)
	*  - Non-const copy constructors, or useless const move constructors.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkConstructors(ErrorFile &errors, const string &path, const vector<Token> &tokens) {
		if (getFileCategory(path) == FileCategory::SOURCE_C) {
			return;
		}

		// Check for constructor specifications inside classes
		iterateClasses(errors, tokens, [&](ErrorFile &errors, const vector<Token> &tokens, size_t pos) -> void {

			const string lintOverride = "/* implicit */";

			const vector<TokenType> stdInitializerSequence = {
				TK_IDENTIFIER, TK_DOUBLE_COLON, TK_IDENTIFIER, TK_LESS
			};
			const vector<TokenType> constructorSequence = {
				TK_IDENTIFIER, TK_LPAREN
			};
			const vector<TokenType> voidConstructorSequence = {
				TK_IDENTIFIER, TK_LPAREN, TK_VOID, TK_RPAREN
			};

			if (!(isTok(tokens[pos], TK_STRUCT) || isTok(tokens[pos], TK_CLASS))) {
				return;
			}

			++pos;
			// Skip C-Style Structs with no name
			if (!isTok(tokens[pos], TK_IDENTIFIER)) {
				return;
			}

			// Get the name of the object
			string objName = tokens[pos].value_;

			// Skip to opening '{'
			for (; pos < tokens.size() && !isTok(tokens[pos], TK_LCURL); ++pos) {
				if (!(pos < tokens.size()) || isTok(tokens[pos], TK_SEMICOLON)) {
					return;
				}
			}
			++pos;

			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				Token tok = tokens[pos];

				// Any time we find an open curly skip straight to the closing one
				if (isTok(tok, TK_LCURL)) {
					pos = skipBlock(tokens, pos);
					continue;
				}

				// If we actually find a closing one we know it's the object's closing bracket
				if (isTok(tok, TK_RCURL)) {
					break;
				}

				if (isTok(tok, TK_EXPLICIT)) {
					pos = skipFunctionDeclaration(tokens, pos);
					break;
				}

				// Are we on a potential constructor?
				if (atSequence(tokens, pos, constructorSequence) && cmpTok(tok, objName)) {

					// Ignore constructors like Foo(void) ...
					if (atSequence(tokens, pos, voidConstructorSequence)) {
						pos = skipFunctionDeclaration(tokens, pos);
						continue;
					}

					// Check for preceding /* implicit */
					if (tok.precedingWhitespace_.find(lintOverride) != string::npos) {
						pos = skipFunctionDeclaration(tokens, pos);
						continue;
					}

					vector<Argument> args;
					Argument func(pos, pos + 1);
					if (!getFunctionNameAndArguments(tokens, pos, func, args)) {
						// Parse fail can be due to limitations in skipTemplateSpec, such as with:
						// fn(std::vector<boost::shared_ptr<ProjectionOperator>> children);)
						return;
					}

					// Allow zero-argument constructors
					if (args.empty()) {
						pos = skipFunctionDeclaration(tokens, pos);
						continue;
					}

					size_t argPos = args[0].first;
					bool foundConversionCtor = false;
					bool isConstArgument = false;
					if (isTok(tokens[argPos], TK_CONST)) {
						isConstArgument = true;
						++argPos;
					}

					// Copy/move constructors may have const (but not type conversion) issues
					// Note: we skip some complicated cases (e.g. template arguments) here
					if (cmpTok(tokens[argPos], objName)) {
						TokenType nextType = (argPos + 1 != args[0].last) ? tokens[argPos + 1].type_ : TK_EOF;
						if (nextType != TK_STAR) {

							if (nextType == TK_AMPERSAND && !isConstArgument) {

								lintError(errors, tok, "Copy constructors should take a const argument: "
									+ formatFunction(tokens, func, args));
							}
							else if (nextType == TK_LOGICAL_AND && isConstArgument) {

								lintError(errors, tok, "Move constructors should not take a const argument: "
									+ formatFunction(tokens, func, args));
							}

							pos = skipFunctionDeclaration(tokens, pos);
							continue;
						}
					}

					// Allow std::initializer_list constructors
					if (atSequence(tokens, argPos, stdInitializerSequence)
						&& cmpTok(tokens[argPos], "std")
						&& cmpTok(tokens[argPos + 2], "initializer_list")) {
						pos = skipFunctionDeclaration(tokens, pos);
						continue;
					}

					if (args.size() == 1) {
						foundConversionCtor = true;
					}
					else if (args.size() >= 2) {
						// 2+ will only be an issue if the second argument is a default argument
						for (argPos = args[1].first; argPos != args[1].last; ++argPos) {
							if (isTok(tokens[argPos], TK_ASSIGN)) {
								foundConversionCtor = true;
								break;
							}
						}
					}

					if (foundConversionCtor) {
						lintError(errors, tok, "Single - argument constructor '"
							+ formatFunction(tokens, func, args)
							+ "' may inadvertently be used as a type conversion constructor.",
							"Prefix the function with the 'explicit' keyword to avoid this, or add an "
							"/* implicit *""/ comment to suppress this warning.");
					}

					pos = skipFunctionDeclaration(tokens, pos);

					continue;
				}
			}
		});
	};

	/**
	* If encounter memset(foo, sizeof(foo), 0), we warn that the order
	* of the arguments is wrong.
	* Known unsupported case: calling memset inside another memset. The inner
	* call will not be checked.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkMemset(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		const vector<TokenType> funcSequence = {
			TK_IDENTIFIER, TK_LPAREN
		};

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			Token tok = tokens[pos];

			if (!atSequence(tokens, pos, funcSequence) || !cmpTok(tok, "memset")) {
				continue;
			}

			vector<Argument> args;
			Argument func(pos, pos);
			if (!getFunctionNameAndArguments(tokens, pos, func, args)) {
				return;
			}

			// If there are more than 3 arguments, then there might be something wrong
			// with skipTemplateSpec but the iterator didn't reach the EOF (because of
			// a '>' somewhere later in the code). So we only deal with the case where
			// the number of arguments is correct.
			if (args.size() == 3) {
				// wrong calls include memset(..., ..., 0) and memset(..., sizeof..., 1)
				bool error =
						((args[2].last - args[2].first) == 1)
						&&
						(
							cmpTok(tokens[args[2].first], "0")
							||
							(cmpTok(tokens[args[2].first], "1") && cmpTok(tokens[args[1].first], "sizeof"))
						);

				if (!error) {
					continue;
				}

				swap(args[1], args[2]);
				lintError(errors, tok, "Did you mean " + formatFunction(tokens, func, args) + " ?");
			}
		}
	};

	/**
	* Ensures .cpp files include their associated header first
	* (this catches #include-time dependency bugs where .h files don't
	* include things they depend on)
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkIncludeAssociatedHeader(ErrorFile &errors, const string &path, const vector<Token> &tokens) {
		if (!isSource(path)) {
			return;
		}

		string file(path);
		size_t fpos = file.find_last_of("/\\");
		if (fpos != string::npos) {
			file = file.substr(fpos + 1);
		}
		string fileBase = getFileNameBase(file);

		uint includesFound = 0;

		for (size_t pos = 0; pos < tokens.size(); ++pos) {

			if (!isTok(tokens[pos], TK_INCLUDE)) {
				continue;
			}

			++pos;

			if (cmpTok(tokens[pos], "PRECOMPILED")) {
				continue;
			}

			++includesFound;

			if (!isTok(tokens[pos], TK_STRING_LITERAL)) {
				continue;
			}

			string includedFile = getIncludedPath(tokens[pos].value_);
			size_t ipos = includedFile.find_last_of("/\\");
			if (ipos != string::npos) {
				continue;
			}

			if (cmpStr(getFileNameBase(includedFile), fileBase)) {
				if (includesFound > 1) {

					lintError(errors, tokens[pos - 1], "The associated header file of .cpp "
						"files should be included before any other includes.",
						"This helps catch missing header file dependencies in the .h");
					break;
				}
			}
		}
	};

	/**
	* If header file contains include guard.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkIncludeGuard(ErrorFile &errors, const string &path, const vector<Token> &tokens) {
		if (getFileCategory(path) != FileCategory::HEADER) {
			return;
		}

		const vector<TokenType> pragmaOnce = {
			TK_PRAGMA, TK_IDENTIFIER
		};

		// Allow #pragma once as an inclue guard
		if (atSequence(tokens, 0, pragmaOnce) && cmpTok(tokens[1], "once")) {
			return;
		}

		const vector<TokenType> includeGuard = {
			TK_IFNDEF, TK_IDENTIFIER, TK_DEFINE, TK_IDENTIFIER
		};

		if (!atSequence(tokens, 0, includeGuard)) {
			lintError(errors, tokens[0], "Missing include guard.");
			return;
		}

		if (!cmpToks(tokens[1], tokens[3])) {
			lintError(errors, tokens[1], "Include guard name mismatch; expected "
				+	tokens[1].value_ + ", saw " + tokens[3].value_);
		}

		int openIf = 1;

		size_t pos;
		for (pos = 1; pos < tokens.size(); ++pos) {

			if (isTok(tokens[pos], TK_IFNDEF) || isTok(tokens[pos], TK_IFDEF) || isTok(tokens[pos], TK_POUNDIF)) {
				++openIf;
				continue;
			}
			if (isTok(tokens[pos], TK_ENDIF)) {

				--openIf;
				if (openIf == 0) {
					break;
				}
				continue;
			}
		}

		if (openIf != 0 || pos < tokens.size() - 2) {
			lintError(errors, tokens.back(), "Include guard doesn't cover the entire file.");
			return;
		}
	};

	/**
	* Warn about implicit casts
	*
	* Implicit casts not marked as explicit can be dangerous if not used carefully
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkImplicitCast(ErrorFile &errors, const string &path, const vector<Token> &tokens) {
		if (getFileCategory(path) == FileCategory::SOURCE_C) {
			return;
		}

		// Check for constructor specifications inside classes
		iterateClasses(errors, tokens, [&](ErrorFile &errors, const vector<Token> &tokens, size_t pos) -> void {

			const string lintOverride = "/* implicit */";

			const vector<TokenType> explicitConstOperator = {
				TK_EXPLICIT, TK_CONSTEXPR, TK_OPERATOR
			};
			const vector<TokenType> explicitOperator = {
				TK_EXPLICIT, TK_OPERATOR
			};
			const vector<TokenType> doubleColonOperator = {
				TK_DOUBLE_COLON, TK_OPERATOR
			};

			const vector<TokenType> boolOperator = {
				TK_OPERATOR, TK_BOOL, TK_LPAREN, TK_RPAREN
			};
			const vector<TokenType> operatorDelete = {
				TK_ASSIGN, TK_DELETE
			};
			const vector<TokenType> operatorConstDelete = {
				TK_CONST, TK_ASSIGN, TK_DELETE
			};

			if (!(isTok(tokens[pos], TK_STRUCT) || isTok(tokens[pos], TK_CLASS))) {
				return;
			}

			// Skip to opening '{'
			for (; pos < tokens.size() && !isTok(tokens[pos], TK_LCURL); ++pos) {
				if (!(pos < tokens.size()) || isTok(tokens[pos], TK_SEMICOLON)) {
					return;
				}
			}
			++pos;

			for (; pos < tokens.size() && !isTok(tokens[pos], TK_EOF); ++pos) {
				Token tok = tokens[pos];

				// Any time we find an open curly skip straight to the closing one
				if (isTok(tok, TK_LCURL)) {
					pos = skipBlock(tokens, pos);
					continue;
				}

				// If we actually find a closing one we know it's the object's closing bracket
				if (isTok(tok, TK_RCURL)) {
					break;
				}

				// Skip explicit functions
				if (atSequence(tokens, pos, explicitConstOperator)) {
					++(++pos);
					continue;
				}
				if (atSequence(tokens, pos, explicitOperator) || atSequence(tokens, pos, doubleColonOperator)) {
					++pos;
					continue;
				}

				// bool Operator case
				if (atSequence(tokens, pos, boolOperator)) {
					if (atSequence(tokens, pos + 4, operatorDelete) || atSequence(tokens, pos + 4, operatorConstDelete)) {
						// Deleted implicit operators are ok.
						continue;
					}

					lintError(errors, tok, "operator bool() is dangerous.",
						"In C++11 use explicit conversion (explicit operator bool()), "
						"otherwise use something like the safe-bool idiom if the syntactic "
						"convenience is justified in this case, or consider defining a "
						"function (see http://www.artima.com/cppsource/safebool.html for more "
						"details).");
					continue;
				}

				// Only want to process operators which do not have the overide
				if (!isTok(tok, TK_OPERATOR)
					|| tok.precedingWhitespace_.find(lintOverride) != string::npos) {
					continue;
				}

				// Assume it is an implicit conversion unless proven otherwise
				bool isImplicitConversion = false;
				string typeString = "";
				for (size_t typePos = pos + 1; typePos < tokens.size(); ++typePos) {
					if (isTok(tokens[typePos], TK_LPAREN)) {
						break;
					}

					if (atBuiltinType(tokens, typePos) || isTok(tokens[typePos], TK_IDENTIFIER)) {
						isImplicitConversion = true;
					}

					if (!typeString.empty()) {
						typeString += ' ';
					}
					typeString += tokens[typePos].value_;
				}

				// The operator my not have been an implicit conversion
				if (!isImplicitConversion) {
					continue;
				}

				lintWarning(errors, tok, "Implicit conversion to '" + typeString + "' may inadvertently be used.",
					"Prefix the function with the 'explicit' keyword to avoid this,"
					" or add an /* implicit *""/ comment to suppress this warning.");
			}
		});
	};

	/**
	* Don't allow heap allocated exception, i.e. throw new Class()
	*
	* A simple check for two consecutive tokens "throw new"
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkThrowsHeapException(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		const vector<TokenType> throwNew = {
			TK_THROW, TK_NEW
		};

		const vector<TokenType> throwConstructor = {
			TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
		};

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (atSequence(tokens, pos, throwNew)) {

				string msg;
				size_t focal = pos + 2;
				if (isTok(tokens[focal], TK_IDENTIFIER)) {
					msg = "Heap-allocated exception: throw new "
						+ tokens[focal].value_ + "();";
				}
				else if (atSequence(tokens, focal, throwConstructor)) {
					// Alternate syntax throw new (Class)()
					++focal;
					msg = "Heap-allocated exception: throw new ("
						+ tokens[focal].value_ + ")();";
				}
				else {
					// Some other usage of throw new Class().
					msg = "Heap-allocated exception: throw new was used.";
				}

				lintError(errors, tokens[focal], msg + " This is usually a mistake in c++.");
			}
		}

	};

	/**
	* Ensures that no files contain deprecated includes.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkDeprecatedIncludes(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		// Set storing the deprecated includes. Add new headers here if you'd like
		// to deprecate them
		static const set<string> deprecatedIncludes = {
			"common/base/Base.h",
			"common/base/StringUtil.h",
		};

		for (size_t pos = 0; pos < tokens.size() - 1; ++pos) {

			if (!isTok(tokens[pos], TK_INCLUDE)) {
				continue;
			}

			++pos;
			if (!isTok(tokens[pos], TK_STRING_LITERAL) || cmpTok(tokens[pos], "PRECOMPILED")) {
				continue;
			}

			string includedFile = getIncludedPath(tokens[pos].value_);
			if (deprecatedIncludes.find(includedFile) != deprecatedIncludes.end()) {
				lintWarning(errors, tokens[pos - 1], "Including deprecated header '"
					+ includedFile + "'");
			}
		}

	};

	/**
	* Makes sure inl headers are included correctly
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkInlHeaderInclusions(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		const vector<TokenType> includeSequence = {
			TK_INCLUDE, TK_STRING_LITERAL
		};

		string file(path);
		size_t fpos = file.find_last_of("/\\");
		if (fpos != string::npos) {
			file = file.substr(fpos + 1);
		}
		string fileBase = getFileNameBase(file);

		for (size_t pos = 0; pos < tokens.size() - 1; ++pos) {

			if (!atSequence(tokens, pos, includeSequence)) {
				continue;
			}
			++pos;

			string includedFile = getIncludedPath(tokens[pos].value_);

			if (getFileCategory(includedFile) != FileCategory::INL_HEADER) {
				continue;
			}

			file = includedFile;
			fpos = includedFile.find_last_of("/\\");
			if (fpos != string::npos) {
				file = includedFile.substr(fpos + 1);
			}
			string includedBase = getFileNameBase(file);

			if (cmpStr(fileBase, includedBase)) {
				continue;
			}

			lintError(errors, tokens[pos], "An -inl file (" + includedFile
				+ ") was included even though this is not its associated header.",
				"Usually files like Foo-inl.h are implementation details and should "
				"not be included outside of Foo.h.");
		}

	};

	/**
	* Classes should not have protected inheritance.
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	*/
	void checkProtectedInheritance(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		iterateClasses(errors, tokens, [&](ErrorFile &errors, const vector<Token> &tokens, size_t pos) -> void {

			const vector<TokenType> protectedSequence = {
				TK_COLON, TK_PROTECTED, TK_IDENTIFIER
			};

			for (; pos < tokens.size() - 2; ++pos) {

				if (isTok(tokens[pos], TK_LCURL) || isTok(tokens[pos], TK_SEMICOLON)) {
					break;
				}

				if (atSequence(tokens, pos, protectedSequence)) {
					lintWarning(errors, tokens[pos], "Protected inheritance is sometimes not a good idea.",
						"Read http://stackoverflow.com/questions/6484306/effective-c-discouraging-protected-inheritance "
						"for more information.");
				}
			}
		});
	};

	// ************************************
	// Merged with banned identifiers check
	// ************************************
	/**
	* Advise nullptr over NULL in C++ files
	*
	* @param errors
	*		Struct to track how many errors/warnings/advice occured
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	
	void checkUpcaseNull(ErrorFile &errors, const string &path, const vector<Token> &tokens) {

		for (size_t pos = 0; pos < tokens.size(); ++pos) {

			if (isTok(tokens[pos], TK_IDENTIFIER) && cmpTok(tokens[pos], "NULL")) {
				lintAdvice(errors, tokens[pos], "Prefer `nullptr' to `NULL' in new C++ code.",
					"Unlike `NULL', `nullptr' can't accidentally be used in arithmetic or as an integer. See "
					"http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2431.pdf"
					" for details.");
			}
		}
	};
	*/

// Shorthand for comparing two strings
#undef cmpStr
#undef cmpTok
#undef cmpToks

// Shorthand for comparing a Token and TokenType
#undef isTok

};
