#ifndef LENDIAN_H
#define LENDIAN_H

#include "lkcore/lglobal.h"
#include "lkcore/io/lbyteswap.h"

// include stdlib.h and hope that it defines __GLIBC__ for glibc-based systems
#include <stdlib.h>
#include <string.h>

#ifdef min // MSVC
#undef min
#undef max
#endif

LK_NAMESPACE_BEGIN

#if LOOKI_LITTLE_ENDIAN == 0

    template <typename T> inline constexpr T lToBigEndian(T source)
{ return source; }
template <typename T> inline constexpr T lFromBigEndian(T source)
{ return source; }
template <typename T> inline constexpr T lToLittleEndian(T source)
{ return byteswap(source); }
template <typename T> inline constexpr T lFromLittleEndian(T source)
{ return byteswap(source); }
template <typename T> inline void lToBigEndian(T src, void *dest)
{ lToUnaligned<T>(src, dest); }
template <typename T> inline void lToLittleEndian(T src, void *dest)
{ byteswap<T>(src, dest); }

template <typename T> inline void lToBigEndian(const void *source, lsizetype count, void *dest)
{ if (source != dest) memcpy(dest, source, count * sizeof(T)); }
template <typename T> inline void lToLittleEndian(const void *source, lsizetype count, void *dest)
{ byteswap<sizeof(T)>(source, count, dest); }
template <typename T> inline void lFromBigEndian(const void *source, lsizetype count, void *dest)
{ if (source != dest) memcpy(dest, source, count * sizeof(T)); }
template <typename T> inline void lFromLittleEndian(const void *source, lsizetype count, void *dest)
{ byteswap<sizeof(T)>(source, count, dest); }
#else //

template <typename T> inline constexpr T lToBigEndian(T source)
{ return byteswap(source); }
template <typename T> inline constexpr T lFromBigEndian(T source)
{ return byteswap(source); }
template <typename T> inline constexpr T lToLittleEndian(T source)
{ return source; }
template <typename T> inline constexpr T lFromLittleEndian(T source)
{ return source; }
template <typename T> inline void lToBigEndian(T src, void *dest)
{ byteswap<T>(src, dest); }
template <typename T> inline void lToLittleEndian(T src, void *dest)
{ lToUnaligned<T>(src, dest); }

template <typename T> inline void lToBigEndian(const void *source, lsizetype count, void *dest)
{ byteswap<sizeof(T)>(source, count, dest); }
template <typename T> inline void lToLittleEndian(const void *source, lsizetype count, void *dest)
{ if (source != dest) memcpy(dest, source, count * sizeof(T)); }
template <typename T> inline void lFromBigEndian(const void *source, lsizetype count, void *dest)
{ byteswap<sizeof(T)>(source, count, dest); }
template <typename T> inline void lFromLittleEndian(const void *source, lsizetype count, void *dest)
{ if (source != dest) memcpy(dest, source, count * sizeof(T)); }
#endif


template <typename T> inline T lFromLittleEndian(const void *src)
{
    return lFromLittleEndian(lFromUnaligned<T>(src));
}

template <> inline uint8_t lFromLittleEndian<uint8_t>(const void *src)
{ return static_cast<const uint8_t *>(src)[0]; }
template <> inline int8_t lFromLittleEndian<int8_t>(const void *src)
{ return static_cast<const int8_t *>(src)[0]; }

template <class T> inline T lFromBigEndian(const void *src)
{
    return lFromBigEndian(lFromUnaligned<T>(src));
}

template <> inline uint8_t lFromBigEndian<uint8_t>(const void *src)
{ return static_cast<const uint8_t *>(src)[0]; }
template <> inline int8_t lFromBigEndian<int8_t>(const void *src)
{ return static_cast<const int8_t *>(src)[0]; }


template<class S>
class LSpecialInteger
{
    typedef typename S::StorageType T;
    T val;
public:
    LSpecialInteger() = default;
    explicit constexpr LSpecialInteger(T i) : val(S::toSpecial(i)) {}

    LSpecialInteger &operator =(T i) { val = S::toSpecial(i); return *this; }
    operator T() const { return S::fromSpecial(val); }

    bool operator ==(LSpecialInteger<S> i) const { return val == i.val; }
    bool operator !=(LSpecialInteger<S> i) const { return val != i.val; }

    LSpecialInteger &operator +=(T i)
    {   return (*this = S::fromSpecial(val) + i); }
    LSpecialInteger &operator -=(T i)
    {   return (*this = S::fromSpecial(val) - i); }
    LSpecialInteger &operator *=(T i)
    {   return (*this = S::fromSpecial(val) * i); }
    LSpecialInteger &operator >>=(T i)
    {   return (*this = S::fromSpecial(val) >> i); }
    LSpecialInteger &operator <<=(T i)
    {   return (*this = S::fromSpecial(val) << i); }
    LSpecialInteger &operator /=(T i)
    {   return (*this = S::fromSpecial(val) / i); }
    LSpecialInteger &operator %=(T i)
    {   return (*this = S::fromSpecial(val) % i); }
    LSpecialInteger &operator |=(T i)
    {   return (*this = S::fromSpecial(val) | i); }
    LSpecialInteger &operator &=(T i)
    {   return (*this = S::fromSpecial(val) & i); }
    LSpecialInteger &operator ^=(T i)
    {   return (*this = S::fromSpecial(val) ^ i); }
    LSpecialInteger &operator ++()
    {   return (*this = S::fromSpecial(val) + 1); }
    LSpecialInteger &operator --()
    {   return (*this = S::fromSpecial(val) - 1); }
    LSpecialInteger operator ++(int)
    {
        LSpecialInteger<S> pre = *this;
        *this += 1;
        return pre;
    }
    LSpecialInteger operator --(int)
    {
        LSpecialInteger<S> pre = *this;
        *this -= 1;
        return pre;
    }

    static constexpr LSpecialInteger max()
    { return LSpecialInteger(std::numeric_limits<T>::max()); }
    static constexpr LSpecialInteger min()
    { return LSpecialInteger(std::numeric_limits<T>::min()); }
};

template<typename T>
class LLittleEndianStorageType {
public:
    typedef T StorageType;
    static constexpr T toSpecial(T source) { return lToLittleEndian(source); }
    static constexpr T fromSpecial(T source) { return lFromLittleEndian(source); }
};

template<typename T>
class LBigEndianStorageType {
public:
    typedef T StorageType;
    static constexpr T toSpecial(T source) { return lToBigEndian(source); }
    static constexpr T fromSpecial(T source) { return lFromBigEndian(source); }
};

template<typename T>
using LLEInteger = LSpecialInteger<LLittleEndianStorageType<T>>;

template<typename T>
using LBEInteger = LSpecialInteger<LBigEndianStorageType<T>>;

typedef LLEInteger<int16_t> int16_t_le;
typedef LLEInteger<int32_t> int32_t_le;
typedef LLEInteger<int64_t> int64_t_le;
typedef LLEInteger<uint16_t> uint16_t_le;
typedef LLEInteger<uint32_t> uint32_t_le;
typedef LLEInteger<uint64_t> uint64_t_le;

typedef LBEInteger<int16_t> int16_t_be;
typedef LBEInteger<int32_t> int32_t_be;
typedef LBEInteger<int64_t> int64_t_be;
typedef LBEInteger<uint16_t> uint16_t_be;
typedef LBEInteger<uint32_t> uint32_t_be;
typedef LBEInteger<uint64_t> uint64_t_be;

#define MAKE_TAG_REVERSE(ch1, ch2, ch3, ch4) (\
    (((uint32_t)(ch4)) << 24) | \
    (((uint32_t)(ch3)) << 16) | \
    (((uint32_t)(ch2)) << 8) | \
    ((uint32_t)(ch1)) \
   )

#define MAKE_TAG(ch1, ch2, ch3, ch4) (\
    (((uint32_t)(ch1)) << 24) | \
    (((uint32_t)(ch2)) << 16) | \
    (((uint32_t)(ch3)) << 8) | \
    ((uint32_t)(ch4)) \
   )

LK_NAMESPACE_END

#endif // QENDIAN_H
