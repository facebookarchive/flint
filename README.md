# `Flint++`
### A Cross Platform Port of Facebook's C++ Linter

`Flint++` is cross-platform, zero-dependency port of `flint`, a lint program for C++ developed and used at Facebook.

The original `flint` is published on [Github](https://github.com/facebook/flint); and for discussions, there is a [Google group](https://groups.google.com/d/forum/facebook-flint).

Usage
-----

	$ flint++ --help
	Usage: flint++ [options:] [files:]

        -r, --recursive         : Search subfolders for files.
        -c, --cmode             : Only perform C based lint checks.
        -j, --json              : Output report in JSON format.
        -l, --level [value:]    : Set the lint level.
                              0 : Errors only
                              1 : Errors & Warnings
                              2 : All feedback

        -h, --help              : Print usage.

Why Lint?
---------

Linting is a form of *static-code analysis* by which common errors and bad practices are flagged for review. This can help to both optimize poorly written code and to set a unified code style for all the code in a project. For large organizations this can be tremendously powerful as it helps to keep the whole codebase consistent.

Dependencies
------------

### Windows, Unix, Linux, OSX

**None!** You're good to go! Happy linting :)