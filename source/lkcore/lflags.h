#ifndef LFLAGS_H
#define LFLAGS_H
#include "lkcore/lookiconfig.h"
#include <type_traits>
#include <bitset>
#include <cassert>
#include <string>

LK_NAMESPACE_BEGIN
template <typename T>
class LFlags
{
    using u_type = std::underlying_type_t<T>;
public:
    LFlags() = default;

    LFlags(const T& val) {
        flags = (u_type)val;
    }

    LFlags(const u_type& val) {
        flags = val;
    }

    LFlags(const LFlags<T>& val) {
        flags = val.flags;
    }

    // Binary operations.
    LFlags<T>& operator&=(const T& val)
    {
        flags &= (u_type)val;
        return *this;
    }

    LFlags<T>& operator&=(const u_type& val)
    {
        flags &= val;
        return *this;
    }

    LFlags<T>& operator&=(const LFlags<T>& o)
    {
        flags &= o.flags;
        return *this;
    }

    LFlags<T>& operator|=(const T& val)
    {
        flags |= (u_type)val;
        return *this;
    }

    LFlags<T>& operator|=(const u_type& val)
    {
        flags |= val;
        return *this;
    }

    LFlags<T>& operator|=(const LFlags<T>& o)
    {
        flags |= o.flags;
        return *this;
    }

    LFlags<T>& operator^=(const T& val)
    {
        flags ^= (u_type)val;
        return *this;
    }

    LFlags<T>& operator^=(const u_type& val)
    {
        flags ^= val;
        return *this;
    }

    LFlags<T>& operator^=(const LFlags<T>& o)
    {
        flags ^= o.flags;
        return *this;
    }

    // The resulting bitset can contain at most 1 bit.
    LFlags<T> operator&(const T& val) const
    {
        LFlags ret(*this);
        ret &= val;
        return ret;
    }

    LFlags<T> operator&(const u_type& val) const
    {
        LFlags ret(*this);
        ret &= val;
        return ret;
    }

    LFlags<T> operator&(const LFlags<T>& val) const
    {
        LFlags ret(*this);
        ret.flags &= val.flags;
        return ret;
    }

    // The resulting bitset contains at least 1 bit.
    LFlags<T> operator|(const T& val) const
    {
        LFlags ret(*this);
        ret |= val;
        return ret;
    }

    LFlags<T> operator|(const u_type& val) const
    {
        LFlags ret(*this);
        ret |= val;
        return ret;
    }

    LFlags<T> operator|(const LFlags<T>& val) const
    {
        LFlags ret(*this);
        ret.flags |= val.flags;
        return ret;
    }

    LFlags<T> operator^(const T& val) const
    {
        LFlags ret(*this);
        ret ^= val;
        return ret;
    }

    LFlags<T> operator^(const u_type& val) const
    {
        LFlags ret(*this);
        ret ^= val;
        return ret;
    }

    LFlags<T> operator^(const LFlags<T>& val) const
    {
        LFlags ret(*this);
        ret.flags ^= val.flags;
        return ret;
    }

    LFlags<T> operator~() const
    {
        LFlags<T> cp(*this);
        cp.flags = ~cp.flags;
        return cp;
    }

    // The bitset evaluates to true if any bit is set.
    inline bool operator!() const noexcept { return !flags; }

    // Methods from std::bitset.

    LFlags<T>& operator=(const LFlags<T>& o) {
        flags = o.flags;
        return *this;
    }
    LFlags<T>& operator=(const T& val) {
        flags = (u_type)val;
        return *this;
    }
    LFlags<T>& operator=(const u_type& val) {
        flags = val;
        return *this;
    }

    bool operator==(const LFlags<T>& o) const { return flags == o.flags; }
    bool operator==(const T& val) const { return flags == (u_type)val; }
    bool operator==(const u_type& val) const { return flags == val; }

    bool operator!=(const LFlags<T>& o) const { return flags != o.flags; }
    bool operator!=(const T& val) const { return flags != (u_type)val; }
    bool operator!=(const u_type& val) const { return flags != val; }

    std::size_t size() const { return sizeof(u_type); }

    LFlags<T>& set()
    {
        flags = ~u_type(0);
        return *this;
    }

    LFlags<T>& reset()
    {
        flags = 0;
        return *this;
    }

    LFlags<T>& flip()
    {
        flags = ~flags;
        return *this;
    }


    LFlags<T>& setFlags(const T inFlags, bool value = true) {
        if (true) {
            flags |= u_type(inFlags);
        }
        else {
            flags &= (~u_type(inFlags));
        }
        return *this;
    }


    LFlags<T>& set(const uint32_t& pos, bool value = true)
    {
        if(value){
            flags |= 1<<pos;
        }else{
            u_type t = 1<<pos;
            flags &= (~t);
        }
        return *this;
    }

    LFlags<T>& reset(const uint32_t& pos)
    {
        flags |= 1<<pos;
        return *this;
    }

    LFlags<T>& flip(const uint32_t& pos)
    {
        u_type t = 1<<pos;
        flags &= (~t);
        return *this;
    }

    constexpr bool operator[](const uint32_t& pos) const {
        return (flags >> pos) & 0x01;
    }

    const u_type& value(){
        return flags;
    }

    inline operator uint32_t() const noexcept { return flags; }
    inline bool testFlag(T flag) const noexcept { return (flags & u_type(flag)) == u_type(flag) && (u_type(flag) != 0 || flags == u_type(flag) ); }
private:
    u_type flags = 0;
};

template <typename T>
struct LFlagsAttribute {
    static constexpr bool value{false};
};

template <typename T>
using LIsEnumflags = std::integral_constant<
    bool,
    std::is_enum<T>::value && LFlagsAttribute<T>::value>;


template <typename T>
std::enable_if_t<LIsEnumflags<T>::value, LFlags<T>> operator|(const T& lhs, const T& rhs)
{
    LFlags<T> fs;
    fs |= lhs;
    fs |= rhs;
    return fs;
}

template <typename T>
std::enable_if_t<LIsEnumflags<T>::value, LFlags<T>> operator|(const T& lhs, const LFlags<T>& rhs)
{
    return rhs|lhs;
}

template <typename T>
std::enable_if_t<LIsEnumflags<T>::value, LFlags<T>> operator&(const T& lhs, const LFlags<T>& rhs)
{
    return rhs&lhs;
}

template <typename T>
std::enable_if_t<LIsEnumflags<T>::value, LFlags<T>> operator^(const T& lhs, const LFlags<T>& rhs)
{
    return rhs^lhs;
}


#define L_EN_FLAGS(Enum)\
template<>\
struct LFlagsAttribute<Enum> { \
    static constexpr bool value{true}; \
};\


LK_NAMESPACE_END
#endif // LFLAGS_H
