#include "lbyteswap.h"
LK_NAMESPACE_BEGIN

#if defined(__SSSE3__)
    using ShuffleMask = uint8_t[16];
L_DECL_ALIGN(16) static const ShuffleMask shuffleMasks[3] = {
    // 16-bit
    {1, 0, 3, 2,  5, 4, 7, 6,  9, 8, 11, 10,  13, 12, 15, 14},
    // 32-bit
    {3, 2, 1, 0,  7, 6, 5, 4,  11, 10, 9, 8,  15, 14, 13, 12},
    // 64-bit
    {7, 6, 5, 4, 3, 2, 1, 0,   15, 14, 13, 12, 11, 10, 9, 8}
};

static size_t sseSwapLoop(const uint8_t *src, size_t bytes, uint8_t *dst,
                          const __m128i *shuffleMaskPtr) noexcept
{
    size_t i = 0;
    const __m128i shuffleMask = _mm_load_si128(shuffleMaskPtr);

#  ifdef __AVX2__
    const __m256i shuffleMask256 = _mm256_inserti128_si256(_mm256_castsi128_si256(shuffleMask), shuffleMask, 1);
    for ( ; i + sizeof(__m256i) <= bytes; i += sizeof(__m256i)) {
        __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + i));
        data = _mm256_shuffle_epi8(data, shuffleMask256);
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst + i), data);
    }
#  else
    for ( ; i + 2 * sizeof(__m128i) <= bytes; i += 2 * sizeof(__m128i)) {
        __m128i data1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + i));
        __m128i data2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + i) + 1);
        data1 = _mm_shuffle_epi8(data1, shuffleMask);
        data2 = _mm_shuffle_epi8(data2, shuffleMask);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i), data1);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i) + 1, data2);
    }
#  endif

    if (i + sizeof(__m128i) <= bytes) {
        __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + i));
        data = _mm_shuffle_epi8(data, shuffleMask);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i), data);
        i += sizeof(__m128i);
    }

    return i;
}
L_ALWAYS_INLINE
template <typename T> static
    size_t simdSwapLoop(const uint8_t *src, size_t bytes, uint8_t *dst) noexcept
{
    auto shuffleMaskPtr = reinterpret_cast<const __m128i *>(shuffleMasks[0]);
    shuffleMaskPtr += qCountTrailingZeroBits(sizeof(T)) - 1;
    size_t i = sseSwapLoop(src, bytes, dst, shuffleMaskPtr);

    // epilogue
    for (size_t _i = 0 ; i < bytes && _i < sizeof(__m128i); i += sizeof(T), _i += sizeof(T))
        byteswap(qFromUnaligned<T>(src + i), dst + i);

    // return the total, so the bswapLoop below does nothing
    return bytes;
}
#elif defined(__SSE2__)
    template <typename T> static
    size_t simdSwapLoop(const uint8_t *, size_t, uint8_t *) noexcept
{
    // no generic version: we can't do 32- and 64-bit swaps easily,
    // so we won't try
    return 0;
}

template <> size_t simdSwapLoop<uint16_t>(const uint8_t *src, size_t bytes, uint8_t *dst) noexcept
{
    auto swapEndian = [](__m128i &data) {
        __m128i lows = _mm_srli_epi16(data, 8);
        __m128i highs = _mm_slli_epi16(data, 8);
        data = _mm_xor_si128(lows, highs);
    };

    size_t i = 0;
    for ( ; i + 2 * sizeof(__m128i) <= bytes; i += 2 * sizeof(__m128i)) {
        __m128i data1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + i));
        __m128i data2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + i) + 1);
        swapEndian(data1);
        swapEndian(data2);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i), data1);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i) + 1, data2);
    }

    if (i + sizeof(__m128i) <= bytes) {
        __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + i));
        swapEndian(data);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i), data);
        i += sizeof(__m128i);
    }

    // epilogue
    for (size_t _i = 0 ; i < bytes && _i < sizeof(__m128i); i += sizeof(uint16_t), _i += sizeof(uint16_t))
        byteswap(qFromUnaligned<uint16_t>(src + i), dst + i);

    // return the total, so the bswapLoop below does nothing
    return bytes;
}
#else
template <typename T>
static L_ALWAYS_INLINE size_t simdSwapLoop(const uint8_t *, size_t, uint8_t *) noexcept
{
    return 0;
}
#endif

template <typename T>
static L_ALWAYS_INLINE void *bswapLoop(const uint8_t *src, size_t n, uint8_t *dst) noexcept
{
    size_t i = simdSwapLoop<T>(src, n, dst);

    for ( ; i < n; i += sizeof(T)){
        byteswap(lFromUnaligned<T>(src + i), dst + i);
    }
    return dst + i;
}

template <> void *byteswap<2>(const void *source, lsizetype n, void *dest) noexcept
{
    const uint8_t *src = reinterpret_cast<const uint8_t *>(source);
    uint8_t *dst = reinterpret_cast<uint8_t *>(dest);

    return bswapLoop<uint16_t>(src, n << 1, dst);
}

template <> void *byteswap<4>(const void *source, lsizetype n, void *dest) noexcept
{
    const uint8_t *src = reinterpret_cast<const uint8_t *>(source);
    uint8_t *dst = reinterpret_cast<uint8_t *>(dest);

    return bswapLoop<uint32_t>(src, n << 2, dst);
}

template <> void *byteswap<8>(const void *source, lsizetype n, void *dest) noexcept
{
    const uint8_t *src = reinterpret_cast<const uint8_t *>(source);
    uint8_t *dst = reinterpret_cast<uint8_t *>(dest);

    return bswapLoop<uint64_t>(src, n << 3, dst);
}




LK_NAMESPACE_END
