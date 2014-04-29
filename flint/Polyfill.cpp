#include "Polyfill.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace flint {
	
	/**
	* Checks if a given path is a file or directory
	*
	* @param path
	*		The path to test
	* @return
	*		Returns a flag representing what the path was
	*/
	FSType fsObjectExists(const string &path) {

		struct stat info;
		if (stat(path.c_str(), &info) != 0) {
			// Cannot Access
			return FSType::NO_ACCESS;
		}
		else if (info.st_mode & S_IFDIR) {
			// Is a Directory
			return FSType::IS_DIR;
		}
		else if (info.st_mode & S_IFREG) {
			// Is a File
			return FSType::IS_FILE;
		}
		return FSType::NO_ACCESS;
	};

	/**
	* Checks if a given path contains a .nolint file
	*
	* @param path
	*		The path to test
	* @return
	*		Returns a bool of whether a .nolint file was found or not
	*/
	bool fsContainsNoLint(const string &path) {

		string fileName = path + FS_SEP + ".nolint";
		return (fsObjectExists(fileName) == FSType::IS_FILE);
	};

	/**
	* Parses a directory and returns a list of it's contents
	*
	* @param path
	*		The path to search
	* @param dirs
	*		A vector to fill with objects
	* @return
	*		Returns true if any valid objects were found
	*/
	bool fsGetDirContents(const string &path, vector<string> &dirs) {
		dirs.clear();

#ifdef _WIN32
		//
		// windows.h Implementation of directory traversal for Windows systems
		//
		HANDLE dir;
		WIN32_FIND_DATA fileData;

		if ((dir = FindFirstFile((path + FS_SEP + "*").c_str(), &fileData)) == INVALID_HANDLE_VALUE) {
			return false; /* No files found */
		}

		do {
			const string fsObj = fileData.cFileName;
			if (FS_ISNOT_LINK(fsObj) && FS_ISNOT_GIT(fsObj)) {

				const string fileName = path + FS_SEP + fsObj;
				dirs.push_back(fileName);
			}
		} while (FindNextFile(dir, &fileData));

		FindClose(dir);
#else
		//
		// dirent.h Implementation of directory traversal for POSIX systems
		//
		DIR *pDIR;
		struct dirent *entry;
		if (pDIR = opendir(path.c_str())) {
			while (entry = readdir(pDIR)) {
				const string fsObj = entry->d_name;
				if (FS_ISNOT_LINK(fsObj) && FS_ISNOT_GIT(fsObj)) {

					const string fileName = path + FS_SEP + fsObj;
					dirs.push_back(fileName);
				}
			}
			closedir(pDIR);
		}
#endif
		return !dirs.empty();
	};

	/**
	* Attempts to load a file into a std::string
	*
	* @param path
	*		The file to load
	* @param file
	*		The string to load into
	* @return
	*		Returns a bool of whether the load was successful
	*/
	bool getFileContents(const string &path, string &file) {

		ifstream in(path);
		if (in) {
			stringstream buffer;
			buffer << in.rdbuf();
			file = buffer.str();

			return true;
		}
		return false;
	};

	/**
	* Escapes a C++ std::string
	*
	* @param input
	*		The string to sanitize
	* @return
	*		Returns a string with no escape characters
	*/
	string escapeString(const string &input) {
		
		string output = "";
		for (int i = 0; i < input.size(); ++i) {
			char c = input[i];

			switch (c) {
			case '\n':
				output += "\\n";
				break;
			case '\t':
				output += "\\t";
				break;
			case '\r':
				output += "\\r";
				break;
			case '\\':
				output += "\\\\";
				break;
			case '"':
				output += "\\\"";
				break;

			default:
				output += c;
			}
		}
		return output;
	};
};