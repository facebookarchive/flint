// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt

#ifndef CPPLINT_FILECATEGORIES_H_
#define CPPLINT_FILECATEGORIES_H_

#include <string>
#include "folly/Range.h"

namespace facebook { namespace flint {

enum class FileCategory {
  HEADER, INL_HEADER, SOURCE_C, SOURCE_CPP, UNKNOWN, };

FileCategory getFileCategory(folly::StringPiece fpath);

bool isHeader(const std::string& fpath);
bool isSource(const std::string& fpath);

std::string getFileNameBase(const std::string& filename);

}} // namespaces

#endif
