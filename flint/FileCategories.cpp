#include "FileCategories.hpp"

#include <algorithm>
#include <functional>
#include <vector>

#include "Polyfill.hpp"

namespace flint {

	// File Extensions to lint
	const vector<string> extsHeader    = { ".h", ".hpp", ".hh" };
	const vector<string> extsSourceC   = { ".c" };
	const vector<string> extsSourceCpp = { ".C", ".cc", ".cpp", ".CPP", ".c++", ".cp", ".cxx" };

	/**
	 * Tests if a given string ends with a suffix
	 *
	 * @param str
	 *		The string to search
	 * @param suffix
	 *		The suffix to search for
	 * @return
	 *		Returns true if str ends with an instance of suffix
	 */
	bool hasSuffix(const string &str, const string &suffix) {
		return (str.size() >= suffix.size()) &&
			   (str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
	}

	/**
	 * Tests if a given string ends with a suffix within the container
	 *
	 * @param str
	 *		The string to search
	 * @param suffixes
	 *		The suffixes to search for
	 * @return
	 *		Returns true if str ends with an instance of a suffix found within the container
	 */
	 template <class Container>
	 bool containsSuffix(const string &str, Container suffixes) {
		return find_if(begin(suffixes), end(suffixes), bind(hasSuffix, str, placeholders::_1)) != end(suffixes);
	 }

	/**
	 * Attempts to discern whether what type the given file is
	 * based on it's extension
	 *
	 * @param path
	 *		The path of the file to identify
	 * @return
	 *		Returns an identifier flag of enum type FileCategory
	 */
	FileCategory getFileCategory(const string &path) {

		// Test header extensions
		for (const string &ext : extsHeader) {
			if (hasSuffix(path, ("-inl" + ext))) {
				return FileCategory::INL_HEADER;
			}
			else if (hasSuffix(path, ext)) {
				return FileCategory::HEADER;
			}
		}

		// Test C extensions
		if (containsSuffix(path, extsSourceC)) {
			return FileCategory::SOURCE_C;
		}

		// Test CPP extensions
		if (containsSuffix(path, extsSourceCpp)) {
			return FileCategory::SOURCE_CPP;
		}

		return FileCategory::UNKNOWN;
	};

	/**
	* Attempts to discern whether the given path is a header
	* file based on it's extension
	*
	* @param path
	*		The path of the file to identify
	* @return
	*		Returns true if the file is a header or inline header file
	*/
	bool isHeader(const string &path) {
		FileCategory fCat = getFileCategory(path);
		return (fCat == FileCategory::INL_HEADER || fCat == FileCategory::HEADER);
	};

	/**
	* Attempts to discern whether the given path is a source
	* file based on it's extension
	*
	* @param path
	*		The path of the file to identify
	* @return
	*		Returns true if the file is a source or c source file
	*/
	bool isSource(const string &path) {
		FileCategory fCat = getFileCategory(path);
		return (fCat == FileCategory::SOURCE_C || fCat == FileCategory::SOURCE_CPP);
	};

	/**
	* Strips the extension off of the given string and returns the file name
	*
	* @param path
	*		The path of the file trim
	* @return
	*		Returns the file name without any linter extensions
	*/
	string getFileNameBase(const string &path) {

		// Test header extensions
		for (const string &ext : extsHeader) {
			string inlext = "-inl" + ext;
			if (hasSuffix(path, inlext)) {
				return path.substr(0, path.size() - inlext.size());
			}
			else if (hasSuffix(path, ext)) {
				return path.substr(0, path.size() - ext.size());
			}
		}

		// Test C extensions
		for (const string &ext : extsSourceC) {
			if (hasSuffix(path, ext)) {
				return path.substr(0, path.size() - ext.size());
			}
		}

		// Test CPP extensions
		for (const string &ext : extsSourceCpp) {
			if (hasSuffix(path, ext)) {
				return path.substr(0, path.size() - ext.size());
			}
		}

		// No extension to strip
		return path;
	};

	/**
	* Strips the path off of a file name
	*
	* @param path
	*		The path of the file trim
	* @return
	*		Returns the file name without any linter extensions
	*/
	string getFileName(const string &path) {
		size_t pos = path.find_last_of(FS_SEP);

		if (pos == string::npos || pos == path.size() - 2) {
			return path;
		}
		return path.substr(pos + 1);
	};
};
