#include "Checks.hpp"

#include <map>
#include <unordered_map>
#include <set>
#include <stack>
#include <cassert>

namespace flint {
	
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

		void lintError(const Token &tok, const string &error) {
			fprintf(stderr, "%s(%u): %s", tok.file_.c_str(), tok.line_, error.c_str());
		};

		void lintWarning(const Token &tok, const string &warning) {
			lintError(tok, "Warning: " + warning);
		};

		void lintAdvice(const Token &tok, const string &advice) {
			lintError(tok, "Advice: " + advice);
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

			for (size_t i = 0; i < list.size(); i++, ++pos) {
				if (tokens[pos].type_ != list[i]) {
					return false;
				}
			}
			return true;
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
			assert(tokens[pos].type_ == TK_LESS);

			uint angleNest = 1; // Because we began on the leading '<'
			uint parenNest = 0;

			if (containsArray) {
				*containsArray = false;
			}

			for (; tokens[pos].type_ != TK_EOF; ++pos) {
				TokenType tok = tokens[pos].type_;

				if (tok == TK_LPAREN) {
					parenNest++;
					continue;
				}
				if (tok == TK_RPAREN) {
					parenNest--;
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
					angleNest++;
					continue;
				}
				if (tok == TK_GREATER) {
					// Removed decrement/zero-check as one line
					// It's not a race guys, readability > length of code
					angleNest--;
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

			const vector<TokenType> builtIns = {
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

			TokenType tok = tokens[pos].type_;
			for (size_t i = 0; i < builtIns.size(); ++i) {
				if (tok == builtIns[i]) {
					return true;
				}
			}
			return false;
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
			for (; tokens[pos].type_ == TK_IDENTIFIER || tokens[pos].type_ == TK_DOUBLE_COLON; ++pos) {
				if (tokens[pos].type_ == TK_IDENTIFIER) {
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
			assert(tokens[pos].type_ == TK_LCURL);

			uint openBraces = 1; // Because we began on the leading '{'

			for (; tokens[pos].type_ != TK_EOF; ++pos) {
				TokenType tok = tokens[pos].type_;

				if (tok == TK_LCURL) {
					openBraces++;
					continue;
				}
				if (tok == TK_RCURL) {
					// Removed decrement/zero-check as one line
					// It's not a race guys, readability > length of code
					openBraces--;
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
		* @param tokens
		*		The token list for the file
		* @param pos
		*		The current index position inside the token list
		* @param callback
		*		The function to run on each code object
		* @return
		*		Returns the sum of running callback on each object
		*/
		template<class Callback>
		uint iterateClasses(const vector<Token> &tokens, const Callback &callback) {

			uint result = 0;

			for (size_t pos = 0; tokens[pos].type_ != TK_EOF; ++pos) {
				// Skip template sequence if we find ... template< ...
				if (atSequence(tokens, pos, { TK_TEMPLATE, TK_LESS })) {
					pos = skipTemplateSpec(tokens, pos);
					continue;
				}

				TokenType tok = tokens[pos].type_;
				if (tok == TK_CLASS || tok == TK_STRUCT || tok == TK_UNION) {
					result += callback(tokens, pos);
				}
			}

			return result;
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

			for (; tokens[pos].type_ != TK_EOF; ++pos) {
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
				assert(first < last); 
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

			for (size_t pos = arg.first; pos <= arg.last; ++pos) {
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
			assert(tokens[pos].type_ == TK_LPAREN);

			size_t argStart = pos + 1; // First arg starts after parenthesis
			int parenCount = 1;

			for (; tokens[pos].type_ != TK_EOF; ++pos) {
				TokenType tok = tokens[pos].type_;

				if (tok == TK_LPAREN) {
					parenCount++;
					continue;
				}
				if (tok == TK_RPAREN) {
					// Removed decrement/zero-check as one line
					// It's not a race guys, readability > length of code
					parenCount--;
					if (parenCount == 0) {
						break;
					}
					continue;
				}
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
				if (tok == TK_COMMA) {
					if (parenCount == 1) {
						// end an argument of the function we are looking at
						args.push_back(Argument(argStart, pos));
						argStart = pos + 1;
					}
					continue;
				}
			}

			if (tokens[pos].type_ == TK_EOF) {
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

			if (tokens[pos].type_ == TK_LESS) {
				pos = skipTemplateSpec(tokens, pos);

				if (tokens[pos].type_ == TK_EOF) {
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
	uint checkInitializeFromItself(const string &path, const vector<Token> &tokens) {
		
		// Token Sequences for parameter initializers
		const vector<TokenType> firstInitializer = {
			TK_COLON, TK_IDENTIFIER, TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
		};
		const vector<TokenType> nthInitializer = {
			TK_COMMA, TK_IDENTIFIER, TK_LPAREN, TK_IDENTIFIER, TK_RPAREN
		};

		uint result = 0;

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (atSequence(tokens, pos, firstInitializer) || atSequence(tokens, pos, nthInitializer)) {

				int outerPos = ++pos;     // +1 for identifier
				int innerPos = ++(++pos); // +2 again for the inner identifier

				bool isMember = tokens[outerPos].value_.back() == '_' ||
								startsWith(tokens[outerPos].value_, "m_");

				if (isMember && (tokens[outerPos].value_.compare(tokens[innerPos].value_) == 0)) {
					lintError(tokens[outerPos], "Looks like you're initializing class member [" 
						+ tokens[outerPos].value_ + "] with itself.\n");
					++result;
				}
			}
		}

		return result;
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
	uint checkBlacklistedSequences(const string &path, const vector<Token> &tokens) {

		struct BlacklistEntry {
			vector<TokenType> tokens;
			string descr;
			bool cpponly;
			BlacklistEntry(const vector<TokenType> &t, const string &d, bool cpponly)
				: tokens(t), descr(d), cpponly(cpponly) {};
		};

		static const vector<BlacklistEntry> blacklist = {
			{ { TK_VOLATILE },
			"'volatile' does not make your code thread-safe. If multiple threads are "
			"sharing data, use std::atomic or locks. In addition, 'volatile' may "
			"force the compiler to generate worse code than it could otherwise. "
			"For more about why 'volatile' doesn't do what you think it does, see "
			"http://fburl.com/volatile or http://www.kernel.org/doc/Documentation/"
			"volatile-considered-harmful.txt.\n",
			true, // C++ only.
			}
		};

		static const vector< vector<TokenType> > exceptions = {
			{ TK_ASM, TK_VOLATILE }
		};

		uint result = 0;
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
				/*if (FLAGS_cmode && entry.cpponly) {
					continue;
				}*/

				lintWarning(tokens[pos], entry.descr);
				++result;
			}
		}

		return result;
	};

	/**
	* Check for blacklisted identifiers
	*
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	* @return
	*		Returns the number of errors this check found in the token stream
	*/
	uint checkBlacklistedIdentifiers(const string &path, const vector<Token> &tokens) {


		static const map<string, string> blacklist = {
			{ "strtok",
			"strtok() is not thread safe, and has safer alternatives. Consider strtok_r.\n" 
			}
		};

		uint result = 0;

		for (size_t pos = 0; pos < tokens.size(); ++pos) {

			if (tokens[pos].type_ == TK_IDENTIFIER) {
				for (const auto &entry : blacklist) {
					if (tokens[pos].value_.compare(entry.first) == 0) {
						lintError(tokens[pos], entry.second);
						++result;
						continue;
					}
				}
			}
		}

		return result;
	};

	/**
	* No #defined names use an identifier reserved to the
	* implementation.
	*
	* These are enforcing rules that actually apply to all identifiers,
	* but we're only raising warnings for #define'd ones right now.
	*
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	* @return
	*		Returns the number of errors this check found in the token stream
	*/
	uint checkDefinedNames(const string &path, const vector<Token> &tokens) {

		// Exceptions to the check
		static const set<string> okNames = {
			"__STDC_LIMIT_MACROS",
			"__STDC_FORMAT_MACROS",
			"_GNU_SOURCE",
			"_XOPEN_SOURCE"
		};

		uint result = 0;

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (tokens[pos].type_ != TK_DEFINE) {
				continue;
			}
			
			Token tok = tokens[pos + 1];
			string sym = tok.value_;

			if (tok.type_ != TK_IDENTIFIER) {
				// This actually happens because people #define private public
				//   for unittest reasons
				lintWarning(tok, "You're not supposed to #define " + sym + "\n");
				++result;
				continue;
			}

			if (sym.size() >= 2 && sym[0] == '_' && isupper(sym[1])) {
				if (okNames.find(sym) != okNames.end()) {
					continue;
				}
				lintWarning(tok, "Symbol " + sym 
					+ " invalid. A symbol may not start with an underscore followed by a capital letter.\n");
				++result;
			}
			else if (sym.size() >= 2 && sym[0] == '_' && sym[1] == '_') {
				if (okNames.find(sym) != okNames.end()) {
					continue;
				}
				lintWarning(tok, "Symbol " + sym 
					+ " invalid. A symbol may not begin with two adjacent underscores.\n");
				++result;
			}
			else if (sym.find("__") != string::npos) { // !FLAGS_c_mode /* C is less restrictive about this */ && 
				if (okNames.find(sym) != okNames.end()) {
					continue;
				}
				lintWarning(tok, "Symbol " + sym 
					+ " invalid. A symbol may not contain two adjacent underscores.\n");
				++result;
			}
		}

		return result;
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
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	* @return
	*		Returns the number of errors this check found in the token stream
	*/
	uint checkCatchByReference(const string &path, const vector<Token> &tokens) {

		uint result = 0;

		for (size_t pos = 0; pos < tokens.size(); ++pos) {
			if (tokens[pos].type_ != TK_CATCH) {
				continue;
			}

			size_t focal = pos + 1;
			if (tokens[focal].type_ != TK_LPAREN) { // a "(" comes always after catch
				throw runtime_error(tokens[focal].file_ + ":" + to_string(tokens[focal].line_) 
					+ ": Invalid C++ source code, please compile before lint.");
			}
			++focal;

			if (tokens[focal].type_ == TK_ELLIPSIS) {
				// catch (...
				continue;
			}
			if (tokens[focal].type_ == TK_CONST) {
				// catch (const
				++focal;
			}
			if (tokens[focal].type_ == TK_TYPENAME) {
				// catch ([const] typename
				++focal;
			}
			if (tokens[focal].type_ == TK_DOUBLE_COLON) {
				// catch ([const] [typename] ::
				++focal;
			}

			// At this position we must have an identifier - the type caught,
			// e.g. FBException, or the first identifier in an elaborate type
			// specifier, such as facebook::FancyException<int, string>.
			if (tokens[focal].type_ != TK_IDENTIFIER) {
				
				const Token &tok = tokens[focal];
				lintWarning(tok, "Symbol " + tok.value_ 
					+ " invalid in catch clause.  You may only catch user-defined types.\n");
				++result;
				continue;
			}
			++focal;

			// We move the focus to the closing paren to detect the "&". We're
			// balancing parens because there are weird corner cases like
			// catch (Ex<(1 + 1)> & e).
			for (size_t parens = 0;; ++focal) {
				if (focal >= tokens.size()) {
					throw runtime_error(tokens[focal].file_ + ":" + to_string(tokens[focal].line_) 
						+ ": Invalid C++ source code, please compile before lint.");
				}
				if (tokens[focal].type_ == TK_RPAREN) {
					if (parens == 0) break;
					--parens;
				}
				else if (tokens[focal].type_ == TK_LPAREN) {
					++parens;
				}
			}

			// At this point we're straight on the closing ")". Backing off
			// from there we should find either "& identifier" or "&" meaning
			// anonymous identifier.
			if (tokens[focal - 1].type_ == TK_AMPERSAND) {
				// check! catch (whatever &)
				continue;
			}
			if (tokens[focal - 1].type_ == TK_IDENTIFIER &&
				tokens[focal - 2].type_ == TK_AMPERSAND) {
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
			lintError(tok, "Symbol " + tok.value_ + " of type " + theType 
				+ " caught by value.  Use catch by (preferably const) reference throughout.\n");
			++result;
		}

		return result;
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
	* @param path
	*		The path to the file currently being linted
	* @param tokens
	*		The token list for the file
	* @return
	*		Returns the number of errors this check found in the token stream
	*/
	uint checkThrowSpecification(const string &path, const vector<Token> &tokens) {

		uint result = 0;

		// Check for throw specifications inside classes
		result += iterateClasses(tokens, [&](const vector<Token> &tokens, size_t pos) -> uint {
			return 0;
		});

		return result;
	};

};