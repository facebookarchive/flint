#include <string>
#include <iostream>
#include <vector>

#include "Options.hpp"
#include "ErrorReport.hpp"
#include "Polyfill.hpp"
#include "FileCategories.hpp"
#include "Ignored.hpp"
#include "Tokenizer.hpp"
#include "Checks.hpp"

using namespace std;
using namespace flint;

/**
* Run lint on the given path
*
* @param errors
*		An object to hold the error details
* @param path
*		The path to lint
* @param depth
*		Tracks the recursion depth
* @return
*		Returns the number of errors found
*/
void checkEntry(ErrorReport &errors, const string &path, uint depth = 0) {
	
	FSType fsType = fsObjectExists(path);
	if (fsType == FSType::NO_ACCESS) {
		return;
	}

	if (fsType == FSType::IS_DIR) {
		if ((!Options.RECURSIVE && depth > 0) || fsContainsNoLint(path)) {
			return;
		}

		// For each object in the directory
		vector<string> dirs;
		if (!fsGetDirContents(path, dirs)) {
			return;
		}
		
		for (uint i = 0; i < dirs.size(); ++i) {
			checkEntry(errors, dirs[i], depth + 1);
		}
		return;
	}

	FileCategory srcType = getFileCategory(path);
	if (srcType == FileCategory::UNKNOWN) {
		return;
	}
	
	string file;
	if (!getFileContents(path, file)) {
		return;
	}

	// Remove code that occurs in pairs of
	// "// %flint: pause" & "// %flint: resume"
	file = removeIgnoredCode(file, path);
	
	try {
		ErrorFile errorFile(path);

		vector<Token> tokens;
		tokenize(file, path, tokens);
		
		// Checks which note Errors
		checkBlacklistedIdentifiers(errorFile, path, tokens);
		checkInitializeFromItself(errorFile, path, tokens);
		checkIfEndifBalance(errorFile, path, tokens);
		checkMemset(errorFile, path, tokens);
		checkIncludeAssociatedHeader(errorFile, path, tokens);
		checkIncludeGuard(errorFile, path, tokens);
		checkInlHeaderInclusions(errorFile, path, tokens);
		
		if (!Options.CMODE) {
			checkConstructors(errorFile, path, tokens);
			checkCatchByReference(errorFile, path, tokens);
			checkThrowsHeapException(errorFile, path, tokens);
		}

		// Checks which note Warnings
		if (Options.LEVEL >= Lint::WARNING) {

			checkBlacklistedSequences(errorFile, path, tokens);
			checkDefinedNames(errorFile, path, tokens);
			checkDeprecatedIncludes(errorFile, path, tokens);

			if (!Options.CMODE) {
				checkImplicitCast(errorFile, path, tokens);
				checkProtectedInheritance(errorFile, path, tokens);
				checkThrowSpecification(errorFile, path, tokens);
			}
		}

		// Checks which note Advice
		if (Options.LEVEL >= Lint::ADVICE) {

			checkIterators(errorFile, path, tokens);

			if (!Options.CMODE) {
				checkUpcaseNull(errorFile, path, tokens);
			}
		}

		errors.addFile(errorFile);

	} catch (exception const &e) {
		fprintf(stderr, "Exception thrown during checks on %s.\n%s", path.c_str(), e.what());
	}
};

/**
 * Program entry point
 */
int main(int argc, char *argv[]) {
	// Parse commandline flags
	vector<string> paths;
	parseArgs(argc, argv, paths);

	// Check each file
	ErrorReport errors;
	for (uint i = 0; i < paths.size(); ++i) {
		checkEntry(errors, paths[i]);
	}

	// Print summary
	cout << errors.toString() << endl;

#ifdef _DEBUG 
	// Stop visual studio from closing the window...
	system("PAUSE");
#endif
	return 0;
};

