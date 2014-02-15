// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt

import Tokenizer;
import std.conv, std.exception, std.file, std.range, std.stdio;

/**
 * Reads the files passed as arguments and prints out Cxx tokens one by line
 */
void main(string[] args) {
  enforce(args.length > 1,
          text("Usage:", args[0], " files..."));

  foreach (fi; 1 .. args.length) {
    auto filename = args[fi];

    Token[] tokens;
    auto file = std.file.readText(filename);
    tokenize(file, filename, tokens);

    // Strip terminating \0
    assert(tokens.back.type_ == tk!"\0");
    tokens.popBack;

    foreach (idx; 0 .. tokens.length) {
      stdout.writef("%s %s\n", tokens[idx].type_.sym(), tokens[idx].value_);
    }
  }
}
