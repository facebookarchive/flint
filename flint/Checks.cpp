#include "Checks.hpp"

#include <map>
#include <unordered_map>
#include <set>
#include <stack>
#include <cassert>

namespace flint {
	
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
		fprintf(stderr, "%.*s(%u): %s", tok.file_->c_str(), tok.line_, error.c_str());
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
	bool atSequence(vector<Token> &tokens, int pos, const vector<TokenType> &list) {

		for (int i = 0; i < list.size(); i++, pos++) {
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
	string getIncludedPath(string path) {
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
	int skipTemplateSpec(vector<Token> &tokens, int pos, bool *containsArray = nullptr) {
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
			if(tok == TK_RPAREN) {
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
	bool atBuiltinType(vector<Token> &tokens, int pos) {
		
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
		for (int i = 0; i < builtIns.size(); i++) {
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
	vector<string> readQualifiedIdentifier(vector<Token> &tokens, int &pos) {
		
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
	int skipBlock(vector<Token> &tokens, int pos) {
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
	uint iterateClasses(vector<Token> &tokens, const Callback &callback) {
		
		uint result = 0;

		for (int pos = 0; tokens[pos].type_ != TK_EOF; pos++) {
			// Skip template sequence if we find ... template< ...
			if (atSequence(tokens, pos, {TK_TEMPLATE, TK_LESS})) {
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
	int skipFunctionDeclaration(vector<Token> &tokens, int pos) {
		
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
};