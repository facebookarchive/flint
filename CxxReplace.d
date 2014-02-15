// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import Tokenizer;
import std.algorithm, std.conv, std.exception, std.file, std.range, std.stdio;

bool verbose;

bool compareTokens(ref const Token lhs, ref const Token rhs) {
  if (lhs.type_ != rhs.type_) return false;
  if (lhs.type_ == tk!"identifier") {
    // Must compare values too
    return lhs.value_ == rhs.value_;
  }
  return true;
}

/**
 * Entry point. Reads in turn and operates on each file passed onto
 * the command line.
 */
int main(string[] args) {
  enforce(args.length > 3, text("Usage: ", args[0],
          " 'replace this code' 'with this code' files..."));

  // Tokenize the code to replace
  string oldCode = args[1], newCode = args[2];
  string oldFilename = "old_code";
  Token[] oldTokens;
  tokenize(oldCode, oldFilename, oldTokens);
  // Get rid of the EOF token
  enforce(!oldTokens.empty && oldTokens.back.type_ == tk!"\0");
  oldTokens.popBack;

  // Tokenize the code to replace with
  string newFilename = "new_code";
  Token[] newTokens;
  tokenize(newCode, newFilename, newTokens);
  enforce(!newTokens.empty && newTokens.back.type_ == tk!"\0");
  newTokens.popBack;

  // Operate on each file
  foreach (idx; 3 .. args.length) {
    writef("Processing: %s\n", args[idx]);
    // Get file intro memory
    auto file = std.file.readText(args[idx]);
    string filename = args[idx];
    Token[] tokens;
    // Tokenize the file's contents
    tokenize(file, filename, tokens);
    // No need for the terminating \0
    assert(tokens.back.type_ == tk!"\0");
    string eofWS = tokens.back.precedingWhitespace_;
    tokens.popBack;

    // This is the output of the processing
    Token[] result;
    uint replacements = 0;

    for (auto b = tokens; ; ) {
      auto i = b.find!compareTokens(oldTokens);
      if (i.empty && !replacements) {
        // Nothing was ever found, we're done with this file
        break;
      }

      // Copy whatever was before the match
      result ~= b[0 .. b.length - i.length];

      if (i.empty) {
        // Done
        break;
      }

      // Copy the replacement tokens, making sure the comment (if any)
      // is tacked just before the first token
      if (!newTokens.empty) {
        newTokens.front.precedingWhitespace_ = i.front.precedingWhitespace_;
        result ~= newTokens;
      }
      ++replacements;
      // Move on
      if (i.empty) {
        // Done with this file
        break;
      }
      // Skip over the old tokens
      b = i[oldTokens.length .. $];
    }

    if (!replacements) {
      // Nothing to do for this file
      continue;
    }

    // We now have the new tokens, write'em to the replacement file
    auto newFile = args[idx] ~ ".tmp";
    auto f = File(newFile, "w");
    foreach (ref t; result) {
      f.write(t.precedingWhitespace_, t.value);
    }
    f.write(eofWS);
    f.close;

    // Forcibly move the new file over the old one
    std.file.rename(newFile, args[idx]);

    // Gloat if asked to
    if (verbose) {
      stderr.writef("%s: %u replacements.\n", args[idx], replacements);
    }
  }

  return 0;
}
