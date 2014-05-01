CXX=g++

# ARCH=32
ARCH=64

# Compiler and Linker flags
CXXFLAGS= -O3 -m$(ARCH) -std=c++11
LXXFLAGS= 

# Inlude and Library paths
INCLUDES=
LIBS= 

# Inputs (grab all .cpp files, and assume change to .o)
SOURCES := $(wildcard *.cpp)
OBJS := $(patsubst %.c,%.o,$(wildcard *.cpp))

# Ouput to binary
BINF=../bin/deb$(ARCH)/flint++

all: core

core: 
	$(CXX) $(CXXFLAGS) -o $(BINF) $(OBJS) $(LXXFLAGS) $(LIBS)
	rm -rf *o

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OBJS) -c $<

clean:
	rm -rf *o $(BINF)
