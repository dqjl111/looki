#ifndef LVARIANT_H
#define LVARIANT_H
#include "lkcore/livar.h"

LK_NAMESPACE_BEGIN

class LVariant;
using LVariants = std::vector<LVariant>;

class LKCOREDEF LVariant {
public:
    enum Type {
        VarNull,
        VarBool,		// boolean
        VarChar,		// 字符
        VarInt8,		// 有符号8位有意义
        VarUInt8,		// 无符合8位有意义
        VarInt16,		// 有符号16位有意义
        VarUInt16,		// 无符合16位有意义
        VarInt32,		// 有符号32位有意义
        VarUInt32,		// 无符合32位有意义
        VarInt64,		// 有符号64位有意义
        VarUInt64,		// 无符合64位有意义
        VarFloat,	// 32位浮点
        VarDouble,	// 64位浮点
        VarTimePoint, //系统时间戳
        VarByteArray,	// 8位二进制数据
        VarString,	// 8位字符串数据
        VarWString,		// 字符串数据块'\0'结尾
        VarVariants,	// 嵌套的变量
        VarCbor, //Cbor序列化数据
        VarLIVarPtr //shared_ptr<LIVar>
    };

    inline Type type(void) const {
        return m_data.type;
    }

    inline void setType(Type t) {
        if(m_data.type != t){
            m_data.clear();
            m_data.type = t;
        }
    }

    inline void setBool(bool v) {
        m_data.clear();
        m_data.type = VarBool;
        m_data.data.b01 = v;
    }
    inline void setChar(char v) {
        m_data.clear();
        m_data.type = VarChar;
        m_data.data.c08 = v;
    }
    inline void setInt8(int8_t v) {
        m_data.clear();
        m_data.type = VarInt8;
        m_data.data.i32 = v;
    }
    inline void setUInt8(uint8_t v) {
        m_data.clear();
        m_data.type = VarUInt8;
        m_data.data.i32 = v;
    }
    inline void setInt16(int16_t v) {
        m_data.clear();
        m_data.type = VarInt16;
        m_data.data.i32 = v;
    }
    inline void setUInt16(uint16_t v) {
        m_data.clear();
        m_data.type = VarUInt16;
        m_data.data.i32 = v;
    }
    inline void setInt32(int32_t v) {
        m_data.clear();
        m_data.type = VarInt32;
        m_data.data.i32 = v;
    }
    inline void setUInt32(uint32_t v) {
        m_data.clear();
        m_data.type = VarUInt32;
        m_data.data.i32 = v;
    }
    inline void setInt64(int64_t v) {
        m_data.clear();
        m_data.type = VarInt64;
        m_data.data.i64 = v;
    }
    inline void setUInt64(uint64_t v) {
        m_data.clear();
        m_data.type = VarUInt64;
        m_data.data.i64 = v;
    }

    inline void setFloat(float v) {
        m_data.clear();
        m_data.type = VarFloat;
        m_data.data.f32 = v;
    }

    inline void setDouble(double v) {
        m_data.clear();
        m_data.type = VarDouble;
        m_data.data.f64 = v;
    }

    inline void setTimePoint(LSysTimePoint v) {
        m_data.clear();
        m_data.type = VarTimePoint;
        m_data.data.tp = LSysClock::to_time_t(v);
    }
    inline void setByteArray(const char *v, uint32_t s) {
        m_data.setByteArray(v, s);
    }
    inline void setString(const char *v, uint32_t s) {
        m_data.setString(v, s);
    }
    inline void setString(const std::string &v) {
        m_data.setString(v.c_str(), static_cast<uint32_t>(v.size()));
    }
    inline void setWString(const wchar_t *v, uint32_t s) {
        m_data.setWString(v, s);
    }
    inline void setWString(const std::wstring &v) {
        m_data.setWString(v.c_str(), static_cast<uint32_t>(v.size()));
    }
    inline void setVariants(const LVariants &v) {
        m_data.setVariants(v);
    }

    inline void setCbor(const LCbor &v) {
        m_data.setCbor(v);
    }

    inline void setIVar(const std::shared_ptr<LIVar> ivar){
        m_data.clear();
        m_data.type = VarLIVarPtr;
        m_data.ivar = ivar;
    }

    inline bool getBool(void) const {
        return m_data.data.b01;
    }
    inline char getChar(void) const {
        return m_data.data.c08;
    }
    inline int8_t getInt8(void) const {
        return m_data.data.i08;
    }
    inline uint8_t getUInt8(void) const {
        return m_data.data.i08;
    }
    inline int16_t getInt16(void) const {
        return m_data.data.i16;
    }
    inline uint16_t getUInt16(void) const {
        return m_data.data.i16;
    }
    inline int32_t getInt32(void) const {
        return m_data.data.i32;
    }
    inline uint32_t getUInt32(void) const {
        return m_data.data.i32;
    }
    inline int64_t getInt64(void) const {
        return m_data.data.i64;
    }
    inline uint64_t getUInt64(void) const {
        return m_data.data.i64;
    }
    inline float getFloat(void) const {
        return m_data.data.f32;
    }
    inline double getDouble(void) const {
        return m_data.data.f64;
    }
    inline LSysTimePoint getTimePoint(void) const {
        return LSysClock::from_time_t(m_data.data.tp);
    }
    inline char *getBinData(void) const {
        return static_cast< char * >(m_data.data.bin.bin);
    }

    inline char *getString(void) const {
        return static_cast< char * >(m_data.data.bin.bin);
    }

    inline wchar_t *getWString(void) const {
        return static_cast< wchar_t * >(m_data.data.bin.bin);
    }

    inline uint32_t getByteSize(void) const {
        return m_data.data.bin.len;
    }

    inline LVariants *getVariants(void) const {
        return m_data.data.var;
    }

    inline LCbor *getCbor(void) const {
        return m_data.data.cbor;
    }

    inline std::shared_ptr<LIVar> getIVar() const{
        return m_data.ivar;
    }


    inline LVariant(Type t) : m_data(t) {}
    inline LVariant(bool v) : m_data(v) {}
    inline LVariant(char v) : m_data(v) {}
    inline LVariant(int8_t v) : m_data(v) {}
    inline LVariant(uint8_t v) : m_data(v) {}
    inline LVariant(int16_t v) : m_data(v) {}
    inline LVariant(uint16_t v) : m_data(v) {}
    inline LVariant(int32_t v) : m_data(v) {}
    inline LVariant(uint32_t v) : m_data(v) {}
    inline LVariant(int64_t v) : m_data(v) {}
    inline LVariant(uint64_t v) : m_data(v) {}
    inline LVariant(float v) : m_data(v) {}
    inline LVariant(double v) : m_data(v) {}
    inline LVariant(LSysTimePoint v) : m_data(v) {}
    inline LVariant(const void *v, uint32_t s) : m_data(v, s) {}
    inline LVariant(const char *v, uint32_t s) : m_data(v, s) {}
    inline LVariant(const wchar_t *v, uint32_t s) : m_data(v, s) {}

    inline LVariant(const std::string &v) :
        m_data(v.c_str(), static_cast<uint32_t>(v.size())) {}
    inline LVariant(const std::wstring &v) :
        m_data(v.c_str(), static_cast<uint32_t>(v.size())) {}
    inline LVariant(const std::u32string &v) :
        m_data(v.c_str(), static_cast<uint32_t>(v.size())) {}

    inline LVariant(const LVariants &v) : m_data(v) {}
    inline LVariant(LVariants&& v) : m_data(std::move(v)) {}
    inline LVariant(const LCbor& v) : m_data(v) {}
    inline LVariant(const std::shared_ptr<LIVar> v) : m_data(v){
    }

    inline LVariant(void) = default;
    inline ~LVariant(void) = default;
    inline LVariant(const LVariant &v) = default;
    inline LVariant(LVariant&& v) = default;

    inline LVariant &operator = (const LVariant &obj) {
        m_data = obj.m_data;
        return *this;
    }
    inline LVariant &operator = (LVariant&& obj) noexcept {
        m_data = std::move(obj.m_data);
        return *this;
    }

    inline bool operator == (const LVariant &obj) const {
        return m_data == obj.m_data;
    }
    inline bool operator != (const LVariant &obj) const {
        return m_data != obj.m_data;
    }

    template< class T > inline void set(T v);
    template< class T > inline T get(void) const;
    template< class T > inline std::shared_ptr<T> getIVarT() const;

    std::string toString(bool* isOk = nullptr) const;
    std::wstring toWString(bool* isOk = nullptr) const;
    LCbor toCbor(bool* isOk = nullptr) const;

    bool toBool(bool* isOk = nullptr) const;
    int64_t toInt64(bool* isOk = nullptr) const;
    uint64_t toUInt64(bool* isOk = nullptr) const;
    int toInt(bool* isOk = nullptr) const;
    uint32_t toUInt(bool* isOk = nullptr) const;
    double toDouble(bool* isOk = nullptr) const;
    float toFloat(bool* isOk = nullptr) const;

    bool isNull()const;
    uint64_t hashValue() const;
    inline void clear(){
        m_data.clear();
    }

    static const LVariant& staticNull();
private:
    struct VariantBin {
        void *	bin;
        uint32_t	len;
    };
    struct VariantBase {
        Type	type;
        std::shared_ptr<LIVar> ivar = nullptr;
        union UD{
            bool b01;
            char c08;
            uint8_t i08;
            uint16_t i16;
            uint32_t i32;
            uint64_t i64;
            float f32;
            double f64;
            __time64_t tp;
            LVariants*	var;
            LCbor* cbor;
            VariantBin	bin;
        }	data;

        inline VariantBase(Type t) : type(t) {
            initUD();
        }

        inline VariantBase(void) : type(VarNull) {
            initUD();
        }
        inline VariantBase(bool v) : type(VarBool) {
            initUD();
            data.b01 = v;
        }
        inline VariantBase(char v) : type(VarChar) {
            initUD();
            data.c08 = v;
        }
        inline VariantBase(int8_t v) : type(VarInt8) {
            initUD();
            data.i32 = v;
        }
        inline VariantBase(uint8_t v) : type(VarUInt8) {
            initUD();
            data.i32 = v;
        }
        inline VariantBase(int16_t v) : type(VarInt16) {
            initUD();
            data.i32 = v;
        }
        inline VariantBase(uint16_t v) : type(VarUInt16) {
            initUD();
            data.i32 = v;
        }
        inline VariantBase(int32_t v) : type(VarInt32) {
            initUD();
            data.i32 = v;
        }
        inline VariantBase(uint32_t v) : type(VarUInt32) {
            initUD();
            data.i32 = v;
        }
        inline VariantBase(int64_t v) : type(VarInt64) {
            initUD();
            data.i64 = v;
        }
        inline VariantBase(uint64_t v) : type(VarUInt64) {
            initUD();
            data.i64 = v;
        }
        inline VariantBase(float v) : type(VarFloat) {
            initUD();
            data.f32 = v;
        }
        inline VariantBase(double v) : type(VarDouble) {
            initUD();
            data.f64 = v;
        }
        inline VariantBase(LSysTimePoint v) : type(VarTimePoint) {
            initUD();
            data.tp = LSysClock::to_time_t(v);
        }
        VariantBase(const void *v, uint32_t s);
        VariantBase(const char *v, uint32_t s);
        VariantBase(const wchar_t *v, uint32_t s);

        VariantBase(const LVariants &v);
        VariantBase(LVariants &&v);
        VariantBase(const LCbor &v);
        VariantBase(const std::shared_ptr<LIVar> v);

        inline VariantBase(const VariantBase &obj) {
            initUD();
            cpyObjc(obj);
        }
        inline VariantBase(VariantBase&& obj) noexcept {
            initUD();
            movObjc(std::move(obj));
        }
        inline ~VariantBase(void) {
            clear();
        }

        inline VariantBase &operator = (const VariantBase &obj) {
            clear();
            cpyObjc(obj);
            return *this;
        }
        inline VariantBase& operator = (VariantBase&& obj) noexcept {
            clear();
            movObjc(std::move(obj));
            return *this;
        }

        bool operator == (const VariantBase &obj) const;
        bool operator != (const VariantBase &obj) const;

        void clear(void);

        void setVariants(const LVariants &v);
        void setCbor(const LCbor &v);
        void setByteArray(const char *v, uint32_t s);
        void setString(const char *v, uint32_t s);
        void setWString(const wchar_t *v, uint32_t s);

    private:
        void setData(const void *mem, uint32_t len);
        void cpyObjc(const VariantBase &obj);
        void movObjc(VariantBase&& obj);
        inline void initUD(){
#ifdef _DEBUG
            memset((char*)(&data), 0, sizeof(UD));
#endif
        }
    } m_data;
};

template<> inline void LVariant::set(bool v) {
    LVariant::setBool(v);
}
template<> inline void LVariant::set(char v) {
    LVariant::setChar(v);
}
template<> inline void LVariant::set(int8_t v) {
    LVariant::setInt8(v);
}
template<> inline void LVariant::set(uint8_t v) {
    LVariant::setUInt8(v);
}
template<> inline void LVariant::set(int16_t v) {
    LVariant::setInt16(v);
}
template<> inline void LVariant::set(uint16_t v) {
    LVariant::setUInt16(v);
}
template<> inline void LVariant::set(int32_t v) {
    LVariant::setInt32(v);
}
template<> inline void LVariant::set(uint32_t v) {
    LVariant::setUInt32(v);
}
template<> inline void LVariant::set(int64_t v) {
    LVariant::setInt64(v);
}
template<> inline void LVariant::set(uint64_t v) {
    LVariant::setUInt64(v);
}
template<> inline void LVariant::set(float v) {
    LVariant::setFloat(v);
}
template<> inline void LVariant::set(double v) {
    LVariant::setDouble(v);
}
template<> inline void LVariant::set(LSysTimePoint v) {
    LVariant::setTimePoint(v);
}
template<> inline void LVariant::set(const std::string &v) {
    LVariant::setString(v);
}
template<> inline void LVariant::set(const std::wstring &v) {
    LVariant::setWString(v);
}
template<> inline void LVariant::set(const LVariants &v) {
    LVariant::setVariants(v);
}
template<> inline void LVariant::set(const LCbor &v) {
    LVariant::setCbor(v);
}
template<> inline void LVariant::set(const std::shared_ptr<LIVar> v) {
    LVariant::setIVar(v);
}

template<> inline LVariants *LVariant::get(void) const {
    return LVariant::getVariants();
}

template<> inline LCbor *LVariant::get(void) const {
    return LVariant::getCbor();
}

template<> inline LCbor LVariant::get(void) const {
    return toCbor();
}

template<> inline std::shared_ptr<LIVar> LVariant::get(void) const {
    return LVariant::getIVar();
}

template<> inline char *LVariant::get(void) const {
    return LVariant::getString();
}

template<> inline bool LVariant::get(void) const {
    return LVariant::getBool();
}
template<> inline char LVariant::get(void) const {
    return LVariant::getChar();
}
template<> inline int8_t LVariant::get(void) const {
    return LVariant::getInt8();
}
template<> inline uint8_t LVariant::get(void) const {
    return LVariant::getUInt8();
}
template<> inline int16_t LVariant::get(void) const {
    return LVariant::getInt16();
}
template<> inline uint16_t LVariant::get(void) const {
    return LVariant::getUInt16();
}
template<> inline int32_t LVariant::get(void) const {
    return LVariant::getInt32();
}
template<> inline uint32_t LVariant::get(void) const {
    return LVariant::getUInt32();
}
template<> inline int64_t LVariant::get(void) const {
    return LVariant::getInt64();
}
template<> inline uint64_t LVariant::get(void) const {
    return LVariant::getUInt64();
}
template<> inline float LVariant::get(void) const {
    return LVariant::getFloat();
}
template<> inline double LVariant::get(void) const {
    return LVariant::getDouble();
}
template<> inline LSysTimePoint LVariant::get(void) const {
    return LVariant::getTimePoint();
}
template<> inline std::string LVariant::get(void) const {
    return m_data.type == VarString ? std::string{LVariant::getString(), LVariant::getByteSize() - 1U} : toString();
}
template<> inline std::wstring LVariant::get(void) const {
    return m_data.type == VarWString ? std::wstring{LVariant::getWString(), LVariant::getByteSize()/sizeof(wchar_t) - 1U}: toWString();
}


template< class T >
inline std::shared_ptr<T> LVariant::getIVarT() const{
    if(m_data.ivar != nullptr && m_data.type == VarLIVarPtr){
        return m_data.ivar->getPtrT<T>();
    }
    return nullptr;
}

LK_NAMESPACE_END
#endif // LVARIANT_H
