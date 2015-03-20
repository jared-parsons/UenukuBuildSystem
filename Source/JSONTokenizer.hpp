#ifndef GUARD_JSONTokenizer_hpp
#define GUARD_JSONTokenizer_hpp

#include <string>
#include "InputStream.hpp"

enum class JSONTokenType {
	StartObject,
	EndObject,
	ListSeparator,
	PairSeparator,
	StartList,
	EndList,
	String,
	Number,
	True,
	False,
	Null,
};

class JSONToken final {
	JSONTokenType _type;
	std::string _value;
	std::size_t _lineNumber;

public:
	JSONToken() {
		_type = JSONTokenType::Null;
		_lineNumber = 0;
	}

	explicit JSONToken(JSONTokenType type, std::size_t lineNumber) {
		_type = type;
		_lineNumber = lineNumber;
	}

	explicit JSONToken(JSONTokenType type, std::string value, std::size_t lineNumber) {
		_type = type;
		_value = std::move(value);
		_lineNumber = lineNumber;
	}

	JSONTokenType GetType() const {
		return _type;
	}

	std::string GetValue() const {
		return _value;
	}

	std::size_t GetLineNumber() const {
		return _lineNumber;
	}
};

class JSONTokenizer final {
	InputStream &_input;
	std::size_t _lineNumber = 1;

public:
	explicit JSONTokenizer(InputStream &input);
	bool ReadToken(JSONToken &token);
	JSONToken ReadRequiredToken();
	JSONToken ReadRequiredToken(JSONTokenType type);
};

#endif
