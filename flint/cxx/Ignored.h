// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt

#ifndef LINTERS_CPPLINT_IGNORED_H_
#define LINTERS_CPPLINT_IGNORED_H_

#include <string>
#include <algorithm>

namespace facebook {
namespace flint {

/**
 ** Removes the code that appears between pairs of "// %flint: pause" and
 ** "// %flint: resume", so that intentionally written code, that may
 ** generate warnings, can be ignored by lint.
 */
std::string removeIgnoredCode(const std::string& file,
                              const std::string& fpath);
}  // namespace flint
}  // namespace facebook

#endif
