#ifndef GUARD_InputStream_hpp
#define GUARD_InputStream_hpp

#include <stdio.h>

class InputStream {
public:
	FILE *file = nullptr;

	int get() {
		return fgetc(file); // thang : error check?
	}

	int peek() {
		const int result = fgetc(file); // thang : error check?
		ungetc(result, file); // thang : error check?
		return result;
	}

	void Close() {
		if (file) {
			fclose(file); // thang : error check?
			file = nullptr;
		}
	}

	~InputStream() {
		try {
			Close();
		} catch (...) {
		}
	}
};

#endif
