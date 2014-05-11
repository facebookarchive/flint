CXX=g++

# ARCH=32
ARCH=64

# Compiler and Linker flags
CXXFLAGS= -O3 -m$(ARCH) -std=c++11
LXXFLAGS= 

# Include and Library paths
INCLUDES=
LIBS= 

# Inputs (grab all .cpp files, and assume change to .o)
SOURCES := $(wildcard *.cpp)
OBJS := $(patsubst %.c,%.o,$(wildcard *.cpp))

# Output to binary
BINF=../bin/deb$(ARCH)/flint++

# Test files
EXPECTED=tests/expected.txt
ACTUAL=tests/actual.txt

.PHONY: all tests core clean

all: core

core:
	$(CXX) $(CXXFLAGS) -o $(BINF) $(OBJS) $(LXXFLAGS) $(LIBS)
	rm -rf *o

tests:
	$(BINF) ./tests > $(ACTUAL)
	@cmp -s $(EXPECTED) $(ACTUAL); \
    	RETVAL=$$?; \
    	if [ $$RETVAL -eq 0 ]; then \
    	        echo "RESULTS ARE AS EXPECTED"; \
		rm $(ACTUAL); \
    	else \
    	       echo "RESULTS DIFFER, COMPARE $(ACTUAL) AND $(EXPECTED) TO SEE DIFFERENCES"; \
	fi

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -c $<

clean:
	rm -rf *o $(BINF)
