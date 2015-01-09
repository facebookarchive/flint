noinst_LTLIBRARIES = \
	libflint.la \
	libgtest.la \
	libgtest_main.la

libflint_la_SOURCES = \
	Checks.h \
	Checks.cpp \
	FileCategories.h \
	FileCategories.cpp \
	Ignored.h \
	Ignored.cpp \
	Tokenizer.h \
	Tokenizer.cpp
libflint_la_LIBADD = \
	-lfolly \
	$(BOOST_LIBS) \
	$(BOOST_FILESYSTEM_LIB)
libflint_la_CPPFLAGS = $(BOOST_CPPFLAGS)

libgtest_la_CPPFLAGS = \
	-Igtest-1.6.0 \
	-Igtest-1.6.0/src \
	-Igtest-1.6.0/include
libgtest_la_SOURCES = gtest-1.6.0/src/gtest-all.cc

libgtest_main_la_CPPFLAGS = \
	-Igtest-1.6.0 \
	-Igtest-1.6.0/src \
	-Igtest-1.6.0/include
libgtest_main_la_SOURCES = gtest-1.6.0/src/gtest_main.cc
libgtest_main_la_LIBADD = libgtest.la

bin_PROGRAMS = flint cxx_replace

flint_SOURCES = Main.cpp
flint_LDADD = \
	libflint.la \
	$(BOOST_LIBS) \
	$(BOOST_FILESYSTEM_LIB)
flint_CPPFLAGS = $(BOOST_CPPFLAGS)

cxx_replace_SOURCES = CxxReplace.cpp
cxx_replace_LDADD = libflint.la

noinst_PROGRAMS = flint_test

TESTS= flint_test

flint_test_SOURCES = Test.cpp
flint_test_LDADD = libgtest_main.la libflint.la
flint_test_CPPFLAGS = -isystemgtest-1.6.0/include
