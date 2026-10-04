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
//  - skipValue()             : consumes the current value entirely (nesting deeper
//    than kMaxSkipDepth invalidates the reader).
//  - peekType()              : kind of the value at the current position, without
//    consuming it -- lets a caller ignore wrongly-typed values (skipValue) instead of
//    desynchronising the reader.
//
// Once valid() is false (malformed or truncated input) every navigation call fails
// quickly, so a loop `while (r.nextArrayElement()) ...` always terminates.
class JsonReader {
public:
    enum class Type { Number, String, Array, Object, Null, Bool, Invalid };
    static constexpr int kMaxSkipDepth = 8;

    JsonReader(const char* data, size_t len);
    bool valid() const;

    bool beginObject();
    bool beginArray();
    bool nextObjectMember(std::string& key);
    bool nextArrayElement();
    bool parseString(std::string& out);
    bool parseNumber(double& out);
    void skipValue();
    Type peekType();

private:
    void skipValueAt(int depth);

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
