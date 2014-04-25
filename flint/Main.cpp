// TODO: Add license info

#include <string>
#include <iostream>
#include <vector>
#include <dirent.h>

#include "Polyfill.hpp"
#include "FileCategories.hpp"
#include "Ignored.hpp"
#include "Tokenizer.hpp"
#include "Checks.hpp"

using namespace std;
using namespace flint;

// TODO: Find GFlags alternative
bool FLAGS_recursive = true;
bool FLAGS_cmode     = false;
bool FLAGS_verbose   = false;

enum class Lint {
	ERROR, WARNING, ADVICE
};
Lint FLAGS_level     = Lint::ADVICE;

/**
* Run lint on the given path
*
* @param path
*		The path to lint
* @return
*		Returns the number of errors found
*/
void checkEntry(Errors &errors, const string &path, uint &fileCount) {
	
	FSType fsType = fsObjectExists(path);
	if (fsType == FSType::NO_ACCESS) {
		return;
	}

	if (fsType == FSType::IS_DIR) {
		if (!FLAGS_recursive || fsContainsNoLint(path)) {
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
					checkEntry(errors, fileName.c_str(), fileCount);
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

	if (FLAGS_verbose) {
		cout << endl << "Linting File: " << path << endl;
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

		if (!FLAGS_cmode) {
			checkConstructors(errors, path, tokens);
			checkCatchByReference(errors, path, tokens);
			checkThrowSpecification(errors, path, tokens);
		}

		// Checks which note Warnings
		if (FLAGS_level >= Lint::WARNING) {

			checkBlacklistedSequences(errors, path, tokens);
			checkDefinedNames(errors, path, tokens);

			if (!FLAGS_cmode) {

			}
		}

		// Checks which note Advice
		if (FLAGS_level >= Lint::ADVICE) {

			checkIterators(errors, path, tokens);

			if (!FLAGS_cmode) {

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

	// Check each file
	Errors errors;
	uint fileCount = 0;
	for (int i = 1; i < argc; ++i) {
		checkEntry(errors, string(argv[i]), fileCount);
	}

	// Print summary
	cout << endl << fileCount << " files linted [E: " << errors.errors 
		<< " W: " << errors.warnings 
		<< " A: " << errors.advice << "]" << endl;

	// Stop visual studio from closing the window...
	system("PAUSE");
	return 0;
};

