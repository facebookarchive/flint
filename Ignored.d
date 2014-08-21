// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.range, std.stdio, std.string;

const kIgnorePause = "// %flint: pause";
const kIgnoreResume = "// %flint: resume";

string removeIgnoredCode(string file, string fpath) {
  string result;
  size_t pos = 0;

  while (true) {
    // Find the position of ignorePause starting from position 'pos'
    auto posPause = file[pos .. $].indexOf(kIgnorePause);
    // If no instance of posPause is found, then add everything from
    // pos to end_of_file to result and return.
    if (posPause < 0) {
      result ~= file[pos .. $];
      break;
    }

    posPause += pos;
    // If an instance of ignorePause was found, then find the corresponding
    // position of ignoreResume.
    auto posResume = file[posPause + kIgnorePause.length .. $]
      .indexOf(kIgnoreResume);
    // If no instance of ignoreResume was found, then show an error to the
    // user, with the line number for ignorePause.
    if (posResume < 0) {
      auto lineNo = file[0 .. posPause].count('\n') + 1;
      stderr.writef("%s:%d: No matching \"%s\" found for \"%s\"\n",
                    fpath, lineNo, kIgnoreResume, kIgnorePause);
      result ~= file[pos .. $];
      break;
    }
    posResume += posPause + kIgnorePause.length;

    // Otherwise add everything from pos to posPause - 1, empty line for
    // each line from posPause to posResume + size(posResume) - 1 so that
    // other lint errors show correct line number, and set pos to the next
    // position after ignoreResume.
    result ~= file[pos .. posPause];
    auto emptyLinesToAdd =
      file[posPause .. posResume + kIgnoreResume.length].count('\n');
    result ~= std.range.repeat('\n', emptyLinesToAdd).array;
    pos = posResume + kIgnoreResume.length;
  }
  return result;
}
