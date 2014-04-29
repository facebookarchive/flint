#include "Ignored.hpp"

#include <algorithm>

#include "Polyfill.hpp"

namespace flint {
	
	// Constants
	const string kIgnorePause  = "// %flint: pause";
	const string kIgnoreResume = "// %flint: resume";

	/**
	 * Removed code between lint tags
	 *
	 * @param file
	 *		A string of the file contents to modify
	 * @param path
	 *		The path the file resides at, for debug messages
	 * @return
	 *		Returns the modified string
	 */
	string removeIgnoredCode(const string &file, const string &path) {
		
		string result;
		size_t pos = 0;

		while (true) {
			size_t posPause = 0;

			// Find the position of ignorePause starting from position 'pos'
			posPause = file.find(kIgnorePause, pos);

			// If no instance of posPause is found, then add everything from
			// pos to end_of_file to result and return.
			if (posPause == string::npos) {
				result += file.substr(pos);
				break;
			}
			else 
			{
				size_t posResume = 0;

				// If an instance of ignorePause was found, then find the corresponding
				// position of ignoreResume.
				posResume = file.find(kIgnoreResume, posPause + kIgnorePause.size());

				// If no instance of ignoreResume was found, then show an error to the
				// user, with the line number for ignorePause.
				if (posResume == string::npos) {

					uint lineNo = (uint)count(file.begin(), file.begin() + posPause, '\n');
					++lineNo;

					fprintf(stderr, "%s(%d): No matching \"%s\" found for \"%s\"\n",
						path, lineNo, kIgnoreResume.c_str(), kIgnorePause.c_str());

					result += file.substr(pos);
					break;
				}
				else 
				{
					// Otherwise add everything from pos to posPause - 1, empty line for
					// each line from posPause to posResume + size(posResume) - 1 so that
					// other lint errors show correct line number, and set pos to the next
					// position after ignoreResume.
					result += file.substr(pos, posPause - pos);

					uint emptyLinesToAdd = (uint)count(file.begin() + posPause,
												file.begin() + posResume + kIgnoreResume.size(), '\n');

					result += string(emptyLinesToAdd, '\n');
					pos = posResume + kIgnoreResume.size();
				}
			}
		}

		return result;
	};
};