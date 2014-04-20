#pragma once

#include <string>

using namespace std;

namespace flint {
	
	/**
	 * Removes the code that appears between pairs of "// %flint: pause" and
	 * "// %flint: resume", so that intentionally written code, that may
	 * generate warnings, can be ignored by lint.
	 */
	string removeIgnoredCode(const string &file, const string &path);
};