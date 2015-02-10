// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.algorithm, std.range;

immutable string[]
  extsHeader = [ ".h", ".hpp", ".hh", ],
  extsSourceC = [ ".c", ],
  extsSourceCpp = [ ".C", ".cc", ".cpp", ".CPP", ".c++", ".cp",  ".cxx", ];

enum FileCategory {
  header, inl_header, source_c, source_cpp, unknown,
};

FileCategory getFileCategory(string fpath) {
  foreach (ext; extsHeader) {
    if (fpath.endsWith(chain("-inl", ext))) return FileCategory.inl_header;
    if (fpath.endsWith(ext)) return FileCategory.header;
  }
  foreach (ext; extsSourceC) {
    if (fpath.endsWith(ext)) {
      return FileCategory.source_c;
    }
  }
  foreach (ext; extsSourceCpp) {
    if (fpath.endsWith(ext)) {
      return FileCategory.source_cpp;
    }
  }
  return FileCategory.unknown;
}

bool isHeader(string fpath) {
  auto fileCategory = getFileCategory(fpath);
  return fileCategory == FileCategory.header ||
    fileCategory == FileCategory.inl_header;
}

bool isSource(string fpath) {
  auto fileCategory = getFileCategory(fpath);
  return fileCategory == FileCategory.source_c ||
    fileCategory == FileCategory.source_cpp;
}

bool isTestFile(string fpath) {
  import std.string : toLower;
  return !fpath.toLower.find("test").empty;
}

string getFileNameBase(string filename) {
  foreach (ext; extsHeader) {
    auto inlExt = "-inl" ~ ext;
    if (filename.endsWith(inlExt)) {
      return filename[0 .. $ - inlExt.length];
    } else if (filename.endsWith(ext)) {
      return filename[0 .. $ - ext.length];
    }
  }
  foreach (ext; chain(extsSourceC, extsSourceCpp)) {
    if (filename.endsWith(ext)) {
      return filename[0 .. $ - ext.length];
    }
  }
  return filename;
}
