#include "JSONTokenizer.hpp"
#include <stdexcept>

JSONTokenizer::JSONTokenizer(std::istream &input)
	: _input(input)
{
}

namespace {
	bool IsWhitespace(int character, std::size_t &lineNumber) {
		if (character == '\n') {
			++lineNumber;
			return true;
		}
		return character == ' ' || character == '\t' || character == '\r';
	}

	void SkipWhitespace(std::istream &input, std::size_t &lineNumber) {
		int character;
		while (character = input.peek(), character != EOF) {
			if (IsWhitespace(character, lineNumber)) {
				input.get();
			} else {
				return;
			}
		}
	}
}

bool JSONTokenizer::ReadToken(JSONToken &token) {
	SkipWhitespace(_input, _lineNumber);

	int character = _input.get();
	switch (character) {
		case EOF:
			return false;
		case '{':
			token = JSONToken(JSONTokenType::StartObject, _lineNumber);
			return true;
		case '}':
			token = JSONToken(JSONTokenType::EndObject, _lineNumber);
			return true;
		case ',':
			token = JSONToken(JSONTokenType::ListSeparator, _lineNumber);
			return true;
		case ':':
			token = JSONToken(JSONTokenType::PairSeparator, _lineNumber);
			return true;
		case '[':
			token = JSONToken(JSONTokenType::StartList, _lineNumber);
			return true;
		case ']':
			token = JSONToken(JSONTokenType::EndList, _lineNumber);
			return true;
		case '"':
		{
			std::string value;
			int character;
			while (character = _input.get(), character != EOF) {
				// thang : escape sequences...
				if (character == '"') {
					token = JSONToken(JSONTokenType::String, std::move(value), _lineNumber);
					return true;
				} else {
					value.push_back(character);
				}
			}
			throw std::runtime_error("Unexpected end of file in string.");
		}
		default:
			throw std::runtime_error("Unknown character.");
	}
}

/*
	String,
	Number,
	True,
	False,
	Null,
*/

JSONToken JSONTokenizer::ReadRequiredToken() {
	JSONToken result;
	if (!ReadToken(result)) {
		throw std::runtime_error("Required token not found.");
	}
	return result;
}

JSONToken JSONTokenizer::ReadRequiredToken(JSONTokenType type) {
	JSONToken result = ReadRequiredToken();
	if (result.GetType() != type) {
		throw std::runtime_error("Token is of wrong type.");
	}
	return result;
}
