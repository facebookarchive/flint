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
* @param loc
*		Reference to a var to count the estimated number
*		of lines linted
* @param depth
*		Tracks the recursion depth
* @return
*		Returns the number of errors found
*/
void checkEntry(ErrorReport &errors, const string &path, size_t &loc, uint depth = 0) {

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

		for (size_t i = 0; i < dirs.size(); ++i) {
			checkEntry(errors, dirs[i], loc, depth + 1);
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
		ErrorFile errorFile((Options.VERBOSE ? path : getFileName(path)));

		vector<Token> tokens;
		vector<size_t> structures;
		loc += tokenize(file, path, tokens, structures, errorFile);

		// Checks which note Errors
		checkBlacklistedIdentifiers(errorFile, path, tokens);
		checkInitializeFromItself(errorFile, path, tokens);
		checkIfEndifBalance(errorFile, path, tokens);
		checkMemset(errorFile, path, tokens);
		checkIncludeAssociatedHeader(errorFile, path, tokens);
		checkIncludeGuard(errorFile, path, tokens);
		checkInlHeaderInclusions(errorFile, path, tokens);

		if (!Options.CMODE) {
			checkMutexHolderHasName(errorFile, path, tokens);
			checkConstructors(errorFile, path, tokens, structures);
			checkCatchByReference(errorFile, path, tokens);
			checkThrowsHeapException(errorFile, path, tokens);
			checkUniquePtrUsage(errorFile, path, tokens);
		}

		// Checks which note Warnings
		if (Options.LEVEL >= Lint::WARNING) {

			checkBlacklistedSequences(errorFile, path, tokens);
			checkDefinedNames(errorFile, path, tokens);
			checkDeprecatedIncludes(errorFile, path, tokens);
			checkNamespaceScopedStatics(errorFile, path, tokens);
			checkUsingNamespaceDirectives(errorFile, path, tokens);

			if (!Options.CMODE) {
				checkSmartPtrUsage(errorFile, path, tokens);
				checkImplicitCast(errorFile, path, tokens, structures);
				checkProtectedInheritance(errorFile, path, tokens, structures);
				checkExceptionInheritance(errorFile, path, tokens, structures);
				checkVirtualDestructors(errorFile, path, tokens, structures);

				checkThrowSpecification(errorFile, path, tokens, structures);
			}
		}

		// Checks which note Advice
		if (Options.LEVEL >= Lint::ADVICE) {

			// Deprecated due to too many false positives
			//checkIncrementers(errorFile, path, tokens);

			if (!Options.CMODE) {
				// Merged into banned identifiers
				//checkUpcaseNull(errorFile, path, tokens);
			}
		}

		errors.addFile(move(errorFile));

	}
	catch (exception const &e) {
		fprintf(stderr, "Exception thrown during checks on %s.\n%s\n\n", path.c_str(), e.what());
	}
};

/**
 * Program entry point
 */
int main(int argc, char *argv[]) {
	// Parse commandline flags
	vector<string> paths;
	parseArgs(argc, argv, paths);

	size_t totalLOC = 0;
	// Check each file
	ErrorReport errors;
	for (size_t i = 0; i < paths.size(); ++i) {
		checkEntry(errors, paths[i], totalLOC);
	}

	// Print summary
	errors.print();
	if (!Options.JSON) {
		cout << endl << "Estimated Lines of Code: " << to_string(totalLOC) << endl;
	}

#ifdef _DEBUG
	// Stop visual studio from closing the window...
	system("PAUSE");
#endif

	if (errors.getWarnings() || errors.getErrors()) {
		return 1;
	}

	return 0;
};

