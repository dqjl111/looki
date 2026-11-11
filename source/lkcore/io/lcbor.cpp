#include "lcbor.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <spanstream>
#include "lkcore/ldebug.h"
#include "lkcore/io/lresource.h"

LK_NAMESPACE_BEGIN


LCbor::LCbor (unsigned short value) : m_type (LCbor::TypeUnsigned), m_value (value) {
}
LCbor::LCbor (unsigned value) : m_type (LCbor::TypeUnsigned), m_value (value) {
}
LCbor::LCbor (unsigned long value) : m_type (LCbor::TypeUnsigned), m_value (value) {
}
LCbor::LCbor (unsigned long long value) : m_type (LCbor::TypeUnsigned), m_value (value) {
}
LCbor::LCbor (short value) : m_type (value < 0 ? LCbor::TypeNegative : LCbor::TypeUnsigned), m_value (value < 0 ? -1 - value : value) {
}
LCbor::LCbor (int value) : m_type (value < 0 ? LCbor::TypeNegative : LCbor::TypeUnsigned), m_value (value < 0 ? -1 - value : value) {
}
LCbor::LCbor (long value) : m_type (value < 0 ? LCbor::TypeNegative : LCbor::TypeUnsigned), m_value (value < 0 ? -1 - value : value) {
}
LCbor::LCbor (long long value) : m_type (value < 0 ? LCbor::TypeNegative : LCbor::TypeUnsigned), m_value (value < 0 ? -1 - value : value) {
}
LCbor::LCbor (const LCbor::binary &value) : m_type (LCbor::TypeBinary), m_binary (value) {
}
LCbor::LCbor (const LCbor::string &value) : m_type (LCbor::TypeString), m_string (value) {
}
LCbor::LCbor (const char *value) : m_type (LCbor::TypeString), m_string (value) {
}
LCbor::LCbor (const LCbor::array &value) : m_type (LCbor::TypeArray), m_array (value) {
}
LCbor::LCbor (const LCbor::map &value) : m_type (LCbor::TypeMap), m_map (value) {
}
LCbor LCbor::tagged (unsigned long long tag, const LCbor &value) {
    LCbor result;
    result.m_type = LCbor::TypeTagged;
    result.m_value = tag;
    result.m_array = LCbor::array (1, value);
    return result;
}
LCbor::LCbor (LCbor::simple value) : m_type (LCbor::TypeSimple), m_value (value & 255) {
}
LCbor::LCbor (bool value) : m_type (LCbor::TypeSimple), m_value (value ? LCbor::SIMPLE_TRUE : LCbor::SIMPLE_FALSE) {
}
LCbor::LCbor (float value) : m_type (LCbor::TypeFloat), m_float (value) {
}
LCbor::LCbor (double value) : m_type (LCbor::TypeFloat), m_float (value) {
}

LCbor::LCbor(std::nullptr_t) : m_type(LCbor::TypeSimple), m_value(LCbor::SIMPLE_NULL) {
}




bool LCbor::isUnsigned () const {
    return this->m_type == LCbor::TypeUnsigned;
}
bool LCbor::isSigned () const {
    return (this->m_type == LCbor::TypeUnsigned || this->m_type == LCbor::TypeNegative) && (this->m_value >> 63) == 0;
}
bool LCbor::isInt () const {
    return this->m_type == LCbor::TypeUnsigned || this->m_type == LCbor::TypeNegative;
}
bool LCbor::isBinary () const {
    return this->m_type == LCbor::TypeBinary;
}
bool LCbor::isString () const {
    return this->m_type == LCbor::TypeString;
}
bool LCbor::isArray () const {
    return this->m_type == LCbor::TypeArray;
}
bool LCbor::isMap () const {
    return this->m_type == LCbor::TypeMap;
}
bool LCbor::isTagged () const {
    return this->m_type == LCbor::TypeTagged;
}
bool LCbor::isSimple () const {
    return this->m_type == LCbor::TypeSimple;
}
bool LCbor::isBool () const {
    return this->m_type == LCbor::TypeSimple && (this->m_value == LCbor::SIMPLE_FALSE || this->m_value == LCbor::SIMPLE_TRUE);
}
bool LCbor::isNull () const {
    return this->m_type == LCbor::TypeSimple && this->m_value == LCbor::SIMPLE_NULL;
}
bool LCbor::isUndefined () const {
    return this->m_type == LCbor::TypeSimple && this->m_value == LCbor::SIMPLE_UNDEFINED;
}
bool LCbor::isFloat () const {
    return this->m_type == LCbor::TypeFloat;
}
bool LCbor::isNumber () const {
    return this->m_type == LCbor::TypeUnsigned || this->m_type == LCbor::TypeNegative || this->m_type == LCbor::TypeFloat;
}

bool LCbor::isValid() const
{
    if(m_type == LCbor::TypeSimple){
        return !(this->m_value == LCbor::SIMPLE_NULL || this->m_value == LCbor::SIMPLE_UNDEFINED);
    }
    return true;
}
uint64_t LCbor::toUInt64 () const {
    switch (this->m_type) {
    case LCbor::TypeUnsigned:
        return this->m_value;
    case LCbor::TypeNegative:
        return ~this->m_value;
    case LCbor::TypeTagged:
        if(m_array.empty()){
            return 0;
        }
        return this->m_array.front ().toUInt64();
    case LCbor::TypeFloat:
        return this->m_float;
    default:
        return 0;
    }
}
int64_t LCbor::toInt64 () const {
    switch (this->m_type) {
    case LCbor::TypeUnsigned:
        return int64_t (this->m_value);
    case LCbor::TypeNegative:
        return -1 - int64_t (this->m_value);
    case LCbor::TypeTagged:
        if(m_array.empty()){
            return 0;
        }
        return this->m_array.front ().toInt64 ();
    case LCbor::TypeFloat:
        return this->m_float;
    default:
        return 0;
    }
}
LCbor::binary LCbor::toBinary () const {
    switch (this->m_type) {
    case LCbor::TypeBinary:
        return this->m_binary;
    case LCbor::TypeTagged:
        return this->m_array.front ().toBinary ();
    default:
        return LCbor::binary ();
    }
}

//Todo
LCbor::string LCbor::toString () const {
    switch (this->m_type) {
    case LCbor::TypeString:
        return this->m_string;
    case LCbor::TypeTagged:
        return this->m_array.front ().toString ();
    default:
        return LCbor::string ();
    }
}
LCbor::array LCbor::toArray () const {
    switch (this->m_type) {
    case LCbor::TypeArray:
        return this->m_array;
    case LCbor::TypeTagged:
        return this->m_array.front ().toArray ();
    default:
        return LCbor::array ();
    }
}
LCbor::map LCbor::toMap () const {
    switch (this->m_type) {
    case LCbor::TypeMap:
        return this->m_map;
    case LCbor::TypeTagged:
        return this->m_array.front ().toMap ();
    default:
        return LCbor::map ();
    }
}
LCbor::simple LCbor::toSimple () const {
    switch (this->m_type) {
    case LCbor::TypeTagged:
        return this->m_array.front ().toSimple ();
    case LCbor::TypeSimple:
        return LCbor::simple (this->m_value);
    default:
        return LCbor::SIMPLE_UNDEFINED;
    }
}
bool LCbor::toBool () const {
    switch (this->m_type) {
    case LCbor::TypeTagged:
        return this->m_array.front ().toBool ();
    case LCbor::TypeSimple:
        return this->m_value == LCbor::SIMPLE_TRUE;
    default:
        return false;
    }
}
double LCbor::toDouble () const {
    switch (this->m_type) {
    case LCbor::TypeUnsigned:
        return double (this->m_value);
    case LCbor::TypeNegative:
        return ldexp (-1 - int64_t (this->m_value >> 32), 32) + (-1 - int64_t (this->m_value << 32 >> 32));
    case LCbor::TypeTagged:
        if(m_array.empty()){
            return 0.0;
        }
        return this->m_array.front ().toDouble ();
    case LCbor::TypeFloat:
        return this->m_float;
    default:
        return 0.0;
    }
}

LCbor::operator unsigned short () const {
    return this->toUInt64 ();
}
LCbor::operator unsigned () const {
    return this->toUInt64 ();
}
LCbor::operator unsigned long () const {
    return this->toUInt64 ();
}
LCbor::operator unsigned long long () const {
    return this->toUInt64 ();
}
LCbor::operator short () const {
    return this->toInt64 ();
}
LCbor::operator int () const {
    return this->toInt64 ();
}
LCbor::operator long () const {
    return this->toInt64 ();
}
LCbor::operator long long () const {
    return this->toInt64 ();
}
LCbor::operator LCbor::binary () const {
    return this->toBinary ();
}
LCbor::operator LCbor::string () const {
    return this->toString ();
}
LCbor::operator LCbor::array () const {
    return this->toArray ();
}
LCbor::operator LCbor::map () const {
    return this->toMap ();
}
LCbor::operator LCbor::simple () const {
    return this->toSimple ();
}
LCbor::operator bool () const {
    return this->toBool ();
}
LCbor::operator double () const {
    return this->toDouble ();
}
LCbor::Type LCbor::type () const {
    return this->m_type;
}
uint64_t LCbor::tag () const {
    switch (this->m_type) {
    case LCbor::TypeTagged:
        return this->m_value;
    default:
        return 0;
    }
}
LCbor LCbor::child () const {
    switch (this->m_type) {
    case LCbor::TypeTagged:
        return this->m_array.front ();
    default:
        return LCbor ();
    }
}
bool LCbor::operator < (const LCbor &other) const {
    if (this->m_type < other.m_type) {
        return true;
    }
    if (this->m_type > other.m_type) {
        return false;
    }
    switch (this->m_type) {
    case LCbor::TypeBinary:
        return this->m_binary < other.m_binary;
    case LCbor::TypeString:
        return this->m_string < other.m_string;
    case LCbor::TypeArray:
        return this->m_array < other.m_array;
    case LCbor::TypeMap:
        return this->m_map < other.m_map;
    case LCbor::TypeTagged:
        if (this->m_value < other.m_value) {
            return true;
        }
        if (this->m_value > other.m_value) {
            return false;
        }
        return this->m_array.front () < other.m_array.front ();
    default:
        return this->m_value < other.m_value;
    }
}
bool LCbor::operator == (const LCbor &other) const {
    if (this->m_type != other.m_type) {
        return false;
    }
    switch (this->m_type) {
    case LCbor::TypeBinary:
        return this->m_binary == other.m_binary;
    case LCbor::TypeString:
        return this->m_string == other.m_string;
    case LCbor::TypeArray:
        return this->m_array == other.m_array;
    case LCbor::TypeMap:
        return this->m_map == other.m_map;
    case LCbor::TypeTagged:
        if (this->m_value != other.m_value) {
            return false;
        }
        return this->m_array == other.m_array;
    default:
        return this->m_value == other.m_value;
    }
}
bool LCbor::operator != (const LCbor &other) const {
    return !(*this == other);
}
void read_uint (std::istream &in, int &major, int &minor, uint64_t &value) {
    major = (in.peek () >> 5) & 7;
    minor = in.get () & 31;
    value = 0;
    switch (minor) {
    case 27:
        value |= (unsigned long long) in.get () << 56;
        value |= (unsigned long long) in.get () << 48;
        value |= (unsigned long long) in.get () << 40;
        value |= (unsigned long long) in.get () << 32;
    case 26:
        value |= in.get () << 24;
        value |= in.get () << 16;
    case 25:
        value |= in.get () << 8;
    case 24:
        value |= in.get ();
        break;
    default:
        value = minor;
        break;
    }
}
bool LCbor::read (std::istream &in) {
    LCbor item;
    int major, minor;
    uint64_t value;
    read_uint (in, major, minor, value);
    switch (major) {
    case 0:
        if (minor > 27) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeUnsigned;
        item.m_value = value;
        break;
    case 1:
        if (minor > 27) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeNegative;
        item.m_value = value;
        break;
    case 2:
        if (minor > 27 && minor < 31) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeBinary;
        if (minor == 31) {
            while (in.good () && in.peek () != 255) {
                read_uint (in, major, minor, value);
                if (major != 2 || minor > 27) {
                    in.setstate (std::ios_base::failbit);
                    return false;
                }
                for (uint64_t i = 0; in.good () && i != value; ++i) {
                    item.m_binary.push_back (in.get ());
                }
            }
            in.get ();
        } else {
            for (uint64_t i = 0; in.good () && i != value; ++i) {
                item.m_binary.push_back (in.get ());
            }
        }
        break;
    case 3:
        if (minor > 27 && minor < 31) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeString;
        if (minor == 31) {
            while (in.good () && in.peek () != 255) {
                read_uint (in, major, minor, value);
                if (major  != 3 || minor > 27) {
                    in.setstate (std::ios_base::failbit);
                    return false;
                }
                for (uint64_t i = 0; in.good () && i != value; ++i) {
                    item.m_string.push_back (in.get ());
                }
            }
            in.get ();
        } else {
            for (uint64_t i = 0; in.good () && i != value; ++i) {
                item.m_string.push_back (in.get ());
            }
        }
        break;
    case 4:
        if (minor > 27 && minor < 31) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeArray;
        if (minor == 31) {
            while (in.good () && in.peek () != 255) {
                LCbor child;
                child.read (in);
                item.m_array.push_back (child);
            }
            in.get ();
        } else {
            for (uint64_t i = 0; in.good () && i != value; ++i) {
                LCbor child;
                child.read (in);
                item.m_array.push_back (child);
            }
        }
        break;
    case 5:
        if (minor > 27 && minor < 31) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeMap;
        if (minor == 31) {
            while (in.good () && in.peek () != 255) {
                LCbor key, value;
                key.read (in);
                value.read (in);
                item.m_map.insert (std::make_pair (key, value));
            }
            in.get ();
        } else {
            for (uint64_t i = 0; in.good () && i != value; ++i) {
                LCbor key, value;
                key.read (in);
                value.read (in);
                item.m_map.insert (std::make_pair (key, value));
            }
        }
        break;
    case 6: {
        if (minor > 27) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        item.m_type = LCbor::TypeTagged;
        item.m_value = value;
        LCbor child;
        child.read (in);
        item.m_array.push_back (child);
        break;
    }
    case 7:
        if (minor > 27) {
            in.setstate (std::ios_base::failbit);
            return false;
        }
        switch (minor) {
        case 25: {
            item.m_type = LCbor::TypeFloat;
            int sign = value >> 15;
            int exponent = value >> 10 & 31;
            int significand = value & 1023;
            if (exponent == 31) {
                if (significand) {
                    item.m_float = NAN;
                } else if (sign) {
                    item.m_float = -INFINITY;
                } else {
                    item.m_float = INFINITY;
                }
            } else if (exponent == 0) {
                if (sign) {
                    item.m_float = -ldexp (significand, -24);
                } else {
                    item.m_float = ldexp (significand, -24);
                }
            } else {
                if (sign) {
                    item.m_float = -ldexp (1024 | significand, exponent - 25);
                } else {
                    item.m_float = ldexp (1024 | significand, exponent - 25);
                }
            }
            break;
        }
        case 26: {
            union {
                float f;
                uint32_t i;
            };
            i = value;
            item.m_type = LCbor::TypeFloat;
            item.m_float = f;
            break;
        }
        case 27:
            item.m_type = LCbor::TypeFloat;
            item.m_value = value;
            break;
        default:
            item.m_type = LCbor::TypeSimple;
            item.m_value = value;
        }
        break;
    }
    if (!in.good ()) {
        in.setstate (std::ios_base::failbit);
        return false;
    }
    *this = item;
    return true;
}
void write_uint8 (std::ostream &out, int major, uint64_t value) {
    if (value < 24) {
        out.put (major << 5 | value);
    } else {
        out.put (major << 5 | 24);
        out.put (value);
    }
}
void write_uint16 (std::ostream &out, int major, uint64_t value) {
    out.put (major << 5 | 25);
    out.put (value >> 8);
    out.put (value);
}
void write_uint32 (std::ostream &out, int major, uint64_t value) {
    out.put (major << 5 | 26);
    out.put (value >> 24);
    out.put (value >> 16);
    out.put (value >> 8);
    out.put (value);
}
void write_uint64 (std::ostream &out, int major, uint64_t value) {
    out.put (major << 5 | 27);
    out.put (value >> 56);
    out.put (value >> 48);
    out.put (value >> 40);
    out.put (value >> 32);
    out.put (value >> 24);
    out.put (value >> 16);
    out.put (value >> 8);
    out.put (value);
}
void write_uint (std::ostream &out, int major, uint64_t value) {
    if ((value >> 8) == 0) {
        write_uint8 (out, major, value);
    } else if ((value >> 16) == 0) {
        write_uint16 (out, major, value);
    } else if (value >> 32 == 0) {
        write_uint32 (out, major, value);
    } else {
        write_uint64 (out, major, value);
    }
}
void write_float (std::ostream &out, double value) {
    if (double (float (value)) == value) {
        union {
            float f;
            uint32_t i;
        };
        f = value;
        write_uint32 (out, 7, i);
    } else {
        union {
            double f;
            uint64_t i;
        };
        f = value;
        write_uint64 (out, 7, i);
    }
}
void LCbor::write (std::ostream &out) const {
    switch (this->m_type) {
    case LCbor::TypeUnsigned:
        write_uint (out, 0, this->m_value);
        break;
    case LCbor::TypeNegative:
        write_uint (out, 1, this->m_value);
        break;
    case LCbor::TypeBinary:
        write_uint (out, 2, this->m_binary.size ());
        out.write (reinterpret_cast <const char *> (this->m_binary.data ()), this->m_binary.size ());
        break;
    case LCbor::TypeString:
        write_uint (out, 3, this->m_string.size ());
        out.write (this->m_string.data (), this->m_string.size ());
        break;
    case LCbor::TypeArray:
        write_uint (out, 4, this->m_array.size ());
        for (LCbor::array::const_iterator it = this->m_array.begin (); it != this->m_array.end (); ++it) {
            it->write (out);
        }
        break;
    case LCbor::TypeMap:
        write_uint (out, 5, this->m_map.size ());
        for (LCbor::map::const_iterator it = this->m_map.begin (); it != this->m_map.end (); ++it) {
            it->first.write (out);
            it->second.write (out);
        }
        break;
    case LCbor::TypeTagged:
        write_uint (out, 6, this->m_value);
        this->m_array.front ().write (out);
        break;
    case LCbor::TypeSimple:
        write_uint8 (out, 7, this->m_value);
        break;
    case LCbor::TypeFloat:
        write_float (out, this->m_float);
        break;
    }
}

bool LCbor::read(std::string &filePath)
{
    if(filePath.starts_with(":")){
        LResource resource(filePath);
        std::vector<uint8_t> data = resource.uncompressedData();
        if(!data.empty()){
            std::spanstream	stream(std::span<char>((char*)(data.data()), data.size()), std::ios_base::in);
            return read(stream);
        }else{
            lWarn() << "Failed to open resource: " << filePath;
            return false;
        }
    }else{
        std::ifstream file(filePath);
        if (file.is_open()) {
            return read(file);
        }else{
            lWarn() << "Failed to open file: " << filePath;
            return false;
        }
    }
}

bool LCbor::write(std::string &filePath) const
{
    std::ofstream file(filePath);
    if (file.is_open()) {
        write(file);
        return true;
    }
    return  false;
}
bool LCbor::validate (const LCbor::binary &in) {
    std::istringstream buf1 (std::string (in.begin (), in.end ()));
    LCbor buf2;
    return buf2.read (buf1) && buf1.peek () == EOF;
}
LCbor LCbor::decode (const LCbor::binary &in) {
    std::istringstream buf1 (std::string (in.begin (), in.end ()));
    LCbor buf2;
    if (buf2.read (buf1) && buf1.peek () == EOF) {
        return buf2;
    }
    return LCbor ();
}
LCbor::binary LCbor::encode (const LCbor &in) {
    std::ostringstream buf1;
    in.write (buf1);
    std::string buf2 = buf1.str ();
    return LCbor::binary (buf2.begin (), buf2.end ());
}
LCbor::string LCbor::debug (const LCbor &in) {
    std::ostringstream out;
    switch (in.m_type) {
    case LCbor::TypeUnsigned:
        out << in.m_value;
        break;
    case LCbor::TypeNegative:
        if (1 + in.m_value == 0) {
            out << "-18446744073709551616";
        } else {
            out << "-" << 1 + in.m_value;
        }
        break;
    case LCbor::TypeBinary:
        out << "h'";
        out << std::hex;
        out.fill ('0');
        for (LCbor::binary::const_iterator it = in.m_binary.begin (); it != in.m_binary.end (); ++it) {
            out.width (2);
            out << int (*it);
        }
        out << "'";
        break;
    case LCbor::TypeString:
        out << "\"";
        out << std::hex;
        out.fill ('0');
        for (LCbor::string::const_iterator it = in.m_string.begin (); it != in.m_string.end (); ++it) {
            switch (*it) {
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            default:
                if ((unsigned char) *it < '\x20') {
                    out << "\\u";
                    out.width (4);
                    out << (int) (unsigned char) *it;
                } else {
                    out << *it;
                }
                break;
            }
        }
        out << "\"";
        break;
    case LCbor::TypeArray:
        out << "[";
        for (LCbor::array::const_iterator it = in.m_array.begin (); it != in.m_array.end (); ++it) {
            if (it != in.m_array.begin ()) {
                out << ", ";
            }
            out << LCbor::debug (*it);
        }
        out << "]";
        break;
    case LCbor::TypeMap:
        out << "{";
        for (LCbor::map::const_iterator it = in.m_map.begin (); it != in.m_map.end (); ++it) {
            if (it != in.m_map.begin ()) {
                out << ", ";
            }
            out << LCbor::debug (it->first) << ": " << LCbor::debug (it->second);
        }
        out << "}";
        break;
    case LCbor::TypeTagged:
        out << in.m_value << "(" << LCbor::debug (in.m_array.front ()) << ")";
        break;
    case LCbor::TypeSimple:
        switch (in.m_value) {
        case LCbor::SIMPLE_FALSE:
            out << "false";
            break;
        case LCbor::SIMPLE_TRUE:
            out << "true";
            break;
        case LCbor::SIMPLE_NULL:
            out << "null";
            break;
        case LCbor::SIMPLE_UNDEFINED:
            out << "undefined";
            break;
        default:
            out << "simple(" << in.m_value << ")";
            break;
        }
        break;
    case LCbor::TypeFloat:
        if (std::isinf (in.m_float)) {
            if (in.m_float < 0) {
                out << "-";
            }
            out << "Infinity";
        } else if (std::isnan (in.m_float)) {
            out << "NaN";
        } else {
            out << std::showpoint << in.m_float;
        }
        break;
    }
    return out.str ();
}

LK_NAMESPACE_END
