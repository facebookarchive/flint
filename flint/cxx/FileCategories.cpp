// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt

#include "FileCategories.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <vector>

namespace facebook { namespace flint {

using namespace folly;

const std::vector<std::string> extsHeader = { ".h", ".hpp", ".hh", };
const std::vector<std::string> extsSourceC = { ".c", };
const std::vector<std::string> extsSourceCpp = {
  ".C", ".cc", ".cpp", ".CPP", ".c++", ".cp",  ".cxx", };

FileCategory getFileCategory(const StringPiece fpath) {
  for (const auto& ext : extsHeader) {
    if (fpath.endsWith("-inl" + ext)) {
      return FileCategory::INL_HEADER;
    } else if (fpath.endsWith(ext)) {
      return FileCategory::HEADER;
    }
  }
  for (const auto& ext : extsSourceC) {
    if (fpath.endsWith(ext)) {
      return FileCategory::SOURCE_C;
    }
  }
  for (const auto& ext : extsSourceCpp) {
    if (fpath.endsWith(ext)) {
      return FileCategory::SOURCE_CPP;
    }
  }
  return FileCategory::UNKNOWN;
}

bool isHeader(const std::string& fpath) {
  FileCategory fileCategory = getFileCategory(fpath);
  return fileCategory == FileCategory::HEADER ||
         fileCategory == FileCategory::INL_HEADER;
}

bool isSource(const std::string& fpath) {
  FileCategory fileCategory = getFileCategory(fpath);
  return fileCategory == FileCategory::SOURCE_C ||
         fileCategory == FileCategory::SOURCE_CPP;
}

std::string getFileNameBase(const std::string& filename) {
  for (const auto& ext : extsHeader) {
    auto inlExt = "-inl" + ext;
    if (StringPiece(filename).endsWith(inlExt)) {
      return boost::erase_last_copy(filename, inlExt);
    } else if (StringPiece(filename).endsWith(ext)) {
      return boost::erase_last_copy(filename, ext);
    }
  }
  for (const auto& ext : extsSourceC) {
    if (StringPiece(filename).endsWith(ext)) {
      return boost::erase_last_copy(filename, ext);
    }
  }
  for (const auto& ext : extsSourceCpp) {
    if (StringPiece(filename).endsWith(ext)) {
      return boost::erase_last_copy(filename, ext);
    }
  }
  return filename;
}

}}
