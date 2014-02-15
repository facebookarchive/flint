// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt

#include "Ignored.h"

namespace facebook {
namespace flint {

const std::string kIgnorePause = "// %flint: pause";
const std::string kIgnoreResume = "// %flint: resume";

std::string removeIgnoredCode(const std::string& file,
                              const std::string& fpath) {
  std::string result;
  size_t pos = 0;

  while (true) {
    size_t posPause = 0;
    // Find the position of ignorePause starting from position 'pos'
    posPause = file.find(kIgnorePause, pos);
    // If no instance of posPause is found, then add everything from
    // pos to end_of_file to result and return.
    if (posPause == std::string::npos) {
      result += file.substr(pos);
      break;
    } else {
      size_t posResume = 0;
      // If an instance of ignorePause was found, then find the corresponding
      // position of ignoreResume.
      posResume = file.find(kIgnoreResume, posPause + kIgnorePause.size());
      // If no instance of ignoreResume was found, then show an error to the
      // user, with the line number for ignorePause.
      if (posResume == std::string::npos) {
        int lineNo = std::count(file.begin(), file.begin() + posPause, '\n');
        lineNo++;
        fprintf(stderr, "%s(%d): No matching \"%s\" found for \"%s\"\n",
                fpath.c_str(), lineNo, kIgnoreResume.c_str(),
                kIgnorePause.c_str());
        result += file.substr(pos);
        break;
      } else {
        // Otherwise add everything from pos to posPause - 1, empty line for
        // each line from posPause to posResume + size(posResume) - 1 so that
        // other lint errors show correct line number, and set pos to the next
        // position after ignoreResume.
        result += file.substr(pos, posPause - pos);
        int emptyLinesToAdd = std::count(file.begin() + posPause,
            file.begin() + posResume + kIgnoreResume.size(), '\n');
        result += std::string(emptyLinesToAdd, '\n');
        pos = posResume + kIgnoreResume.size();
      }
    }
  }
  return result;
}

}  // namespace flint
}  // namepace facebook
