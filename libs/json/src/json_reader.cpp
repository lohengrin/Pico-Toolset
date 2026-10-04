#include "pico_toolset/json_reader.h"

#include <cstdlib>

namespace pico_toolset {

namespace {
inline bool isWs(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
inline bool isDelim(char c) {
    return c == ',' || c == '}' || c == ']';
}
}  // namespace

JsonReader::JsonReader(const char* data, size_t len)
    : data_(data), len_(len), pos_(0), valid_(true) {}

bool JsonReader::valid() const { return valid_; }

char JsonReader::peek() const {
    return eof() ? '\0' : data_[pos_];
}

bool JsonReader::eof() const { return pos_ >= len_; }

void JsonReader::skipWs() {
    while (!eof() && isWs(peek())) pos_++;
}

bool JsonReader::match(char c) {
    if (!eof() && data_[pos_] == c) {
        pos_++;
        return true;
    }
    return false;
}

bool JsonReader::consumeLiteral(const char* lit) {
    size_t i = 0;
    while (lit[i] != '\0') {
        if (eof() || data_[pos_] != lit[i]) return false;
        pos_++;
        i++;
    }
    return true;
}

bool JsonReader::expect(char c) {
    skipWs();
    if (match(c)) return true;
    valid_ = false;
    return false;
}

bool JsonReader::beginObject() {
    skipWs();
    return expect('{');
}

bool JsonReader::beginArray() {
    skipWs();
    return expect('[');
}

bool JsonReader::nextObjectMember(std::string& key) {
    skipWs();
    if (match('}')) return false;  // end of object
    match(',');                    // optional separator
    if (!parseString(key)) {
        valid_ = false;
        return false;
    }
    return expect(':');
}

bool JsonReader::nextArrayElement() {
    skipWs();
    if (match(']')) return false;  // end of array
    match(',');
    return true;
}

bool JsonReader::parseString(std::string& out) {
    skipWs();
    if (!match('"')) {
        // JSON null is a legal value for nullable string fields: consume it
        // and report "no value" without failing the reader.
        if (consumeLiteral("null")) return false;
        valid_ = false;
        return false;
    }
    out.clear();
    while (!eof()) {
        const char c = peek();
        pos_++;
        if (c == '"') return true;
        if (c == '\\') {
            if (eof()) break;
            const char e = peek();
            pos_++;
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '/': out += '/'; break;
                case '\\': out += '\\'; break;
                case '"': out += '"'; break;
                case 'u':
                    // Skip 4 hex digits; our fields are plain ASCII.
                    for (int i = 0; i < 4 && !eof(); i++) pos_++;
                    break;
                default:
                    valid_ = false;
                    return false;
            }
        } else {
            out += c;
        }
    }
    valid_ = false;
    return false;
}

bool JsonReader::parseNumber(double& out) {
    skipWs();
    // JSON null is a legal value for nullable numeric fields (lat/lon/alt...).
    if (consumeLiteral("null")) return false;
    char buf[40];
    size_t n = 0;
    while (!eof() && !isWs(peek()) && !isDelim(peek()) && n < sizeof(buf) - 1) {
        buf[n++] = peek();
        pos_++;
    }
    buf[n] = '\0';
    if (n == 0) {
        valid_ = false;
        return false;
    }
    char* end = nullptr;
    out = std::strtod(buf, &end);
    if (end == buf) {
        valid_ = false;
        return false;
    }
    return true;
}

void JsonReader::skipStringRaw() {
    if (!match('"')) {
        valid_ = false;
        return;
    }
    while (!eof()) {
        const char c = peek();
        pos_++;
        if (c == '"') return;
        if (c == '\\' && !eof()) pos_++;  // skip escaped char
    }
    valid_ = false;
}

void JsonReader::skipValue() {
    skipWs();
    if (eof()) {
        valid_ = false;
        return;
    }
    switch (peek()) {
        case '"':
            skipStringRaw();
            return;
        case '{': {
            pos_++;
            std::string key;
            while (nextObjectMember(key)) skipValue();
            return;
        }
        case '[': {
            pos_++;
            while (nextArrayElement()) skipValue();
            return;
        }
        default:
            // number / true / false / null literal
            while (!eof() && !isDelim(peek())) pos_++;
            return;
    }
}

}  // namespace pico_toolset
