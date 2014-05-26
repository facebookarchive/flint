#pragma once

/**
 * This file contains many of the definitions and utility functions 
 * that were hidden away in boost or facebooks folly library
 */

#include <string>
#include <vector>

using namespace std;

typedef unsigned int uint;

#ifdef _MSC_VER  
#define FS_SEP "\\"
#define NOEXCEPT
#else
#define FS_SEP "/"
#define NOEXCEPT noexcept
#endif


// Quick checks for path names
#define FS_ISNOT_LINK(file) ((file.compare(".") != 0) && (file.compare("..") != 0))
#define FS_ISNOT_GIT(file) (file.compare(".git") != 0)

namespace flint {
	
	// File System object types
	enum class FSType {
		NO_ACCESS,
		IS_FILE,
		IS_DIR
	};

	FSType fsObjectExists(const string &path);

	bool fsContainsNoLint(const string &path);

	bool fsGetDirContents(const string &path, vector<string> &dir);

	bool getFileContents(const string &path, string &file);

	bool startsWith(const string &str, const string &prefix);

	bool startsWith(string::const_iterator str_iter, const char *prefix);

	string escapeString(const string &input);
};
