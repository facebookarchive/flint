#pragma once

#include <iostream>
#include <string>
#include <map>

using namespace std;

namespace flint {
	
	enum Lint {
		ERROR, WARNING, ADVICE
	};

	static struct {
		bool RECURSIVE;
		bool CMODE;
		bool JSON;
		bool VERBOSE;

		int  LEVEL;
	} Options;

	/**
	* Prints the usage information for the program, then exits.
	*/
	static void printHelp() {
		printf("Usage: flint [options:] [files:]\n\n"
			   "\t-r, --recursive	: Search subfolders for files.\n\n"
			   "\t-c, --cmode	: Only perform C based lint checks.\n\n"
			   "\t-j, --json	: Output report in JSON format.\n\n"
			   "\t-v, --verbose	: Give detailed feedback.\n\n"
			   "\t-h, --help	: Print usage.\n\n");
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
	static void parseArgs(int argc, char* argv[], vector<string> &paths) {
		
		// Set default values
		Options.RECURSIVE	= false;
		Options.CMODE		= false;
		Options.JSON		= false;
		Options.VERBOSE		= false;
		Options.LEVEL		= Lint::ADVICE;
		bool HELP			= false;

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
		Arg argLevel		= { ArgType::INT,  &Options.LEVEL };
		Arg argVerbose		= { ArgType::BOOL, &Options.VERBOSE };

		static const map<string, Arg &> params = {
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

					int val = atoi(argv[i]);
					*arg = val;
					continue;
				}
			}
			else {
				// Push another path onto the lint list
				paths.push_back(string(argv[i]));
			}
		}

		if (HELP) {
			printHelp();
		}
	};
};