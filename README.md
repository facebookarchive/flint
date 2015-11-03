- - -

**_This project is not actively maintained. Proceed at your own risk!_**

- - -  

`flint`: Facebook's C++ Linter
-----------------------------

`flint` is an open-source lint program for C++ developed and used at Facebook.

`flint` is published on Github at https://github.com/facebook/flint; for
discussions, there is a Google group at https://groups.google.com/d/forum/facebook-flint.

There are two versions of `flint`. The main one (`flint/*.d`) is written in the D programming language and is supported going forward. We also provide our older implementation in C++ (`flint/cxx/*.cpp`) for historical perspective and comparison purposes.

Currently `flint`'s build has only been tested on Ubuntu. The motivated user should have no problem adapting it to other systems. More officially supported OSs to follow.

Dependencies
------------

- folly (https://github.com/facebook/folly)

    Follow the instructions to download and install folly.

- double-conversion (http://code.google.com/p/double-conversion/)
    
    Follow the instructions listed on the Folly page to build and install

- googletest (Google C++ Testing Framework)

    Grab gtest 1.6.0 from: http://googletest.googlecode.com/files/gtest-1.6.0.zip. Unzip it inside of the cxx/ subdirectory.

- additional platform specific dependencies

    Ubuntu 13.10 64-bit
    - g++ (tested with 4.7.1, 4.8.1)
    - gdc
    - automake
    - autoconf
    - autoconf-archive
    - libtool
    - libboost1.54-all-dev
    - libgoogle-glog-dev
    - libgflags-dev

To Build
--------

    autoreconf --install
    LDFLAGS=-L<double-conversion> CPPFLAGS=-I<double-conversion>/src configure ...
    make
