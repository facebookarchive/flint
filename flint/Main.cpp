// TODO: Add license info

#include <string>
#include <iostream>
#include <vector>
#include <dirent.h>

#include "Options.hpp"
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
* @param path
*		The path to lint
* @return
*		Returns the number of errors found
*/
void checkEntry(Errors &errors, const string &path, uint &fileCount, uint depth = 0) {
	
	FSType fsType = fsObjectExists(path);
	if (fsType == FSType::NO_ACCESS) {
		return;
	}

	if (fsType == FSType::IS_DIR) {
		if ((!Options.RECURSIVE && depth > 0) || fsContainsNoLint(path)) {
			return;
		}

		// For each object in the directory
		DIR *pDIR;
		struct dirent *entry;
		if (pDIR = opendir(path.c_str())) {
			while (entry = readdir(pDIR)) {
				string fsObj = entry->d_name;
				if (FS_ISNOT_LINK(fsObj) && FS_ISNOT_GIT(fsObj)) {

					string fileName = path + FS_SEP + fsObj;
					checkEntry(errors, fileName.c_str(), fileCount, depth + 1);
				}
			}
			closedir(pDIR);
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

	vector<Token> tokens;
	
	try {
		++fileCount;
		tokenize(file, path, tokens);
		
		// Checks which note Errors
		checkBlacklistedIdentifiers(errors, path, tokens);
		checkInitializeFromItself(errors, path, tokens);
		checkIfEndifBalance(errors, path, tokens);
		checkMemset(errors, path, tokens);
		checkIncludeAssociatedHeader(errors, path, tokens);
		checkIncludeGuard(errors, path, tokens);
		checkInlHeaderInclusions(errors, path, tokens);
		
		if (!Options.CMODE) {
			checkConstructors(errors, path, tokens);
			checkCatchByReference(errors, path, tokens);
			checkThrowsHeapException(errors, path, tokens);
		}

		// Checks which note Warnings
		if (Options.LEVEL >= Lint::WARNING) {

			checkBlacklistedSequences(errors, path, tokens);
			checkDefinedNames(errors, path, tokens);
			checkDeprecatedIncludes(errors, path, tokens);

			if (!Options.CMODE) {
				checkImplicitCast(errors, path, tokens);
				checkProtectedInheritance(errors, path, tokens);
				checkThrowSpecification(errors, path, tokens);
			}
		}

		// Checks which note Advice
		if (Options.LEVEL >= Lint::ADVICE) {

			checkIterators(errors, path, tokens);

			if (!Options.CMODE) {
				checkUpcaseNull(errors, path, tokens);
			}
		}

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
	Errors errors;
	uint fileCount = 0;
	for (int i = 0; i < paths.size(); ++i) {
		checkEntry(errors, paths[i], fileCount);
	}

	// Print summary
	cout << endl << fileCount << " files linted [E: " << errors.errors 
		<< " W: " << errors.warnings 
		<< " A: " << errors.advice << "]" << endl;

#ifdef _DEBUG 
	// Stop visual studio from closing the window...
	system("PAUSE");
#endif
	return 0;
};

