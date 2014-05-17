#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace flint {
	
	enum Lint {
		ERROR, WARNING, ADVICE
	};

	struct OptionsInfo {
		bool RECURSIVE;
		bool CMODE;
		bool JSON;

		int  LEVEL;
	};
	extern OptionsInfo Options;	
	
        void printHelp(); 
        void parseArgs(int argc, char* argv[], std::vector<std::string> &paths);
};
