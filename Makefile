SOURCE_FILES=$(wildcard Source/*.cpp)
HEADER_FILES=$(wildcard Source/*.hpp)

build : $(SOURCE_FILES) $(HEADER_FILES) Makefile
	clang++ -std=c++11 -Wall -Wextra -Werror -g -o $@ $(SOURCE_FILES)

.PHONY : clean
clean :
	rm build
