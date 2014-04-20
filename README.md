`fluff`: A Cross Platform Port of Facebook's C++ Linter
-------------------------------------------------------

`fluff` is Cross Platform port of an open-source lint program for C++ developed and used at Facebook called `flint`.

`flint` is published on Github at https://github.com/facebook/flint; for
discussions, there is a Google group at https://groups.google.com/d/forum/facebook-flint.

Dependencies
------------

### Unix, Linux, OSX

None! You're good to go! Happy linting :)

### Windows

Only the one! `fluff` makes use of the `Dirent.h` header file for doing directory listings. This file is included as a standard header on Posix systems, for windows however you just need to grab the header file from [here](http://www.softagalleria.net/dirent.php) and include it in your build setup. 