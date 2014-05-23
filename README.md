# `Flint++`
## A Cross Platform Port of Facebook's C++ Linter

`Flint++` is cross-platform, zero-dependency port of `flint`, a lint program for C++ developed and used at Facebook.

This project was motivated by a desire for a modern and extendable C++ Linter that just worked. Facebook had already done a fantastic job with their `flint` project; but through an unnecessarily high number of dependencies, poor documentation, and OS dependent coding the project is almost unusable. `Flint++` aims to solve these problems by only using the C++11 std::library along with a minimal number of polyfill functions developed to bridge the gaps in the needed functionality.

The original `flint` is published on [Github](https://github.com/facebook/flint); and for discussions, there is a [Google group](https://groups.google.com/d/forum/facebook-flint).

Upcoming Features
-----------------

* More lint tests!
* Visual Studio Integration!
* JSON Config files to allow project dependent Lint settings
	* Set custom blacklisted identifiers/token sequences/includes
	* Enable/Disable certain tests
	* Track the config file with Git to give everyone on your team the same Lint checks

Current Lint Checks
-------------------

* Errors
	* Blacklisted Identifiers
	* Initialization from Self
	* `#if ... #endif` Balance
	* `memset` Usage
	* Include Associated Header First
	* Include Guards
	* Inl-Header Inclusions
	* Check for unamed `mutex` holders
	* `explicit` single argument constructors
	* `try-catch` by reference
	* Check for `throw new ...`
	* Check `unique_ptr` arrays
* Warnings
	* Blacklisted Sequences
	* `#define` name rules
	* Deprecated `#includes`
	* Check for `static` scope
	* Warn about `smart_ptr` usage
	* `implicit` casts
	* `protected` inheritance
	* Check `exception` inheritance
	* Check for `virtual` destructors
	* function level `throws`
* Advice
	* `nullptr` over `NULL`

Usage
-----

	$ flint++ --help
	Usage: flint++ [options:] [files:]

	-r, --recursive		: Search subfolders for files.
	-c, --cmode		: Only perform C based lint checks.
	-j, --json		: Output report in JSON format.
	-v, --verbose		: Print full file paths.
	-l, --level [def = 3]   : Set the lint level.
			      1 : Errors only
			      2 : Errors & Warnings
			      3 : All feedback

	-h, --help		: Print usage.

Does it pass Linting itself? 
-------------------------

### Yes!
	
	$ flint++ ./

	Lint Summary: 13 files
	Errors: 0 Warnings: 0 Advice: 0

Compiling `Flint++` from source
-------------------------------

From the flint subdirectory, use `make` with the included makefile to build on a Posix based system using G++ > v4.7. To run the simple output test case run `make tests` after compilation. This will run `Flint++` on the test directory and compare its output to the text stored in `tests/expected.txt`.

Why Lint?
---------

Linting is a form of *static-code analysis* by which common errors and bad practices are flagged for review. This can help to both optimize poorly written code and to set a unified code style for all the code in a project. For large organizations this can be tremendously powerful as it helps to keep the whole codebase consistent.

Dependencies
------------

### Windows, Unix, Linux, OSX

**None!** You're good to go! Happy linting :)

*Edit:* It's not really a dependency, but it's worth noting that this project makes extensive use of the C++11 Feature Set. You probably won't be able to compile it on a Pre-C++11 version of your compiler. 

Tested On
---------

So far I've managed to compile and test `Flint++` on Windows 7/8/8.1, Ubuntu 14.04, and Raspbian Wheezy; compiling under MSVC++ '13 and G++ 4.7 respectively. Edit: We also have word people have successfully compiled under OSX with Clang 3.5

---

Lead Contributers: @L2Program (Joss Whittle) & @kanielc (Denton Cockburn) 

---

##Pull Requests Welcome!
