#we always set the c++11 and optimization flags in addition
#to anything the make invokation has set. this makes it possible
#for users to specify their own flags with "make CXXFLAGS=.... "
EXTRACXXFLAGS := -O3 -std=c++11


# Inputs (grab all .cpp files, and assume change to .o)
SOURCES := $(wildcard *.cpp)
OBJS := $(SOURCES:.cpp=.o)

# Output to binary
BINF:=flint++

# Test files
EXPECTED=tests/expected.txt
ACTUAL=tests/actual.txt

.PHONY: all tests

all: $(BINF)


$(BINF): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(BINF) $(OBJS)

tests: $(BINF)
	./$(BINF) ./tests > $(ACTUAL)
	@cmp -s $(EXPECTED) $(ACTUAL); \
	RETVAL=$$?; \
	  if [ $$RETVAL -eq 0 ]; then \
	   echo "RESULTS ARE AS EXPECTED"; \
	   rm $(ACTUAL); \
	else \
	 echo "RESULTS DIFFER, COMPARE $(ACTUAL) AND $(EXPECTED) TO SEE DIFFERENCES"; \
	fi

.cpp.o: $<
	$(CXX) $(EXTRACXXFLAGS) $(CXXFLAGS) -o $@  -c $<

clean:
	-rm -rf *o $(BINF)
