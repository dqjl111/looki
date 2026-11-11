#ifndef LBYTESWAP_H
#define LBYTESWAP_H

#include "lkcore/lglobal.h"
LK_NAMESPACE_BEGIN

template <typename T> T byteswap(T source);

#if defined(_MSC_VER)
template <>
inline uint64_t byteswap<uint64_t>(uint64_t v) { return _byteswap_uint64(v); }

template <>
inline uint32_t byteswap<uint32_t>(uint32_t v) { return _byteswap_ulong(v); }

template <>
inline uint16_t byteswap<uint16_t>(uint16_t v) { return _byteswap_ushort(v); }

template <>
inline int64_t byteswap<int64_t>(int64_t v) { return (int64_t)_byteswap_uint64(v); }

template <>
inline int32_t byteswap<int32_t>(int32_t v) { return (int32_t)_byteswap_ulong(v); }

template <>
inline int16_t byteswap<int16_t>(int16_t v) { return (int16_t)_byteswap_ushort(v); }
#else
template <>
inline uint64_t byteswap<uint64_t>(uint64_t v) { return __builtin_bswap64(v); }

template <>
inline uint32_t byteswap<uint32_t>(uint32_t v) { return __builtin_bswap32(v); }

template <>
inline uint16_t byteswap<uint16_t>(uint16_t v) { return __builtin_bswap16(v); }

template <>
inline int64_t byteswap<int64_t>(int64_t v) { return (int64_t)__builtin_bswap64(v); }

template <>
inline int32_t byteswap<int32_t>(int32_t v) { return (int32_t)__builtin_bswap32(v); }

template <>
inline int16_t byteswap<int16_t>(int16_t v) { return (int16_t)__builtin_bswap16(v); }
#endif

template <>
inline uint8_t byteswap<uint8_t>(uint8_t v) { return v; }

template <>
inline int8_t byteswap<int8_t>(int8_t v) { return v; }

// Used to implement a type-safe and alignment-safe copy operation
// If you want to avoid the memcpy, you must write specializations for these functions
template <typename T> L_ALWAYS_INLINE void lToUnaligned(const T src, void *dest)
{
#if defined __has_builtin
#if __has_builtin(__builtin_memcpy)
    __builtin_memcpy(dest, &src, sizeof(T));
#else
    memcpy(dest, &src, sizeof(T));
#endif
#else
    memcpy(dest, &src, sizeof(T));
#endif
}

template <typename T> L_ALWAYS_INLINE T lFromUnaligned(const void *src)
{
    T dest;
#if defined __has_builtin
#if __has_builtin(__builtin_memcpy)
    __builtin_memcpy(&dest, src, sizeof(T));
#else
    memcpy(&dest, src, sizeof(T));
#endif
#else
    memcpy(&dest, src, sizeof(T));
#endif
    return dest;
}

// floating specializations
template<typename Float>
Float lbswapFloatHelper(Float source)
{
    // memcpy call in lFromUnaligned is recognized by optimizer as a correct way of type prunning
    auto temp = lFromUnaligned<typename LIntegerForSizeof<Float>::Unsigned>(&source);
    temp = byteswap(temp);
    return lFromUnaligned<Float>(&temp);
}

inline float byteswap(float source)
{
    return lbswapFloatHelper(source);
}

inline double byteswap(double source)
{
    return lbswapFloatHelper(source);
}


/*
 * lbswap(const T src, const void *dest);
 * Changes the byte order of \a src from big endian to little endian or vice versa
 * and stores the result in \a dest.
 * There is no alignment requirements for \a dest.
*/
template <typename T> inline void byteswap(const T src, void *dest)
{
    lToUnaligned<T>(byteswap(src), dest);
}

template <int Size> void *byteswap(const void *source, lsizetype count, void *dest) noexcept;
template<> inline void *byteswap<1>(const void *source, lsizetype count, void *dest) noexcept
{
    return source != dest ? memcpy(dest, source, size_t(count)) : dest;
}
template<> LKCOREDEF void *byteswap<2>(const void *source, lsizetype count, void *dest) noexcept;
template<> LKCOREDEF void *byteswap<4>(const void *source, lsizetype count, void *dest) noexcept;
template<> LKCOREDEF void *byteswap<8>(const void *source, lsizetype count, void *dest) noexcept;

LK_NAMESPACE_END
#endif // LBYTESWAP_H
