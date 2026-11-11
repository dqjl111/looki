#ifndef LJSON_H
#define LJSON_H
#include "lkcore/lookiconfig.h"
#include <string>
#include <map>
#include <vector>

LK_NAMESPACE_BEGIN

enum JsonParse {
    STANDARD, COMMENTS
};

class LJsonValue;

class LKCOREDEF LJson{
public:
    // Types
    enum Type {
        NUL, NUMBER, BOOL, STRING, ARRAY, OBJECT
    };

    // Array and object typedefs
    typedef std::vector<LJson> array;
    typedef std::map<std::string, LJson> map;

    // Constructors for the various types of JSON value.
    LJson() noexcept;                // NUL
    LJson(std::nullptr_t) noexcept;  // NUL
    LJson(double value);             // NUMBER
    LJson(int value);                // NUMBER
    LJson(bool value);               // BOOL
    LJson(const std::string &value); // STRING
    LJson(std::string &&value);      // STRING
    LJson(const char * value);       // STRING
    LJson(const LJson::array &values);      // ARRAY
    LJson(LJson::array &&values);           // ARRAY
    LJson(const LJson::map &values);     // OBJECT
    LJson(LJson::map &&values);          // OBJECT

    // Implicit constructor: anything with a to_json() function.
    template <class T, class = decltype(&T::toJson)>
    LJson(const T & t) : LJson(t.toJson()) {}

    // Implicit constructor: map-like objects (std::map, std::unordered_map, etc)
    template <class M, typename std::enable_if<
                          std::is_constructible<std::string, decltype(std::declval<M>().begin()->first)>::value
                              && std::is_constructible<LJson, decltype(std::declval<M>().begin()->second)>::value,
                          int>::type = 0>
    LJson(const M & m) : LJson(map(m.begin(), m.end())) {}

    // Implicit constructor: vector-like objects (std::list, std::vector, std::set, etc)
    template <class V, typename std::enable_if<
                          std::is_constructible<LJson, decltype(*std::declval<V>().begin())>::value,
                          int>::type = 0>
    LJson(const V & v) : LJson(array(v.begin(), v.end())) {}

    // This prevents Json(some_pointer) from accidentally producing a bool. Use
    // Json(bool(some_pointer)) if that behavior is desired.
    LJson(void *) = delete;

    // Accessors
    Type type() const;

    bool isNull()   const { return type() == NUL; }
    bool isNumber() const { return type() == NUMBER; }
    bool isBool()   const { return type() == BOOL; }
    bool isString() const { return type() == STRING; }
    bool isArray()  const { return type() == ARRAY; }
    bool isObject() const { return type() == OBJECT; }

    // Return the enclosed value if this is a number, 0 otherwise. Note that json11 does not
    // distinguish between integer and non-integer numbers - number_value() and int_value()
    // can both be applied to a NUMBER-typed object.
    double toDouble() const;
    int toInt() const;

    // Return the enclosed value if this is a boolean, false otherwise.
    bool toBool() const;
    // Return the enclosed string if this is a string, "" otherwise.
    const std::string &toString() const;
    // Return the enclosed std::vector if this is an array, or an empty vector otherwise.
    const LJson::array &toArray() const;
    // Return the enclosed std::map if this is an object, or an empty map otherwise.
    const LJson::map &toMap() const;

    // Return a reference to arr[i] if this is an array, Json() otherwise.
    const LJson & operator[](size_t i) const;
    // Return a reference to obj[key] if this is an object, Json() otherwise.
    const LJson & operator[](const std::string &key) const;

    // Serialize.
    void dump(std::string &out) const;
    std::string dump() const {
        std::string out;
        dump(out);
        return out;
    }

    // Parse. If parse fails, return Json() and assign an error message to err.
    static LJson parse(const std::string & in,
                      std::string & err,
                      JsonParse strategy = JsonParse::STANDARD);
    static LJson parse(const char * in,
                      std::string & err,
                      JsonParse strategy = JsonParse::STANDARD) {
        if (in) {
            return parse(std::string(in), err, strategy);
        } else {
            err = "null input";
            return nullptr;
        }
    }
    static LJson parseFile(const std::string& filePath,
                           JsonParse strategy = JsonParse::STANDARD);
    // Parse multiple objects, concatenated or separated by whitespace
    static LJson::array parseMulti(
        const std::string & in,
        std::string::size_type & parser_stop_pos,
        std::string & err,
        JsonParse strategy = JsonParse::STANDARD);

    static inline LJson::array parseMulti(
        const std::string & in,
        std::string & err,
        JsonParse strategy = JsonParse::STANDARD) {
        std::string::size_type parser_stop_pos;
        return parseMulti(in, parser_stop_pos, err, strategy);
    }

    bool operator== (const LJson &rhs) const;
    bool operator<  (const LJson &rhs) const;
    bool operator!= (const LJson &rhs) const { return !(*this == rhs); }
    bool operator<= (const LJson &rhs) const { return !(rhs < *this); }
    bool operator>  (const LJson &rhs) const { return  (rhs < *this); }
    bool operator>= (const LJson &rhs) const { return !(*this < rhs); }

    /* has_shape(types, err)
     *
     * Return true if this is a JSON object and, for each item in types, has a field of
     * the given type. If not, return false and set err to a descriptive message.
     */
    typedef std::initializer_list<std::pair<std::string, Type>> shape;
    bool hasShape(const shape & types, std::string & err) const;

private:
    std::shared_ptr<LJsonValue> m_ptr;
};

LK_NAMESPACE_END
#endif // LJSON_H
