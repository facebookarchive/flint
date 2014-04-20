#include "Checks.hpp"

#include <map>
#include <unordered_map>
#include <set>
#include <stack>

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
	}

	void lintWarning(const Token &tok, const string &warning) {
		lintError(tok, "Warning: " + warning);
	}

	void lintAdvice(const Token &tok, const string &advice) {
		lintError(tok, "Advice: " + advice);
	}
};