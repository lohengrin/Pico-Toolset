#pragma once

#include <cstddef>
#include <string>

namespace pico_toolset {

// Minimal pull-style JSON reader for small documents. No DOM is built;
// values are copied on demand into caller-provided buffers.
//
// Navigation contract:
//  - beginObject/beginArray  : must be called at '{' / '['.
//  - nextObjectMember(key)   : inside an object; called after '{' or after a
//    member value. Returns false and consumes '}' at the end.
//  - nextArrayElement()      : inside an array; called after '[' or after a
//    value. Returns false and consumes ']' at the end.
//  - parseString/parseNumber : must be called at the member value position.
//  - skipValue()             : consumes the current value entirely.
class JsonReader {
public:
    JsonReader(const char* data, size_t len);
    bool valid() const;

    bool beginObject();
    bool beginArray();
    bool nextObjectMember(std::string& key);
    bool nextArrayElement();
    bool parseString(std::string& out);
    bool parseNumber(double& out);
    void skipValue();

private:
    void skipWs();
    bool match(char c);
    bool expect(char c);
    bool consumeLiteral(const char* lit);
    char peek() const;
    bool eof() const;
    void skipStringRaw();

    const char* data_;
    size_t len_;
    size_t pos_;
    bool valid_;
};

}  // namespace pico_toolset
