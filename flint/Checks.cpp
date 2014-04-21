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
	* Traverses the token list until the whole template sequence has been passed...
	* Returns with pos placed on the closing angle bracket
	*
	* @param tokens
	*		The token list for the file
	* @param pos
	*		The current index position inside the token list
	* @param containsArray
	*		Optional parameter to return a bool of whether an array was found inside
	*		the template list
	*/
	void skipTemplateSpec(vector<Token> &tokens, int &pos, bool *containsArray = nullptr) {
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
	bool atBuiltinType(vector<Token> &tokens, int &pos) {
		
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
};