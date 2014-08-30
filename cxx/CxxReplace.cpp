// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

#include "Tokenizer.h"
#include "folly/Foreach.h"
#include "folly/FileUtil.h"
#include "folly/Exception.h"
#include <gflags/gflags.h>
#include <string>

DEFINE_bool(verbose, false, "Verbose");

using namespace std;
using namespace folly;
using namespace facebook;
using namespace facebook::flint;

struct CompareTokens : std::binary_function<Token, Token, bool> {
  bool operator()(const Token & lhs, const Token & rhs) const {
    if (lhs.type_ != rhs.type_) return false;
    if (lhs.type_ == TK_IDENTIFIER) {
      // Must compare values too
      return lhs.value_ == rhs.value_;
    }
    return true;
  }
};

/**
 * Entry point. Reads in turn and operates on each file passed onto
 * the command line.
 */
int main(int argc, char ** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  if (argc <= 3) {
    fprintf(stderr,
            "Usage: '%s' 'replace this code' 'with this code' files...",
            argv[0]);
    return 1;
  }

  // Tokenize the code to replace
  string oldCode = argv[1], newCode = argv[2];
  string oldFilename = "old_code";
  vector<Token> oldTokens;
  tokenize(oldCode, oldFilename, oldTokens);
  // Get rid of the EOF token
  CHECK(!oldTokens.empty() && oldTokens.back().type_ == TK_EOF);
  oldTokens.pop_back();

  // Tokenize the code to replace with
  string newFilename = "new_code";
  vector<Token> newTokens;
  tokenize(newCode, newFilename, newTokens);
  CHECK(!newTokens.empty() && newTokens.back().type_ == TK_EOF);
  newTokens.pop_back();

  // Operate on each file
  FOR_EACH_RANGE (i, 3, argc) {
    printf("Processing: %s\n", argv[i]);
    string file;
    // Get file intro memory
    if (!readFile(argv[i], file)) {
      throwSystemError();
    }
    string filename = argv[i];
    vector<Token> tokens;
    // Tokenize the file's contents
    tokenize(file, filename, tokens);

    // This is the output of the processing
    vector<Token> result;
    uint replacements = 0;

    for (auto b = tokens.begin(); ; ) {
      auto i = search(b, tokens.end(), oldTokens.begin(), oldTokens.end(),
                      CompareTokens());
      if (i == tokens.end() && !replacements) {
        // Nothing was ever found, we're done with this file
        break;
      }

      // Copy whatever was before the match
      copy(b, i, back_inserter(result));

      if (i == tokens.end()) {
        // Done
        break;
      }

      // Copy the replacement tokens, making sure the comment (if any)
      // is tacked just before the first token
      if (!newTokens.empty()) {
        newTokens.front().precedingWhitespace_ = i->precedingWhitespace_;
        copy(newTokens.begin(), newTokens.end(), back_inserter(result));
      }
      ++replacements;
      // Move on
      if (i == tokens.end()) {
        // Done with this file
        break;
      }
      // Skip over the old tokens
      b = i + oldTokens.size();
    }

    if (!replacements) {
      // Nothing to do for this file
      continue;
    }

    // We now have the new tokens, write'em to the replacement file
    auto newFile = string(argv[i]) + ".tmp";
    auto f = fopen(newFile.c_str(), "w");
    if (!f) {
      throwSystemError();
    }
    FOR_EACH (t, result) {
      checkUnixError(
        fprintf(f, "%.*s%.*s",
                static_cast<uint>(t->precedingWhitespace_.size()),
                t->precedingWhitespace_.data(),
                static_cast<uint>(t->value_.size()),
                t->value_.data()));
    }
    checkPosixError(fclose(f));

    // Forcibly move the new file over the old one
    checkUnixError(rename(newFile.c_str(), argv[i]));

    // Gloat if asked to
    if (FLAGS_verbose) {
      checkUnixError(fprintf(stderr,
                             "%s: %u replacements.\n", argv[i], replacements));
    }
  }

  return 0;
}
