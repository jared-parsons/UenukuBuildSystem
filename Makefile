SOURCE_FILES=$(wildcard Source/*.cpp)
HEADER_FILES=$(wildcard Source/*.hpp)
BUILD_DIRECTORY=Build

$(BUILD_DIRECTORY)/build : $(SOURCE_FILES) $(HEADER_FILES) Makefile
	mkdir -p $(BUILD_DIRECTORY)
	clang++ -std=c++11 -Wall -Wextra -Werror -g -o $@ $(SOURCE_FILES)

.PHONY : clean
clean :
	rm -rf $(BUILD_DIRECTORY)
