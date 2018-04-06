#include "Options.hpp"

#include <iostream>
#include <unordered_map>

namespace flint {

	OptionsInfo Options;

	/**
	* Prints the usage information for the program, then exits.
	*/
	void printHelp() {
		printf("Usage: flint++ [options:] [files:]\n\n"
			   "\t-r, --recursive		: Search subfolders for files.\n"
			   "\t-c, --cmode			: Only perform C based lint checks.\n"
			   "\t-j, --json			: Output report in JSON format.\n"
			   "\t-v, --verbose		: Print full file paths.\n"
			   "\t-l, --level [def=3] : Set the lint level.\n"
			   "			          1 : Errors only\n"
			   "			          2 : Errors & Warnings\n"
			   "			          3 : All feedback\n\n"
			   "\t-h, --help		    : Print usage.\n\n");
#ifdef _DEBUG 
		// Stop visual studio from closing the window...
		system("PAUSE");
#endif
		exit(1);
	};

	/**
	* Given an argument count and list, parses the arguments
	* and sets the global options as desired
	*
	* @param argc
	*		The number of arguments
	* @param argv
	*		The list of cmd line arguments
	* @param paths
	*		A vector of strings to be filled with lint paths
	*/
	void parseArgs(int argc, char* argv[], vector<string> &paths) {
		// Set default values
		Options.RECURSIVE	= false;
		Options.CMODE		= false;
		Options.JSON		= false;
		Options.VERBOSE		= false;
		Options.LEVEL		= Lint::ADVICE;
		bool HELP			= false;

		bool l1 = false;
		bool l2 = false;
		bool l3 = false;

		enum ArgType {
			BOOL, INT
		};
		struct Arg {
			ArgType type;
			const void *ptr;
		};

		// Map values to their cmd line flags
		Arg argHelp			= { ArgType::BOOL, &HELP };
		Arg argRecursive	= { ArgType::BOOL, &Options.RECURSIVE };
		Arg argCMode		= { ArgType::BOOL, &Options.CMODE };
		Arg argJSON			= { ArgType::BOOL, &Options.JSON };
		Arg argVerbose		= { ArgType::BOOL, &Options.VERBOSE };

		Arg argLevel		= { ArgType::INT,  &Options.LEVEL };
		Arg argL1 = { ArgType::BOOL, &l1 };
		Arg argL2 = { ArgType::BOOL, &l2 };
		Arg argL3 = { ArgType::BOOL, &l3 };

		static const unordered_map<string, Arg &> params = {
			{ "-h", argHelp },
			{ "--help", argHelp },

			{ "-r", argRecursive },
			{ "--recursive", argRecursive },

			{ "-c", argCMode },
			{ "--cmode", argCMode },

			{ "-j", argJSON },
			{ "--json", argJSON },

			{ "-l", argLevel },
			{ "--level", argLevel },
			{ "-l1", argL1 },
			{ "-l2", argL2 },
			{ "-l3", argL3 },

			{ "-v", argVerbose },
			{ "--verbose", argVerbose }
		};

		// Loop over the given argument list
		for (int i = 1; i < argc; ++i) {
			
			// If the current argument is in the map
			// then set its value to true
			auto it = params.find(string(argv[i]));
			if (it != params.end()) {
				
				if (it->second.type == ArgType::BOOL) {
					bool *arg = (bool*)it->second.ptr;
					*arg = true;
				}
				else if (it->second.type == ArgType::INT) {
					int *arg = (int*) it->second.ptr;

					++i;
					if (i >= argc) {
						printf("Missing (int) value for parameter: %s\n\n", 
							it->first.c_str());
						printHelp();
					}

					int val = atoi(argv[i]) - 1;
					*arg = val;
					continue;
				}
			}
			else {
				// Push another path onto the lint list
				string p = argv[i];
				if (p.back() == '/' || p.back() == '\\') {
					p.erase(p.end()-1, p.end());
				}
				paths.push_back(move(p));
			}
		}

		if (l1) {
			Options.LEVEL = Lint::ERROR;
		}
		else if (l2) {
			Options.LEVEL = Lint::WARNING;
		}
		else if (l3) {
			Options.LEVEL = Lint::ADVICE;
		}

		// Make sure level was given a correct value
		Options.LEVEL = ((Options.LEVEL > Lint::ADVICE) ? 
			Lint::ADVICE : 
			((Options.LEVEL < Lint::ERROR) ? 
				Lint::ERROR : 
				Options.LEVEL));

		if (paths.size() == 0) {
			paths.push_back(".");
		}

		if (HELP) {
			printHelp();
		}
	};
};
