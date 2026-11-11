#ifndef LCBOR_H
#define LCBOR_H
#include "lkcore/lookiconfig.h"

#include <iostream>
#include <map>
#include <stdint.h>
#include <vector>
#include <initializer_list>

LK_NAMESPACE_BEGIN

class LKCOREDEF LCbor
{
public:
    enum Type {
        TypeUnsigned,
        TypeNegative,
        TypeBinary,
        TypeString,
        TypeArray,
        TypeMap,
        TypeTagged,
        TypeSimple,
        TypeFloat
    };

    typedef std::vector <unsigned char> binary;
    typedef std::string string;
    typedef std::vector <LCbor> array;
    typedef std::map <LCbor, LCbor> map;
    enum simple {
        SIMPLE_FALSE = 20,
        SIMPLE_TRUE,
        SIMPLE_NULL,
        SIMPLE_UNDEFINED,
        null = SIMPLE_NULL,
        undefined = SIMPLE_UNDEFINED
    };

    LCbor (unsigned short value);
    LCbor (unsigned value);
    LCbor (unsigned long value);
    LCbor (unsigned long long value);
    LCbor (short value);
    LCbor (int value);
    LCbor (long value);
    LCbor (long long value);
    LCbor (const LCbor::binary &value);
    LCbor (const LCbor::string &value);
    LCbor (const char *value);
    LCbor (const LCbor::array &value);
    LCbor (const LCbor::map &value);
    LCbor (LCbor::simple value = LCbor::SIMPLE_UNDEFINED);
    LCbor (bool value);
    LCbor (float value);
    LCbor (double value);
    LCbor(std::nullptr_t);
    LCbor(const LCbor& cbor) =default;
    LCbor(LCbor&& cbor) =default;

    static LCbor tagged (unsigned long long tag, const LCbor &value);

    bool isUnsigned () const;
    bool isSigned () const;
    bool isInt () const;
    bool isBinary () const;
    bool isString () const;
    bool isArray () const;
    bool isMap () const;
    bool isTagged () const;
    bool isSimple () const;
    bool isBool () const;
    bool isNull () const;
    bool isUndefined () const;
    bool isFloat () const;
    bool isNumber () const;
    bool isValid() const;


    uint64_t toUInt64 () const;
    int64_t toInt64 () const;
    LCbor::binary toBinary () const;
    LCbor::string toString () const;
    LCbor::array toArray () const;
    LCbor::map toMap () const;
    LCbor::simple toSimple () const;
    bool toBool () const;
    double toDouble () const;

    operator unsigned short () const;
    operator unsigned () const;
    operator unsigned long () const;
    operator unsigned long long () const;
    operator short () const;
    operator int () const;
    operator long () const;
    operator long long () const;
    operator LCbor::binary () const;
    operator LCbor::string () const;
    operator LCbor::array () const;
    operator LCbor::map () const;
    operator LCbor::simple () const;
    operator bool () const;
    operator double () const;

    uint64_t tag () const;
    LCbor child () const;

    LCbor::Type type () const;

    bool read (std::istream &in);
    void write (std::ostream &out) const;

    bool read (std::string &filePath);
    bool write (std::string &filePath) const;


    static bool validate (const LCbor::binary &in);
    static LCbor decode (const LCbor::binary &in);
    static LCbor::binary encode (const LCbor &in);
    static LCbor::string debug (const LCbor &in);

    bool operator ==(const LCbor &other) const;
    bool operator !=(const LCbor &other) const;
    bool operator <(const LCbor &other) const;
    LCbor &operator =(const LCbor &obj) = default;
private:
    LCbor::Type m_type;
    union {
        uint64_t m_value;
        double m_float;
    };
    LCbor::binary m_binary;
    LCbor::string m_string;
    LCbor::array m_array;
    LCbor::map m_map;
};

LK_NAMESPACE_END
#endif // LCBOR_H
