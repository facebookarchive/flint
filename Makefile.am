bin_PROGRAMS = flint
noinst_PROGRAMS = flint_test cxx_replace cxx_tokenize

ACLOCAL_AMFLAGS = -I m4

FLINT_SRCS := \
	Checks.d \
	FileCategories.d \
	Ignored.d \
	Tokenizer.d

flint$(EXEEXT): Main.d $(FLINT_SRCS)
	$(DC) -of$@ $^

flint_test$(EXEEXT): Test.d $(FLINT_SRCS)
	$(DC) -of$@ $^

cxx_replace$(EXEEXT): CxxReplace.d $(FLINT_SRCS)
	$(DC) -of$@ $^

cxx_tokenize$(EXEEXT): CxxTokenize.d $(FLINT_SRCS)
	$(DC) -of$@ $^
