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
bool FLAGS_cmode = false;
bool FLAGS_verbose = true;

/**
* Run lint on the given path
*
* @param path
*		The path to lint
* @return
*		Returns the number of errors found
*/
uint checkEntry(const string &path) {
	
	FSType fsType = fsObjectExists(path);
	if (fsType == FSType::NO_ACCESS) {
		return 0;
	}

	uint errors = 0;

	if (fsType == FSType::IS_DIR) {
		if (!FLAGS_recursive || fsContainsNoLint(path)) {
			return 0;
		}

		// For each object in the directory
		DIR *pDIR;
		struct dirent *entry;
		if (pDIR = opendir(path.c_str())) {
			while (entry = readdir(pDIR)) {
				string fsObj = entry->d_name;
				if (FS_ISNOT_LINK(fsObj) && FS_ISNOT_GIT(fsObj)) {

					string fileName = path + FS_SEP + fsObj;
					errors += checkEntry(fileName.c_str());
				}
			}
			closedir(pDIR);
		}

		return errors;
	}

	FileCategory srcType = getFileCategory(path);
	if (srcType == FileCategory::UNKNOWN) {
		return 0;
	}

	if (FLAGS_verbose) {
		cout << endl << "Linting File: " << path << endl;
	}
	
	string file;
	if (!getFileContents(path, file)) {
		return 0;
	}

	// Remove code that occurs in pairs of
	// "// %flint: pause" & "// %flint: resume"
	file = removeIgnoredCode(file, path);

	vector<Token> tokens;
	
	try {
		tokenize(file, path, tokens);
		
		errors += checkBlacklistedSequences(path, tokens);
		errors += checkBlacklistedIdentifiers(path, tokens);
		errors += checkDefinedNames(path, tokens);

		errors += checkInitializeFromItself(path, tokens);

		if (!FLAGS_cmode) {
			
		}
	} catch (exception const &e) {
		fprintf(stderr, "Exception thrown during checks on %s.\n%s", path.c_str(), e.what());
	}

	return errors;
};

/**
 * Program entry point
 */
int main(int argc, char *argv[]) {
	// Parse commandline flags

	// Check each file
	uint errors = 0;
	for (int i = 1; i < argc; ++i) {
		errors += checkEntry(string(argv[i]));
	}

	// Print summary
	cout << endl << "Linting Finished with " << errors << " Error" 
		 << (errors == 1 ? "" : "s") << "." << endl;

	// Stop visual studio from closing the window...
	system("PAUSE");
	return 0;
};

