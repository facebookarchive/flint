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

Building the D version
----------------------

To build the `D` (latest) version of `Flint` you just need the latest version of
`dmd` (http://dlang.org/download.html) and `make`.

To compile execute:

```
make -f MakefileD
```

To run the tests execute:

```
make test -f MakefileD
```

don't worry about all the generated output. In case of an error you will see
something like:

```
core.exception.AssertError@Test.d(863): Assertion failure
----------------
./flint_test() [0x62c4d5]
./flint_test(void Test.EXPECT_EQ!(uint, int, "Test.d", 32uL).EXPECT_EQ(uint, int)+0x7d) [0x625a29]
./flint_test(void Test.__unittestL8_7()+0x75) [0x61e731]
./flint_test(void Test.__modtest()+0x9) [0x62c395]
./flint_test(int core.runtime.runModuleUnitTests().__foreachbody3(object.ModuleInfo*)+0x34) [0x646f9c]
./flint_test(int object.ModuleInfo.opApply(scope int delegate(object.ModuleInfo*)).__lambda2(immutable(object.ModuleInfo*))+0x1c) [0x62f6a0]
./flint_test(int rt.minfo.moduleinfos_apply(scope int delegate(immutable(object.ModuleInfo*))).__foreachbody2(ref rt.sections_linux.DSO)+0x47) [0x636a97]
./flint_test(int rt.sections_linux.DSO.opApply(scope int delegate(ref rt.sections_linux.DSO))+0x42) [0x636b0e]
./flint_test(int rt.minfo.moduleinfos_apply(scope int delegate(immutable(object.ModuleInfo*)))+0x25) [0x636a35]
./flint_test(int object.ModuleInfo.opApply(scope int delegate(object.ModuleInfo*))+0x25) [0x62f679]
./flint_test(runModuleUnitTests+0xa8) [0x646e30]
./flint_test(void rt.dmain2._d_run_main(int, char**, extern (C) int function(char[][])*).runAll()+0x17) [0x63215f]
./flint_test(void rt.dmain2._d_run_main(int, char**, extern (C) int function(char[][])*).tryExec(scope void delegate())+0x2a) [0x632112]
./flint_test(_d_run_main+0x193) [0x632093]
./flint_test(main+0x17) [0x62c523]
/lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xed) [0x2ae25ecfc76d]
make: *** [test] Error 1

```

Dependencies
------------

- folly (https://github.com/facebook/folly)

    Follow the instructions to download and install folly.

- double-conversion (https://github.com/google/double-conversion)
    
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
