#pragma once

#include <iostream>
#include <string>
#include <map>

using namespace std;

namespace flint {
	
	static struct {
		bool RECURSIVE;
		bool CMODE;
		bool JSON;
		bool VERBOSE;
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
	*/
	static void parseArgs(int argc, char* argv[]) {
		
		// Set default values
		Options.RECURSIVE	= false;
		Options.CMODE		= false;
		Options.JSON		= false;
		Options.VERBOSE		= false;
		bool HELP			= false;

		// Map values to their cmd line flags
		static const map<string, bool &> params = {
			{ "-r",				Options.RECURSIVE },
			{ "--recursive",	Options.RECURSIVE },

			{ "-c",				Options.CMODE },
			{ "--cmode",		Options.CMODE },

			{ "-j",				Options.JSON },
			{ "--json",			Options.JSON },

			{ "-v",				Options.VERBOSE },
			{ "--verbose",		Options.VERBOSE },

			{ "-h",				HELP }, 
			{ "--help",			HELP }
		};

		// Loop over the given argument list
		for (int i = 1; i < argc; ++i) {
			
			// If the current argument is in the map
			// then set its value to true
			auto it = params.find(string(argv[i]));
			if (it != params.end()) {
				it->second = true;

				if (HELP) {
					printHelp();
					break;
				}
			}
		}
	};
};