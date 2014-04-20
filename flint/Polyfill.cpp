#include "Polyfill.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

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
};