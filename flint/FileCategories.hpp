#pragma once

#include <string>

using namespace std;

namespace flint {

	// Indentifiers for files marked for linting
	enum class FileCategory {
		HEADER, INL_HEADER, SOURCE_C, SOURCE_CPP, UNKNOWN,
	};

	// File identifying functions...
	FileCategory getFileCategory(const string &path);

	bool isHeader(const string &path);
	bool isSource(const string &path);

	string getFileNameBase(const string &path);
	string getFileName(const string &path);
};