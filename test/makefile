CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O3
INCLUDES = -I "../src"

SRCDIR = ../src

.phony: clean all test

all: test

test: tests.cpp $(SRCDIR)/lex.cpp $(SRCDIR)/lex.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ tests.cpp $(SRCDIR)/lex.cpp
	
runtest : test
	@./test && : || { echo ">>> Test 1 failed!"; exit 1; }
	@echo "      _"
	@echo "     /(|"
	@echo "    (  :"
	@echo "   __\  \  _____"
	@echo " (____)  '|"
	@echo "(____)|   |"
	@echo " (____).__|"
	@echo "  (___)__.|_____"
	@echo ""
# https://asciiart.website/index.php?art=people/body%20parts/hand%20gestures

clean:
	rm test
