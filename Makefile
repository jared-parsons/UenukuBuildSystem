build : Source/BuildSystem.cpp Makefile
	clang++ -std=c++11 -Wall -Wextra -Werror -g -o $@ $<
