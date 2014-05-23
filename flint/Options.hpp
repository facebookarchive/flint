#pragma once

#include <string>
#include <vector>

using namespace std;

namespace flint {
	
	enum Lint {
		ERROR, WARNING, ADVICE
	};

	struct OptionsInfo {
		bool RECURSIVE;
		bool CMODE;
		bool JSON;
		bool VERBOSE;
		int  LEVEL;
	};
	extern OptionsInfo Options;	
	
	void printHelp(); 
	void parseArgs(int argc, char *argv[], vector<string> &paths);
};
