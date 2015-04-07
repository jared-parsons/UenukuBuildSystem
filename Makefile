SOURCE_FILES=$(wildcard Source/*.cpp)
HEADER_FILES=$(wildcard Source/*.hpp)
BUILD_DIRECTORY=Build
BINARY_NAME=ubuild

$(BUILD_DIRECTORY)/$(BINARY_NAME) : $(SOURCE_FILES) $(HEADER_FILES) Makefile
	mkdir -p $(BUILD_DIRECTORY)
	clang++ -std=c++11 -Wall -Wextra -Wundef -Werror -g -o $@ $(SOURCE_FILES)

.PHONY : install
install :
	install -d $(DESTDIR)/usr/bin
	install -m755 Build/ubuild $(DESTDIR)/usr/bin

.PHONY : clean
clean :
	rm -rf $(BUILD_DIRECTORY)
