#pragma once

/**
 * This file contains many of the definitions and utility functions 
 * that were hidden away in boost or facebooks folly library
 */

#include <string>
#include <vector>

using namespace std;

#define uint unsigned int

#ifdef _WIN32  
#define FS_SEP "\\"
#else
#define FS_SEP "/"
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

	string escapeString(const string &input);
};