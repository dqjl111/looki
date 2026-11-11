#include "ljson.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <fstream>
#include "lkcore/ldebug.h"
#include "lkcore/io/lresource.h"

LK_NAMESPACE_BEGIN

static const int maxDepth = 200;

class LJsonValue {
protected:
    friend class LJson;
    friend class JsonInt;
    friend class JsonDouble;
    virtual LJson::Type type() const = 0;
    virtual bool equals(const LJsonValue * other) const = 0;
    virtual bool less(const LJsonValue * other) const = 0;
    virtual void dump(std::string &out) const = 0;
    virtual double numberValue() const;
    virtual int intValue() const;
    virtual bool boolValue() const;
    virtual const std::string &stringValue() const;
    virtual const LJson::array &arrayItems() const;
    virtual const LJson &operator[](size_t i) const;
    virtual const LJson::map &objectItems() const;
    virtual const LJson &operator[](const std::string &key) const;
    virtual ~LJsonValue() {}
};

/* Helper for representing null - just a do-nothing struct, plus comparison
 * operators so the helpers in JsonValue work. We can't use nullptr_t because
 * it may not be orderable.
 */
struct NullStruct {
    bool operator==(NullStruct) const { return true; }
    bool operator<(NullStruct) const { return false; }
};

/* * * * * * * * * * * * * * * * * * * *
 * Serialization
 */

namespace json {
static void dump(NullStruct, std::string &out) {
    out += "null";
}

static void dump(double value, std::string &out) {
    if (std::isfinite(value)) {
        char buf[32];
        snprintf(buf, sizeof buf, "%.17g", value);
        out += buf;
    } else {
        out += "null";
    }
}

static void dump(int value, std::string &out) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d", value);
    out += buf;
}

static void dump(bool value, std::string &out) {
    out += value ? "true" : "false";
}

static void dump(const std::string &value, std::string &out) {
    out += '"';
    for (size_t i = 0; i < value.length(); i++) {
        const char ch = value[i];
        if (ch == '\\') {
            out += "\\\\";
        } else if (ch == '"') {
            out += "\\\"";
        } else if (ch == '\b') {
            out += "\\b";
        } else if (ch == '\f') {
            out += "\\f";
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch == '\t') {
            out += "\\t";
        } else if (static_cast<uint8_t>(ch) <= 0x1f) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", ch);
            out += buf;
        } else if (static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(value[i+1]) == 0x80
                   && static_cast<uint8_t>(value[i+2]) == 0xa8) {
            out += "\\u2028";
            i += 2;
        } else if (static_cast<uint8_t>(ch) == 0xe2 && static_cast<uint8_t>(value[i+1]) == 0x80
                   && static_cast<uint8_t>(value[i+2]) == 0xa9) {
            out += "\\u2029";
            i += 2;
        } else {
            out += ch;
        }
    }
    out += '"';
}

static void dump(const LJson::array &values, std::string &out) {
    bool first = true;
    out += "[";
    for (const auto &value : values) {
        if (!first){
            out += ", ";
        }
        value.dump(out);
        first = false;
    }
    out += "]";
}

static void dump(const LJson::map &values, std::string &out) {
    bool first = true;
    out += "{";
    for (const auto &kv : values) {
        if (!first){
            out += ", ";
        }
        dump(kv.first, out);
        out += ": ";
        kv.second.dump(out);
        first = false;
    }
    out += "}";
}
}

void LJson::dump(std::string &out) const {
    m_ptr->dump(out);
}

/* * * * * * * * * * * * * * * * * * * *
 * Value wrappers
 */

template <LJson::Type tag, typename T>
class Value : public LJsonValue {
protected:

    // Constructors
    explicit Value(const T &value) : m_value(value) {}
    explicit Value(T &&value)      : m_value(std::move(value)) {}

    // Get type tag
    LJson::Type type() const override {
        return tag;
    }

    // Comparisons
    bool equals(const LJsonValue * other) const override {
        return m_value == static_cast<const Value<tag, T> *>(other)->m_value;
    }
    bool less(const LJsonValue * other) const override {
        return m_value < static_cast<const Value<tag, T> *>(other)->m_value;
    }

    const T m_value;
    void dump(std::string &out) const override { json::dump(m_value, out); }
};

class JsonDouble final : public Value<LJson::NUMBER, double> {
    double numberValue() const override { return m_value; }
    int intValue() const override { return static_cast<int>(m_value); }
    bool equals(const LJsonValue * other) const override { return m_value == other->numberValue(); }
    bool less(const LJsonValue * other)   const override { return m_value <  other->numberValue(); }
public:
    explicit JsonDouble(double value) : Value(value) {}
};

class JsonInt final : public Value<LJson::NUMBER, int> {
    double numberValue() const override { return m_value; }
    int intValue() const override { return m_value; }
    bool equals(const LJsonValue * other) const override { return m_value == other->numberValue(); }
    bool less(const LJsonValue * other)   const override { return m_value <  other->numberValue(); }
public:
    explicit JsonInt(int value) : Value(value) {}
};

class JsonBoolean final : public Value<LJson::BOOL, bool> {
    bool boolValue() const override { return m_value; }
public:
    explicit JsonBoolean(bool value) : Value(value) {}
};

class JsonString final : public Value<LJson::STRING, std::string> {
    const std::string &stringValue() const override { return m_value; }
public:
    explicit JsonString(const std::string &value) : Value(value) {}
    explicit JsonString(std::string &&value)      : Value(move(value)) {}
};

class JsonArray final : public Value<LJson::ARRAY, LJson::array> {
    const LJson::array &arrayItems() const override { return m_value; }
    const LJson & operator[](size_t i) const override;
public:
    explicit JsonArray(const LJson::array &value) : Value(value) {}
    explicit JsonArray(LJson::array &&value)      : Value(move(value)) {}
};

class JsonObject final : public Value<LJson::OBJECT, LJson::map> {
    const LJson::map &objectItems() const override { return m_value; }
    const LJson & operator[](const std::string &key) const override;
public:
    explicit JsonObject(const LJson::map &value) : Value(value) {}
    explicit JsonObject(LJson::map &&value)      : Value(move(value)) {}
};

class JsonNull final : public Value<LJson::NUL, NullStruct> {
public:
    JsonNull() : Value({}) {}
};

/* * * * * * * * * * * * * * * * * * * *
 * Static globals - static-init-safe
 */
struct Statics {
    const std::shared_ptr<LJsonValue> null = std::make_shared<JsonNull>();
    const std::shared_ptr<LJsonValue> t = std::make_shared<JsonBoolean>(true);
    const std::shared_ptr<LJsonValue> f = std::make_shared<JsonBoolean>(false);
    const std::string emptyString;
    const LJson::array emptyVector;
    const LJson::map emptyMap;
    Statics() {}
};

static const Statics & statics() {
    static const Statics s {};
    return s;
}

static const LJson & staticNull() {
    // This has to be separate, not in Statics, because Json() accesses statics().null.
    static const LJson jsonNull;
    return jsonNull;
}

/* * * * * * * * * * * * * * * * * * * *
 * Constructors
 */

LJson::LJson() noexcept                  : m_ptr(statics().null) {}
LJson::LJson(std::nullptr_t) noexcept    : m_ptr(statics().null) {}
LJson::LJson(double value)               : m_ptr(std::make_shared<JsonDouble>(value)) {}
LJson::LJson(int value)                  : m_ptr(std::make_shared<JsonInt>(value)) {}
LJson::LJson(bool value)                 : m_ptr(value ? statics().t : statics().f) {}
LJson::LJson(const std::string &value)        : m_ptr(std::make_shared<JsonString>(value)) {}
LJson::LJson(std::string &&value)             : m_ptr(std::make_shared<JsonString>(move(value))) {}
LJson::LJson(const char * value)         : m_ptr(std::make_shared<JsonString>(value)) {}
LJson::LJson(const LJson::array &values)  : m_ptr(std::make_shared<JsonArray>(values)) {}
LJson::LJson(LJson::array &&values)       : m_ptr(std::make_shared<JsonArray>(move(values))) {}
LJson::LJson(const LJson::map &values) : m_ptr(std::make_shared<JsonObject>(values)) {}
LJson::LJson(LJson::map &&values)      : m_ptr(std::make_shared<JsonObject>(move(values))) {}

/* * * * * * * * * * * * * * * * * * * *
 * Accessors
 */

LJson::Type LJson::type()                           const { return m_ptr->type();         }
double LJson::toDouble()                       const { return m_ptr->numberValue(); }
int LJson::toInt()                             const { return m_ptr->intValue();    }
bool LJson::toBool()                           const { return m_ptr->boolValue();   }
const std::string & LJson::toString()               const { return m_ptr->stringValue(); }
const LJson::array& LJson::toArray()          const { return m_ptr->arrayItems();  }
const LJson::map& LJson::toMap()    const { return m_ptr->objectItems(); }
const LJson & LJson::operator[] (size_t i)          const { return (*m_ptr)[i];           }
const LJson & LJson::operator[] (const std::string &key) const { return (*m_ptr)[key];         }

double                    LJsonValue::numberValue()              const { return 0; }
int                       LJsonValue::intValue()                 const { return 0; }
bool                      LJsonValue::boolValue()                const { return false; }
const std::string &            LJsonValue::stringValue()              const { return statics().emptyString; }
const LJson::array&      LJsonValue::arrayItems()               const { return statics().emptyVector; }
const LJson::map & LJsonValue::objectItems()              const { return statics().emptyMap; }
const LJson &              LJsonValue::operator[] (size_t)         const { return staticNull(); }
const LJson &              LJsonValue::operator[] (const std::string &) const { return staticNull(); }

const LJson & JsonObject::operator[] (const std::string &key) const {
    auto iter = m_value.find(key);
    return (iter == m_value.end()) ? staticNull() : iter->second;
}
const LJson & JsonArray::operator[] (size_t i) const {
    if (i >= m_value.size()) return staticNull();
    else return m_value[i];
}

/* * * * * * * * * * * * * * * * * * * *
 * Comparison
 */

bool LJson::operator== (const LJson &other) const {
    if (m_ptr == other.m_ptr){
        return true;
    }
    if (m_ptr->type() != other.m_ptr->type()){
        return false;
    }
    return m_ptr->equals(other.m_ptr.get());
}

bool LJson::operator< (const LJson &other) const {
    if (m_ptr == other.m_ptr){
        return false;
    }
    if (m_ptr->type() != other.m_ptr->type()){
        return m_ptr->type() < other.m_ptr->type();
    }
    return m_ptr->less(other.m_ptr.get());
}

/* * * * * * * * * * * * * * * * * * * *
 * Parsing
 */

/* esc(c)
 *
 * Format char c suitable for printing in an error message.
 */
static inline std::string esc(char c) {
    char buf[12];
    if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f) {
        snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
    } else {
        snprintf(buf, sizeof buf, "(%d)", c);
    }
    return std::string(buf);
}

static inline bool inRange(long x, long lower, long upper) {
    return (x >= lower && x <= upper);
}

namespace {
/* JsonParser
 *
 * Object that tracks all state of an in-progress parse.
 */
struct JsonParser final {

    /* State
     */
    const std::string &str;
    size_t i;
    std::string &err;
    bool failed;
    const JsonParse strategy;

    /* fail(msg, err_ret = Json())
     *
     * Mark this parse as failed.
     */
    LJson fail(std::string &&msg) {
        return fail(move(msg), LJson());
    }

    template <typename T>
    T fail(std::string &&msg, const T err_ret) {
        if (!failed){
            err = std::move(msg);
        }
        failed = true;
        return err_ret;
    }

    /* consume_whitespace()
     *
     * Advance until the current character is non-whitespace.
     */
    void consumeWhitespace() {
        while (str[i] == ' ' || str[i] == '\r' || str[i] == '\n' || str[i] == '\t'){
            i++;
        }
    }

    /* consume_comment()
     *
     * Advance comments (c-style inline and multiline).
     */
    bool consumeComment() {
        bool comment_found = false;
        if (str[i] == '/') {
            i++;
            if (i == str.size())
                return fail("unexpected end of input after start of comment", false);
            if (str[i] == '/') { // inline comment
                i++;
                // advance until next line, or end of input
                while (i < str.size() && str[i] != '\n') {
                    i++;
                }
                comment_found = true;
            }
            else if (str[i] == '*') { // multiline comment
                i++;
                if (i > str.size()-2){
                    return fail("unexpected end of input inside multi-line comment", false);
                }
                // advance until closing tokens
                while (!(str[i] == '*' && str[i+1] == '/')) {
                    i++;
                    if (i > str.size()-2)
                        return fail("unexpected end of input inside multi-line comment", false);
                }
                i += 2;
                comment_found = true;
            }
            else
                return fail("malformed comment", false);
        }
        return comment_found;
    }

    /* consume_garbage()
     *
     * Advance until the current character is non-whitespace and non-comment.
     */
    void consumeGarbage() {
        consumeWhitespace();
        if(strategy == JsonParse::COMMENTS) {
            bool commentFound = false;
            do {
                commentFound = consumeComment();
                if (failed){
                    return;
                }
                consumeWhitespace();
            }
            while(commentFound);
        }
    }

    /* get_next_token()
     *
     * Return the next non-whitespace character. If the end of the input is reached,
     * flag an error and return 0.
     */
    char getNextToken() {
        consumeGarbage();
        if (failed) return (char)0;
        if (i == str.size()){
            return fail("unexpected end of input", (char)0);
        }
        return str[i++];
    }

    /* encode_utf8(pt, out)
     *
     * Encode pt as UTF-8 and add it to out.
     */
    void encodeUtf8(long pt, std::string & out) {
        if (pt < 0){
            return;
        }

        if (pt < 0x80) {
            out += static_cast<char>(pt);
        } else if (pt < 0x800) {
            out += static_cast<char>((pt >> 6) | 0xC0);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        } else if (pt < 0x10000) {
            out += static_cast<char>((pt >> 12) | 0xE0);
            out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        } else {
            out += static_cast<char>((pt >> 18) | 0xF0);
            out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
            out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        }
    }

    /* parse_string()
     *
     * Parse a string, starting at the current position.
     */
    std::string parseString() {
        std::string out;
        long lastEscapedCodepoint = -1;
        while (true) {
            if (i == str.size())
                return fail("unexpected end of input in string", "");

            char ch = str[i++];

            if (ch == '"') {
                encodeUtf8(lastEscapedCodepoint, out);
                return out;
            }

            if (inRange(ch, 0, 0x1f))
                return fail("unescaped " + esc(ch) + " in string", "");

            // The usual case: non-escaped characters
            if (ch != '\\') {
                encodeUtf8(lastEscapedCodepoint, out);
                lastEscapedCodepoint = -1;
                out += ch;
                continue;
            }

            // Handle escapes
            if (i == str.size())
                return fail("unexpected end of input in string", "");

            ch = str[i++];

            if (ch == 'u') {
                // Extract 4-byte escape sequence
                std::string esc = str.substr(i, 4);
                // Explicitly check length of the substring. The following loop
                // relies on std::string returning the terminating NUL when
                // accessing str[length]. Checking here reduces brittleness.
                if (esc.length() < 4) {
                    return fail("bad \\u escape: " + esc, "");
                }
                for (size_t j = 0; j < 4; j++) {
                    if (!inRange(esc[j], 'a', 'f') && !inRange(esc[j], 'A', 'F')
                        && !inRange(esc[j], '0', '9')){
                        return fail("bad \\u escape: " + esc, "");
                    }
                }

                long codepoint = strtol(esc.data(), nullptr, 16);

                // JSON specifies that characters outside the BMP shall be encoded as a pair
                // of 4-hex-digit \u escapes encoding their surrogate pair components. Check
                // whether we're in the middle of such a beast: the previous codepoint was an
                // escaped lead (high) surrogate, and this is a trail (low) surrogate.
                if (inRange(lastEscapedCodepoint, 0xD800, 0xDBFF)
                    && inRange(codepoint, 0xDC00, 0xDFFF)) {
                    // Reassemble the two surrogate pairs into one astral-plane character, per
                    // the UTF-16 algorithm.
                    encodeUtf8((((lastEscapedCodepoint - 0xD800) << 10)
                                 | (codepoint - 0xDC00)) + 0x10000, out);
                    lastEscapedCodepoint = -1;
                } else {
                    encodeUtf8(lastEscapedCodepoint, out);
                    lastEscapedCodepoint = codepoint;
                }

                i += 4;
                continue;
            }

            encodeUtf8(lastEscapedCodepoint, out);
            lastEscapedCodepoint = -1;

            if (ch == 'b') {
                out += '\b';
            } else if (ch == 'f') {
                out += '\f';
            } else if (ch == 'n') {
                out += '\n';
            } else if (ch == 'r') {
                out += '\r';
            } else if (ch == 't') {
                out += '\t';
            } else if (ch == '"' || ch == '\\' || ch == '/') {
                out += ch;
            } else {
                return fail("invalid escape character " + esc(ch), "");
            }
        }
    }

    /* parse_number()
     *
     * Parse a double.
     */
    LJson parseNumber() {
        size_t startPos = i;

        if (str[i] == '-'){
            i++;
        }

        // Integer part
        if (str[i] == '0') {
            i++;
            if (inRange(str[i], '0', '9'))
                return fail("leading 0s not permitted in numbers");
        } else if (inRange(str[i], '1', '9')) {
            i++;
            while (inRange(str[i], '0', '9')){
                i++;
            }
        } else {
            return fail("invalid " + esc(str[i]) + " in number");
        }

        if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
            && (i - startPos) <= static_cast<size_t>(std::numeric_limits<int>::digits10)) {
            return std::atoi(str.c_str() + startPos);
        }

        // Decimal part
        if (str[i] == '.') {
            i++;
            if (!inRange(str[i], '0', '9')){
                return fail("at least one digit required in fractional part");
            }

            while (inRange(str[i], '0', '9')){
                i++;
            }
        }

        // Exponent part
        if (str[i] == 'e' || str[i] == 'E') {
            i++;

            if (str[i] == '+' || str[i] == '-'){
                i++;
            }

            if (!inRange(str[i], '0', '9')){
                return fail("at least one digit required in exponent");
            }

            while (inRange(str[i], '0', '9')){
                i++;
            }
        }

        return std::strtod(str.c_str() + startPos, nullptr);
    }

    /* expect(str, res)
     *
     * Expect that 'str' starts at the character that was just read. If it does, advance
     * the input and return res. If not, flag an error.
     */
    LJson expect(const std::string &expected, LJson res) {
        assert(i != 0);
        i--;
        if (str.compare(i, expected.length(), expected) == 0) {
            i += expected.length();
            return res;
        } else {
            return fail("parse error: expected " + expected + ", got " + str.substr(i, expected.length()));
        }
    }

    /* parse_json()
     *
     * Parse a JSON object.
     */
    LJson parseJson(int depth) {
        if (depth > maxDepth) {
            return fail("exceeded maximum nesting depth");
        }

        char ch = getNextToken();
        if (failed){
            return LJson();
        }

        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            i--;
            return parseNumber();
        }

        if (ch == 't')
            return expect("true", true);

        if (ch == 'f')
            return expect("false", false);

        if (ch == 'n')
            return expect("null", LJson());

        if (ch == '"')
            return parseString();

        if (ch == '{') {
            LJson::map data;
            ch = getNextToken();
            if (ch == '}')
                return data;

            while (1) {
                if (ch != '"'){
                    return fail("expected '\"' in object, got " + esc(ch));
                }

                std::string key = parseString();
                if (failed)
                    return LJson();

                ch = getNextToken();
                if (ch != ':')
                    return fail("expected ':' in object, got " + esc(ch));

                data[std::move(key)] = parseJson(depth + 1);
                if (failed)
                    return LJson();

                ch = getNextToken();
                if (ch == '}')
                    break;
                if (ch != ',')
                    return fail("expected ',' in object, got " + esc(ch));

                ch = getNextToken();
            }
            return data;
        }

        if (ch == '[') {
            LJson::array data;
            ch = getNextToken();
            if (ch == ']'){
                return data;
            }

            while (1) {
                i--;
                data.push_back(parseJson(depth + 1));
                if (failed){
                    return LJson();
                }

                ch = getNextToken();
                if (ch == ']'){
                    break;
                }
                if (ch != ','){
                    return fail("expected ',' in list, got " + esc(ch));
                }

                ch = getNextToken();
                (void)ch;
            }
            return data;
        }

        return fail("expected value, got " + esc(ch));
    }
};
}//namespace {

LJson LJson::parse(const std::string &in, std::string &err, JsonParse strategy) {
    JsonParser parser { in, 0, err, false, strategy };
    LJson result = parser.parseJson(0);

    // Check for any trailing garbage
    parser.consumeGarbage();
    if (parser.failed){
        return LJson();
    }
    if (parser.i != in.size()){
        return parser.fail("unexpected trailing " + esc(in[parser.i]));
    }
    return result;
}

LJson LJson::parseFile(const std::string &filePath, JsonParse strategy)
{
    if(filePath.starts_with(":")){
        LResource resource(filePath);
        std::vector<uint8_t> data = resource.uncompressedData();
        if(!data.empty()){
            std::string fileContent((char*)(data.data()), data.size());
            std::string  err;
            LJson result = parse(fileContent, err, strategy);
            if(err.empty()){
                lWarn() << err;
            }
            return result;
        }else{
            lWarn() << "Failed to open resource: " << filePath;
            return LJson();
        }
    }else{
        std::ifstream file(filePath);

        if (!file.is_open()) {
            lWarn() << "Failed to open file: " << filePath;
            return LJson();
        }

        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::string err;
        LJson result = parse(fileContent, err, strategy);
        if(err.empty()){
            lWarn() << err;
        }
        return result;
    }
}

LJson::array LJson::parseMulti(const std::string &in,
                               std::string::size_type &parser_stop_pos,
                               std::string &err,
                               JsonParse strategy) {
    JsonParser parser { in, 0, err, false, strategy };
    parser_stop_pos = 0;
    std::vector<LJson> json_vec;
    while (parser.i != in.size() && !parser.failed) {
        json_vec.push_back(parser.parseJson(0));
        if (parser.failed){
            break;
        }

        // Check for another object
        parser.consumeGarbage();
        if (parser.failed){
            break;
        }
        parser_stop_pos = parser.i;
    }
    return json_vec;
}

/* * * * * * * * * * * * * * * * * * * *
 * Shape-checking
 */

bool LJson::hasShape(const shape & types, std::string & err) const {
    if (!isObject()) {
        err = "expected JSON object, got " + dump();
        return false;
    }

    for (auto & item : types) {
        if ((*this)[item.first].type() != item.second) {
            err = "bad type for " + item.first + " in " + dump();
            return false;
        }
    }

    return true;
}

LK_NAMESPACE_END
