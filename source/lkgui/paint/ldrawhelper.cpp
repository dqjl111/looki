#include "ldrawhelper.h"
#include "lrasterpaintengine.h"
#include "lcolortrclut.h"
#include "lkgui/lguiapplication.h"


LK_NAMESPACE_BEGIN
namespace draw {
//////////////////
void convertIndexedToARGB32PM(uint32_t *buffer, int count, const std::vector<uint32_t> *clut)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::premultiply(clut->at(buffer[i]));
    }
}

const uint64_t* convertIndexedToRGBA64PM(uint64_t *buffer,
                                                const uint32_t *src,
                                                int count,
                                                const std::vector<uint32_t> *clut,
                                                LPoint<int> *)
{
    LColor c;
    for (int i = 0; i < count; ++i){
        c.fromUInt(LColor::premultiply(clut->at(src[i])));
        buffer[i] = c.toUInt64();
    }
    return buffer;
}


const uint64_t* convertRGB32ToRGB64(uint64_t *buffer,
                                       const uint32_t *src,
                                       int count,
                                       const std::vector<uint32_t>*,
                                       LPoint<int>*)
{
    LColor c;
    for (int i = 0; i < count; ++i){
        c.fromUInt(0xff000000 | src[i]);
        buffer[i] = c.toUInt64();
    }
    return buffer;
}

const uint64_t* fetchRGB32ToRGB64(uint64_t *buffer,
                                 const uint8_t *src,
                                 int index,
                                 int count,
                                 const std::vector<uint32_t> *,
                                 LPoint<int> *)
{
    return convertRGB32ToRGB64(buffer, reinterpret_cast<const uint32_t *>(src) + index, count, nullptr, nullptr);
}

const uint64_t* convertARGB32ToRGBA64PM(uint64_t *buffer,
                                       const uint32_t *src,
                                       int count,
                                       const std::vector<uint32_t> *,
                                       LPoint<int> *)
{
    LColor c;
    for (int i = 0; i < count; ++i){
        c.fromUInt(LColor::premultiply(src[i]));
        buffer[i] = c.toUInt64();
    }
    return buffer;
}

static const uint64_t* fetchARGB32ToRGBA64PM(uint64_t *buffer,
                                             const uint8_t *src,
                                             int index,
                                             int count,
                                             const std::vector<uint32_t> *,
                                             LPoint<int> *)
{
    return convertARGB32ToRGBA64PM(buffer, reinterpret_cast<const uint32_t *>(src) + index, count, nullptr, nullptr);
}

static const uint64_t* convertARGB32PMToRGBA64PM(uint64_t *buffer,
                                                 const uint32_t *src,
                                                 int count,
                                                 const std::vector<uint32_t> *,
                                                 LPoint<int> *)
{
    LColor c;
    for (int i = 0; i < count; ++i){
        c.fromUInt(src[i]);
        buffer[i] = c.toUInt64();
    }
    return buffer;
}

static const uint64_t* fetchARGB32PMToRGBA64PM(uint64_t *buffer,
                                               const uint8_t *src,
                                               int index,
                                               int count,
                                               const std::vector<uint32_t> *,
                                               LPoint<int> *)
{
    return convertARGB32PMToRGBA64PM(buffer, reinterpret_cast<const uint32_t *>(src) + index, count, nullptr, nullptr);
}


template <BPP bpp>
static uint32_t fetchPixel(const uint8_t *, int)
{
    return 0;
}

template <>
inline uint32_t fetchPixel<BPP1LSB>(const uint8_t *src, int index)
{
    return (src[index >> 3] >> (index & 7)) & 1;
}

template <>
inline uint32_t fetchPixel<BPP1MSB>(const uint8_t *src, int index)
{
    return (src[index >> 3] >> (~index & 7)) & 1;
}

template <>
inline uint32_t fetchPixel<BPP8>(const uint8_t *src, int index)
{
    return src[index];
}

template <>
inline uint32_t fetchPixel<BPP16>(const uint8_t *src, int index)
{
    return reinterpret_cast<const uint16_t *>(src)[index];
}

template <>
inline uint32_t fetchPixel<BPP24>(const uint8_t *src, int index)
{
    return reinterpret_cast<const uint24_t *>(src)[index];
}

template <>
inline uint32_t fetchPixel<BPP32>(const uint8_t *src, int index)
{
    return reinterpret_cast<const uint32_t*>(src)[index];
}

template <>
inline uint32_t fetchPixel<BPP64>(const uint8_t *src, int index)
{
    // We have to do the conversion in fetch to fit into a 32bit uint
    uint64_t c = reinterpret_cast<const uint64_t *>(src)[index];
    return LColor::argb64ToArgb(c);
}

template <BPP bpp>
static uint64_t fetchPixel64(const uint8_t *src, int index)
{
    //std::assert(bpp != BPP64);
    return fetchPixel<bpp>(src, index);
}

template <BPP width>
static void  storePixel(uint8_t *dest, int index, uint32_t pixel);

template <>
inline void storePixel<BPP16>(uint8_t *dest, int index, uint32_t pixel)
{
    reinterpret_cast<uint16_t *>(dest)[index] = uint16_t(pixel);
}

template <>
inline void  storePixel<BPP24>(uint8_t *dest, int index, uint32_t pixel)
{
    reinterpret_cast<uint24_t *>(dest)[index] = uint24_t(pixel);
}

//////////////////
template<BPP bpp>
static const uint32_t* fetchIndexedToARGB32PM(uint32_t *buffer,
                                              const uint8_t *src,
                                              int index,
                                              int count,
                                              const std::vector<uint32_t> *clut,
                                              LPoint<int> *)
{
    for (int i = 0; i < count; ++i) {
        const uint32_t s = fetchPixel<bpp>(src, index + i);
        buffer[i] = LColor::premultiply(clut->at(s));
    }
    return buffer;
}

template<BPP bpp>
static const uint64_t* fetchIndexedToRGBA64PM(uint64_t *buffer,
                                              const uint8_t *src,
                                              int index,
                                              int count,
                                              const std::vector<uint32_t> *clut,
                                              LPoint<int> *)
{
    LColor c;
    for (int i = 0; i < count; ++i) {
        const uint32_t s = fetchPixel<bpp>(src, index + i);
        c.fromUInt(LColor::premultiply(clut->at(s)));
        buffer[i] = c.toUInt64();
    }
    return buffer;
}

/////////////////
void rbSwap_rgb32(uint8_t *d, const uint8_t *s, int count)
{
    const uint32_t *src = reinterpret_cast<const uint32_t *>(s);
    uint32_t *dest = reinterpret_cast<uint32_t *>(d);
    for (int i = 0; i < count; ++i) {
        const uint32_t c = src[i];
        const uint32_t ag = c & 0xff00ff00;
        const uint32_t rb = c & 0x00ff00ff;
        dest[i] = ag | (rb << 16) | (rb >> 16);
    }
}

static void convertPassThrough(uint32_t*, int, const std::vector<uint32_t> *)
{
}

static const uint32_t* fetchPassThrough(uint32_t*,
                                        const uint8_t* src,
                                        int index, int,
                                        const std::vector<uint32_t> *, LPoint<int> *)
{
    return reinterpret_cast<const uint32_t *>(src) + index;
}

static const uint64_t* fetchPassThrough64(uint64_t *, const uint8_t *src,
                                          int index, int,
                                          const std::vector<uint32_t> *, LPoint<int> *)
{
    return reinterpret_cast<const uint64_t *>(src) + index;
}

static void storePassThrough(uint8_t *dest,
                             const uint32_t *src,
                             int index, int count,
                             const std::vector<uint32_t> *, LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    if (d != src){
        memcpy(d, src, count * sizeof(uint32_t));
    }
}

// We manually unalias the variables to make sure the compiler
// fully optimizes both aliased and unaliased cases.
#define UNALIASED_CONVERSION_LOOP(buffer, src, count, conversion) \
if (src == buffer) { \
        for (int i = 0; i < count; ++i) \
        buffer[i] = conversion(buffer[i]); \
} else { \
        for (int i = 0; i < count; ++i) \
        buffer[i] = conversion(src[i]); \
}


static inline const uint32_t *lConvertARGB32ToARGB32PM(uint32_t *buffer, const uint32_t *src, int count)
{
    UNALIASED_CONVERSION_LOOP(buffer, src, count, LColor::premultiply);
    return buffer;
}

static inline const uint32_t *lConvertABGR32ToARGB32PM(uint32_t *buffer, const uint32_t *src, int count)
{
    UNALIASED_CONVERSION_LOOP(buffer, src, count, [](uint32_t s) { return LColor::premultiply(LColor::abgrToArgb(s));});
    return buffer;
}


static void convertARGB32ToARGB32PM(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    lConvertARGB32ToARGB32PM(buffer, buffer, count);
}

static const uint32_t* fetchARGB32ToARGB32PM(uint32_t *buffer,
                                             const uint8_t *src,
                                             int index,
                                             int count,
                                             const std::vector<uint32_t> *,
                                             LPoint<int> *)
{
    return lConvertARGB32ToARGB32PM(buffer, reinterpret_cast<const uint32_t *>(src) + index, count);
}


static void storeARGB32FromARGB32PM(uint8_t *dest,
                                    const uint32_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, [](uint32_t c) { return LColor::unpremultiply(c); });
}


static void storeXBGRFromARGB32PM(uint8_t *dest,
                                  const uint32_t *src, int index, int count,
                                  const std::vector<uint32_t> *,
                                  LPoint<int> *)
{

    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, [](uint32_t c) {
        return LColor::argbToAbgr(0xff000000 | LColor::unpremultiply(c));
    });
}

static void storeXBGRFromRGB32(uint8_t *dest,
                               const uint32_t *src,
                               int index,
                               int count,
                               const std::vector<uint32_t> *,
                               LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, [](uint32_t c) { return LColor::argbToAbgr(0xff000000 | c); });
}

static void storeABGR32FromARGB32PM(uint8_t *dest,
                                      const uint32_t *src,
                                      int index,
                                      int count,
                                      const std::vector<uint32_t> *,
                                      LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, [](uint32_t c) {
        return LColor::argbToAbgr(LColor::unpremultiply(c));
    });
}

static void storeABGR32PMFromARGB32PM(uint8_t *dest,
                                        const uint32_t *src,
                                        int index,
                                        int count,
                                        const std::vector<uint32_t> *,
                                        LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, LColor::argbToAbgr);
}

template<LImage::Format> uint32_t redWidth();
template<LImage::Format> uint32_t redShift();
template<LImage::Format> uint32_t greenWidth();
template<LImage::Format> uint32_t greenShift();
template<LImage::Format> uint32_t blueWidth();
template<LImage::Format> uint32_t blueShift();
template<LImage::Format> uint32_t alphaWidth();
template<LImage::Format> uint32_t alphaShift();

template<> uint32_t redWidth<LImage::Format::RGB16>() { return 5; }
template<> uint32_t redWidth<LImage::Format::RGB444>() { return 4; }
template<> uint32_t redWidth<LImage::Format::RGB555>() { return 5; }
template<> uint32_t redWidth<LImage::Format::RGB666>() { return 6; }
template<> uint32_t redWidth<LImage::Format::RGB888>() { return 8; }
template<> uint32_t redWidth<LImage::Format::BGR888>() { return 8; }
template<> uint32_t redWidth<LImage::Format::ARGB4444Premultiplied>() { return 4; }
template<> uint32_t redWidth<LImage::Format::ARGB8555Premultiplied>() { return 5; }
template<> uint32_t redWidth<LImage::Format::ARGB8565Premultiplied>() { return 5; }
template<> uint32_t redWidth<LImage::Format::ARGB6666Premultiplied>() { return 6; }
template<> uint32_t redWidth<LImage::Format::XBGR32>() { return 8; }
template<> uint32_t redWidth<LImage::Format::ABGR32>() { return 8; }
template<> uint32_t redWidth<LImage::Format::ABGR32Premultiplied>() { return 8; }

template<> uint32_t redShift<LImage::Format::RGB16>() { return  11; }
template<> uint32_t redShift<LImage::Format::RGB444>() { return  8; }
template<> uint32_t redShift<LImage::Format::RGB555>() { return 10; }
template<> uint32_t redShift<LImage::Format::RGB666>() { return 12; }
template<> uint32_t redShift<LImage::Format::RGB888>() { return 16; }
template<> uint32_t redShift<LImage::Format::BGR888>() { return 0; }
template<> uint32_t redShift<LImage::Format::ARGB4444Premultiplied>() { return  8; }
template<> uint32_t redShift<LImage::Format::ARGB8555Premultiplied>() { return 18; }
template<> uint32_t redShift<LImage::Format::ARGB8565Premultiplied>() { return 19; }
template<> uint32_t redShift<LImage::Format::ARGB6666Premultiplied>() { return 12; }
template<> uint32_t redShift<LImage::Format::XBGR32>() { return 0; }
template<> uint32_t redShift<LImage::Format::ABGR32>() { return 0; }
template<> uint32_t redShift<LImage::Format::ABGR32Premultiplied>() { return 0; }

template<> uint32_t greenWidth<LImage::Format::RGB16>() { return 6; }
template<> uint32_t greenWidth<LImage::Format::RGB444>() { return 4; }
template<> uint32_t greenWidth<LImage::Format::RGB555>() { return 5; }
template<> uint32_t greenWidth<LImage::Format::RGB666>() { return 6; }
template<> uint32_t greenWidth<LImage::Format::RGB888>() { return 8; }
template<> uint32_t greenWidth<LImage::Format::BGR888>() { return 8; }
template<> uint32_t greenWidth<LImage::Format::ARGB4444Premultiplied>() { return 4; }
template<> uint32_t greenWidth<LImage::Format::ARGB8555Premultiplied>() { return 5; }
template<> uint32_t greenWidth<LImage::Format::ARGB8565Premultiplied>() { return 6; }
template<> uint32_t greenWidth<LImage::Format::ARGB6666Premultiplied>() { return 6; }
template<> uint32_t greenWidth<LImage::Format::XBGR32>() { return 8; }
template<> uint32_t greenWidth<LImage::Format::ABGR32>() { return 8; }
template<> uint32_t greenWidth<LImage::Format::ABGR32Premultiplied>() { return 8; }

template<> uint32_t greenShift<LImage::Format::RGB16>() { return  5; }
template<> uint32_t greenShift<LImage::Format::RGB444>() { return 4; }
template<> uint32_t greenShift<LImage::Format::RGB555>() { return 5; }
template<> uint32_t greenShift<LImage::Format::RGB666>() { return 6; }
template<> uint32_t greenShift<LImage::Format::RGB888>() { return 8; }
template<> uint32_t greenShift<LImage::Format::BGR888>() { return 8; }
template<> uint32_t greenShift<LImage::Format::ARGB4444Premultiplied>() { return  4; }
template<> uint32_t greenShift<LImage::Format::ARGB8555Premultiplied>() { return 13; }
template<> uint32_t greenShift<LImage::Format::ARGB8565Premultiplied>() { return 13; }
template<> uint32_t greenShift<LImage::Format::ARGB6666Premultiplied>() { return  6; }
template<> uint32_t greenShift<LImage::Format::XBGR32>() { return 8; }
template<> uint32_t greenShift<LImage::Format::ABGR32>() { return 8; }
template<> uint32_t greenShift<LImage::Format::ABGR32Premultiplied>() { return 8; }
template<> uint32_t blueWidth<LImage::Format::RGB16>() { return 5; }
template<> uint32_t blueWidth<LImage::Format::RGB444>() { return 4; }
template<> uint32_t blueWidth<LImage::Format::RGB555>() { return 5; }
template<> uint32_t blueWidth<LImage::Format::RGB666>() { return 6; }
template<> uint32_t blueWidth<LImage::Format::RGB888>() { return 8; }
template<> uint32_t blueWidth<LImage::Format::BGR888>() { return 8; }
template<> uint32_t blueWidth<LImage::Format::ARGB4444Premultiplied>() { return 4; }
template<> uint32_t blueWidth<LImage::Format::ARGB8555Premultiplied>() { return 5; }
template<> uint32_t blueWidth<LImage::Format::ARGB8565Premultiplied>() { return 5; }
template<> uint32_t blueWidth<LImage::Format::ARGB6666Premultiplied>() { return 6; }
template<> uint32_t blueWidth<LImage::Format::XBGR32>() { return 8; }
template<> uint32_t blueWidth<LImage::Format::ABGR32>() { return 8; }
template<> uint32_t blueWidth<LImage::Format::ABGR32Premultiplied>() { return 8; }

template<> uint32_t blueShift<LImage::Format::RGB16>() { return 0; }
template<> uint32_t blueShift<LImage::Format::RGB444>() { return 0; }
template<> uint32_t blueShift<LImage::Format::RGB555>() { return 0; }
template<> uint32_t blueShift<LImage::Format::RGB666>() { return 0; }
template<> uint32_t blueShift<LImage::Format::RGB888>() { return 0; }
template<> uint32_t blueShift<LImage::Format::BGR888>() { return 16; }
template<> uint32_t blueShift<LImage::Format::ARGB4444Premultiplied>() { return 0; }
template<> uint32_t blueShift<LImage::Format::ARGB8555Premultiplied>() { return 8; }
template<> uint32_t blueShift<LImage::Format::ARGB8565Premultiplied>() { return 8; }
template<> uint32_t blueShift<LImage::Format::ARGB6666Premultiplied>() { return 0; }
template<> uint32_t blueShift<LImage::Format::XBGR32>() { return 16; }
template<> uint32_t blueShift<LImage::Format::ABGR32>() { return 16; }
template<> uint32_t blueShift<LImage::Format::ABGR32Premultiplied>() { return 16; }
template<> uint32_t alphaWidth<LImage::Format::RGB16>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::RGB444>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::RGB555>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::RGB666>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::RGB888>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::BGR888>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::ARGB4444Premultiplied>() { return  4; }
template<> uint32_t alphaWidth<LImage::Format::ARGB8555Premultiplied>() { return  8; }
template<> uint32_t alphaWidth<LImage::Format::ARGB8565Premultiplied>() { return  8; }
template<> uint32_t alphaWidth<LImage::Format::ARGB6666Premultiplied>() { return  6; }
template<> uint32_t alphaWidth<LImage::Format::XBGR32>() { return 0; }
template<> uint32_t alphaWidth<LImage::Format::ABGR32>() { return 8; }
template<> uint32_t alphaWidth<LImage::Format::ABGR32Premultiplied>() { return 8; }

template<> uint32_t alphaShift<LImage::Format::RGB16>() { return 0; }
template<> uint32_t alphaShift<LImage::Format::RGB444>() { return 0; }
template<> uint32_t alphaShift<LImage::Format::RGB555>() { return 0; }
template<> uint32_t alphaShift<LImage::Format::RGB666>() { return 0; }
template<> uint32_t alphaShift<LImage::Format::RGB888>() { return 0; }
template<> uint32_t alphaShift<LImage::Format::BGR888>() { return 0; }
template<> uint32_t alphaShift<LImage::Format::ARGB4444Premultiplied>() { return 12; }
template<> uint32_t alphaShift<LImage::Format::ARGB8555Premultiplied>() { return  0; }
template<> uint32_t alphaShift<LImage::Format::ARGB8565Premultiplied>() { return  0; }
template<> uint32_t alphaShift<LImage::Format::ARGB6666Premultiplied>() { return 18; }

template<> uint32_t alphaShift<LImage::Format::XBGR32>() { return 24; }
template<> uint32_t alphaShift<LImage::Format::ABGR32>() { return 24; }
template<> uint32_t alphaShift<LImage::Format::ABGR32Premultiplied>() { return 24; }

template<LImage::Format> constexpr BPP bitsPerPixel();
template<> constexpr BPP bitsPerPixel<LImage::Format::RGB16>() { return BPP16; }
template<> constexpr BPP bitsPerPixel<LImage::Format::RGB444>() { return BPP16; }
template<> constexpr BPP bitsPerPixel<LImage::Format::RGB555>() { return BPP16; }
template<> constexpr BPP bitsPerPixel<LImage::Format::RGB666>() { return BPP24; }
template<> constexpr BPP bitsPerPixel<LImage::Format::RGB888>() { return BPP24; }
template<> constexpr BPP bitsPerPixel<LImage::Format::BGR888>() { return BPP24; }
template<> constexpr BPP bitsPerPixel<LImage::Format::ARGB4444Premultiplied>() { return BPP16; }
template<> constexpr BPP bitsPerPixel<LImage::Format::ARGB8555Premultiplied>() { return BPP24; }
template<> constexpr BPP bitsPerPixel<LImage::Format::ARGB8565Premultiplied>() { return BPP24; }
template<> constexpr BPP bitsPerPixel<LImage::Format::ARGB6666Premultiplied>() { return BPP24; }
template<> constexpr BPP bitsPerPixel<LImage::Format::XBGR32>() { return BPP32; }
template<> constexpr BPP bitsPerPixel<LImage::Format::ABGR32>() { return BPP32; }
template<> constexpr BPP bitsPerPixel<LImage::Format::ABGR32Premultiplied>() { return BPP32; }


template<LImage::Format Format>
static void rbSwap(uint8_t *dst, const uint8_t *src, int count)
{
    const uint8_t aWidth = alphaWidth<Format>();
    const uint8_t aShift = alphaShift<Format>();
    const uint8_t rWidth = redWidth<Format>();
    const uint8_t rShift = redShift<Format>();
    const uint8_t gWidth = greenWidth<Format>();
    const uint8_t gShift = greenShift<Format>();
    //constexpr uint8_t bWidth = blueWidth<Format>(); //bWidth == rWIdth
    const uint8_t bShift = blueShift<Format>();

    const uint32_t redBlueMask = (1 << rWidth) - 1;
    const uint32_t alphaGreenMask = (((1 << aWidth) - 1) << aShift)
                                        | (((1 << gWidth) - 1) << gShift);
    constexpr BPP bpp = bitsPerPixel<Format>();

    for (int i = 0; i < count; ++i) {
        const uint32_t c = fetchPixel<bpp>(src, i);
        const uint32_t r = (c >> rShift) & redBlueMask;
        const uint32_t b = (c >> bShift) & redBlueMask;
        const uint32_t t = (c & alphaGreenMask)
                           | (r << bShift)
                           | (b << rShift);
        storePixel<bpp>(dst, i, t);
    }
}

template<>
void rbSwap<LImage::Format::ABGR32>(uint8_t *d, const uint8_t *s, int count)
{
    return rbSwap_rgb32(d, s, count);
}

static void rbSwap_rgb30(uint8_t *d, const uint8_t *s, int count)
{
    const uint32_t *src = reinterpret_cast<const uint32_t *>(s);
    uint32_t *dest = reinterpret_cast<uint32_t *>(d);
    UNALIASED_CONVERSION_LOOP(dest, src, count, LColor::rgbSwapRgb30);
}

template<LImage::Format Format>
static uint32_t convertPixelToRGB32(uint32_t s)
{
    const uint32_t redMask = ((1 << redWidth<Format>()) - 1);
    const uint32_t greenMask = ((1 << greenWidth<Format>()) - 1);
    const uint32_t blueMask = ((1 << blueWidth<Format>()) - 1);

    const uint8_t redLeftShift = 8 - redWidth<Format>();
    const uint8_t greenLeftShift = 8 - greenWidth<Format>();
    const uint8_t blueLeftShift = 8 - blueWidth<Format>();

    const uint8_t redRightShift = 2 * redWidth<Format>() - 8;
    const uint8_t greenRightShift = 2 * greenWidth<Format>() - 8;
    const uint8_t blueRightShift = 2 * blueWidth<Format>() - 8;

    uint32_t red   = (s >> redShift<Format>()) & redMask;
    uint32_t green = (s >> greenShift<Format>()) & greenMask;
    uint32_t blue  = (s >> blueShift<Format>()) & blueMask;

    red = ((red << redLeftShift) | (red >> redRightShift)) << 16;
    green = ((green << greenLeftShift) | (green >> greenRightShift)) << 8;
    blue = (blue << blueLeftShift) | (blue >> blueRightShift);
    return 0xff000000 | red | green | blue;
}

template<LImage::Format Format>
static void convertToRGB32(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToRGB32<Format>(buffer[i]);
    }
}

template<LImage::Format Format>
static uint64_t convertPixelToRGB64(uint32_t s)
{
    LColor c;
    c.fromUInt(convertPixelToRGB32<Format>(s));
    return c.toUInt64();
}

template<LImage::Format Format>
static const uint64_t* convertToRGB64(uint64_t *buffer,
                                      const uint32_t *src,
                                      int count,
                                      const std::vector<uint32_t> *clut,
                                      LPoint<int> *dither)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToRGB64<Format>(src[i]);
    }
    return buffer;
}

template<LImage::Format Format>
static const uint32_t* fetchRGBToRGB32(uint32_t* buffer,
                                       const uint8_t *src,
                                       int index,
                                       int count,
                                       const std::vector<uint32_t> *clut,
                                       LPoint<int> *dither)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToRGB32<Format>(fetchPixel<bitsPerPixel<Format>()>(src, index + i));
    }
    return buffer;
}


template<LImage::Format Format>
static const uint64_t* fetchRGBToRGB64(uint64_t *buffer,
                                       const uint8_t *src,
                                       int index,
                                       int count,
                                       const std::vector<uint32_t> *clut,
                                       LPoint<int> *dither)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToRGB64<Format>(fetchPixel<bitsPerPixel<Format>()>(src, index + i));
    }
    return buffer;
}


template<LImage::Format Format, bool fromRGB>
static void storeRGBFromARGB32PM(uint8_t *dest,
                                 const uint32_t *src,
                                 int index,
                                 int count,
                                 const std::vector<uint32_t> *clut,
                                 LPoint<int> *dither)
{
    const uint8_t rWidth = redWidth<Format>();
    const uint8_t gWidth = greenWidth<Format>();
    const uint8_t bWidth = blueWidth<Format>();
    constexpr BPP bpp = bitsPerPixel<Format>();

    // RGB32 -> RGB888 is not a precision loss.
    if (!dither || (rWidth == 8 && gWidth == 8 && bWidth == 8)) {
        const uint32_t rMask = (1 << redWidth<Format>()) - 1;
        const uint32_t gMask = (1 << greenWidth<Format>()) - 1;
        const uint32_t bMask = (1 << blueWidth<Format>()) - 1;
        const uint8_t rRightShift = 24 - redWidth<Format>();
        const uint8_t gRightShift = 16 - greenWidth<Format>();
        const uint8_t bRightShift =  8 - blueWidth<Format>();

        for (int i = 0; i < count; ++i) {
            const uint32_t c = fromRGB ? src[i] : LColor::unpremultiply(src[i]);
            const uint32_t r = ((c >> rRightShift) & rMask) << redShift<Format>();
            const uint32_t g = ((c >> gRightShift) & gMask) << greenShift<Format>();
            const uint32_t b = ((c >> bRightShift) & bMask) << blueShift<Format>();
            storePixel<bpp>(dest, index + i, r | g | b);
        };
    } else {
        // We do ordered dither by using a rounding conversion, but instead of
        // adding half of input precision, we add the adjusted result from the
        // bayer matrix before narrowing.
        // Note: Rounding conversion in itself is different from the naive
        // conversion we do above for non-dithering.
        const uint32_t *bayer_line = lBayerMatrix[dither->m_y & 15];
        for (int i = 0; i < count; ++i) {
            const uint32_t c = fromRGB ? src[i] : LColor::unpremultiply(src[i]);
            const int d = bayer_line[(dither->m_x + i) & 15];
            const int dr = d - ((d + 1) >> rWidth);
            const int dg = d - ((d + 1) >> gWidth);
            const int db = d - ((d + 1) >> bWidth);
            int r = lRed(c);
            int g = lGreen(c);
            int b = lBlue(c);
            r = (r + ((dr - r) >> rWidth) + 1) >> (8 - rWidth);
            g = (g + ((dg - g) >> gWidth) + 1) >> (8 - gWidth);
            b = (b + ((db - b) >> bWidth) + 1) >> (8 - bWidth);
            const uint32_t s = (r << redShift<Format>())
                               | (g << greenShift<Format>())
                               | (b << blueShift<Format>());
            storePixel<bpp>(dest, index + i, s);
        }
    }
}

template<LImage::Format Format, bool fromRGB>
static void storeARGBPMFromARGB32PM(uint8_t *dest,
                                    const uint32_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *clut,
                                    LPoint<int> *dither)
{
    constexpr BPP bpp = bitsPerPixel<Format>();
    if (!dither) {
        const uint32_t aMask = (1 << alphaWidth<Format>()) - 1;
        const uint32_t rMask = (1 << redWidth<Format>()) - 1;
        const uint32_t gMask = (1 << greenWidth<Format>()) - 1;
        const uint32_t bMask = (1 << blueWidth<Format>()) - 1;

        const uint8_t aRightShift = 32 - alphaWidth<Format>();
        const uint8_t rRightShift = 24 - redWidth<Format>();
        const uint8_t gRightShift = 16 - greenWidth<Format>();
        const uint8_t bRightShift =  8 - blueWidth<Format>();

        const uint32_t aOpaque = aMask << alphaShift<Format>();
        for (int i = 0; i < count; ++i) {
            const uint32_t c = src[i];
            const uint32_t a = fromRGB ? aOpaque : (((c >> aRightShift) & aMask) << alphaShift<Format>());
            const uint32_t r = ((c >> rRightShift) & rMask) << redShift<Format>();
            const uint32_t g = ((c >> gRightShift) & gMask) << greenShift<Format>();
            const uint32_t b = ((c >> bRightShift) & bMask) << blueShift<Format>();
            storePixel<bpp>(dest, index + i, a | r | g | b);
        };
    } else {
        const uint8_t aWidth = alphaWidth<Format>();
        const uint8_t rWidth = redWidth<Format>();
        const uint8_t gWidth = greenWidth<Format>();
        const uint8_t bWidth = blueWidth<Format>();

        const uint32_t *bayer_line = lBayerMatrix[dither->m_y & 15];
        for (int i = 0; i < count; ++i) {
            const uint32_t c = src[i];
            const int d = bayer_line[(dither->m_x + i) & 15];
            const int da = d - ((d + 1) >> aWidth);
            const int dr = d - ((d + 1) >> rWidth);
            const int dg = d - ((d + 1) >> gWidth);
            const int db = d - ((d + 1) >> bWidth);
            int a = lAlpha(c);
            int r = lRed(c);
            int g = lGreen(c);
            int b = lBlue(c);
            if (fromRGB){
                a = (1 << aWidth) - 1;
            }
            else{
                a = (a + ((da - a) >> aWidth) + 1) >> (8 - aWidth);
            }
            r = (r + ((dr - r) >> rWidth) + 1) >> (8 - rWidth);
            g = (g + ((dg - g) >> gWidth) + 1) >> (8 - gWidth);
            b = (b + ((db - b) >> bWidth) + 1) >> (8 - bWidth);
            uint32_t s = (a << alphaShift<Format>())
                         | (r << redShift<Format>())
                         | (g << greenShift<Format>())
                         | (b << blueShift<Format>());
            storePixel<bpp>(dest, index + i, s);
        }
    }
}

template<LImage::Format Format>
static uint32_t convertPixelToARGB32PM(uint32_t s)
{
    const uint32_t alphaMask = ((1 << alphaWidth<Format>()) - 1);
    const uint32_t redMask = ((1 << redWidth<Format>()) - 1);
    const uint32_t greenMask = ((1 << greenWidth<Format>()) - 1);
    const uint32_t blueMask = ((1 << blueWidth<Format>()) - 1);

    const uint8_t alphaLeftShift = 8 - alphaWidth<Format>();
    const uint8_t redLeftShift = 8 - redWidth<Format>();
    const uint8_t greenLeftShift = 8 - greenWidth<Format>();
    const uint8_t blueLeftShift = 8 - blueWidth<Format>();

    const uint8_t alphaRightShift = 2 * alphaWidth<Format>() - 8;
    const uint8_t redRightShift = 2 * redWidth<Format>() - 8;
    const uint8_t greenRightShift = 2 * greenWidth<Format>() - 8;
    const uint8_t blueRightShift = 2 * blueWidth<Format>() - 8;

    const bool mustMin = (alphaWidth<Format>() != redWidth<Format>()) ||
                             (alphaWidth<Format>() != greenWidth<Format>()) ||
                             (alphaWidth<Format>() != blueWidth<Format>());

    uint32_t alpha = (s >> alphaShift<Format>()) & alphaMask;
    uint32_t red   = (s >> redShift<Format>()) & redMask;
    uint32_t green = (s >> greenShift<Format>()) & greenMask;
    uint32_t blue  = (s >> blueShift<Format>()) & blueMask;

    alpha = (alpha << alphaLeftShift) | (alpha >> alphaRightShift);
    red   = (red << redLeftShift) | (red >> redRightShift);
    green = (green << greenLeftShift) | (green >> greenRightShift);
    blue  = (blue << blueLeftShift) | (blue >> blueRightShift);

    if (mustMin) {
        red   = std::min<uint32_t>(alpha, red);
        green = std::min<uint32_t>(alpha, green);
        blue  = std::min<uint32_t>(alpha, blue);
    }

    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

template<LImage::Format Format>
static void convertARGBPMToARGB32PM(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToARGB32PM<Format>(buffer[i]);
    }
}

template<LImage::Format Format>
static const uint32_t* fetchARGBPMToARGB32PM(uint32_t *buffer,
                                             const uint8_t *src,
                                             int index,
                                             int count,
                                             const std::vector<uint32_t> *clut,
                                             LPoint<int> *dither)
{
    constexpr BPP bpp = bitsPerPixel<Format>();
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToARGB32PM<Format>(fetchPixel<bpp>(src, index + i));
    }
    return buffer;
}

template<LImage::Format Format>
static uint64_t convertPixelToRGBA64PM(uint32_t s)
{
    LColor c;
    c.fromUInt(convertPixelToARGB32PM<Format>(s));
    return c.toUInt64();
}

template<LImage::Format Format>
static const uint64_t* convertARGBPMToRGBA64PM(uint64_t *buffer,
                                               const uint32_t *src,
                                               int count,
                                               const std::vector<uint32_t> *clut,
                                               LPoint<int> *dither)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToRGB64<Format>(src[i]);
    }
    return buffer;
}

template<LImage::Format Format>
static const uint64_t* fetchARGBPMToRGBA64PM(uint64_t *buffer,
                                             const uint8_t *src,
                                             int index,
                                             int count,
                                             const std::vector<uint32_t> *clut,
                                             LPoint<int> *dither)
{
    constexpr BPP bpp = bitsPerPixel<Format>();
    for (int i = 0; i < count; ++i){
        buffer[i] = convertPixelToRGBA64PM<Format>(fetchPixel<bpp>(src, index + i));
    }
    return buffer;
}


static void convertABGR32PMToARGB32PM(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::abgrToArgb(buffer[i]);
    }
}

static const uint32_t* fetchABGR32PMToARGB32PM(uint32_t *buffer,
                                                 const uint8_t *src,
                                                 int index,
                                                 int count,
                                                 const std::vector<uint32_t> *,
                                                 LPoint<int> *)
{
    const uint32_t *s  = reinterpret_cast<const uint32_t *>(src) + index;
    UNALIASED_CONVERSION_LOOP(buffer, s, count, LColor::abgrToArgb);
    return buffer;
}

static void convertABGR32ToARGB32PM(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    lConvertABGR32ToARGB32PM(buffer, buffer, count);
}

static const uint32_t* fetchABGR32ToARGB32PM(uint32_t *buffer,
                                               const uint8_t *src,
                                               int index,
                                               int count,
                                               const std::vector<uint32_t> *,
                                               LPoint<int> *)
{
    return lConvertABGR32ToARGB32PM(buffer, reinterpret_cast<const uint32_t *>(src) + index, count);
}


static const uint64_t* convertABGR32ToRGBA64PM(uint64_t *buffer,
                                                 const uint32_t *src,
                                                 int count,
                                                 const std::vector<uint32_t> *,
                                                 LPoint<int> *)
{
    LColor c;
    for (int i = 0; i < count; ++i){
        c.fromUInt(LColor::premultiply(LColor::abgrToArgb(src[i])));
        buffer[i] = c.toUInt64();
    }
    return buffer;
}

static const uint64_t* fetchABGR32ToRGBA64PM(uint64_t *buffer,
                                               const uint8_t *src,
                                               int index,
                                               int count,
                                               const std::vector<uint32_t> *,
                                               LPoint<int> *)
{
    return convertABGR32ToRGBA64PM(buffer, reinterpret_cast<const uint32_t *>(src) + index, count, nullptr, nullptr);
}

static const uint64_t* convertABGR32PMToRGBA64PM(uint64_t *buffer,
                                                   const uint32_t *src,
                                                   int count,
                                                   const std::vector<uint32_t> *,
                                                   LPoint<int> *)
{
    LColor c;
    for (int i = 0; i < count; ++i){
        c.fromUInt(LColor::abgrToArgb(src[i]));
        buffer[i] = c.toUInt64();
    }
    return buffer;
}

static const uint64_t* fetchABGR32PMToRGBA64PM(uint64_t *buffer,
                                                 const uint8_t *src,
                                                 int index,
                                                 int count,
                                                 const std::vector<uint32_t> *,
                                                 LPoint<int> *)
{
    return convertABGR32PMToRGBA64PM(buffer, reinterpret_cast<const uint32_t *>(src) + index, count, nullptr, nullptr);
}

template<PixelOrder order>
static void convertA2RGB30PMToARGB32PM(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = lConvertA2rgb30ToArgb32<order>(buffer[i]);
    }
}

template<PixelOrder order>
static const uint32_t* fetchA2RGB30PMToARGB32PM(uint32_t *buffer,
                                                const uint8_t *s,
                                                int index,
                                                int count,
                                                const std::vector<uint32_t> *,
                                                LPoint<int> *dither)
{
    const uint32_t *src = reinterpret_cast<const uint32_t *>(s) + index;
    if (!dither) {
        UNALIASED_CONVERSION_LOOP(buffer, src, count, lConvertA2rgb30ToArgb32<order>);
    } else {
        for (int i = 0; i < count; ++i) {
            const uint32_t c = src[i];
            short d10 = (lBayerMatrix[dither->m_y & 15][(dither->m_x + i) & 15] << 2);
            short a10 = (c >> 30) * 0x155;
            short r10 = ((c >> 20) & 0x3ff);
            short g10 = ((c >> 10) & 0x3ff);
            short b10 = (c & 0x3ff);
            if (order == PixelOrderBGR){
                std::swap(r10, b10);
            }
            short a8 = (a10 + ((d10 - a10) >> 8)) >> 2;
            short r8 = (r10 + ((d10 - r10) >> 8)) >> 2;
            short g8 = (g10 + ((d10 - g10) >> 8)) >> 2;
            short b8 = (b10 + ((d10 - b10) >> 8)) >> 2;
            buffer[i] = LColor::argbToUInt(r8, g8, b8, a8);
        }
    }
    return buffer;
}


template<PixelOrder order>
static const uint64_t* convertA2RGB30PMToRGBA64PM(uint64_t *buffer,
                                                  const uint32_t *src,
                                                  int count,
                                                  const std::vector<uint32_t> *,
                                                  LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = lConvertA2rgb30ToRgb64<order>(src[i]);
    }
    return buffer;
}

template<PixelOrder order>
static const uint64_t* fetchA2RGB30PMToRGBA64PM(uint64_t *buffer,
                                                const uint8_t *src,
                                                int index,
                                                int count,
                                                const std::vector<uint32_t> *,
                                                LPoint<int> *)
{
    return convertA2RGB30PMToRGBA64PM<order>(buffer, reinterpret_cast<const uint32_t *>(src) + index, count, nullptr, nullptr);
}


template<PixelOrder order>
static void storeA2RGB30PMFromARGB32PM(uint8_t *dest,
                                       const uint32_t *src,
                                       int index,
                                       int count,
                                       const std::vector<uint32_t> *,
                                       LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, lConvertArgb32ToA2rgb30<order>);
}

template<PixelOrder order>
static void storeRGB30FromRGB32(uint8_t *dest,
                                const uint32_t *src,
                                int index,
                                int count,
                                const std::vector<uint32_t> *,
                                LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, lConvertRgb32ToRgb30<order>);
}

template<PixelOrder order>
static void storeRGB30FromARGB32PM(uint8_t *dest,
                                   const uint32_t *src,
                                   int index,
                                   int count,
                                   const std::vector<uint32_t> *,
                                   LPoint<int> *)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    UNALIASED_CONVERSION_LOOP(d, src, count, lConvertRgb32ToRgb30<order>);
}


static void convertAlpha8ToRGB32(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argbToUInt(0, 0, 0, buffer[i]);
    }
}

static const uint32_t* fetchAlpha8ToRGB32(uint32_t *buffer,
                                          const uint8_t *src,
                                          int index,
                                          int count,
                                          const std::vector<uint32_t> *,
                                          LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argbToUInt(0, 0, 0, src[index + i]);
    }
    return buffer;
}

static const uint64_t* convertAlpha8ToRGB64(uint64_t *buffer,
                                            const uint32_t *src,
                                            int count,
                                            const std::vector<uint32_t> *,
                                            LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argbToUInt64(0, 0, 0, src[i]);
    }
    return buffer;
}
static const uint64_t* fetchAlpha8ToRGB64(uint64_t *buffer,
                                          const uint8_t *src,
                                          int index,
                                          int count,
                                          const std::vector<uint32_t> *,
                                          LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argbToUInt64(0, 0, 0, src[index + i]);
    }
    return buffer;
}

static void convertGrayscale8ToRGB32(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i) {
        const uint32_t s = buffer[i];
        buffer[i] = LColor::argbToUInt(s, s, s);
    }
}

static const uint32_t* fetchGrayscale8ToRGB32(uint32_t *buffer,
                                              const uint8_t *src,
                                              int index,
                                              int count,
                                              const std::vector<uint32_t> *,
                                              LPoint<int> *)
{
    for (int i = 0; i < count; ++i) {
        const uint32_t s = src[index + i];
        buffer[i] = LColor::argbToUInt(s, s, s);
    }
    return buffer;
}

static const uint64_t* convertGrayscale8ToRGB64(uint64_t *buffer,
                                                const uint32_t *src,
                                                int count,
                                                const std::vector<uint32_t> *,
                                                LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argbToUInt64(src[i], src[i], src[i], 255);
    }
    return buffer;
}

static const uint64_t* fetchGrayscale8ToRGB64(uint64_t *buffer,
                                              const uint8_t *src,
                                              int index,
                                              int count,
                                              const std::vector<uint32_t> *,
                                              LPoint<int> *)
{
    for (int i = 0; i < count; ++i) {
        const uint32_t s = src[index + i];
        buffer[i] = LColor::argbToUInt64(s, s, s, 255);
    }
    return buffer;
}

static void storeAlpha8FromARGB32PM(uint8_t *dest,
                                    const uint32_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        dest[index + i] = LColor::uintToAlpha(src[i]);
    }
}

static void storeGrayscale8FromRGB32(uint8_t *dest,
                                     const uint32_t *src,
                                     int index,
                                     int count,
                                     const std::vector<uint32_t> *,
                                     LPoint<int> *)
{
    for (int i = 0; i < count; ++i)
        dest[index + i] = LColor::toGray(src[i]);
}

static void storeGrayscale8FromARGB32PM(uint8_t *dest,
                                        const uint32_t *src,
                                        int index,
                                        int count,
                                        const std::vector<uint32_t> *,
                                        LPoint<int> *)
{
    for (int i = 0; i < count; ++i){
        dest[index + i] = LColor::toGray(LColor::unpremultiply(src[i]));
    }
}

static void storeGrayscale16FromRGB32(uint8_t *dest,
                                      const uint32_t *src,
                                      int index,
                                      int count,
                                      const std::vector<uint32_t> *,
                                      LPoint<int> *)
{
    unsigned short *d = reinterpret_cast<unsigned short *>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::toGray(src[i]) * 257;
    }
}

static void storeGrayscale16FromARGB32PM(uint8_t *dest,
                                         const uint32_t *src,
                                         int index,
                                         int count,
                                         const std::vector<uint32_t> *,
                                         LPoint<int> *)
{
    unsigned short *d = reinterpret_cast<unsigned short *>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::toGray(LColor::unpremultiply(src[i])) * 257;
    }
}

static const uint32_t* fetchRGB64ToRGB32(uint32_t *buffer,
                                         const uint8_t *src,
                                         int index,
                                         int count,
                                         const std::vector<uint32_t> *,
                                         LPoint<int> *)
{
    const uint64_t *s = reinterpret_cast<const uint64_t *>(src) + index;
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argb64ToArgb(s[i]);
    }
    return buffer;
}

static void storeRGB64FromRGB32(uint8_t *dest,
                                const uint32_t *src,
                                int index,
                                int count,
                                const std::vector<uint32_t> *,
                                LPoint<int> *)
{
    uint64_t *d = reinterpret_cast<uint64_t *>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::argbToArgb64(src[i]);
    }
}

static const uint32_t* fetchRGBA64ToARGB32PM(uint32_t *buffer,
                                             const uint8_t *src,
                                             int index,
                                             int count,
                                             const std::vector<uint32_t> *,
                                             LPoint<int> *)
{
    const uint64_t *s = reinterpret_cast<const uint64_t *>(src) + index;
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argb64ToArgb(LColor::premultiply(s[i]));
    }
    return buffer;
}

static void storeRGBA64FromARGB32PM(uint8_t *dest,
                                    const uint32_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    uint64_t *d = reinterpret_cast<uint64_t *>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::unpremultiply(LColor::argbToArgb64(src[i]));
    }
}

static const uint64_t* fetchRGBA64ToRGBA64PM(uint64_t *buffer,
                                             const uint8_t *src,
                                             int index,
                                             int count,
                                             const std::vector<uint32_t> *,
                                             LPoint<int> *)
{
    const uint64_t *s = reinterpret_cast<const uint64_t *>(src) + index;
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::premultiply(s[i]);
    }
    return buffer;
}


static void convertGrayscale16ToRGB32(uint32_t *buffer, int count, const std::vector<uint32_t> *)
{
    for (int i = 0; i < count; ++i) {
        const uint32_t x = LColor::div257(buffer[i]);
        buffer[i] = LColor::argbToUInt(x, x, x);
    }
}

static const uint64_t* convertGrayscale16ToRGBA64(uint64_t *buffer,
                                                  const uint32_t *src,
                                                  int count,
                                                  const std::vector<uint32_t> *,
                                                  LPoint<int> *)
{
    const uint16_t *s = reinterpret_cast<const uint16_t *>(src);
    for (int i = 0; i < count; ++i){
        buffer[i] = LColor::argb16ToUInt64(s[i], s[i], s[i], 65535);
    }
    return buffer;
}


static const uint32_t* fetchGrayscale16ToRGB32(uint32_t *buffer,
                                               const uint8_t *src,
                                               int index,
                                               int count,
                                               const std::vector<uint32_t> *,
                                               LPoint<int> *)
{
    const uint16_t *s = reinterpret_cast<const uint16_t *>(src) + index;
    for (int i = 0; i < count; ++i) {
        const uint32_t x = LColor::div257(s[i]);
        buffer[i] = LColor::argbToUInt(x, x, x);
    }
    return buffer;
}

static const uint64_t* fetchGrayscale16ToRGBA64(uint64_t *buffer,
                                                const uint8_t *src,
                                                int index,
                                                int count,
                                                const std::vector<uint32_t> *,
                                                LPoint<int> *)
{
    const uint16_t *s = reinterpret_cast<const uint16_t *>(src) + index;
    for (int i = 0; i < count; ++i) {
        buffer[i] = LColor::argb16ToUInt64(s[i], s[i], s[i], 65535);
    }
    return buffer;
}

template<LImage::Format Format>
static inline LPixelLayout pixelLayoutRGB()
{
    LPixelLayout ret;
    ret.hasAlphaChannel = false;
    ret.premultiplied = false;
    ret.bpp = bitsPerPixel<Format>();
    ret.rbSwap = rbSwap<Format>;
    ret.convertToARGB32PM = convertToRGB32<Format>;
    ret.convertToRGBA64PM = convertToRGB64<Format>;
    ret.fetchToARGB32PM = fetchRGBToRGB32<Format>;
    ret.fetchToRGBA64PM = fetchRGBToRGB64<Format>;
    ret.storeFromARGB32PM = storeRGBFromARGB32PM<Format, false>;
    ret.storeFromRGB32 = storeRGBFromARGB32PM<Format, true>;
    return ret;
}



template<LImage::Format Format>
static inline LPixelLayout pixelLayoutARGBPM()
{
    LPixelLayout ret;
    ret.hasAlphaChannel = true;
    ret.premultiplied = true;
    ret.bpp = bitsPerPixel<Format>();
    ret.rbSwap = rbSwap<Format>;
    ret.convertToARGB32PM = convertARGBPMToARGB32PM<Format>;
    ret.convertToRGBA64PM = convertARGBPMToRGBA64PM<Format>;
    ret.fetchToARGB32PM = fetchARGBPMToARGB32PM<Format>;
    ret.fetchToRGBA64PM = fetchARGBPMToRGBA64PM<Format>;
    ret.storeFromARGB32PM = storeARGBPMFromARGB32PM<Format, false>;
    ret.storeFromRGB32 = storeARGBPMFromARGB32PM<Format, true>;
    return ret;
}

// Note:
// convertToArgb32() assumes that no color channel is less than 4 bits.
// storeRGBFromARGB32PM() assumes that no color channel is more than 8 bits.
// LImage::rgbSwapped() assumes that the red and blue color channels have the same number of bits.
LPixelLayout lPixelLayouts[(int)LImage::Format::NImageFormats] = {
    { false, false, BPPNone, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }, // Format_Invalid
    { false, false, BPP1MSB, nullptr,
     convertIndexedToARGB32PM, convertIndexedToRGBA64PM,
     fetchIndexedToARGB32PM<BPP1MSB>, fetchIndexedToRGBA64PM<BPP1MSB>,
     nullptr, nullptr }, // Format_Mono
    { false, false, BPP1LSB, nullptr,
     convertIndexedToARGB32PM, convertIndexedToRGBA64PM,
     fetchIndexedToARGB32PM<BPP1LSB>, fetchIndexedToRGBA64PM<BPP1LSB>,
     nullptr, nullptr }, // Format_MonoLSB
    { false, false, BPP8, nullptr,
     convertIndexedToARGB32PM, convertIndexedToRGBA64PM,
     fetchIndexedToARGB32PM<BPP8>, fetchIndexedToRGBA64PM<BPP8>,
     nullptr, nullptr }, // Format_Indexed8
    // Technically using convertPassThrough to convert from ARGB32PM to RGB32 is wrong,
    // but everywhere this generic conversion would be wrong is currently overloaded.
    { false, false, BPP32, rbSwap_rgb32, convertPassThrough,
     convertRGB32ToRGB64, fetchPassThrough, fetchRGB32ToRGB64, storePassThrough, storePassThrough }, // Format_RGB32
    { true, false, BPP32, rbSwap_rgb32, convertARGB32ToARGB32PM,
     convertARGB32ToRGBA64PM, fetchARGB32ToARGB32PM, fetchARGB32ToRGBA64PM, storeARGB32FromARGB32PM, storePassThrough }, // Format_ARGB32
    { true, true, BPP32, rbSwap_rgb32, convertPassThrough,
     convertARGB32PMToRGBA64PM, fetchPassThrough, fetchARGB32PMToRGBA64PM, storePassThrough, storePassThrough }, // Format_ARGB32_Premultiplied
    pixelLayoutRGB<LImage::Format::RGB16>(),
    pixelLayoutARGBPM<LImage::Format::ARGB8565Premultiplied>(),
    pixelLayoutRGB<LImage::Format::RGB666>(),
    pixelLayoutARGBPM<LImage::Format::ARGB6666Premultiplied>(),
    pixelLayoutRGB<LImage::Format::RGB555>(),
    pixelLayoutARGBPM<LImage::Format::ARGB8555Premultiplied>(),
    pixelLayoutRGB<LImage::Format::RGB888>(),
    pixelLayoutRGB<LImage::Format::RGB444>(),
    pixelLayoutARGBPM<LImage::Format::ARGB4444Premultiplied>(),
    { false, false, BPP32, rbSwap<LImage::Format::ABGR32>, convertABGR32PMToARGB32PM,
     convertABGR32PMToRGBA64PM, fetchABGR32PMToARGB32PM, fetchABGR32PMToRGBA64PM, storeXBGRFromARGB32PM, storeXBGRFromRGB32 }, // Format_RGBX8888
    { true, false, BPP32, rbSwap<LImage::Format::ABGR32>, convertABGR32ToARGB32PM,
     convertABGR32ToRGBA64PM, fetchABGR32ToARGB32PM, fetchABGR32ToRGBA64PM, storeABGR32FromARGB32PM, storeXBGRFromRGB32 }, // Format_ABGR32
    { true, true, BPP32, rbSwap<LImage::Format::ABGR32>, convertABGR32PMToARGB32PM,
     convertABGR32PMToRGBA64PM, fetchABGR32PMToARGB32PM, fetchABGR32PMToRGBA64PM, storeABGR32PMFromARGB32PM, storeXBGRFromRGB32 },  // Format_ABGR32_Premultiplied
    { false, false, BPP32, rbSwap_rgb30,
        convertA2RGB30PMToARGB32PM<PixelOrderBGR>,
        convertA2RGB30PMToRGBA64PM<PixelOrderBGR>,
        fetchA2RGB30PMToARGB32PM<PixelOrderBGR>,
        fetchA2RGB30PMToRGBA64PM<PixelOrderBGR>,
        storeRGB30FromARGB32PM<PixelOrderBGR>,
        storeRGB30FromRGB32<PixelOrderBGR>
    }, // Format_BGR30
    { true, true, BPP32, rbSwap_rgb30,
        convertA2RGB30PMToARGB32PM<PixelOrderBGR>,
        convertA2RGB30PMToRGBA64PM<PixelOrderBGR>,
        fetchA2RGB30PMToARGB32PM<PixelOrderBGR>,
        fetchA2RGB30PMToRGBA64PM<PixelOrderBGR>,
        storeA2RGB30PMFromARGB32PM<PixelOrderBGR>,
        storeRGB30FromRGB32<PixelOrderBGR>
    },  // Format_A2BGR30_Premultiplied
    { false, false, BPP32, rbSwap_rgb30,
        convertA2RGB30PMToARGB32PM<PixelOrderRGB>,
        convertA2RGB30PMToRGBA64PM<PixelOrderRGB>,
        fetchA2RGB30PMToARGB32PM<PixelOrderRGB>,
        fetchA2RGB30PMToRGBA64PM<PixelOrderRGB>,
        storeRGB30FromARGB32PM<PixelOrderRGB>,
        storeRGB30FromRGB32<PixelOrderRGB>
    }, // Format_RGB30
    { true, true, BPP32, rbSwap_rgb30,
        convertA2RGB30PMToARGB32PM<PixelOrderRGB>,
        convertA2RGB30PMToRGBA64PM<PixelOrderRGB>,
        fetchA2RGB30PMToARGB32PM<PixelOrderRGB>,
        fetchA2RGB30PMToRGBA64PM<PixelOrderRGB>,
        storeA2RGB30PMFromARGB32PM<PixelOrderRGB>,
        storeRGB30FromRGB32<PixelOrderRGB>
    },  // Format_A2RGB30_Premultiplied
    { true, true, BPP8, nullptr,
     convertAlpha8ToRGB32, convertAlpha8ToRGB64,
     fetchAlpha8ToRGB32, fetchAlpha8ToRGB64,
     storeAlpha8FromARGB32PM, nullptr }, // Format_Alpha8
    { false, false, BPP8, nullptr,
     convertGrayscale8ToRGB32, convertGrayscale8ToRGB64,
     fetchGrayscale8ToRGB32, fetchGrayscale8ToRGB64,
     storeGrayscale8FromARGB32PM, storeGrayscale8FromRGB32 }, // Format_Grayscale8
    { false, false, BPP64, nullptr,
     convertPassThrough, nullptr,
     fetchRGB64ToRGB32, fetchPassThrough64,
     storeRGB64FromRGB32, storeRGB64FromRGB32 }, // Format_RGBX64
    { true, false, BPP64, nullptr,
     convertARGB32ToARGB32PM, nullptr,
     fetchRGBA64ToARGB32PM, fetchRGBA64ToRGBA64PM,
     storeRGBA64FromARGB32PM, storeRGB64FromRGB32 }, // Format_RGBA64
    { true, true, BPP64, nullptr,
     convertPassThrough, nullptr,
     fetchRGB64ToRGB32, fetchPassThrough64,
     storeRGB64FromRGB32, storeRGB64FromRGB32 }, // Format_RGBA64_Premultiplied
    { false, false, BPP16, nullptr,
     convertGrayscale16ToRGB32, convertGrayscale16ToRGBA64,
     fetchGrayscale16ToRGB32, fetchGrayscale16ToRGBA64,
     storeGrayscale16FromARGB32PM, storeGrayscale16FromRGB32 }, // Format_Grayscale16
    pixelLayoutRGB<LImage::Format::BGR888>(),
    };


static void convertFromRgb64(uint32_t *dest, const uint64_t *src, int length)
{
    for (int i = 0; i < length; ++i) {
        dest[i] = LColor::argb64ToArgb(src[i]);
    }
}


template<LImage::Format format>
static void storeGenericFromRGBA64PM(uint8_t *dest,
                                     const uint64_t *src,
                                     int index,
                                     int count,
                                     const std::vector<uint32_t> *clut,
                                     LPoint<int> *dither)
{
    uint32_t buffer[draw::BufferSize];
    convertFromRgb64(buffer, src, count);
    lPixelLayouts[(int)format].storeFromARGB32PM(dest, buffer, index, count, clut, dither);
}


template<PixelOrder order>
static void  storeRGB30FromRGBA64PM(uint8_t *dest,
                                   const uint64_t *src,
                                   int index,
                                   int count,
                                   const std::vector<uint32_t> *,
                                   LPoint<int> *)
{
    uint32_t *d = (uint32_t*)dest + index;
    for (int i = 0; i < count; ++i){
        d[i] = lConvertRgb64ToRgb30<order>(src[i]);
    }
}

static void storeARGB32FromRGBA64PM(uint8_t *dest,
                                    const uint64_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    uint32_t *d = (uint32_t*)dest + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::argb64ToArgb(LColor::unpremultiply(src[i]));
    }
}

static void storeABGR32FromRGBA64PM(uint8_t *dest,
                                      const uint64_t *src,
                                      int index,
                                      int count,
                                      const std::vector<uint32_t> *,
                                      LPoint<int> *)
{
    uint32_t *d = (uint32_t*)dest + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::argbToAbgr(LColor::argb64ToArgb(LColor::unpremultiply(src[i])));
    }
}

static void storeRGBX64FromRGBA64PM(uint8_t *dest,
                                    const uint64_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    uint64_t *d = reinterpret_cast<uint64_t*>(dest) + index;
    for (int i = 0; i < count; ++i) {
        d[i] = LColor::unpremultiply(src[i]);
        d[i] = LColor::setAlpha16(d[i], 65535);
    }
}

static void storeRGBA64FromRGBA64PM(uint8_t *dest,
                                    const uint64_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    uint64_t *d = reinterpret_cast<uint64_t*>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = LColor::unpremultiply(src[i]);
    }
}

static void storeRGBA64PMFromRGBA64PM(uint8_t *dest,
                                      const uint64_t *src,
                                      int index,
                                      int count,
                                      const std::vector<uint32_t> *,
                                      LPoint<int> *)
{
    uint64_t *d = reinterpret_cast<uint64_t*>(dest) + index;
    if (d != src){
        memcpy(d, src, count * sizeof(uint64_t));
    }
}

static void storeGray16FromRGBA64PM(uint8_t *dest,
                                    const uint64_t *src,
                                    int index,
                                    int count,
                                    const std::vector<uint32_t> *,
                                    LPoint<int> *)
{
    uint16_t *d = reinterpret_cast<uint16_t*>(dest) + index;
    for (int i = 0; i < count; ++i) {
        uint64_t s =  LColor::unpremultiply(src[i]);
        d[i] = LColor::toGray(LColor::red16(s), LColor::green16(s), LColor::blue16(s));
    }
}


ConvertAndStorePixelsFunc64 lStoreFromRGBA64PM[(int)LImage::Format::NImageFormats] = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    storeGenericFromRGBA64PM<LImage::Format::RGB32>,
    storeARGB32FromRGBA64PM,
    storeGenericFromRGBA64PM<LImage::Format::ARGB32Premultiplied>,
    storeGenericFromRGBA64PM<LImage::Format::RGB16>,
    storeGenericFromRGBA64PM<LImage::Format::ARGB8565Premultiplied>,
    storeGenericFromRGBA64PM<LImage::Format::RGB666>,
    storeGenericFromRGBA64PM<LImage::Format::ARGB6666Premultiplied>,
    storeGenericFromRGBA64PM<LImage::Format::RGB555>,
    storeGenericFromRGBA64PM<LImage::Format::ARGB8555Premultiplied>,
    storeGenericFromRGBA64PM<LImage::Format::RGB888>,
    storeGenericFromRGBA64PM<LImage::Format::RGB444>,
    storeGenericFromRGBA64PM<LImage::Format::ARGB4444Premultiplied>,
    storeGenericFromRGBA64PM<LImage::Format::XBGR32>,
    storeABGR32FromRGBA64PM,
    storeGenericFromRGBA64PM<LImage::Format::ABGR32Premultiplied>,
    storeRGB30FromRGBA64PM<PixelOrderBGR>,
    storeRGB30FromRGBA64PM<PixelOrderBGR>,
    storeRGB30FromRGBA64PM<PixelOrderRGB>,
    storeRGB30FromRGBA64PM<PixelOrderRGB>,
    storeGenericFromRGBA64PM<LImage::Format::Alpha8>,
    storeGenericFromRGBA64PM<LImage::Format::Grayscale8>,
    storeRGBX64FromRGBA64PM,
    storeRGBA64FromRGBA64PM,
    storeRGBA64PMFromRGBA64PM,
    storeGray16FromRGBA64PM,
    storeGenericFromRGBA64PM<LImage::Format::BGR888>,
};


}

/*
  Destination fetch. This is simple as we don't have to do bounds checks or
  transformations
*/

static uint32_t * destFetchMono(uint32_t *buffer, LRasterBuffer *rasterBuffer, int x, int y, int length)
{
    uint8_t * data = rasterBuffer->scanLine(y);
    uint32_t *start = buffer;
    const uint32_t *end = buffer + length;
    while (buffer < end) {
        *buffer = data[x>>3] & (0x80 >> (x & 7)) ? rasterBuffer->destColor1 : rasterBuffer->destColor0;
        ++buffer;
        ++x;
    }
    return start;
}

static uint32_t* destFetchMonoLsb(uint32_t *buffer, LRasterBuffer *rasterBuffer, int x, int y, int length)
{
    uint8_t* data = rasterBuffer->scanLine(y);
    uint32_t* start = buffer;
    const uint32_t *end = buffer + length;
    while (buffer < end) {
        *buffer = data[x>>3] & (0x1 << (x & 7)) ? rasterBuffer->destColor1 : rasterBuffer->destColor0;
        ++buffer;
        ++x;
    }
    return start;
}

static uint32_t * destFetchARGB32P(uint32_t *, LRasterBuffer *rasterBuffer, int x, int y, int)
{
    return (uint32_t *)rasterBuffer->scanLine(y) + x;
}

static uint32_t* destFetchRGB16(uint32_t *buffer, LRasterBuffer *rasterBuffer, int x, int y, int length)
{
    const uint16_t * data = (const uint16_t *)rasterBuffer->scanLine(y) + x;
    for (int i = 0; i < length; ++i){
        buffer[i] = LColor::rgb16ToArgb(data[i]);
    }
    return buffer;
}

static uint32_t * destFetch(uint32_t *buffer, LRasterBuffer *rasterBuffer, int x, int y, int length)
{
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)rasterBuffer->format];
    return const_cast<uint32_t*>(layout->fetchToARGB32PM(buffer, rasterBuffer->scanLine(y), x, length, nullptr, nullptr));
}

static uint32_t* destFetchUndefined(uint32_t *buffer, LRasterBuffer *, int, int, int)
{
    return buffer;
}

static LDestFetchProc destFetchProc[(int)LImage::Format::NImageFormats] =
{
    nullptr,            // Format_Invalid
    destFetchMono,      // Format_Mono,
    destFetchMonoLsb,   // Format_MonoLSB
    nullptr,            // Format_Indexed8
    destFetchARGB32P,   // Format_RGB32
    destFetch,          // Format_ARGB32,
    destFetchARGB32P,   // Format_ARGB32_Premultiplied
    destFetchRGB16,     // Format_RGB16
    destFetch,          // Format_ARGB8565_Premultiplied
    destFetch,          // Format_RGB666
    destFetch,          // Format_ARGB6666_Premultiplied
    destFetch,          // Format_RGB555
    destFetch,          // Format_ARGB8555_Premultiplied
    destFetch,          // Format_RGB888
    destFetch,          // Format_RGB444
    destFetch,          // Format_ARGB4444_Premultiplied
    destFetch,          // Format_RGBX8888
    destFetch,          // Format_ABGR32
    destFetch,          // Format_ABGR32_Premultiplied
    destFetch,          // Format_BGR30
    destFetch,          // Format_A2BGR30_Premultiplied
    destFetch,          // Format_RGB30
    destFetch,          // Format_A2RGB30_Premultiplied
    destFetch,          // Format_Alpha8
    destFetch,          // Format_Grayscale8
    destFetch,          // Format_RGBX64
    destFetch,          // Format_RGBA64
    destFetch,          // Format_RGBA64_Premultiplied
    destFetch,          // Format_Grayscale16
    destFetch,          // Format_BGR888
};

/*
   Returns the color in the mono destination color table
   that is the "nearest" to /color/.
*/
static inline uint32_t findNearestColor(uint32_t color, LRasterBuffer *rbuf)
{
    uint32_t color_0 = LColor::premultiply(rbuf->destColor0);
    uint32_t color_1 = LColor::premultiply(rbuf->destColor1);
    color = LColor::premultiply(color);

    int r = LColor::uintToRed(color);
    int g = LColor::uintToGreen(color);
    int b = LColor::uintToBlue(color);
    int rx, gx, bx;
    int dist_0, dist_1;

    rx = r - LColor::uintToRed(color_0);
    gx = g - LColor::uintToGreen(color_0);
    bx = b - LColor::uintToBlue(color_0);
    dist_0 = rx*rx + gx*gx + bx*bx;

    rx = r - LColor::uintToRed(color_1);
    gx = g - LColor::uintToGreen(color_1);
    bx = b - LColor::uintToBlue(color_1);
    dist_1 = rx*rx + gx*gx + bx*bx;

    if (dist_0 < dist_1){
        return color_0;
    }
    return color_1;
}
/*
  Destination store.
*/

static void destStoreMono(LRasterBuffer *rasterBuffer, int x, int y, const uint32_t *buffer, int length)
{
    uint8_t * data = (uint8_t *)rasterBuffer->scanLine(y);
    if (rasterBuffer->monoDestinationWithClut) {
        for (int i = 0; i < length; ++i) {
            if (buffer[i] == rasterBuffer->destColor0) {
                data[x >> 3] &= ~(0x80 >> (x & 7));
            } else if (buffer[i] == rasterBuffer->destColor1) {
                data[x >> 3] |= 0x80 >> (x & 7);
            } else if (findNearestColor(buffer[i], rasterBuffer) == rasterBuffer->destColor0) {
                data[x >> 3] &= ~(0x80 >> (x & 7));
            } else {
                data[x >> 3] |= 0x80 >> (x & 7);
            }
            ++x;
        }
    } else {
        for (int i = 0; i < length; ++i) {
            if (LColor::toGray(buffer[i]) < int(draw::lBayerMatrix[y & 15][x & 15])){
                data[x >> 3] |= 0x80 >> (x & 7);
            }
            else{
                data[x >> 3] &= ~(0x80 >> (x & 7));
            }
            ++x;
        }
    }
}

static void destStoreMonoLsb(LRasterBuffer *rasterBuffer, int x, int y, const uint32_t *buffer, int length)
{
    uint8_t * data = (uint8_t *)rasterBuffer->scanLine(y);
    if (rasterBuffer->monoDestinationWithClut) {
        for (int i = 0; i < length; ++i) {
            if (buffer[i] == rasterBuffer->destColor0) {
                data[x >> 3] &= ~(1 << (x & 7));
            } else if (buffer[i] == rasterBuffer->destColor1) {
                data[x >> 3] |= 1 << (x & 7);
            } else if (findNearestColor(buffer[i], rasterBuffer) == rasterBuffer->destColor0) {
                data[x >> 3] &= ~(1 << (x & 7));
            } else {
                data[x >> 3] |= 1 << (x & 7);
            }
            ++x;
        }
    } else {
        for (int i = 0; i < length; ++i) {
            if (LColor::toGray(buffer[i]) < int(draw::lBayerMatrix[y & 15][x & 15])){
                data[x >> 3] |= 1 << (x & 7);
            }
            else{
                data[x >> 3] &= ~(1 << (x & 7));
            }
            ++x;
        }
    }
}

static void destStoreRGB16(LRasterBuffer *rasterBuffer, int x, int y, const uint32_t *buffer, int length)
{
    uint16_t *data = (uint16_t*)rasterBuffer->scanLine(y) + x;
    for (int i = 0; i < length; ++i){
        data[i] = LColor::argbToRgb16(buffer[i]);
    }
}

static void destStore(LRasterBuffer *rasterBuffer, int x, int y, const uint32_t *buffer, int length)
{
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)rasterBuffer->format];
    draw::ConvertAndStorePixelsFunc store = layout->storeFromARGB32PM;
    if (!layout->premultiplied && !layout->hasAlphaChannel){
        store = layout->storeFromRGB32;
    }
    uint8_t *dest = rasterBuffer->scanLine(y);
    store(dest, buffer, x, length, nullptr, nullptr);
}

static LDestStoreProc destStoreProc[(int)LImage::Format::NImageFormats] =
{
    nullptr,            // Format_Invalid
    destStoreMono,      // Format_Mono,
    destStoreMonoLsb,   // Format_MonoLSB
    nullptr,            // Format_Indexed8
    nullptr,            // Format_RGB32
    destStore,          // Format_ARGB32,
    nullptr,            // Format_ARGB32_Premultiplied
    destStoreRGB16,     // Format_RGB16
    destStore,          // Format_ARGB8565_Premultiplied
    destStore,          // Format_RGB666
    destStore,          // Format_ARGB6666_Premultiplied
    destStore,          // Format_RGB555
    destStore,          // Format_ARGB8555_Premultiplied
    destStore,          // Format_RGB888
    destStore,          // Format_RGB444
    destStore,          // Format_ARGB4444_Premultiplied
    destStore,          // Format_RGBX8888
    destStore,          // Format_ABGR32
    destStore,          // Format_ABGR32_Premultiplied
    destStore,          // Format_BGR30
    destStore,          // Format_A2BGR30_Premultiplied
    destStore,          // Format_RGB30
    destStore,          // Format_A2RGB30_Premultiplied
    destStore,          // Format_Alpha8
    destStore,          // Format_Grayscale8
    destStore,          // Format_RGBX64
    destStore,          // Format_RGBA64
    destStore,          // Format_RGBA64_Premultiplied
    destStore,          // Format_Grayscale16
    destStore,          // Format_BGR888
};

enum TextureBlendType {
    BlendUntransformed,
    BlendTiled,
    BlendTransformed,
    BlendTransformedTiled,
    BlendTransformedBilinear,
    BlendTransformedBilinearTiled,
    NBlendTypes
};

static TextureBlendType getBlendType(const LSpanData *data)
{
    TextureBlendType ft;
    if (data->txop <= LTransform2D::TxTranslate){
        if (data->texture->type == LTextureData::Tiled){
            ft = BlendTiled;
        }else{
            ft = BlendUntransformed;
        }
    }else if (data->bilinear){
        if (data->texture->type == LTextureData::Tiled){
            ft = BlendTransformedBilinearTiled;
        }else{
            ft = BlendTransformedBilinear;
        }
    }else{
        if (data->texture->type == LTextureData::Tiled){
            ft = BlendTransformedTiled;
        }else{
            ft = BlendTransformed;
        }
    }
    return ft;
}

struct LinearGradientValues
{
    double dx = 0;
    double dy = 0;
    double l = 0;
    double off = 0;
};

struct RadialGradientValues
{
    double dx = 0;
    double dy = 0;
    double dr = 0;
    double sqrfr = 0;
    double a = 0;
    double inv2a = 0;
    bool extended = 0;
};

static const uint32_t * fetchUntransformed(uint32_t *buffer, const LDrawOperator *,
                                           const LSpanData *data, int y, int x, int length)
{
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)data->texture->format];
    return layout->fetchToARGB32PM(buffer, data->texture->scanLine(y), x, length, data->texture->colorTable, nullptr);
}

static const uint32_t * fetchUntransformedARGB32PM(uint32_t *, const LDrawOperator *,
                                                          const LSpanData *data, int y, int x, int)
{
    const uint8_t *scanLine = data->texture->scanLine(y);
    return reinterpret_cast<const uint32_t *>(scanLine) + x;
}

static const uint32_t * fetchUntransformedRGB16(uint32_t *buffer, const LDrawOperator *,
                                                       const LSpanData *data, int y, int x,
                                                       int length)
{
    const uint16_t *scanLine = (const uint16_t *)data->texture->scanLine(y) + x;
    for (int i = 0; i < length; ++i){
        buffer[i] = LColor::rgb16ToArgb(scanLine[i]);
    }
    return buffer;
}


template<TextureBlendType blendType>
inline void fetchTransformedPixelBounds(int max, int l1, int l2, int &v)
{
    if (blendType == BlendTransformedTiled) {
        if (v < 0 || v >= max) {
            v %= max;
            if (v < 0) v += max;
        }
    } else {
        v = lBound(l1, v, l2);
    }
}

namespace {
const static int fixed_scale = 1 << 16;
const static int half_point = 1 << 15;
}

static inline bool canUseFastMatrixPath(const double cx, const double cy,
                                        const lsizetype length, const LSpanData *data)
{
    if (L_UNLIKELY(!data->fastMatrix)){
        return false;
    }

    double fx = (data->m21 * cy + data->m11 * cx + data->dx) * fixed_scale;
    double fy = (data->m22 * cy + data->m12 * cx + data->dy) * fixed_scale;
    double minc = std::min<double>(fx, fy);
    double maxc = std::max<double>(fx, fy);
    fx += std::trunc(data->m11 * fixed_scale) * length;
    fy += std::trunc(data->m12 * fixed_scale) * length;
    minc = std::min<double>(minc, std::min<double>(fx, fy));
    maxc = std::max<double>(maxc, std::max<double>(fx, fy));

    return (minc >= INT_MIN) && (maxc <= INT_MAX);
}


typedef uint32_t (*FetchPixelFunc)(const uint8_t *src, int index);

template <draw::BPP bpp> static
    uint32_t  fetchPixel(const uint8_t *, int)
{
    return 0;
}

template <>
inline uint32_t  fetchPixel<draw::BPP1LSB>(const uint8_t *src, int index)
{
    return (src[index >> 3] >> (index & 7)) & 1;
}

template <>
inline uint32_t  fetchPixel<draw::BPP1MSB>(const uint8_t *src, int index)
{
    return (src[index >> 3] >> (~index & 7)) & 1;
}

template <>
inline uint32_t  fetchPixel<draw::BPP8>(const uint8_t *src, int index)
{
    return src[index];
}

template <>
inline uint32_t  fetchPixel<draw::BPP16>(const uint8_t *src, int index)
{
    return reinterpret_cast<const uint16_t *>(src)[index];
}

template <>
inline uint32_t  fetchPixel<draw::BPP24>(const uint8_t *src, int index)
{
    return reinterpret_cast<const uint24_t *>(src)[index];
}

template <>
inline uint32_t  fetchPixel<draw::BPP32>(const uint8_t *src, int index)
{
    return reinterpret_cast<const uint32_t *>(src)[index];
}

template <>
inline uint32_t  fetchPixel<draw::BPP64>(const uint8_t *src, int index)
{
    // We have to do the conversion in fetch to fit into a 32bit uint
    uint64_t c = reinterpret_cast<const uint64_t *>(src)[index];
    return LColor::argb64ToArgb(c);
}

static const FetchPixelFunc lFetchPixel[draw::BPPCount] = {
    nullptr, // BPPNone
    fetchPixel<draw::BPP1MSB>, // BPP1MSB
    fetchPixel<draw::BPP1LSB>, // BPP1LSB
    fetchPixel<draw::BPP8>, // BPP8
    fetchPixel<draw::BPP16>, // BPP16
    fetchPixel<draw::BPP24>, // BPP24
    fetchPixel<draw::BPP32>, // BPP32
    fetchPixel<draw::BPP64> // BPP64
};

template<TextureBlendType blendType, draw::BPP bpp, typename T>
static void fetchTransformedFetcher(T *buffer, const LSpanData *data,
                                                 int y, int x, int length)
{
    const std::shared_ptr<LTextureData> image = data->texture;
    if(image == nullptr){
        return;
    }
    const double cx = x + double(0.5);
    const double cy = y + double(0.5);

    constexpr bool useFetch = (bpp < draw::BPP32) && sizeof(T) == sizeof(uint32_t);
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)image->format];

    // When templated 'fetch' should be inlined at compile time:
    const FetchPixelFunc fetch = (bpp == draw::BPPNone) ?
                                     lFetchPixel[layout->bpp] : FetchPixelFunc(fetchPixel<bpp>);

    if (canUseFastMatrixPath(cx, cy, length, data)) {
        // The increment pr x in the scanline
        int fdx = (int)(data->m11 * fixed_scale);
        int fdy = (int)(data->m12 * fixed_scale);

        int fx = int((data->m21 * cy
                      + data->m11 * cx + data->dx) * fixed_scale);
        int fy = int((data->m22 * cy
                      + data->m12 * cx + data->dy) * fixed_scale);

        if (fdy == 0) { // simple scale, no rotation or shear
            int py = (fy >> 16);
            fetchTransformedPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, py);
            const uint8_t *src = image->scanLine(py);

            int i = 0;
            if (blendType == BlendTransformed) {
                int fastLen = length;
                if (fdx > 0){
                    fastLen = std::min<int>(fastLen, int((int64_t(image->x2 - 1) * fixed_scale - fx) / fdx));
                }else if (fdx < 0){
                    fastLen = std::min<int>(fastLen, int((int64_t(image->x1) * fixed_scale - fx) / fdx));
                }

                for (; i < fastLen; ++i) {
                    int x1 = (fx >> 16);
                    int x2 = x1;
                    fetchTransformedPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1);
                    if (x1 == x2)
                        break;
                    if (useFetch)
                        buffer[i] = fetch(src, x1);
                    else
                        buffer[i] = reinterpret_cast<const T*>(src)[x1];
                    fx += fdx;
                }

                for (; i < fastLen; ++i) {
                    int px = (fx >> 16);
                    if (useFetch)
                        buffer[i] = fetch(src, px);
                    else
                        buffer[i] = reinterpret_cast<const T*>(src)[px];
                    fx += fdx;
                }
            }

            for (; i < length; ++i) {
                int px = (fx >> 16);
                fetchTransformedPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, px);
                if (useFetch)
                    buffer[i] = fetch(src, px);
                else
                    buffer[i] = reinterpret_cast<const T*>(src)[px];
                fx += fdx;
            }
        } else { // rotation or shear
            int i = 0;
            if (blendType == BlendTransformed) {
                int fastLen = length;
                if (fdx > 0){
                    fastLen = std::min<int>(fastLen, int((int64_t(image->x2 - 1) * fixed_scale - fx) / fdx));
                }
                else if (fdx < 0){
                    fastLen = std::min<int>(fastLen, int((int64_t(image->x1) * fixed_scale - fx) / fdx));
                }
                if (fdy > 0){
                    fastLen = std::min<int>(fastLen, int((int64_t(image->y2 - 1) * fixed_scale - fy) / fdy));
                }
                else if (fdy < 0){
                    fastLen = std::min<int>(fastLen, int((int64_t(image->y1) * fixed_scale - fy) / fdy));
                }

                for (; i < fastLen; ++i) {
                    int x1 = (fx >> 16);
                    int y1 = (fy >> 16);
                    int x2 = x1;
                    int y2 = y1;
                    fetchTransformedPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1);
                    fetchTransformedPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1);
                    if (x1 == x2 && y1 == y2)
                        break;
                    if (useFetch)
                        buffer[i] = fetch(image->scanLine(y1), x1);
                    else
                        buffer[i] = reinterpret_cast<const T*>(image->scanLine(y1))[x1];
                    fx += fdx;
                    fy += fdy;
                }

                for (; i < fastLen; ++i) {
                    int px = (fx >> 16);
                    int py = (fy >> 16);
                    if (useFetch)
                        buffer[i] = fetch(image->scanLine(py), px);
                    else
                        buffer[i] = reinterpret_cast<const T*>(image->scanLine(py))[px];
                    fx += fdx;
                    fy += fdy;
                }
            }

            for (; i < length; ++i) {
                int px = (fx >> 16);
                int py = (fy >> 16);
                fetchTransformedPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, px);
                fetchTransformedPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, py);
                if (useFetch)
                    buffer[i] = fetch(image->scanLine(py), px);
                else
                    buffer[i] = reinterpret_cast<const T*>(image->scanLine(py))[px];
                fx += fdx;
                fy += fdy;
            }
        }
    } else {
        const double fdx = data->m11;
        const double fdy = data->m12;
        const double fdw = data->m13;

        double fx = data->m21 * cy + data->m11 * cx + data->dx;
        double fy = data->m22 * cy + data->m12 * cx + data->dy;
        double fw = data->m23 * cy + data->m13 * cx + data->m33;

        T *const end = buffer + length;
        T *b = buffer;
        while (b < end) {
            const double iw = fw == 0 ? 1 : 1 / fw;
            const double tx = fx * iw;
            const double ty = fy * iw;
            int px = std::floor(tx);
            int py = std::floor(ty);

            fetchTransformedPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, py);
            fetchTransformedPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, px);
            if (useFetch){
                *b = fetch(image->scanLine(py), px);
            }else{
                *b = reinterpret_cast<const T*>(image->scanLine(py))[px];
            }

            fx += fdx;
            fy += fdy;
            fw += fdw;
            //force increment to avoid /0
            if (!fw) {
                fw += fdw;
            }
            ++b;
        }
    }
}



template<TextureBlendType blendType, draw::BPP bpp>
static const uint32_t * fetchTransformed(uint32_t *buffer, const LDrawOperator *, const
                                        LSpanData *data, int y, int x, int length)
{
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)data->texture->format];
    fetchTransformedFetcher<blendType, bpp, uint32_t>(buffer, data, y, x, length);
    layout->convertToARGB32PM(buffer, length, data->texture->colorTable);
    return buffer;
}

// A buffer of intermediate results used by simple bilinear scaling.
struct IntermediateBuffer
{
    // The idea is first to do the interpolation between the row s1 and the row s2
    // into this intermediate buffer, then later interpolate between two pixel of this buffer.
    //
    // buffer_rb is a buffer of red-blue component of the pixel, in the form 0x00RR00BB
    // buffer_ag is the alpha-green component of the pixel, in the form 0x00AA00GG
    // +1 for the last pixel to interpolate with, and +1 for rounding errors.
    uint32_t buffer_rb[draw::BufferSize+2];
    uint32_t buffer_ag[draw::BufferSize+2];
};

// Completes the partial interpolation stored in IntermediateBuffer.
// by performing the x-axis interpolation and joining the RB and AG buffers.
static void intermediateAdder(uint32_t *b, uint32_t *end, const IntermediateBuffer &intermediate, int offset, int &fx, int fdx)
{
    // Switch to intermediate buffer coordinates
    fx -= offset * fixed_scale;

    while (b < end) {
        const int x = (fx >> 16);

        const uint32_t distx = (fx & 0x0000ffff) >> 8;
        const uint32_t idistx = 256 - distx;
        const uint32_t rb = (intermediate.buffer_rb[x] * idistx + intermediate.buffer_rb[x + 1] * distx) & 0xff00ff00;
        const uint32_t ag = (intermediate.buffer_ag[x] * idistx + intermediate.buffer_ag[x + 1] * distx) & 0xff00ff00;
        *b = (rb >> 8) | ag;
        b++;
        fx += fdx;
    }
    fx += offset * fixed_scale;
}

template<TextureBlendType blendType>
void fetchTransformedBilinearPixelBounds(int max, int l1, int l2, int &v1, int &v2);

template<>
inline void fetchTransformedBilinearPixelBounds<BlendTransformedBilinearTiled>(int max, int, int, int &v1, int &v2)
{
    v1 %= max;
    if (v1 < 0)
        v1 += max;
    v2 = v1 + 1;
    if (v2 == max)
        v2 = 0;
    assert(v1 >= 0 && v1 < max);
    assert(v2 >= 0 && v2 < max);
}

template<>
inline void fetchTransformedBilinearPixelBounds<BlendTransformedBilinear>(int, int l1, int l2, int &v1, int &v2)
{
    if (v1 < l1)
        v2 = v1 = l1;
    else if (v1 >= l2)
        v2 = v1 = l2;
    else
        v2 = v1 + 1;
    assert(v1 >= l1 && v1 <= l2);
    assert(v2 >= l1 && v2 <= l2);
}


template<TextureBlendType blendType>
static void fetchTransformedBilinearSimpleScaleHelper(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image,
                                                                     int &fx, int &fy, int fdx, int /*fdy*/)
{
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)image->format];
    const std::vector<uint32_t>*clut = image->colorTable;
    const draw::FetchAndConvertPixelsFunc fetch = layout->fetchToARGB32PM;

    int y1 = (fy >> 16);
    int y2;
    fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
    const uint8_t *s1 = image->scanLine(y1);
    const uint8_t *s2 = image->scanLine(y2);

    const int disty = (fy & 0x0000ffff) >> 8;
    const int idisty = 256 - disty;
    const int length = end - b;

    // The intermediate buffer is generated in the positive direction
    const int adjust = (fdx < 0) ? fdx * length : 0;
    const int offset = (fx + adjust) >> 16;
    int x = offset;

    IntermediateBuffer intermediate;
    uint32_t *buf1 = intermediate.buffer_rb;
    uint32_t *buf2 = intermediate.buffer_ag;
    const uint32_t *ptr1;
    const uint32_t *ptr2;

    int count = (int64_t(length) * std::abs(fdx) + fixed_scale - 1) / fixed_scale + 2;
    assert(count <= draw::BufferSize + 2);

    if (blendType == BlendTransformedBilinearTiled) {
        x %= image->width;
        if (x < 0){
            x += image->width;
        }
        int len1 = std::min<int>(count, image->width - x);
        int len2 = std::min<int>(x, count - len1);

        ptr1 = fetch(buf1, s1, x, len1, clut, nullptr);
        ptr2 = fetch(buf2, s2, x, len1, clut, nullptr);
        for (int i = 0; i < len1; ++i) {
            uint32_t t = ptr1[i];
            uint32_t b = ptr2[i];
            buf1[i] = (((t & 0xff00ff) * idisty + (b & 0xff00ff) * disty) >> 8) & 0xff00ff;
            buf2[i] = ((((t >> 8) & 0xff00ff) * idisty + ((b >> 8) & 0xff00ff) * disty) >> 8) & 0xff00ff;
        }

        if (len2) {
            ptr1 = fetch(buf1 + len1, s1, 0, len2, clut, nullptr);
            ptr2 = fetch(buf2 + len1, s2, 0, len2, clut, nullptr);
            for (int i = 0; i < len2; ++i) {
                uint32_t t = ptr1[i];
                uint32_t b = ptr2[i];
                buf1[i + len1] = (((t & 0xff00ff) * idisty + (b & 0xff00ff) * disty) >> 8) & 0xff00ff;
                buf2[i + len1] = ((((t >> 8) & 0xff00ff) * idisty + ((b >> 8) & 0xff00ff) * disty) >> 8) & 0xff00ff;
            }
        }
        // Generate the rest by repeatedly repeating the previous set of pixels
        for (int i = image->width; i < count; ++i) {
            buf1[i] = buf1[i - image->width];
            buf2[i] = buf2[i - image->width];
        }
    } else {
        int start = std::max<int>(x, image->x1);
        int end = std::min<int>(x + count, image->x2);
        int len = std::max<int>(1, end - start);
        int leading = start - x;

        ptr1 = fetch(buf1 + leading, s1, start, len, clut, nullptr);
        ptr2 = fetch(buf2 + leading, s2, start, len, clut, nullptr);

        for (int i = 0; i < len; ++i) {
            uint32_t t = ptr1[i];
            uint32_t b = ptr2[i];
            buf1[i + leading] = (((t & 0xff00ff) * idisty + (b & 0xff00ff) * disty) >> 8) & 0xff00ff;
            buf2[i + leading] = ((((t >> 8) & 0xff00ff) * idisty + ((b >> 8) & 0xff00ff) * disty) >> 8) & 0xff00ff;
        }

        for (int i = 0; i < leading; ++i) {
            buf1[i] = buf1[leading];
            buf2[i] = buf2[leading];
        }
        for (int i = leading + len; i < count; ++i) {
            buf1[i] = buf1[i - 1];
            buf2[i] = buf2[i - 1];
        }
    }

    // Now interpolate the values from the intermediate.buffer to get the final result.
    intermediateAdder(b, end, intermediate, offset, fx, fdx);
}


template<TextureBlendType blendType, draw::BPP bpp, typename T>
static void fetchTransformedBilinearFetcher(T *buf1, T *buf2, const int len, const std::shared_ptr<LTextureData> image,
                                                         int fx, int fy, const int fdx, const int fdy)
{
    const draw::LPixelLayout &layout = draw::lPixelLayouts[(int)image->format];
    constexpr bool useFetch = (bpp < draw::BPP32);

    const FetchPixelFunc fetch1 = (bpp == draw::BPPNone) ? lFetchPixel[layout.bpp] : fetchPixel<bpp>;
    if (fdy == 0) {
        int y1 = (fy >> 16);
        int y2;
        fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
        const uint8_t *s1 = image->scanLine(y1);
        const uint8_t *s2 = image->scanLine(y2);

        int i = 0;
        if (blendType == BlendTransformedBilinear) {
            for (; i < len; ++i) {
                int x1 = (fx >> 16);
                int x2;
                fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
                if (x1 != x2)
                    break;
                if constexpr (useFetch) {
                    buf1[i * 2 + 0] = buf1[i * 2 + 1] = fetch1(s1, x1);
                    buf2[i * 2 + 0] = buf2[i * 2 + 1] = fetch1(s2, x1);
                } else {
                    buf1[i * 2 + 0] = buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x1];
                    buf2[i * 2 + 0] = buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x1];
                }
                fx += fdx;
            }
            int fastLen = len;
            if (fdx > 0)
                fastLen = std::min<int>(fastLen, int((int64_t(image->x2 - 1) * fixed_scale - fx) / fdx));
            else if (fdx < 0)
                fastLen = std::min<int>(fastLen, int((int64_t(image->x1) * fixed_scale - fx) / fdx));

            for (; i < fastLen; ++i) {
                int x = (fx >> 16);
                if constexpr (useFetch) {
                    buf1[i * 2 + 0] = fetch1(s1, x);
                    buf1[i * 2 + 1] = fetch1(s1, x + 1);
                    buf2[i * 2 + 0] = fetch1(s2, x);
                    buf2[i * 2 + 1] = fetch1(s2, x + 1);
                } else {
                    buf1[i * 2 + 0] = reinterpret_cast<const T *>(s1)[x];
                    buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x + 1];
                    buf2[i * 2 + 0] = reinterpret_cast<const T *>(s2)[x];
                    buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x + 1];
                }
                fx += fdx;
            }
        }

        for (; i < len; ++i) {
            int x1 = (fx >> 16);
            int x2;
            fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
            if constexpr (useFetch) {
                buf1[i * 2 + 0] = fetch1(s1, x1);
                buf1[i * 2 + 1] = fetch1(s1, x2);
                buf2[i * 2 + 0] = fetch1(s2, x1);
                buf2[i * 2 + 1] = fetch1(s2, x2);
            } else {
                buf1[i * 2 + 0] = reinterpret_cast<const T *>(s1)[x1];
                buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x2];
                buf2[i * 2 + 0] = reinterpret_cast<const T *>(s2)[x1];
                buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x2];
            }
            fx += fdx;
        }
    } else {
        int i = 0;
        if (blendType == BlendTransformedBilinear) {
            for (; i < len; ++i) {
                int x1 = (fx >> 16);
                int x2;
                int y1 = (fy >> 16);
                int y2;
                fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
                fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
                if (x1 != x2 && y1 != y2)
                    break;
                const uint8_t *s1 = image->scanLine(y1);
                const uint8_t *s2 = image->scanLine(y2);
                if constexpr (useFetch) {
                    buf1[i * 2 + 0] = fetch1(s1, x1);
                    buf1[i * 2 + 1] = fetch1(s1, x2);
                    buf2[i * 2 + 0] = fetch1(s2, x1);
                    buf2[i * 2 + 1] = fetch1(s2, x2);
                } else {
                    buf1[i * 2 + 0] = reinterpret_cast<const T *>(s1)[x1];
                    buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x2];
                    buf2[i * 2 + 0] = reinterpret_cast<const T *>(s2)[x1];
                    buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x2];
                }
                fx += fdx;
                fy += fdy;
            }
            int fastLen = len;
            if (fdx > 0)
                fastLen = std::min<int>(fastLen, int((int64_t(image->x2 - 1) * fixed_scale - fx) / fdx));
            else if (fdx < 0)
                fastLen = std::min<int>(fastLen, int((int64_t(image->x1) * fixed_scale - fx) / fdx));
            if (fdy > 0)
                fastLen = std::min<int>(fastLen, int((int64_t(image->y2 - 1) * fixed_scale - fy) / fdy));
            else if (fdy < 0)
                fastLen = std::min<int>(fastLen, int((int64_t(image->y1) * fixed_scale - fy) / fdy));

            for (; i < fastLen; ++i) {
                int x = (fx >> 16);
                int y = (fy >> 16);
                const uint8_t *s1 = image->scanLine(y);
                const uint8_t *s2 = s1 + image->bytesPerLine;
                if constexpr (useFetch) {
                    buf1[i * 2 + 0] = fetch1(s1, x);
                    buf1[i * 2 + 1] = fetch1(s1, x + 1);
                    buf2[i * 2 + 0] = fetch1(s2, x);
                    buf2[i * 2 + 1] = fetch1(s2, x + 1);
                } else {
                    buf1[i * 2 + 0] = reinterpret_cast<const T *>(s1)[x];
                    buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x + 1];
                    buf2[i * 2 + 0] = reinterpret_cast<const T *>(s2)[x];
                    buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x + 1];
                }
                fx += fdx;
                fy += fdy;
            }
        }

        for (; i < len; ++i) {
            int x1 = (fx >> 16);
            int x2;
            int y1 = (fy >> 16);
            int y2;
            fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
            fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);

            const uint8_t *s1 = image->scanLine(y1);
            const uint8_t *s2 = image->scanLine(y2);
            if constexpr (useFetch) {
                buf1[i * 2 + 0] = fetch1(s1, x1);
                buf1[i * 2 + 1] = fetch1(s1, x2);
                buf2[i * 2 + 0] = fetch1(s2, x1);
                buf2[i * 2 + 1] = fetch1(s2, x2);
            } else {
                buf1[i * 2 + 0] = reinterpret_cast<const T *>(s1)[x1];
                buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x2];
                buf2[i * 2 + 0] = reinterpret_cast<const T *>(s2)[x1];
                buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x2];
            }
            fx += fdx;
            fy += fdy;
        }
    }
}

template<TextureBlendType blendType, draw::BPP bpp, typename T>
static void fetchTransformedBilinearSlowFetcher(T *buf1, T *buf2, uint16_t *distxs, uint16_t *distys,
                                                              const int len, const std::shared_ptr<LTextureData> image,
                                                              double &fx, double &fy, double &fw,
                                                              const double fdx, const double fdy, const double fdw)
{
    const draw::LPixelLayout &layout = draw::lPixelLayouts[(int)image->format];
    constexpr bool useFetch = (bpp < draw::BPP32);

    const FetchPixelFunc fetch1 = (bpp == draw::BPPNone) ? lFetchPixel[layout.bpp] : fetchPixel<bpp>;

    for (int i = 0; i < len; ++i) {
        const double iw = fw == 0 ? 16384 : 1 / fw;
        const double px = fx * iw - double(0.5);
        const double py = fy * iw - double(0.5);

        int x1 = std::floor(px);
        int x2;
        int y1 = std::floor(py);
        int y2;

        distxs[i] = uint16_t((px - x1) * (1<<16));
        distys[i] = uint16_t((py - y1) * (1<<16));

        fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
        fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);

        const uint8_t *s1 = image->scanLine(y1);
        const uint8_t *s2 = image->scanLine(y2);
        if constexpr (useFetch) {
            buf1[i * 2 + 0] = fetch1(s1, x1);
            buf1[i * 2 + 1] = fetch1(s1, x2);
            buf2[i * 2 + 0] = fetch1(s2, x1);
            buf2[i * 2 + 1] = fetch1(s2, x2);
        } else {
            buf1[i * 2 + 0] = reinterpret_cast<const T *>(s1)[x1];
            buf1[i * 2 + 1] = reinterpret_cast<const T *>(s1)[x2];
            buf2[i * 2 + 0] = reinterpret_cast<const T *>(s2)[x1];
            buf2[i * 2 + 1] = reinterpret_cast<const T *>(s2)[x2];
        }

        fx += fdx;
        fy += fdy;
        fw += fdw;
    }
}

static inline uint32_t INTERPOLATE_PIXEL_256(uint32_t x, uint32_t a, uint32_t y, uint32_t b) {
    uint64_t t = (((uint64_t(x)) | ((uint64_t(x)) << 24)) & 0x00ff00ff00ff00ff) * a;
    t += (((uint64_t(y)) | ((uint64_t(y)) << 24)) & 0x00ff00ff00ff00ff) * b;
    t >>= 8;
    t &= 0x00ff00ff00ff00ff;
    return (uint32_t(t)) | (uint32_t(t >> 24));
}

#if defined(__SSE2__)
static inline uint32_t interpolate_4_pixels_sse2(__m128i vt, __m128i vb, uint32_t distx, uint32_t disty)
{
    // First interpolate top and bottom pixels in parallel.
    vt = _mm_unpacklo_epi8(vt, _mm_setzero_si128());
    vb = _mm_unpacklo_epi8(vb, _mm_setzero_si128());
    vt = _mm_mullo_epi16(vt, _mm_set1_epi16(256 - disty));
    vb = _mm_mullo_epi16(vb, _mm_set1_epi16(disty));
    __m128i vlr = _mm_add_epi16(vt, vb);
    vlr = _mm_srli_epi16(vlr, 8);
    // vlr now contains the result of the first two interpolate calls vlr = unpacked((xright << 64) | xleft)

    // Now the last interpolate between left and right..
    const __m128i vidistx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(256 - distx), _MM_SHUFFLE(0, 0, 0, 0));
    const __m128i vdistx = _mm_shufflelo_epi16(_mm_cvtsi32_si128(distx), _MM_SHUFFLE(0, 0, 0, 0));
    const __m128i vmulx = _mm_unpacklo_epi16(vidistx, vdistx);
    vlr = _mm_unpacklo_epi16(vlr, _mm_srli_si128(vlr, 8));
    // vlr now contains the colors of left and right interleaved { la, ra, lr, rr, lg, rg, lb, rb }
    vlr = _mm_madd_epi16(vlr, vmulx); // Multiply and horizontal add.
    vlr = _mm_srli_epi32(vlr, 8);
    vlr = _mm_packs_epi32(vlr, vlr);
    vlr = _mm_packus_epi16(vlr, vlr);
    return _mm_cvtsi128_si32(vlr);
}

static inline uint32_t interpolate_4_pixels(uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br, uint32_t distx, uint32_t disty)
{
    __m128i vt = _mm_unpacklo_epi32(_mm_cvtsi32_si128(tl), _mm_cvtsi32_si128(tr));
    __m128i vb = _mm_unpacklo_epi32(_mm_cvtsi32_si128(bl), _mm_cvtsi32_si128(br));
    return interpolate_4_pixels_sse2(vt, vb, distx, disty);
}

static inline uint32_t interpolate_4_pixels(const uint32_t t[], const uint32_t b[], uint32_t distx, uint32_t disty)
{
    __m128i vt = _mm_loadl_epi64((const __m128i*)t);
    __m128i vb = _mm_loadl_epi64((const __m128i*)b);
    return interpolate_4_pixels_sse2(vt, vb, distx, disty);
}

static constexpr inline bool hasFastInterpolate4() { return true; }

#define interpolate_4_pixels_16_sse2(tl, tr, bl, br, distx, disty, colorMask, v_256, b)  \
{ \
        const __m128i dxdy = _mm_mullo_epi16 (distx, disty); \
        const __m128i distx_ = _mm_slli_epi16(distx, 4); \
        const __m128i disty_ = _mm_slli_epi16(disty, 4); \
        const __m128i idxidy =  _mm_add_epi16(dxdy, _mm_sub_epi16(v_256, _mm_add_epi16(distx_, disty_))); \
        const __m128i dxidy =  _mm_sub_epi16(distx_, dxdy); \
        const __m128i idxdy =  _mm_sub_epi16(disty_, dxdy); \
 \
        __m128i tlAG = _mm_srli_epi16(tl, 8); \
        __m128i tlRB = _mm_and_si128(tl, colorMask); \
        __m128i trAG = _mm_srli_epi16(tr, 8); \
        __m128i trRB = _mm_and_si128(tr, colorMask); \
        __m128i blAG = _mm_srli_epi16(bl, 8); \
        __m128i blRB = _mm_and_si128(bl, colorMask); \
        __m128i brAG = _mm_srli_epi16(br, 8); \
        __m128i brRB = _mm_and_si128(br, colorMask); \
 \
        tlAG = _mm_mullo_epi16(tlAG, idxidy); \
        tlRB = _mm_mullo_epi16(tlRB, idxidy); \
        trAG = _mm_mullo_epi16(trAG, dxidy); \
        trRB = _mm_mullo_epi16(trRB, dxidy); \
        blAG = _mm_mullo_epi16(blAG, idxdy); \
        blRB = _mm_mullo_epi16(blRB, idxdy); \
        brAG = _mm_mullo_epi16(brAG, dxdy); \
        brRB = _mm_mullo_epi16(brRB, dxdy); \
 \
        /* Add the values, and shift to only keep 8 significant bits per colors */ \
        __m128i rAG =_mm_add_epi16(_mm_add_epi16(tlAG, trAG), _mm_add_epi16(blAG, brAG)); \
        __m128i rRB =_mm_add_epi16(_mm_add_epi16(tlRB, trRB), _mm_add_epi16(blRB, brRB)); \
        rAG = _mm_andnot_si128(colorMask, rAG); \
        rRB = _mm_srli_epi16(rRB, 8); \
        _mm_storeu_si128((__m128i*)(b), _mm_or_si128(rAG, rRB)); \
}

#elif defined(__ARM_NEON__)
static inline uint32_t interpolate_4_pixels_neon(uint32x2_t vt32, uint32x2_t vb32, uint32_t distx, uint32_t disty)
{
    uint16x8_t vt16 = vmovl_u8(vreinterpret_u8_u32(vt32));
    uint16x8_t vb16 = vmovl_u8(vreinterpret_u8_u32(vb32));
    vt16 = vmulq_n_u16(vt16, 256 - disty);
    vt16 = vmlaq_n_u16(vt16, vb16, disty);
    vt16 = vshrq_n_u16(vt16, 8);
    uint16x4_t vl16 = vget_low_u16(vt16);
    uint16x4_t vr16 = vget_high_u16(vt16);
    vl16 = vmul_n_u16(vl16, 256 - distx);
    vl16 = vmla_n_u16(vl16, vr16, distx);
    vl16 = vshr_n_u16(vl16, 8);
    uint8x8_t vr = vmovn_u16(vcombine_u16(vl16, vl16));
    return vget_lane_u32(vreinterpret_u32_u8(vr), 0);
}

static inline uint32_t interpolate_4_pixels(uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br, uint32_t distx, uint32_t disty)
{
    uint32x2_t vt32 = vmov_n_u32(tl);
    uint32x2_t vb32 = vmov_n_u32(bl);
    vt32 = vset_lane_u32(tr, vt32, 1);
    vb32 = vset_lane_u32(br, vb32, 1);
    return interpolate_4_pixels_neon(vt32, vb32, distx, disty);
}

static inline uint32_t interpolate_4_pixels(const uint32_t t[], const uint32_t b[], uint32_t distx, uint32_t disty)
{
    uint32x2_t vt32 = vld1_u32(t);
    uint32x2_t vb32 = vld1_u32(b);
    return interpolate_4_pixels_neon(vt32, vb32, distx, disty);
}

static constexpr inline bool hasFastInterpolate4() { return true; }

#else
static inline uint32_t interpolate_4_pixels(uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br, uint32_t distx, uint32_t disty)
{
    uint32_t idistx = 256 - distx;
    uint32_t idisty = 256 - disty;
    uint32_t xtop = INTERPOLATE_PIXEL_256(tl, idistx, tr, distx);
    uint32_t xbot = INTERPOLATE_PIXEL_256(bl, idistx, br, distx);
    return INTERPOLATE_PIXEL_256(xtop, idisty, xbot, disty);
}

static inline uint32_t interpolate_4_pixels(const uint32_t t[], const uint32_t b[], uint32_t distx, uint32_t disty)
{
    return interpolate_4_pixels(t[0], t[1], b[0], b[1], distx, disty);
}

static constexpr inline bool hasFastInterpolate4() { return false; }

#endif



/** \internal
  interpolate 4 argb pixels with the distx and disty factor.
  distx and disty must be between 0 and 16
 */
static inline uint32_t interpolate_4_pixels_16(uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br, uint32_t distx, uint32_t disty)
{
    uint32_t distxy = distx * disty;
    //idistx * disty = (16-distx) * disty = 16*disty - distxy
    //idistx * idisty = (16-distx) * (16-disty) = 16*16 - 16*distx -16*disty + distxy
    uint32_t tlrb = (tl & 0x00ff00ff)         * (16*16 - 16*distx - 16*disty + distxy);
    uint32_t tlag = ((tl & 0xff00ff00) >> 8)  * (16*16 - 16*distx - 16*disty + distxy);
    uint32_t trrb = ((tr & 0x00ff00ff)        * (distx*16 - distxy));
    uint32_t trag = (((tr & 0xff00ff00) >> 8) * (distx*16 - distxy));
    uint32_t blrb = ((bl & 0x00ff00ff)        * (disty*16 - distxy));
    uint32_t blag = (((bl & 0xff00ff00) >> 8) * (disty*16 - distxy));
    uint32_t brrb = ((br & 0x00ff00ff)        * (distxy));
    uint32_t brag = (((br & 0xff00ff00) >> 8) * (distxy));
    return (((tlrb + trrb + blrb + brrb) >> 8) & 0x00ff00ff) | ((tlag + trag + blag + brag) & 0xff00ff00);
}



// blendType = BlendTransformedBilinear or BlendTransformedBilinearTiled
template<TextureBlendType blendType, draw::BPP bpp>
static const uint32_t* fetchTransformedBilinear(uint32_t *buffer, const LDrawOperator *,
                                                        const LSpanData *data, int y, int x, int length)
{
    const draw::LPixelLayout *layout = &draw::lPixelLayouts[(int)data->texture->format];
    const std::vector<uint32_t>* clut = data->texture->colorTable;

    const double cx = x + double(0.5);
    const double cy = y + double(0.5);

    if (canUseFastMatrixPath(cx, cy, length, data)) {
        // The increment pr x in the scanline
        int fdx = (int)(data->m11 * fixed_scale);
        int fdy = (int)(data->m12 * fixed_scale);

        int fx = int((data->m21 * cy + data->m11 * cx + data->dx) * fixed_scale);
        int fy = int((data->m22 * cy + data->m12 * cx + data->dy) * fixed_scale);

        fx -= half_point;
        fy -= half_point;

        if (fdy == 0) { // simple scale, no rotation or shear
            if (std::abs(fdx) <= fixed_scale) { // scale up on X
                fetchTransformedBilinearSimpleScaleHelper<blendType>(buffer, buffer + length, data->texture, fx, fy, fdx, fdy);
            } else if (std::abs(fdx) <= 2 * fixed_scale) { // scale down on X less than 2x
                const int mid = (length * 2 < draw::BufferSize) ? length : ((length + 1) / 2);
                fetchTransformedBilinearSimpleScaleHelper<blendType>(buffer, buffer + mid, data->texture, fx, fy, fdx, fdy);
                if (mid != length){
                    fetchTransformedBilinearSimpleScaleHelper<blendType>(buffer + mid, buffer + length, data->texture, fx, fy, fdx, fdy);
                }
            } else {
                const auto fetcher = fetchTransformedBilinearFetcher<blendType,bpp,uint32_t>;

                uint32_t buf1[draw::BufferSize];
                uint32_t buf2[draw::BufferSize];
                uint32_t *b = buffer;
                while (length) {
                    int len = std::min<int>(length, draw::BufferSize / 2);
                    fetcher(buf1, buf2, len, data->texture, fx, fy, fdx, 0);
                    layout->convertToARGB32PM(buf1, len * 2, clut);
                    layout->convertToARGB32PM(buf2, len * 2, clut);

                    if (hasFastInterpolate4() || std::abs(data->m22) < double(1./8.)) { // scale up more than 8x (on Y)
                        int disty = (fy & 0x0000ffff) >> 8;
                        for (int i = 0; i < len; ++i) {
                            int distx = (fx & 0x0000ffff) >> 8;
                            b[i] = interpolate_4_pixels(buf1 + i * 2, buf2 + i * 2, distx, disty);
                            fx += fdx;
                        }
                    } else {
                        int disty = ((fy & 0x0000ffff) + 0x0800) >> 12;
                        for (int i = 0; i < len; ++i) {
                            uint32_t tl = buf1[i * 2 + 0];
                            uint32_t tr = buf1[i * 2 + 1];
                            uint32_t bl = buf2[i * 2 + 0];
                            uint32_t br = buf2[i * 2 + 1];
                            int distx = ((fx & 0x0000ffff) + 0x0800) >> 12;
                            b[i] = interpolate_4_pixels_16(tl, tr, bl, br, distx, disty);
                            fx += fdx;
                        }
                    }
                    length -= len;
                    b += len;
                }
            }
        } else { // rotation or shear
            const auto fetcher = fetchTransformedBilinearFetcher<blendType,bpp,uint32_t>;

            uint32_t buf1[draw::BufferSize];
            uint32_t buf2[draw::BufferSize];
            uint32_t *b = buffer;
            while (length) {
                int len = std::min<int>(length, draw::BufferSize / 2);
                fetcher(buf1, buf2, len, data->texture, fx, fy, fdx, fdy);
                layout->convertToARGB32PM(buf1, len * 2, clut);
                layout->convertToARGB32PM(buf2, len * 2, clut);

                if (hasFastInterpolate4() || std::abs(data->m11) < double(1./8.) || std::abs(data->m22) < double(1./8.)) {
                    // If we are zooming more than 8 times, we use 8bit precision for the position.
                    for (int i = 0; i < len; ++i) {
                        int distx = (fx & 0x0000ffff) >> 8;
                        int disty = (fy & 0x0000ffff) >> 8;

                        b[i] = interpolate_4_pixels(buf1 + i * 2, buf2 + i * 2, distx, disty);
                        fx += fdx;
                        fy += fdy;
                    }
                } else {
                    // We are zooming less than 8x, use 4bit precision
                    for (int i = 0; i < len; ++i) {
                        uint32_t tl = buf1[i * 2 + 0];
                        uint32_t tr = buf1[i * 2 + 1];
                        uint32_t bl = buf2[i * 2 + 0];
                        uint32_t br = buf2[i * 2 + 1];

                        int distx = ((fx & 0x0000ffff) + 0x0800) >> 12;
                        int disty = ((fy & 0x0000ffff) + 0x0800) >> 12;

                        b[i] = interpolate_4_pixels_16(tl, tr, bl, br, distx, disty);
                        fx += fdx;
                        fy += fdy;
                    }
                }

                length -= len;
                b += len;
            }
        }
    } else {
        const auto fetcher = fetchTransformedBilinearSlowFetcher<blendType,bpp,uint32_t>;

        const double fdx = data->m11;
        const double fdy = data->m12;
        const double fdw = data->m13;

        double fx = data->m21 * cy + data->m11 * cx + data->dx;
        double fy = data->m22 * cy + data->m12 * cx + data->dy;
        double fw = data->m23 * cy + data->m13 * cx + data->m33;

        uint32_t buf1[draw::BufferSize];
        uint32_t buf2[draw::BufferSize];
        uint32_t *b = buffer;

        uint16_t distxs[draw::BufferSize / 2];
        uint16_t distys[draw::BufferSize / 2];

        while (length) {
            const int len = std::min<int>(length, draw::BufferSize / 2);
            fetcher(buf1, buf2, distxs, distys, len, data->texture, fx, fy, fw, fdx, fdy, fdw);

            layout->convertToARGB32PM(buf1, len * 2, clut);
            layout->convertToARGB32PM(buf2, len * 2, clut);

            for (int i = 0; i < len; ++i) {
                const int distx = distxs[i] >> 8;
                const int disty = distys[i] >> 8;

                b[i] = interpolate_4_pixels(buf1 + i * 2, buf2 + i * 2, distx, disty);
            }
            length -= len;
            b += len;
        }
    }

    return buffer;
}


enum FastTransformTypes {
    SimpleScaleTransform,
    UpscaleTransform,
    DownscaleTransform,
    RotateTransform,
    FastRotateTransform,
    NFastTransformTypes
};

typedef void (*BilinearFastTransformHelper)(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image, int &fx, int &fy, int fdx, int fdy);
template<TextureBlendType blendType>
static void  fetchTransformedBilinearARGB32PMSimpleScaleHelper(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image,
                                                                             int &fx, int &fy, int fdx, int /*fdy*/)
{
    int y1 = (fy >> 16);
    int y2;
    fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
    const uint32_t *s1 = (const uint32_t *)image->scanLine(y1);
    const uint32_t *s2 = (const uint32_t *)image->scanLine(y2);

    const int disty = (fy & 0x0000ffff) >> 8;
    const int idisty = 256 - disty;
    const int length = end - b;

    // The intermediate buffer is generated in the positive direction
    const int adjust = (fdx < 0) ? fdx * length : 0;
    const int offset = (fx + adjust) >> 16;
    int x = offset;

    IntermediateBuffer intermediate;
    // count is the size used in the intermediate.buffer.
    int count = (int64_t(length) * std::abs(fdx) + fixed_scale - 1) / fixed_scale + 2;
    // length is supposed to be <= BufferSize either because data->m11 < 1 or
    // data->m11 < 2, and any larger buffers split
    assert(count <= draw::BufferSize + 2);
    int f = 0;
    int lim = count;
    if (blendType == BlendTransformedBilinearTiled) {
        x %= image->width;
        if (x < 0) x += image->width;
    } else {
        lim = std::min<int>(count, image->x2 - x);
        if (x < image->x1) {
            assert(x < image->x2);
            uint32_t t = s1[image->x1];
            uint32_t b = s2[image->x1];
            uint32_t rb = (((t & 0xff00ff) * idisty + (b & 0xff00ff) * disty) >> 8) & 0xff00ff;
            uint32_t ag = ((((t>>8) & 0xff00ff) * idisty + ((b>>8) & 0xff00ff) * disty) >> 8) & 0xff00ff;
            do {
                intermediate.buffer_rb[f] = rb;
                intermediate.buffer_ag[f] = ag;
                f++;
                x++;
            } while (x < image->x1 && f < lim);
        }
    }

    if (blendType != BlendTransformedBilinearTiled) {
#if defined(__SSE2__)
        const __m128i disty_ = _mm_set1_epi16(disty);
        const __m128i idisty_ = _mm_set1_epi16(idisty);
        const __m128i colorMask = _mm_set1_epi32(0x00ff00ff);

        lim -= 3;
        for (; f < lim; x += 4, f += 4) {
            // Load 4 pixels from s1, and split the alpha-green and red-blue component
            __m128i top = _mm_loadu_si128((const __m128i*)((const uint32_t *)(s1)+x));
            __m128i topAG = _mm_srli_epi16(top, 8);
            __m128i topRB = _mm_and_si128(top, colorMask);
            // Multiplies each color component by idisty
            topAG = _mm_mullo_epi16 (topAG, idisty_);
            topRB = _mm_mullo_epi16 (topRB, idisty_);

            // Same for the s2 vector
            __m128i bottom = _mm_loadu_si128((const __m128i*)((const uint32_t *)(s2)+x));
            __m128i bottomAG = _mm_srli_epi16(bottom, 8);
            __m128i bottomRB = _mm_and_si128(bottom, colorMask);
            bottomAG = _mm_mullo_epi16 (bottomAG, disty_);
            bottomRB = _mm_mullo_epi16 (bottomRB, disty_);

            // Add the values, and shift to only keep 8 significant bits per colors
            __m128i rAG =_mm_add_epi16(topAG, bottomAG);
            rAG = _mm_srli_epi16(rAG, 8);
            _mm_storeu_si128((__m128i*)(&intermediate.buffer_ag[f]), rAG);
            __m128i rRB =_mm_add_epi16(topRB, bottomRB);
            rRB = _mm_srli_epi16(rRB, 8);
            _mm_storeu_si128((__m128i*)(&intermediate.buffer_rb[f]), rRB);
        }
#elif defined(__ARM_NEON__)
        const int16x8_t disty_ = vdupq_n_s16(disty);
        const int16x8_t idisty_ = vdupq_n_s16(idisty);
        const int16x8_t colorMask = vdupq_n_s16(0x00ff);

        lim -= 3;
        for (; f < lim; x += 4, f += 4) {
            // Load 4 pixels from s1, and split the alpha-green and red-blue component
            int16x8_t top = vld1q_s16((int16_t*)((const uint32_t *)(s1)+x));
            int16x8_t topAG = vreinterpretq_s16_u16(vshrq_n_u16(vreinterpretq_u16_s16(top), 8));
            int16x8_t topRB = vandq_s16(top, colorMask);
            // Multiplies each color component by idisty
            topAG = vmulq_s16(topAG, idisty_);
            topRB = vmulq_s16(topRB, idisty_);

            // Same for the s2 vector
            int16x8_t bottom = vld1q_s16((int16_t*)((const uint32_t *)(s2)+x));
            int16x8_t bottomAG = vreinterpretq_s16_u16(vshrq_n_u16(vreinterpretq_u16_s16(bottom), 8));
            int16x8_t bottomRB = vandq_s16(bottom, colorMask);
            bottomAG = vmulq_s16(bottomAG, disty_);
            bottomRB = vmulq_s16(bottomRB, disty_);

            // Add the values, and shift to only keep 8 significant bits per colors
            int16x8_t rAG = vaddq_s16(topAG, bottomAG);
            rAG = vreinterpretq_s16_u16(vshrq_n_u16(vreinterpretq_u16_s16(rAG), 8));
            vst1q_s16((int16_t*)(&intermediate.buffer_ag[f]), rAG);
            int16x8_t rRB = vaddq_s16(topRB, bottomRB);
            rRB = vreinterpretq_s16_u16(vshrq_n_u16(vreinterpretq_u16_s16(rRB), 8));
            vst1q_s16((int16_t*)(&intermediate.buffer_rb[f]), rRB);
        }
#elif defined(__loongarch_sx)
        const __m128i disty_ = __lsx_vreplgr2vr_h(disty);
        const __m128i idisty_ = __lsx_vreplgr2vr_h(idisty);
        const __m128i colorMask = __lsx_vreplgr2vr_w(0x00ff00ff);

        lim -= 3;
        for (; f < lim; x += 4, f += 4) {
            // Load 4 pixels from s1, and split the alpha-green and red-blue component
            __m128i top = __lsx_vld((const __m128i*)((const uint32_t *)(s1)+x), 0);
            __m128i topAG = __lsx_vsrli_h(top, 8);
            __m128i topRB = __lsx_vand_v(top, colorMask);
            // Multiplies each color component by idisty
            topAG = __lsx_vmul_h(topAG, idisty_);
            topRB = __lsx_vmul_h(topRB, idisty_);

            // Same for the s2 vector
            __m128i bottom = __lsx_vld((const __m128i*)((const uint32_t *)(s2)+x), 0);
            __m128i bottomAG = __lsx_vsrli_h(bottom, 8);
            __m128i bottomRB = __lsx_vand_v(bottom, colorMask);
            bottomAG = __lsx_vmul_h(bottomAG, disty_);
            bottomRB = __lsx_vmul_h(bottomRB, disty_);

            // Add the values, and shift to only keep 8 significant bits per colors
            __m128i rAG = __lsx_vadd_h(topAG, bottomAG);
            rAG = __lsx_vsrli_h(rAG, 8);
            __lsx_vst(rAG, (__m128i*)(&intermediate.buffer_ag[f]), 0);
            __m128i rRB = __lsx_vadd_h(topRB, bottomRB);
            rRB = __lsx_vsrli_h(rRB, 8);
            __lsx_vst(rRB, (__m128i*)(&intermediate.buffer_rb[f]), 0);
        }
#endif
    }
    for (; f < count; f++) { // Same as above but without simd
        if (blendType == BlendTransformedBilinearTiled) {
            if (x >= image->width) x -= image->width;
        } else {
            x = std::min<int>(x, image->x2 - 1);
        }

        uint32_t t = s1[x];
        uint32_t b = s2[x];

        intermediate.buffer_rb[f] = (((t & 0xff00ff) * idisty + (b & 0xff00ff) * disty) >> 8) & 0xff00ff;
        intermediate.buffer_ag[f] = ((((t>>8) & 0xff00ff) * idisty + ((b>>8) & 0xff00ff) * disty) >> 8) & 0xff00ff;
        x++;
    }

    // Now interpolate the values from the intermediate.buffer to get the final result.
    intermediateAdder(b, end, intermediate, offset, fx, fdx);
}

template<TextureBlendType blendType>
static void  fetchTransformedBilinearARGB32PMUpscaleHelper(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image,
                                                                        int &fx, int &fy, int fdx, int /*fdy*/)
{
    int y1 = (fy >> 16);
    int y2;
    fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
    const uint32_t *s1 = (const uint32_t *)image->scanLine(y1);
    const uint32_t *s2 = (const uint32_t *)image->scanLine(y2);
    const int disty = (fy & 0x0000ffff) >> 8;

    if (blendType != BlendTransformedBilinearTiled) {
        const int64_t min_fx = int64_t(image->x1) * fixed_scale;
        const int64_t max_fx = int64_t(image->x2 - 1) * fixed_scale;
        while (b < end) {
            int x1 = (fx >> 16);
            int x2;
            fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
            if (x1 != x2)
                break;
            uint32_t top = s1[x1];
            uint32_t bot = s2[x1];
            *b = INTERPOLATE_PIXEL_256(top, 256 - disty, bot, disty);
            fx += fdx;
            ++b;
        }
        uint32_t *boundedEnd = end;
        if (fdx > 0)
            boundedEnd = std::min<uint32_t*>(boundedEnd, (b + (max_fx - fx) / fdx));
        else if (fdx < 0)
            boundedEnd = std::min<uint32_t*>(boundedEnd, b + (min_fx - fx) / fdx);

        // A fast middle part without boundary checks
        while (b < boundedEnd) {
            int x = (fx >> 16);
            int distx = (fx & 0x0000ffff) >> 8;
            *b = interpolate_4_pixels(s1 + x, s2 + x, distx, disty);
            fx += fdx;
            ++b;
        }
    }

    while (b < end) {
        int x1 = (fx >> 16);
        int x2;
        fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1 , x1, x2);
        uint32_t tl = s1[x1];
        uint32_t tr = s1[x2];
        uint32_t bl = s2[x1];
        uint32_t br = s2[x2];
        int distx = (fx & 0x0000ffff) >> 8;
        *b = interpolate_4_pixels(tl, tr, bl, br, distx, disty);

        fx += fdx;
        ++b;
    }
}

template<TextureBlendType blendType>
static void  fetchTransformedBilinearARGB32PMDownscaleHelper(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image,
                                                                          int &fx, int &fy, int fdx, int /*fdy*/)
{
    int y1 = (fy >> 16);
    int y2;
    fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
    const uint32_t *s1 = (const uint32_t *)image->scanLine(y1);
    const uint32_t *s2 = (const uint32_t *)image->scanLine(y2);
    const int disty8 = (fy & 0x0000ffff) >> 8;
    const int disty4 = (disty8 + 0x08) >> 4;

    if (blendType != BlendTransformedBilinearTiled) {
        const int64_t min_fx = int64_t(image->x1) * fixed_scale;
        const int64_t max_fx = int64_t(image->x2 - 1) * fixed_scale;
        while (b < end) {
            int x1 = (fx >> 16);
            int x2;
            fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
            if (x1 != x2)
                break;
            uint32_t top = s1[x1];
            uint32_t bot = s2[x1];
            *b = INTERPOLATE_PIXEL_256(top, 256 - disty8, bot, disty8);
            fx += fdx;
            ++b;
        }
        uint32_t *boundedEnd = end;
        if (fdx > 0)
            boundedEnd = std::min<uint32_t *>(boundedEnd, b + (max_fx - fx) / fdx);
        else if (fdx < 0)
            boundedEnd = std::min<uint32_t *>(boundedEnd, b + (min_fx - fx) / fdx);
        // A fast middle part without boundary checks
#if defined(__SSE2__)
        const __m128i colorMask = _mm_set1_epi32(0x00ff00ff);
        const __m128i v_256 = _mm_set1_epi16(256);
        const __m128i v_disty = _mm_set1_epi16(disty4);
        const __m128i v_fdx = _mm_set1_epi32(fdx*4);
        const __m128i v_fx_r = _mm_set1_epi32(0x8);
        __m128i v_fx = _mm_setr_epi32(fx, fx + fdx, fx + fdx + fdx, fx + fdx + fdx + fdx);

        while (b < boundedEnd - 3) {
            __m128i offset = _mm_srli_epi32(v_fx, 16);
            const int offset0 = _mm_cvtsi128_si32(offset); offset = _mm_srli_si128(offset, 4);
            const int offset1 = _mm_cvtsi128_si32(offset); offset = _mm_srli_si128(offset, 4);
            const int offset2 = _mm_cvtsi128_si32(offset); offset = _mm_srli_si128(offset, 4);
            const int offset3 = _mm_cvtsi128_si32(offset);
            const __m128i tl = _mm_setr_epi32(s1[offset0], s1[offset1], s1[offset2], s1[offset3]);
            const __m128i tr = _mm_setr_epi32(s1[offset0 + 1], s1[offset1 + 1], s1[offset2 + 1], s1[offset3 + 1]);
            const __m128i bl = _mm_setr_epi32(s2[offset0], s2[offset1], s2[offset2], s2[offset3]);
            const __m128i br = _mm_setr_epi32(s2[offset0 + 1], s2[offset1 + 1], s2[offset2 + 1], s2[offset3 + 1]);

            __m128i v_distx = _mm_srli_epi16(v_fx, 8);
            v_distx = _mm_srli_epi16(_mm_add_epi32(v_distx, v_fx_r), 4);
            v_distx = _mm_shufflehi_epi16(v_distx, _MM_SHUFFLE(2,2,0,0));
            v_distx = _mm_shufflelo_epi16(v_distx, _MM_SHUFFLE(2,2,0,0));

            interpolate_4_pixels_16_sse2(tl, tr, bl, br, v_distx, v_disty, colorMask, v_256, b);
            b += 4;
            v_fx = _mm_add_epi32(v_fx, v_fdx);
        }
        fx = _mm_cvtsi128_si32(v_fx);
#elif defined(__ARM_NEON__)
        const int16x8_t colorMask = vdupq_n_s16(0x00ff);
        const int16x8_t invColorMask = vmvnq_s16(colorMask);
        const int16x8_t v_256 = vdupq_n_s16(256);
        const int16x8_t v_disty = vdupq_n_s16(disty4);
        const int16x8_t v_disty_ = vshlq_n_s16(v_disty, 4);
        int32x4_t v_fdx = vdupq_n_s32(fdx*4);

        int32x4_t v_fx = vmovq_n_s32(fx);
        v_fx = vsetq_lane_s32(fx + fdx, v_fx, 1);
        v_fx = vsetq_lane_s32(fx + fdx * 2, v_fx, 2);
        v_fx = vsetq_lane_s32(fx + fdx * 3, v_fx, 3);

        const int32x4_t v_ffff_mask = vdupq_n_s32(0x0000ffff);
        const int32x4_t v_fx_r = vdupq_n_s32(0x0800);

        // Pre-initialize to work-around code-analysis warnings/crashes in MSVC:
        uint32x4x2_t v_top = {};
        uint32x4x2_t v_bot = {};
        while (b < boundedEnd - 3) {
            int x1 = (fx >> 16);
            fx += fdx;
            v_top = vld2q_lane_u32(s1 + x1, v_top, 0);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 0);
            x1 = (fx >> 16);
            fx += fdx;
            v_top = vld2q_lane_u32(s1 + x1, v_top, 1);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 1);
            x1 = (fx >> 16);
            fx += fdx;
            v_top = vld2q_lane_u32(s1 + x1, v_top, 2);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 2);
            x1 = (fx >> 16);
            fx += fdx;
            v_top = vld2q_lane_u32(s1 + x1, v_top, 3);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 3);

            int32x4_t v_distx = vshrq_n_s32(vaddq_s32(vandq_s32(v_fx, v_ffff_mask), v_fx_r), 12);
            v_distx = vorrq_s32(v_distx, vshlq_n_s32(v_distx, 16));

            interpolate_4_pixels_16_neon(
                vreinterpretq_s16_u32(v_top.val[0]), vreinterpretq_s16_u32(v_top.val[1]),
                vreinterpretq_s16_u32(v_bot.val[0]), vreinterpretq_s16_u32(v_bot.val[1]),
                vreinterpretq_s16_s32(v_distx), v_disty, v_disty_,
                colorMask, invColorMask, v_256, b);
            b+=4;
            v_fx = vaddq_s32(v_fx, v_fdx);
        }
#elif defined (__loongarch_sx)
        const __m128i shuffleMask = (__m128i)(v8i16){0, 0, 2, 2, 4, 4, 6, 6};
        const __m128i v_disty = __lsx_vreplgr2vr_h(disty4);
        const __m128i v_fdx = __lsx_vreplgr2vr_w(fdx*4);
        const __m128i v_fx_r = __lsx_vreplgr2vr_w(0x8);
        __m128i v_fx = (__m128i)(v4i32){fx, fx + fdx, fx + fdx + fdx, fx + fdx + fdx + fdx};

        while (b < boundedEnd - 3) {
            __m128i offset = __lsx_vsrli_w(v_fx, 16);
            const int offset0 = __lsx_vpickve2gr_w(offset, 0);
            const int offset1 = __lsx_vpickve2gr_w(offset, 1);
            const int offset2 = __lsx_vpickve2gr_w(offset, 2);
            const int offset3 = __lsx_vpickve2gr_w(offset, 3);
            const __m128i tl = (__m128i)(v4u32){s1[offset0], s1[offset1], s1[offset2], s1[offset3]};
            const __m128i tr = (__m128i)(v4u32){s1[offset0 + 1], s1[offset1 + 1], s1[offset2 + 1], s1[offset3 + 1]};
            const __m128i bl = (__m128i)(v4u32){s2[offset0], s2[offset1], s2[offset2], s2[offset3]};
            const __m128i br = (__m128i)(v4u32){s2[offset0 + 1], s2[offset1 + 1], s2[offset2 + 1], s2[offset3 + 1]};

            __m128i v_distx = __lsx_vsrli_h(v_fx, 8);
            v_distx = __lsx_vsrli_h(__lsx_vadd_w(v_distx, v_fx_r), 4);
            v_distx = __lsx_vshuf_h(shuffleMask, v_distx, v_distx);

            interpolate_4_pixels_16_lsx(tl, tr, bl, br, v_distx, v_disty, b);
            b += 4;
            v_fx = __lsx_vadd_w(v_fx, v_fdx);
        }
        fx = __lsx_vpickve2gr_w(v_fx, 0);
#endif
        while (b < boundedEnd) {
            int x = (fx >> 16);
            if (hasFastInterpolate4()) {
                int distx8 = (fx & 0x0000ffff) >> 8;
                *b = interpolate_4_pixels(s1 + x, s2 + x, distx8, disty8);
            } else {
                int distx4 = ((fx & 0x0000ffff) + 0x0800) >> 12;
                *b = interpolate_4_pixels_16(s1[x], s1[x + 1], s2[x], s2[x + 1], distx4, disty4);
            }
            fx += fdx;
            ++b;
        }
    }

    while (b < end) {
        int x1 = (fx >> 16);
        int x2;
        fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
        uint32_t tl = s1[x1];
        uint32_t tr = s1[x2];
        uint32_t bl = s2[x1];
        uint32_t br = s2[x2];
        if (hasFastInterpolate4()) {
            int distx8 = (fx & 0x0000ffff) >> 8;
            *b = interpolate_4_pixels(tl, tr, bl, br, distx8, disty8);
        } else {
            int distx4 = ((fx & 0x0000ffff) + 0x0800) >> 12;
            *b = interpolate_4_pixels_16(tl, tr, bl, br, distx4, disty4);
        }
        fx += fdx;
        ++b;
    }
}

template<TextureBlendType blendType>
static void  fetchTransformedBilinearARGB32PMRotateHelper(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image,
                                                                       int &fx, int &fy, int fdx, int fdy)
{
    // if we are zooming more than 8 times, we use 8bit precision for the position.
    while (b < end) {
        int x1 = (fx >> 16);
        int x2;
        int y1 = (fy >> 16);
        int y2;

        fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
        fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);

        const uint32_t *s1 = (const uint32_t *)image->scanLine(y1);
        const uint32_t *s2 = (const uint32_t *)image->scanLine(y2);

        uint32_t tl = s1[x1];
        uint32_t tr = s1[x2];
        uint32_t bl = s2[x1];
        uint32_t br = s2[x2];

        int distx = (fx & 0x0000ffff) >> 8;
        int disty = (fy & 0x0000ffff) >> 8;

        *b = interpolate_4_pixels(tl, tr, bl, br, distx, disty);

        fx += fdx;
        fy += fdy;
        ++b;
    }
}

template<TextureBlendType blendType>
static void  fetchTransformedBilinearARGB32PMFastRotateHelper(uint32_t *b, uint32_t *end, const std::shared_ptr<LTextureData> image,
                                                                            int &fx, int &fy, int fdx, int fdy)
{
    //we are zooming less than 8x, use 4bit precision
    if (blendType != BlendTransformedBilinearTiled) {
        const int64_t min_fx = int64_t(image->x1) * fixed_scale;
        const int64_t max_fx = int64_t(image->x2 - 1) * fixed_scale;
        const int64_t min_fy = int64_t(image->y1) * fixed_scale;
        const int64_t max_fy = int64_t(image->y2 - 1) * fixed_scale;
        // first handle the possibly bounded part in the beginning
        while (b < end) {
            int x1 = (fx >> 16);
            int x2;
            int y1 = (fy >> 16);
            int y2;
            fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
            fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);
            if (x1 != x2 && y1 != y2)
                break;
            const uint32_t *s1 = (const uint32_t *)image->scanLine(y1);
            const uint32_t *s2 = (const uint32_t *)image->scanLine(y2);
            uint32_t tl = s1[x1];
            uint32_t tr = s1[x2];
            uint32_t bl = s2[x1];
            uint32_t br = s2[x2];
            if (hasFastInterpolate4()) {
                int distx = (fx & 0x0000ffff) >> 8;
                int disty = (fy & 0x0000ffff) >> 8;
                *b = interpolate_4_pixels(tl, tr, bl, br, distx, disty);
            } else {
                int distx = ((fx & 0x0000ffff) + 0x0800) >> 12;
                int disty = ((fy & 0x0000ffff) + 0x0800) >> 12;
                *b = interpolate_4_pixels_16(tl, tr, bl, br, distx, disty);
            }
            fx += fdx;
            fy += fdy;
            ++b;
        }
        uint32_t *boundedEnd = end;
        if (fdx > 0)
            boundedEnd = std::min<uint32_t *>(boundedEnd, b + (max_fx - fx) / fdx);
        else if (fdx < 0)
            boundedEnd = std::min<uint32_t *>(boundedEnd, b + (min_fx - fx) / fdx);
        if (fdy > 0)
            boundedEnd = std::min<uint32_t *>(boundedEnd, b + (max_fy - fy) / fdy);
        else if (fdy < 0)
            boundedEnd = std::min<uint32_t *>(boundedEnd, b + (min_fy - fy) / fdy);

        // until boundedEnd we can now have a fast middle part without boundary checks
#if defined(__SSE2__)
        const __m128i colorMask = _mm_set1_epi32(0x00ff00ff);
        const __m128i v_256 = _mm_set1_epi16(256);
        const __m128i v_fdx = _mm_set1_epi32(fdx*4);
        const __m128i v_fdy = _mm_set1_epi32(fdy*4);
        const __m128i v_fxy_r = _mm_set1_epi32(0x8);
        __m128i v_fx = _mm_setr_epi32(fx, fx + fdx, fx + fdx + fdx, fx + fdx + fdx + fdx);
        __m128i v_fy = _mm_setr_epi32(fy, fy + fdy, fy + fdy + fdy, fy + fdy + fdy + fdy);

        const uint8_t *textureData = image->imageData;
        const lsizetype bytesPerLine = image->bytesPerLine;
        const __m128i vbpl = _mm_shufflelo_epi16(_mm_cvtsi32_si128(bytesPerLine/4), _MM_SHUFFLE(0, 0, 0, 0));

        while (b < boundedEnd - 3) {
            const __m128i vy = _mm_packs_epi32(_mm_srli_epi32(v_fy, 16), _mm_setzero_si128());
            // 4x16bit * 4x16bit -> 4x32bit
            __m128i offset = _mm_unpacklo_epi16(_mm_mullo_epi16(vy, vbpl), _mm_mulhi_epi16(vy, vbpl));
            offset = _mm_add_epi32(offset, _mm_srli_epi32(v_fx, 16));
            const int offset0 = _mm_cvtsi128_si32(offset); offset = _mm_srli_si128(offset, 4);
            const int offset1 = _mm_cvtsi128_si32(offset); offset = _mm_srli_si128(offset, 4);
            const int offset2 = _mm_cvtsi128_si32(offset); offset = _mm_srli_si128(offset, 4);
            const int offset3 = _mm_cvtsi128_si32(offset);
            const uint32_t *topData = (const uint32_t *)(textureData);
            const __m128i tl = _mm_setr_epi32(topData[offset0], topData[offset1], topData[offset2], topData[offset3]);
            const __m128i tr = _mm_setr_epi32(topData[offset0 + 1], topData[offset1 + 1], topData[offset2 + 1], topData[offset3 + 1]);
            const uint32_t *bottomData = (const uint32_t *)(textureData + bytesPerLine);
            const __m128i bl = _mm_setr_epi32(bottomData[offset0], bottomData[offset1], bottomData[offset2], bottomData[offset3]);
            const __m128i br = _mm_setr_epi32(bottomData[offset0 + 1], bottomData[offset1 + 1], bottomData[offset2 + 1], bottomData[offset3 + 1]);

            __m128i v_distx = _mm_srli_epi16(v_fx, 8);
            __m128i v_disty = _mm_srli_epi16(v_fy, 8);
            v_distx = _mm_srli_epi16(_mm_add_epi32(v_distx, v_fxy_r), 4);
            v_disty = _mm_srli_epi16(_mm_add_epi32(v_disty, v_fxy_r), 4);
            v_distx = _mm_shufflehi_epi16(v_distx, _MM_SHUFFLE(2,2,0,0));
            v_distx = _mm_shufflelo_epi16(v_distx, _MM_SHUFFLE(2,2,0,0));
            v_disty = _mm_shufflehi_epi16(v_disty, _MM_SHUFFLE(2,2,0,0));
            v_disty = _mm_shufflelo_epi16(v_disty, _MM_SHUFFLE(2,2,0,0));

            interpolate_4_pixels_16_sse2(tl, tr, bl, br, v_distx, v_disty, colorMask, v_256, b);
            b += 4;
            v_fx = _mm_add_epi32(v_fx, v_fdx);
            v_fy = _mm_add_epi32(v_fy, v_fdy);
        }
        fx = _mm_cvtsi128_si32(v_fx);
        fy = _mm_cvtsi128_si32(v_fy);
#elif defined(__ARM_NEON__)
        const int16x8_t colorMask = vdupq_n_s16(0x00ff);
        const int16x8_t invColorMask = vmvnq_s16(colorMask);
        const int16x8_t v_256 = vdupq_n_s16(256);
        int32x4_t v_fdx = vdupq_n_s32(fdx * 4);
        int32x4_t v_fdy = vdupq_n_s32(fdy * 4);

        const uint8_t *textureData = image->imageData;
        const lsizetype bytesPerLine = image->bytesPerLine;

        int32x4_t v_fx = vmovq_n_s32(fx);
        int32x4_t v_fy = vmovq_n_s32(fy);
        v_fx = vsetq_lane_s32(fx + fdx, v_fx, 1);
        v_fy = vsetq_lane_s32(fy + fdy, v_fy, 1);
        v_fx = vsetq_lane_s32(fx + fdx * 2, v_fx, 2);
        v_fy = vsetq_lane_s32(fy + fdy * 2, v_fy, 2);
        v_fx = vsetq_lane_s32(fx + fdx * 3, v_fx, 3);
        v_fy = vsetq_lane_s32(fy + fdy * 3, v_fy, 3);

        const int32x4_t v_ffff_mask = vdupq_n_s32(0x0000ffff);
        const int32x4_t v_round = vdupq_n_s32(0x0800);

        // Pre-initialize to work-around code-analysis warnings/crashes in MSVC:
        uint32x4x2_t v_top = {};
        uint32x4x2_t v_bot = {};
        while (b < boundedEnd - 3) {
            int x1 = (fx >> 16);
            int y1 = (fy >> 16);
            fx += fdx; fy += fdy;
            const uint8_t *sl = textureData + bytesPerLine * y1;
            const uint32_t *s1 = reinterpret_cast<const uint32_t *>(sl);
            const uint32_t *s2 = reinterpret_cast<const uint32_t *>(sl + bytesPerLine);
            v_top = vld2q_lane_u32(s1 + x1, v_top, 0);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 0);
            x1 = (fx >> 16);
            y1 = (fy >> 16);
            fx += fdx; fy += fdy;
            sl = textureData + bytesPerLine * y1;
            s1 = reinterpret_cast<const uint32_t *>(sl);
            s2 = reinterpret_cast<const uint32_t *>(sl + bytesPerLine);
            v_top = vld2q_lane_u32(s1 + x1, v_top, 1);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 1);
            x1 = (fx >> 16);
            y1 = (fy >> 16);
            fx += fdx; fy += fdy;
            sl = textureData + bytesPerLine * y1;
            s1 = reinterpret_cast<const uint32_t *>(sl);
            s2 = reinterpret_cast<const uint32_t *>(sl + bytesPerLine);
            v_top = vld2q_lane_u32(s1 + x1, v_top, 2);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 2);
            x1 = (fx >> 16);
            y1 = (fy >> 16);
            fx += fdx; fy += fdy;
            sl = textureData + bytesPerLine * y1;
            s1 = reinterpret_cast<const uint32_t *>(sl);
            s2 = reinterpret_cast<const uint32_t *>(sl + bytesPerLine);
            v_top = vld2q_lane_u32(s1 + x1, v_top, 3);
            v_bot = vld2q_lane_u32(s2 + x1, v_bot, 3);

            int32x4_t v_distx = vshrq_n_s32(vaddq_s32(vandq_s32(v_fx, v_ffff_mask), v_round), 12);
            int32x4_t v_disty = vshrq_n_s32(vaddq_s32(vandq_s32(v_fy, v_ffff_mask), v_round), 12);
            v_distx = vorrq_s32(v_distx, vshlq_n_s32(v_distx, 16));
            v_disty = vorrq_s32(v_disty, vshlq_n_s32(v_disty, 16));
            int16x8_t v_disty_ = vshlq_n_s16(vreinterpretq_s16_s32(v_disty), 4);

            interpolate_4_pixels_16_neon(
                vreinterpretq_s16_u32(v_top.val[0]), vreinterpretq_s16_u32(v_top.val[1]),
                vreinterpretq_s16_u32(v_bot.val[0]), vreinterpretq_s16_u32(v_bot.val[1]),
                vreinterpretq_s16_s32(v_distx), vreinterpretq_s16_s32(v_disty),
                v_disty_, colorMask, invColorMask, v_256, b);
            b += 4;
            v_fx = vaddq_s32(v_fx, v_fdx);
            v_fy = vaddq_s32(v_fy, v_fdy);
        }
#elif defined(__loongarch_sx)
        const __m128i v_fdx = __lsx_vreplgr2vr_w(fdx*4);
        const __m128i v_fdy = __lsx_vreplgr2vr_w(fdy*4);
        const __m128i v_fxy_r = __lsx_vreplgr2vr_w(0x8);
        __m128i v_fx = (__m128i)(v4i32){fx, fx + fdx, fx + fdx + fdx, fx + fdx + fdx + fdx};
        __m128i v_fy = (__m128i)(v4i32){fy, fy + fdy, fy + fdy + fdy, fy + fdy + fdy + fdy};

        const uint8_t *textureData = image->imageData;
        const lsizetype bytesPerLine = image->bytesPerLine;
        const __m128i zero = __lsx_vldi(0);
        const __m128i shuffleMask = (__m128i)(v8i16){0, 0, 0, 0, 4, 5, 6, 7};
        const __m128i shuffleMask1 = (__m128i)(v8i16){0, 0, 2, 2, 4, 4, 6, 6};
        const __m128i vbpl = __lsx_vshuf_h(shuffleMask, zero, __lsx_vinsgr2vr_w(zero, bytesPerLine/4, 0));

        while (b < boundedEnd - 3) {
            const __m128i vy = __lsx_vpickev_h(zero, __lsx_vsat_w(__lsx_vsrli_w(v_fy, 16), 15));
            // 4x16bit * 4x16bit -> 4x32bit
            __m128i offset = __lsx_vilvl_h(__lsx_vmuh_h(vy, vbpl), __lsx_vmul_h(vy, vbpl));
            offset = __lsx_vadd_w(offset, __lsx_vsrli_w(v_fx, 16));
            const int offset0 = __lsx_vpickve2gr_w(offset, 0);
            const int offset1 = __lsx_vpickve2gr_w(offset, 1);
            const int offset2 = __lsx_vpickve2gr_w(offset, 2);
            const int offset3 = __lsx_vpickve2gr_w(offset, 3);
            const uint32_t *topData = (const uint32_t *)(textureData);
            const __m128i tl = (__m128i)(v4u32){topData[offset0], topData[offset1], topData[offset2], topData[offset3]};
            const __m128i tr = (__m128i)(v4u32){topData[offset0 + 1], topData[offset1 + 1], topData[offset2 + 1], topData[offset3 + 1]};
            const uint32_t *bottomData = (const uint32_t *)(textureData + bytesPerLine);
            const __m128i bl = (__m128i)(v4u32){bottomData[offset0], bottomData[offset1], bottomData[offset2], bottomData[offset3]};
            const __m128i br = (__m128i)(v4u32){bottomData[offset0 + 1], bottomData[offset1 + 1], bottomData[offset2 + 1], bottomData[offset3 + 1]};

            __m128i v_distx = __lsx_vsrli_h(v_fx, 8);
            __m128i v_disty = __lsx_vsrli_h(v_fy, 8);
            v_distx = __lsx_vsrli_h(__lsx_vadd_w(v_distx, v_fxy_r), 4);
            v_disty = __lsx_vsrli_h(__lsx_vadd_w(v_disty, v_fxy_r), 4);
            v_distx = __lsx_vshuf_h(shuffleMask1, zero, v_distx);
            v_disty = __lsx_vshuf_h(shuffleMask1, zero, v_disty);

            interpolate_4_pixels_16_lsx(tl, tr, bl, br, v_distx, v_disty, b);
            b += 4;
            v_fx = __lsx_vadd_w(v_fx, v_fdx);
            v_fy = __lsx_vadd_w(v_fy, v_fdy);
        }
        fx = __lsx_vpickve2gr_w(v_fx, 0);
        fy = __lsx_vpickve2gr_w(v_fy, 0);
#endif
        while (b < boundedEnd) {
            int x = (fx >> 16);
            int y = (fy >> 16);

            const uint32_t *s1 = (const uint32_t *)image->scanLine(y);
            const uint32_t *s2 = (const uint32_t *)image->scanLine(y + 1);

            if (hasFastInterpolate4()) {
                int distx = (fx & 0x0000ffff) >> 8;
                int disty = (fy & 0x0000ffff) >> 8;
                *b = interpolate_4_pixels(s1 + x, s2 + x, distx, disty);
            } else {
                int distx = ((fx & 0x0000ffff) + 0x0800) >> 12;
                int disty = ((fy & 0x0000ffff) + 0x0800) >> 12;
                *b = interpolate_4_pixels_16(s1[x], s1[x + 1], s2[x], s2[x + 1], distx, disty);
            }

            fx += fdx;
            fy += fdy;
            ++b;
        }
    }

    while (b < end) {
        int x1 = (fx >> 16);
        int x2;
        int y1 = (fy >> 16);
        int y2;

        fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
        fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);

        const uint32_t *s1 = (const uint32_t *)image->scanLine(y1);
        const uint32_t *s2 = (const uint32_t *)image->scanLine(y2);

        uint32_t tl = s1[x1];
        uint32_t tr = s1[x2];
        uint32_t bl = s2[x1];
        uint32_t br = s2[x2];

        if (hasFastInterpolate4()) {
            int distx = (fx & 0x0000ffff) >> 8;
            int disty = (fy & 0x0000ffff) >> 8;
            *b = interpolate_4_pixels(tl, tr, bl, br, distx, disty);
        } else {
            int distx = ((fx & 0x0000ffff) + 0x0800) >> 12;
            int disty = ((fy & 0x0000ffff) + 0x0800) >> 12;
            *b = interpolate_4_pixels_16(tl, tr, bl, br, distx, disty);
        }

        fx += fdx;
        fy += fdy;
        ++b;
    }
}


static BilinearFastTransformHelper bilinearFastTransformHelperARGB32PM[2][NFastTransformTypes] = {
    {
        fetchTransformedBilinearARGB32PMSimpleScaleHelper<BlendTransformedBilinear>,
        fetchTransformedBilinearARGB32PMUpscaleHelper<BlendTransformedBilinear>,
        fetchTransformedBilinearARGB32PMDownscaleHelper<BlendTransformedBilinear>,
        fetchTransformedBilinearARGB32PMRotateHelper<BlendTransformedBilinear>,
        fetchTransformedBilinearARGB32PMFastRotateHelper<BlendTransformedBilinear>
    },
    {
        fetchTransformedBilinearARGB32PMSimpleScaleHelper<BlendTransformedBilinearTiled>,
        fetchTransformedBilinearARGB32PMUpscaleHelper<BlendTransformedBilinearTiled>,
        fetchTransformedBilinearARGB32PMDownscaleHelper<BlendTransformedBilinearTiled>,
        fetchTransformedBilinearARGB32PMRotateHelper<BlendTransformedBilinearTiled>,
        fetchTransformedBilinearARGB32PMFastRotateHelper<BlendTransformedBilinearTiled>
    }
};

template<TextureBlendType blendType> /* blendType = BlendTransformedBilinear or BlendTransformedBilinearTiled */
static const uint32_t* fetchTransformedBilinearARGB32PM(uint32_t  *buffer, const LDrawOperator *,
                                                                const LSpanData *data, int y, int x,
                                                                int length)
{
    const double cx = x + double(0.5);
    const double cy = y + double(0.5);
    constexpr int tiled = (blendType == BlendTransformedBilinearTiled) ? 1 : 0;

    uint32_t  *end = buffer + length;
    uint32_t  *b = buffer;
    if (canUseFastMatrixPath(cx, cy, length, data)) {
        // The increment pr x in the scanline
        int fdx = (int)(data->m11 * fixed_scale);
        int fdy = (int)(data->m12 * fixed_scale);

        int fx = int((data->m21 * cy
                      + data->m11 * cx + data->dx) * fixed_scale);
        int fy = int((data->m22 * cy
                      + data->m12 * cx + data->dy) * fixed_scale);

        fx -= half_point;
        fy -= half_point;

        if (fdy == 0) { // simple scale, no rotation or shear
            if (std::abs(fdx) <= fixed_scale) {
                // simple scale up on X
                bilinearFastTransformHelperARGB32PM[tiled][SimpleScaleTransform](b, end, data->texture, fx, fy, fdx, fdy);
            } else if (std::abs(fdx) <= 2 * fixed_scale) {
                // simple scale down on X, less than 2x
                const int mid = (length * 2 < draw::BufferSize) ? length : ((length + 1) / 2);
                bilinearFastTransformHelperARGB32PM[tiled][SimpleScaleTransform](buffer, buffer + mid, data->texture, fx, fy, fdx, fdy);
                if (mid != length)
                    bilinearFastTransformHelperARGB32PM[tiled][SimpleScaleTransform](buffer + mid, buffer + length, data->texture, fx, fy, fdx, fdy);
            } else if (std::abs(data->m22) < double(1./8.)) {
                // scale up more than 8x (on Y)
                bilinearFastTransformHelperARGB32PM[tiled][UpscaleTransform](b, end, data->texture, fx, fy, fdx, fdy);
            } else {
                // scale down on X
                bilinearFastTransformHelperARGB32PM[tiled][DownscaleTransform](b, end, data->texture, fx, fy, fdx, fdy);
            }
        } else { // rotation or shear
            if (std::abs(data->m11) < double(1./8.) || std::abs(data->m22) < double(1./8.) ) {
                // if we are zooming more than 8 times, we use 8bit precision for the position.
                bilinearFastTransformHelperARGB32PM[tiled][RotateTransform](b, end, data->texture, fx, fy, fdx, fdy);
            } else {
                // we are zooming less than 8x, use 4bit precision
                bilinearFastTransformHelperARGB32PM[tiled][FastRotateTransform](b, end, data->texture, fx, fy, fdx, fdy);
            }
        }
    } else {
        const std::shared_ptr<LTextureData> image = data->texture;

        const double fdx = data->m11;
        const double fdy = data->m12;
        const double fdw = data->m13;

        double fx = data->m21 * cy + data->m11 * cx + data->dx;
        double fy = data->m22 * cy + data->m12 * cx + data->dy;
        double fw = data->m23 * cy + data->m13 * cx + data->m33;

        while (b < end) {
            const double iw = fw == 0 ? 1 : 1 / fw;
            const double px = fx * iw - double(0.5);
            const double py = fy * iw - double(0.5);

            int x1 = int(px) - (px < 0);
            int x2;
            int y1 = int(py) - (py < 0);
            int y2;

            int distx = int((px - x1) * 256);
            int disty = int((py - y1) * 256);

            fetchTransformedBilinearPixelBounds<blendType>(image->width, image->x1, image->x2 - 1, x1, x2);
            fetchTransformedBilinearPixelBounds<blendType>(image->height, image->y1, image->y2 - 1, y1, y2);

            const uint32_t  *s1 = (const uint32_t  *)data->texture->scanLine(y1);
            const uint32_t  *s2 = (const uint32_t  *)data->texture->scanLine(y2);

            uint32_t  tl = s1[x1];
            uint32_t  tr = s1[x2];
            uint32_t  bl = s2[x1];
            uint32_t  br = s2[x2];

            *b = interpolate_4_pixels(tl, tr, bl, br, distx, disty);

            fx += fdx;
            fy += fdy;
            fw += fdw;
            //force increment to avoid /0
            if (!fw) {
                fw += fdw;
            }
            ++b;
        }
    }

    return buffer;
}

// FetchUntransformed can have more specialized methods added depending on SIMD features.
static LSourceFetchProc sourceFetchUntransformed[(int)LImage::Format::NImageFormats] = {
    nullptr,                    // Invalid
    fetchUntransformed,         // Mono
    fetchUntransformed,         // MonoLsb
    fetchUntransformed,         // Indexed8
    fetchUntransformedARGB32PM, // RGB32
    fetchUntransformed,         // ARGB32
    fetchUntransformedARGB32PM, // ARGB32_Premultiplied
    fetchUntransformedRGB16,    // RGB16
    fetchUntransformed,         // ARGB8565_Premultiplied
    fetchUntransformed,         // RGB666
    fetchUntransformed,         // ARGB6666_Premultiplied
    fetchUntransformed,         // RGB555
    fetchUntransformed,         // ARGB8555_Premultiplied
    fetchUntransformed,         // RGB888
    fetchUntransformed,         // RGB444
    fetchUntransformed,         // ARGB4444_Premultiplied
    fetchUntransformed,         // RGBX8888
    fetchUntransformed,         // ABGR32
    fetchUntransformed,         // ABGR32_Premultiplied
    fetchUntransformed,         // Format_BGR30
    fetchUntransformed,         // Format_A2BGR30_Premultiplied
    fetchUntransformed,         // Format_RGB30
    fetchUntransformed,         // Format_A2RGB30_Premultiplied
    fetchUntransformed,         // Alpha8
    fetchUntransformed,         // Grayscale8
    fetchUntransformed,         // RGBX64
    fetchUntransformed,         // RGBA64
    fetchUntransformed,         // RGBA64_Premultiplied
    fetchUntransformed,         // Grayscale16
    fetchUntransformed,         // BGR888
};

static const LSourceFetchProc sourceFetchGeneric[NBlendTypes] = {
    fetchUntransformed,                                                             // Untransformed
    fetchUntransformed,                                                             // Tiled
    fetchTransformed<BlendTransformed, draw::BPPNone>,                      // Transformed
    fetchTransformed<BlendTransformedTiled, draw::BPPNone>,                 // TransformedTiled
    fetchTransformedBilinear<BlendTransformedBilinear, draw::BPPNone>,      // TransformedBilinear
    fetchTransformedBilinear<BlendTransformedBilinearTiled, draw::BPPNone>  // TransformedBilinearTiled
};

static LSourceFetchProc sourceFetchARGB32PM[NBlendTypes] = {
    fetchUntransformedARGB32PM,                                     // Untransformed
    fetchUntransformedARGB32PM,                                     // Tiled
    fetchTransformed<BlendTransformed, draw::BPP32>,        // Transformed
    fetchTransformed<BlendTransformedTiled, draw::BPP32>,   // TransformedTiled
    fetchTransformedBilinearARGB32PM<BlendTransformedBilinear>,     // Bilinear
    fetchTransformedBilinearARGB32PM<BlendTransformedBilinearTiled> // BilinearTiled
};

static LSourceFetchProc sourceFetchAny16[NBlendTypes] = {
    fetchUntransformed,                                                             // Untransformed
    fetchUntransformed,                                                             // Tiled
    fetchTransformed<BlendTransformed, draw::BPP16>,                        // Transformed
    fetchTransformed<BlendTransformedTiled, draw::BPP16>,                   // TransformedTiled
    fetchTransformedBilinear<BlendTransformedBilinear, draw::BPP16>,        // TransformedBilinear
    fetchTransformedBilinear<BlendTransformedBilinearTiled, draw::BPP16>    // TransformedBilinearTiled
};

static LSourceFetchProc sourceFetchAny32[NBlendTypes] = {
    fetchUntransformed,                                                             // Untransformed
    fetchUntransformed,                                                             // Tiled
    fetchTransformed<BlendTransformed, draw::BPP32>,                        // Transformed
    fetchTransformed<BlendTransformedTiled, draw::BPP32>,                   // TransformedTiled
    fetchTransformedBilinear<BlendTransformedBilinear, draw::BPP32>,        // TransformedBilinear
    fetchTransformedBilinear<BlendTransformedBilinearTiled, draw::BPP32>    // TransformedBilinearTiled
};

static inline LSourceFetchProc getSourceFetch(TextureBlendType blendType, LImage::Format format)
{
    if (format == LImage::Format::RGB32 || format == LImage::Format::ARGB32Premultiplied){
        return sourceFetchARGB32PM[blendType];
    }
    if (blendType == BlendUntransformed || blendType == BlendTiled){
        return sourceFetchUntransformed[(int)format];
    }
    if (draw::lPixelLayouts[(int)format].bpp == draw::BPP16){
        return sourceFetchAny16[blendType];
    }
    if (draw::lPixelLayouts[(int)format].bpp == draw::BPP32){
        return sourceFetchAny32[blendType];
    }
    return sourceFetchGeneric[blendType];
}

struct LDrawOperator
{
    LDrawOperator(){}

    LCompositionMode mode = LCompositionMode::SourceOver;
    LDestFetchProc destFetch = nullptr;
    LDestStoreProc destStore = nullptr;
    LSourceFetchProc srcFetch = nullptr;
    CompositionFunctionSolid funcSolid = nullptr;
    CompositionFunction func = nullptr;

    union {
        LinearGradientValues linear;
        RadialGradientValues radial;
    };
};



static void getLinearGradientValues(LinearGradientValues *v, const LSpanData *data)
{
    v->dx = data->gradient->m_linear.end.x - data->gradient->m_linear.origin.x;
    v->dy = data->gradient->m_linear.end.y - data->gradient->m_linear.origin.y;
    v->l = v->dx * v->dx + v->dy * v->dy;
    v->off = 0;
    if (v->l != 0) {
        v->dx /= v->l;
        v->dy /= v->l;
        v->off = -v->dx * data->gradient->m_linear.origin.x - v->dy * data->gradient->m_linear.origin.y;
    }
}

static void getRadialGradientValues(RadialGradientValues *v, const LSpanData *data)
{
    v->dx = data->gradient->m_radial.center.x - data->gradient->m_radial.focal.x;
    v->dy = data->gradient->m_radial.center.y - data->gradient->m_radial.focal.y;

    v->dr = data->gradient->m_radial.center.radius - data->gradient->m_radial.focal.radius;
    v->sqrfr = data->gradient->m_radial.focal.radius * data->gradient->m_radial.focal.radius;

    v->a = v->dr * v->dr - v->dx*v->dx - v->dy*v->dy;

    v->extended = !lFuzzyIsZero(data->gradient->m_radial.focal.radius) || v->a <= 0;
}


static inline uint32_t lkGradientClamp(std::shared_ptr<LGradient> data, int ipos)
{
    if (ipos < 0 || ipos >= GRADIENT_STOPTABLE_SIZE) {
        if (data->m_spread == LGradient::Spread::Repeat) {
            ipos = ipos % GRADIENT_STOPTABLE_SIZE;
            ipos = ipos < 0 ? GRADIENT_STOPTABLE_SIZE + ipos : ipos;
        } else if (data->m_spread == LGradient::Spread::Reflect) {
            const int limit = GRADIENT_STOPTABLE_SIZE * 2;
            ipos = ipos % limit;
            ipos = ipos < 0 ? limit + ipos : ipos;
            ipos = ipos >= GRADIENT_STOPTABLE_SIZE ? limit - 1 - ipos : ipos;
        } else {
            if (ipos < 0){
                ipos = 0;
            }
            else if (ipos >= GRADIENT_STOPTABLE_SIZE){
                ipos = GRADIENT_STOPTABLE_SIZE-1;
            }
        }
    }
    return ipos;
}


#define FIXPT_BITS 8
#define FIXPT_SIZE (1<<FIXPT_BITS)
#define FIXPT_MAX (INT_MAX >> (FIXPT_BITS + 1))


class GradientBase
{
public:
    static uint32_t null() { return 0; }
    static uint32_t fetchSingle(std::shared_ptr<LGradient> gradient, double pos)
    {
        int ipos = int(pos * (GRADIENT_STOPTABLE_SIZE - 1) + double(0.5));
        return gradient->m_colorTable[lkGradientClamp(gradient, ipos)];
    }
    static uint32_t fetchSingle(std::shared_ptr<LGradient> gradient, int pos)
    {
        int ipos = (pos + (FIXPT_SIZE / 2)) >> FIXPT_BITS;
        return gradient->m_colorTable[lkGradientClamp(gradient, ipos)];
    }
    static void memfill(uint32_t *buffer, uint32_t fill, int length)
    {
        lMemFillTemplate<uint32_t>(buffer, fill, length);
    }
};


static const uint32_t*  lkFetchLinearGradient(uint32_t *buffer,
                                             const LDrawOperator *op,
                                             const LSpanData *data,
                                             int y, int x, int length)
{
    const uint32_t *b = buffer;
    double t, inc;

    bool affine = true;
    double rx=0, ry=0;
    if (op->linear.l == 0) {
        t = inc = 0;
    } else {
        rx = data->m21 * (y + double(0.5)) + data->m11 * (x + double(0.5)) + data->dx;
        ry = data->m22 * (y + double(0.5)) + data->m12 * (x + double(0.5)) + data->dy;
        t = op->linear.dx*rx + op->linear.dy*ry + op->linear.off;
        inc = op->linear.dx * data->m11 + op->linear.dy * data->m12;
        affine = !data->m13 && !data->m23;

        if (affine) {
            t *= (GRADIENT_STOPTABLE_SIZE - 1);
            inc *= (GRADIENT_STOPTABLE_SIZE - 1);
        }
    }

    const uint32_t *end = buffer + length;
    if (affine) {
        if (inc > double(-1e-5) && inc < double(1e-5)) {
            if (std::abs(t) < FIXPT_MAX)
                GradientBase::memfill(buffer, GradientBase::fetchSingle(data->gradient, int(t * FIXPT_SIZE)), length);
            else
                GradientBase::memfill(buffer, GradientBase::fetchSingle(data->gradient, t / GRADIENT_STOPTABLE_SIZE), length);
        } else {
            if (std::abs(t) < FIXPT_MAX && std::abs(inc) < FIXPT_MAX && std::abs(t + inc * length) < FIXPT_MAX) {
                // we can use fixed point math
                int t_fixed = int(t * FIXPT_SIZE);
                int inc_fixed = int(inc * FIXPT_SIZE);
                while (buffer < end) {
                    *buffer = GradientBase::fetchSingle(data->gradient, t_fixed);
                    t_fixed += inc_fixed;
                    ++buffer;
                }
            } else {
                // we have to fall back to float math
                while (buffer < end) {
                    *buffer = GradientBase::fetchSingle(data->gradient, t/GRADIENT_STOPTABLE_SIZE);
                    t += inc;
                    ++buffer;
                }
            }
        }
    } else { // fall back to float math here as well
        double rw = data->m23 * (y + double(0.5)) + data->m13 * (x + double(0.5)) + data->m33;
        while (buffer < end) {
            double x = rx/rw;
            double y = ry/rw;
            t = (op->linear.dx*x + op->linear.dy *y) + op->linear.off;

            *buffer = GradientBase::fetchSingle(data->gradient, t);
            rx += data->m11;
            ry += data->m12;
            rw += data->m13;
            if (!rw) {
                rw += data->m13;
            }
            ++buffer;
        }
    }

    return b;
}


class RadialFetchFunc : public GradientBase
{
public:
    static void fetch(uint32_t *buffer, uint32_t *end,
                      const LDrawOperator *op, const LSpanData *data, double det,
                      double deltaDet, double deltaDeltaDet, double b, double delta_b)
    {
        if (op->radial.extended) {
            while (buffer < end) {
                uint32_t result = GradientBase::null();
                if (det >= 0) {
                    double w = std::sqrt(det) - b;
                    if (data->gradient->m_radial.focal.radius + op->radial.dr * w >= 0){
                        result = GradientBase::fetchSingle(data->gradient, w);
                    }
                }

                *buffer = result;

                det += deltaDet;
                deltaDet += deltaDeltaDet;
                b += delta_b;

                ++buffer;
            }
        } else {
            while (buffer < end) {
                uint32_t result = GradientBase::null();
                if (det >= 0) {
                    double w = std::sqrt(det) - b;
                    result = GradientBase::fetchSingle(data->gradient, w);
                }

                *buffer++ = result;

                det += deltaDet;
                deltaDet += deltaDeltaDet;
                b += delta_b;
            }
        }
    }
};

static inline double lRadialDeterminant(double a, double b, double c)
{
    return (b * b) - (4 * a * c);
}

const uint32_t* lkFetchRadialGradient(uint32_t *buffer,
                                   const LDrawOperator *op,
                                   const LSpanData *data,
                                   int y, int x, int length)
{
    if (lFuzzyIsZero(op->radial.a)) {
        RadialFetchFunc::memfill(buffer, RadialFetchFunc::null(), length);
        return buffer;
    }

    const uint32_t *b = buffer;
    double rx = data->m21 * (y + double(0.5))
               + data->dx + data->m11 * (x + double(0.5));
    double ry = data->m22 * (y + double(0.5))
               + data->dy + data->m12 * (x + double(0.5));
    bool affine = !data->m13 && !data->m23;

    uint32_t *end = buffer + length;
    double invA = 1 / double(2 * op->radial.a);

    if (affine) {
        rx -= data->gradient->m_radial.focal.x;
        ry -= data->gradient->m_radial.focal.y;

        const double delta_rx = data->m11;
        const double delta_ry = data->m12;

        double b = 2*(op->radial.dr*data->gradient->m_radial.focal.radius + rx * op->radial.dx + ry * op->radial.dy);
        double deltaB = 2*(delta_rx * op->radial.dx + delta_ry * op->radial.dy);
        const double bDeltaB = 2 * b * deltaB;
        const double deltaBDeltaB = 2 * deltaB * deltaB;

        const double bb = b * b;
        const double deltaBb = deltaB * deltaB;

        b *= invA;
        deltaB *= invA;

        const double rxrxryry = rx * rx + ry * ry;
        const double deltaRxrxryry = delta_rx * delta_rx + delta_ry * delta_ry;
        const double rxPlusRy = 2*(rx * delta_rx + ry * delta_ry);
        const double deltaRxPlusRy = 2 * deltaRxrxryry;

        invA *= invA;

        double det = (bb - 4 * op->radial.a * (op->radial.sqrfr - rxrxryry)) * invA;
        double deltaDet = (bDeltaB + deltaBb + 4 * op->radial.a * (rxPlusRy + deltaRxrxryry)) * invA;
        const double deltaDeltaDet = (deltaBDeltaB + 4 * op->radial.a * deltaRxPlusRy) * invA;

        if (std::isfinite(float(det)) && std::isfinite(float(deltaDet))
            && std::isfinite(float(deltaDeltaDet)))
            RadialFetchFunc::fetch(buffer, end, op, data, det, deltaDet, deltaDeltaDet, b, deltaB);
        else
            RadialFetchFunc::memfill(buffer, RadialFetchFunc::null(), length);
    } else {
        double rw = data->m23 * (y + double(0.5))
        + data->m33 + data->m13 * (x + double(0.5));

        while (buffer < end) {
            if (rw == 0) {
                *buffer = RadialFetchFunc::null();
            } else {
                double invRw = 1 / rw;
                double gx = rx * invRw - data->gradient->m_radial.focal.x;
                double gy = ry * invRw - data->gradient->m_radial.focal.y;
                double b  = 2*(op->radial.dr*data->gradient->m_radial.focal.radius + gx*op->radial.dx + gy*op->radial.dy);
                double det = lRadialDeterminant(op->radial.a, b, op->radial.sqrfr - (gx*gx + gy*gy));

                uint32_t result = RadialFetchFunc::null();
                if (det >= 0) {
                    double detSqrt = std::sqrt(det);

                    double s0 = (-b - detSqrt) * invA;
                    double s1 = (-b + detSqrt) * invA;

                    double s = std::max<double>(s0, s1);

                    if (data->gradient->m_radial.focal.radius + op->radial.dr * s >= 0)
                        result = RadialFetchFunc::fetchSingle(data->gradient, s);
                }

                *buffer = result;
            }

            rx += data->m11;
            ry += data->m12;
            rw += data->m13;

            ++buffer;
        }
    }

    return b;
}


static const uint32_t* lkFetchConicalGradient(uint32_t *buffer,
                                              const LDrawOperator *,
                                              const LSpanData *data,
                                              int y, int x, int length)
{
    const uint32_t *b = buffer;
    double rx = data->m21 * (y + double(0.5))
               + data->dx + data->m11 * (x + double(0.5));
    double ry = data->m22 * (y + double(0.5))
               + data->dy + data->m12 * (x + double(0.5));
    bool affine = !data->m13 && !data->m23;

    const double inv2pi = 0.5*std::numbers::inv_pi;

    const uint32_t *end = buffer + length;
    if (affine) {
        rx -= data->gradient->m_conical.center.x;
        ry -= data->gradient->m_conical.center.y;
        while (buffer < end) {
            double angle = std::atan2(ry, rx) + data->gradient->m_conical.angle;

            *buffer = GradientBase::fetchSingle(data->gradient, 1 - angle * inv2pi);

            rx += data->m11;
            ry += data->m12;
            ++buffer;
        }
    } else {
        double rw = data->m23 * (y + double(0.5))
        + data->m33 + data->m13 * (x + double(0.5));
        if (!rw)
            rw = 1;
        while (buffer < end) {
            double angle = std::atan2(ry/rw - data->gradient->m_conical.center.x,
                                 rx/rw - data->gradient->m_conical.center.y)
                          + data->gradient->m_conical.angle;

            *buffer = GradientBase::fetchSingle(data->gradient, 1 - angle * inv2pi);

            rx += data->m11;
            ry += data->m12;
            rw += data->m13;
            if (!rw) {
                rw += data->m13;
            }
            ++buffer;
        }
    }
    return b;
}


static inline LDrawOperator getDrawOperator(const LSpanData *data, const LSpan *spans, int spanCount)
{
    LDrawOperator op;
    bool solidSource = false;

    switch(data->type) {
    case LSpanData::Solid:{
        solidSource = LColor::isOpaque(data->solidColor);
        op.srcFetch = nullptr;
        break;
    }
    case LSpanData::Gradient:{
        if(data->gradient != nullptr){
            solidSource = !data->gradient->m_hasAlpha;

            switch(data->gradient->m_type){
            case LGradient::Type::Linear:{
                getLinearGradientValues(&op.linear, data);
                op.srcFetch = lkFetchLinearGradient;
                break;
            }
            case LGradient::Type::Radial:{
                solidSource = !data->gradient->m_hasAlpha;
                getRadialGradientValues(&op.radial, data);
                op.srcFetch = lkFetchRadialGradient;
                break;
            }
            case LGradient::Type::Conical:{
                op.srcFetch = lkFetchConicalGradient;
                break;
            }
            default:{
                break;
            }
            }
        }
        break;
    }
    case LSpanData::Texture:
        solidSource = !data->texture->hasAlpha;
        op.srcFetch = getSourceFetch(getBlendType(data), data->texture->format);
        break;
    default:
        break;
    }

    op.mode = data->rasterBuffer->compositionMode;
    if (op.mode == LCompositionMode::SourceOver && solidSource){
        op.mode = LCompositionMode::Source;
    }

    op.destFetch = destFetchProc[(int)data->rasterBuffer->format];
    if (op.mode == LCompositionMode::Source &&
        (data->type != LSpanData::Texture || data->texture->constAlpha == 256)) {
        const LSpan *lastSpan = spans + spanCount;
        bool alphaSpans = false;
        while (spans < lastSpan) {
            if (spans->coverage != 255) {
                alphaSpans = true;
                break;
            }
            ++spans;
        }
        if (!alphaSpans && spanCount > 0) {
            // If all spans are opaque we do not need to fetch dest.
            // But don't clear passthrough destFetch as they are just as fast and save destStore.
            if (op.destFetch != destFetchARGB32P){
                op.destFetch = destFetchUndefined;
            }
        }
    }

    op.destStore = destStoreProc[(int)data->rasterBuffer->format];
    op.funcSolid = lkFunctionForModeSolidC[(int)op.mode];
    op.func = lkFunctionForModeC[(int)op.mode];
    return op;
}


static void spanfillFromFirst(LRasterBuffer *rasterBuffer,
                              draw::BPP bpp, int x, int y, int length)
{
    switch (bpp) {
    case draw::BPP64: {
        uint64_t *dest = reinterpret_cast<uint64_t *>(rasterBuffer->scanLine(y)) + x;
        lMemFillTemplate(dest + 1, dest[0], length - 1);
        break;
    }
    case draw::BPP32: {
        uint32_t *dest = reinterpret_cast<uint32_t *>(rasterBuffer->scanLine(y)) + x;
        lMemFillTemplate(dest + 1, dest[0], length - 1);
        break;
    }
    case draw::BPP24: {
        uint24_t *dest = reinterpret_cast<uint24_t *>(rasterBuffer->scanLine(y)) + x;
        lMemFillTemplate(dest + 1, dest[0], length - 1);
        break;
    }
    case draw::BPP16: {
        uint16_t *dest = reinterpret_cast<uint16_t *>(rasterBuffer->scanLine(y)) + x;
        lMemFillTemplate(dest + 1, dest[0], length - 1);
        break;
    }
    case draw::BPP8: {
        uint8_t *dest = rasterBuffer->scanLine(y) + x;
        memset(dest + 1, dest[0], length - 1);
        break;
    }
    default:{
        break;
    }
    }
}

// -------------------- blend methods ---------------------
static void blendColorGeneric(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);
    LDrawOperator op = getDrawOperator(data, nullptr, 0);
    if(op.destStore != nullptr && op.funcSolid != nullptr){
        uint32_t buffer[draw::BufferSize];
        const uint32_t color = data->solidColor;
        const bool solidFill = op.mode == LCompositionMode::Source;
        const draw::BPP bpp = draw::lPixelLayouts[(int)data->rasterBuffer->format].bpp;

        while (count--) {
            int x = spans->x;
            int length = spans->len;
            if (solidFill && bpp >= draw::BPP8 && spans->coverage == 255 && length) {
                // If dest doesn't matter we don't need to bother with blending or converting all the identical pixels
                op.destStore(data->rasterBuffer, x, spans->y, &color, 1);
                spanfillFromFirst(data->rasterBuffer, bpp, x, spans->y, length);
                length = 0;
            }

            while (length) {
                int l = std::min<int>(draw::BufferSize, length);
                uint32_t *dest = op.destFetch(buffer, data->rasterBuffer, x, spans->y, l);
                op.funcSolid(dest, l, color, spans->coverage);
                op.destStore(data->rasterBuffer, x, spans->y, dest, l);
                length -= l;
                x += l;
            }
            ++spans;
        }
    }
}


static void blendColorArgb(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);

    const LDrawOperator op = getDrawOperator(data, nullptr, 0);
    const uint32_t color = data->solidColor;

    if (op.mode == LCompositionMode::Source) {
        // inline for performance
        while (count--) {
            uint32_t *target = ((uint32_t *)data->rasterBuffer->scanLine(spans->y)) + spans->x;
            if (spans->coverage == 255) {
                lMemFillTemplate(target, color, spans->len);
            } else {
                uint32_t c = draw::lByteMul(color, spans->coverage);
                int ialpha = 255 - spans->coverage;
                for (int i = 0; i < spans->len; ++i){
                    target[i] = c + draw::lByteMul(target[i], ialpha);
                }
            }
            ++spans;
        }
        return;
    }

    while (count--) {
        uint32_t *target = ((uint32_t *)data->rasterBuffer->scanLine(spans->y)) + spans->x;
        op.funcSolid(target, spans->len, color, spans->coverage);
        ++spans;
    }
}


static void blendColorRgb16(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);

    LCompositionMode mode = data->rasterBuffer->compositionMode;
    if (mode == LCompositionMode::SourceOver && LColor::isOpaque(data->solidColor)){
        mode = LCompositionMode::Source;
    }

    if (mode == LCompositionMode::Source) {
        // inline for performance
        uint16_t c = LColor::argbToRgb16(data->solidColor);
        for (; count--; spans++) {
            if (!spans->len){
                continue;
            }
            uint16_t *target = ((uint16_t *)data->rasterBuffer->scanLine(spans->y)) + spans->x;
            if (spans->coverage == 255) {
                lMemFillTemplate(target, c, spans->len);
            } else {
                uint16_t color = draw::lByteMulRgb16(c, spans->coverage);
                int ialpha = 255 - spans->coverage;
                const uint16_t *end = target + spans->len;
                while (target < end) {
                    *target = color + draw::lByteMulRgb16(*target, ialpha);
                    ++target;
                }
            }
        }
        return;
    }

    if (mode == LCompositionMode::SourceOver) {
        for (; count--; spans++) {
            if (!spans->len){
                continue;
            }
            uint32_t color = draw::lByteMul(data->solidColor, spans->coverage);
            int ialpha = LColor::uintToAlpha(~color);
            uint16_t c = LColor::argbToRgb16(color);
            uint16_t *target = ((uint16_t *)data->rasterBuffer->scanLine(spans->y)) + spans->x;
            int len = spans->len;
            bool pre = (((luintptr)target) & 0x3) != 0;
            bool post = false;
            if (pre) {
                // skip to word boundary
                *target = c + draw::lByteMulRgb16(*target, ialpha);
                ++target;
                --len;
            }
            if (len & 0x1) {
                post = true;
                --len;
            }
            uint32_t *target32 = (uint32_t*)target;
            uint32_t c32 = c | (c<<16);
            len >>= 1;
            uint32_t salpha = (ialpha+1) >> 3; // calculate here rather than in loop
            while (len--) {
                // blend full words
                *target32 = c32 + draw::lByteMulRgb16_32(*target32, salpha);
                ++target32;
                target += 2;
            }
            if (post) {
                // one last pixel beyond a full word
                *target = c + draw::lByteMulRgb16(*target, ialpha);
            }
        }
        return;
    }

    blendColorGeneric(count, spans, userData);
}


template <class DST> static
inline void lBitmapblitTemplate(LRasterBuffer *rasterBuffer,
                       int x, int y, DST color,
                       const uint8_t *map,
                       int mapWidth, int mapHeight, int mapStride)
{
    DST *dest = reinterpret_cast<DST *>(rasterBuffer->scanLine(y)) + x;
    const int destStride = rasterBuffer->stride<DST>();

    if (mapWidth > 8) {
        while (mapHeight--) {
            int x0 = 0;
            int n = 0;
            for (int x = 0; x < mapWidth; x += 8) {
                uint8_t s = map[x >> 3];
                for (int i = 0; i < 8; ++i) {
                    if (s & 0x80) {
                        ++n;
                    } else {
                        if (n) {
                            lMemFillTemplate(dest + x0, color, n);
                            x0 += n + 1;
                            n = 0;
                        } else {
                            ++x0;
                        }
                        if (!s) {
                            x0 += 8 - 1 - i;
                            break;
                        }
                    }
                    s <<= 1;
                }
            }
            if (n){
                lMemFillTemplate(dest + x0, color, n);
            }
            dest += destStride;
            map += mapStride;
        }
    } else {
        while (mapHeight--) {
            int x0 = 0;
            int n = 0;
            for (uint8_t s = *map; s; s <<= 1) {
                if (s & 0x80) {
                    ++n;
                } else if (n) {
                    lMemFillTemplate(dest + x0, color, n);
                    x0 += n + 1;
                    n = 0;
                } else {
                    ++x0;
                }
            }
            if (n){
                lMemFillTemplate(dest + x0, color, n);
            }
            dest += destStride;
            map += mapStride;
        }
    }
}


inline static void lBitmapblitArgb32(LRasterBuffer *rasterBuffer,
                                        int x, int y, const uint32_t& color,
                                        const uint8_t *map,
                                        int mapWidth, int mapHeight, int mapStride)
{
    lBitmapblitTemplate<uint32_t>(rasterBuffer, x,  y, color,
                                    map, mapWidth, mapHeight, mapStride);
}

inline static void lBitmapblitRgb16(LRasterBuffer *rasterBuffer,
                                         int x, int y, const uint32_t &color,
                                         const uint8_t *map,
                                         int mapWidth, int mapHeight, int mapStride)
{
    lBitmapblitTemplate<uint16_t>(rasterBuffer, x,  y, LColor::argbToRgb16(color),
                                    map, mapWidth, mapHeight, mapStride);
}

inline static void lBitmapblitABGR32(LRasterBuffer *rasterBuffer,
                                          int x, int y, const uint32_t &color,
                                          const uint8_t *map,
                                          int mapWidth, int mapHeight, int mapStride)
{
    lBitmapblitTemplate<uint32_t>(rasterBuffer, x, y, LColor::argbToAbgr(color),
                                    map, mapWidth, mapHeight, mapStride);
}


template<draw::PixelOrder PixelOrder>
inline static void lBitmapblitRgb30(LRasterBuffer *rasterBuffer,
                                       int x, int y, const uint32_t &color,
                                       const uint8_t *map,
                                       int mapWidth, int mapHeight, int mapStride)
{
    lBitmapblitTemplate<uint32_t>(rasterBuffer, x, y, draw::lConvertRgb32ToRgb30<PixelOrder>(color),
                                    map, mapWidth, mapHeight, mapStride);
}


static inline void blendPixel(uint32_t &dst, const uint32_t src)
{
    if (src >= 0xff000000){
        dst = src;
    }
    else if (src != 0){
        dst = src + draw::lByteMul(dst, LColor::uintToAlpha(~src));
    }
}

static inline void blendPixel(uint32_t &dst, const uint32_t src, const int const_alpha)
{
    if (const_alpha == 255){
        return blendPixel(dst, src);
    }
    if (src != 0) {
        const uint32_t s = draw::lByteMul(src, const_alpha);
        dst = s + draw::lByteMul(dst, LColor::uintToAlpha(~s));
    }
}

static inline void grayBlendPixel(uint32_t *dst, int coverage, uint64_t srcLinear, const std::shared_ptr<LColorTrcLut> colorProfile)
{
    // Do a gammacorrected gray alphablend...
    if(colorProfile != nullptr){
        const uint32_t dstLinear = colorProfile->toLinear(*dst);
        uint32_t blend = draw::lByteMul(srcLinear, coverage)+draw::lByteMul(dstLinear, 255 - coverage);
        *dst =colorProfile->fromLinear(blend) ;
    }else{
        uint32_t blend = draw::lByteMul(srcLinear, coverage)+draw::lByteMul(*dst, 255 - coverage);
        *dst =colorProfile->fromLinear(blend) ;
    }
}

static inline void alphamapblendArgb32(uint32_t *dst, int coverage,
                                       uint32_t srcLinear, uint32_t src,
                                       const std::shared_ptr<LColorTrcLut> colorProfile)
{
    if (coverage == 0) {
        // nothing
    } else if (coverage == 255 || (colorProfile == nullptr)) {
        blendPixel(*dst, src, coverage);
    } else if (*dst < 0xff000000) {
        // Give up and do a naive gray alphablend. Needed to deal with ARGB32 and invalid ARGB32_premultiplied, see QTBUG-60571
        blendPixel(*dst, src, coverage);
    } else if (src >= 0xff000000) {
        grayBlendPixel(dst, coverage, srcLinear, colorProfile);
    } else {
        // First do naive blend with text-color
        uint32_t s = *dst;
        blendPixel(s, src);
        // Then gamma-corrected blend with glyph shape
        if(colorProfile){
            s = colorProfile->toLinear(s);
        }
        grayBlendPixel(dst, coverage, s, colorProfile);
    }
}


static void lAlphamapblitArgb32(LRasterBuffer *rasterBuffer,
                                   int x, int y, const uint32_t &color,
                                   const uint8_t *map,
                                   int mapWidth, int mapHeight, int mapStride,
                                   const LSpanClipData *clip, bool useGammaCorrection)
{
    const uint32_t c = color;
    const int destStride = rasterBuffer->stride<uint32_t>();

    if (LColor::isOpaque(color)){
        return;
    }

    std::shared_ptr<LColorTrcLut> colorProfile = nullptr;
    if (useGammaCorrection){
        colorProfile = LGuiApplication::guiInst()->colorProfileForA8Text();
    }

    uint32_t srcColor = color;
    if ((colorProfile != nullptr) && LColor::isOpaque(color)){
        srcColor = colorProfile->toLinear(srcColor);
    }

    if (!clip) {
        uint32_t *dest = reinterpret_cast<uint32_t*>(rasterBuffer->scanLine(y)) + x;
        while (mapHeight--) {
            for (int i = 0; i < mapWidth; ++i) {
                const int coverage = map[i];
                alphamapblendArgb32(dest + i, coverage, srcColor, c, colorProfile);
            }
            dest += destStride;
            map += mapStride;
        }
    } else {
        int bottom = std::min<int>(y + mapHeight, rasterBuffer->height());

        int top = std::max<int>(y, 0);
        map += (top - y) * mapStride;

        const_cast<LSpanClipData *>(clip)->initialize();
        for (int yp = top; yp<bottom; ++yp) {
            const LSpanClipData::ClipLine &line = clip->m_clipLines[yp];

            uint32_t *dest = reinterpret_cast<uint32_t *>(rasterBuffer->scanLine(yp));

            for (int i=0; i<line.count; ++i) {
                const LSpan &clip = line.spans[i];

                int start = std::max<int>(x, clip.x);
                int end = std::min<int>(x + mapWidth, clip.x + clip.len);

                for (int xp=start; xp<end; ++xp) {
                    const int coverage = map[xp - x];
                    alphamapblendArgb32(dest + xp, coverage, srcColor, c, colorProfile);
                } // for (i -> line.count)
            } // for (yp -> bottom)
            map += mapStride;
        }
    }
}


static inline void alphamapblendArgb16(int coverage, uint16_t *dest, int x, const uint16_t srcColor)
{
    if (coverage == 0) {
        // nothing
    } else if (coverage == 255) {
        dest[x] = srcColor;
    } else {
        dest[x] = draw::lByteMulRgb16(srcColor, coverage) + draw::lByteMulRgb16(dest[x], 255 - coverage);
    }
}

static void lAlphamapblitGeneric(LRasterBuffer *rasterBuffer,
                                    int x, int y, const uint32_t &color,
                                    const uint8_t *map,
                                    int mapWidth, int mapHeight, int mapStride,
                                    const LSpanClipData *clip, bool useGammaCorrection)
{
    if (LColor::isTransparent(color)){
        return;
    }

    const uint32_t c = color;
    std::shared_ptr<LColorTrcLut> colorProfile = nullptr;

    if (useGammaCorrection){
        colorProfile = LGuiApplication::guiInst()->colorProfileForA8Text();
    }

    uint32_t srcColor = color;
    if (colorProfile != nullptr && LColor::isOpaque(color)){
        srcColor = colorProfile->toLinear(srcColor);
    }

    uint32_t buffer[draw::BufferSize];
    const LDestFetchProc destFetch = destFetchProc[(int)rasterBuffer->format];
    const LDestStoreProc destStore = destStoreProc[(int)rasterBuffer->format];

    if (!clip) {
        for (int ly = 0; ly < mapHeight; ++ly) {
            int i = x;
            int length = mapWidth;
            while (length > 0) {
                int l = std::min<int>(draw::BufferSize, length);
                uint32_t *dest = destFetch(buffer, rasterBuffer, i, y + ly, l);
                for (int j=0; j < l; ++j) {
                    const int coverage = map[j + (i - x)];
                    alphamapblendArgb32(dest + j, coverage, srcColor, c, colorProfile);
                }
                if (destStore)
                    destStore(rasterBuffer, i, y + ly, dest, l);
                length -= l;
                i += l;
            }
            map += mapStride;
        }
    } else {
        int bottom = std::min<int>(y + mapHeight, rasterBuffer->height());

        int top = std::max<int>(y, 0);
        map += (top - y) * mapStride;
        const_cast<LSpanClipData *>(clip)->initialize();
        for (int yp = top; yp<bottom; ++yp) {
            const LSpanClipData::ClipLine &line = clip->m_clipLines[yp];

            for (int i=0; i<line.count; ++i) {
                const LSpan &clip = line.spans[i];

                int start = std::max<int>(x, clip.x);
                int end = std::min<int>(x + mapWidth, clip.x + clip.len);
                if (end <= start){
                    continue;
                }

                uint32_t *dest = destFetch(buffer, rasterBuffer, start, clip.y, end - start);

                for (int xp=start; xp<end; ++xp) {
                    const int coverage = map[xp - x];
                    alphamapblendArgb32(dest + xp - x, coverage, srcColor, color, colorProfile);
                }
                if (destStore){
                    destStore(rasterBuffer, start, clip.y, dest, end - start);
                }
            } // for (i -> line.count)
            map += mapStride;
        } // for (yp -> bottom)
    }
}

void lAlphamapblitArgb16(LRasterBuffer *rasterBuffer,
                             int x, int y, const uint32_t &color,
                             const uint8_t *map,
                             int mapWidth, int mapHeight, int mapStride,
                             const LSpanClipData *clip, bool useGammaCorrection)
{
    if (useGammaCorrection || !LColor::isOpaque(color)) {
        lAlphamapblitGeneric(rasterBuffer, x, y, color, map, mapWidth, mapHeight, mapStride, clip, useGammaCorrection);
        return;
    }

    const uint16_t c = LColor::argbToRgb16(color);

    if (!clip) {
        uint16_t *dest = reinterpret_cast<uint16_t*>(rasterBuffer->scanLine(y)) + x;
        const int destStride = rasterBuffer->stride<uint16_t>();
        while (mapHeight--) {
            for (int i = 0; i < mapWidth; ++i){
                alphamapblendArgb16(map[i], dest, i, c);
            }
            dest += destStride;
            map += mapStride;
        }
    } else {
        int top = std::max<int>(y, 0);
        int bottom = std::min<int>(y + mapHeight, rasterBuffer->height());
        map += (top - y) * mapStride;

        const_cast<LSpanClipData *>(clip)->initialize();
        for (int yp = top; yp<bottom; ++yp) {
            const LSpanClipData::ClipLine &line = clip->m_clipLines[yp];

            uint16_t *dest = reinterpret_cast<uint16_t*>(rasterBuffer->scanLine(yp));

            for (int i=0; i<line.count; ++i) {
                const LSpan &clip = line.spans[i];

                int start = std::max<int>(x, clip.x);
                int end = std::min<int>(x + mapWidth, clip.x + clip.len);

                for (int xp=start; xp<end; ++xp){
                    alphamapblendArgb16(map[xp - x], dest, xp, c);
                }
            } // for (i -> line.count)
            map += mapStride;
        } // for (yp -> bottom)
    }
}


static inline uint32_t rgbBlend(uint32_t d, uint32_t s, uint32_t rgbAlpha)
{
    const int dr = LColor::uintToRed(d);
    const int dg = LColor::uintToGreen(d);
    const int db = LColor::uintToBlue(d);

    const int sr = LColor::uintToRed(s);
    const int sg = LColor::uintToGreen(s);
    const int sb = LColor::uintToBlue(s);

    const int mr = LColor::uintToRed(rgbAlpha);
    const int mg = LColor::uintToGreen(rgbAlpha);
    const int mb = LColor::uintToBlue(rgbAlpha);

    const int nr = LColor::div255(sr * mr + dr * (255 - mr));
    const int ng = LColor::div255(sg * mg + dg * (255 - mg));
    const int nb = LColor::div255(sb * mb + db * (255 - mb));

    return 0xff000000 | (nr << 16) | (ng << 8) | nb;
}

static inline void rgbBlendPixel(uint32_t *dst, int coverage, uint32_t slinear, const std::shared_ptr<LColorTrcLut> colorProfile)
{
    // Do a gammacorrected RGB alphablend...
    if(colorProfile != nullptr){
        const uint32_t dlinear = colorProfile->toLinear(*dst);
        uint32_t blend = rgbBlend(dlinear, slinear, coverage);
        *dst = colorProfile->fromLinear(blend) ;
    }else{
        *dst = rgbBlend(*dst, slinear, coverage);
    }
}

static inline void alphargbblendArgb32(uint32_t *dst, uint32_t coverage,
                                       const uint32_t &srcLinear, uint32_t src,
                                       const std::shared_ptr<LColorTrcLut> colorProfile)
{
    if (coverage == 0xff000000) {
        // nothing
    } else if (coverage == 0xffffffff && LColor::uintToAlpha(src) == 255) {
        blendPixel(*dst, src);
    } else if (*dst < 0xff000000) {
        // Give up and do a naive gray alphablend. Needed to deal with ARGB32 and invalid ARGB32_premultiplied, see QTBUG-60571
        blendPixel(*dst, src, LColor::rgbAvg(coverage));
    } else if (colorProfile == nullptr) {
        // First do naive blend with text-color
        uint32_t s = *dst;
        blendPixel(s, src);
        // Then a naive blend with glyph shape
        *dst = rgbBlend(*dst, s, coverage);
    } else if (LColor::isOpaque(srcLinear)) {
        rgbBlendPixel(dst, coverage, srcLinear, colorProfile);
    } else {
        // First do naive blend with text-color
        uint32_t s = *dst;
        blendPixel(s, src);
        // Then gamma-corrected blend with glyph shape
        if(colorProfile){
            s = colorProfile->toLinear(s);
        }
        rgbBlendPixel(dst, coverage, s, colorProfile);
    }
}

static void lAlphargbblitArgb32(LRasterBuffer *rasterBuffer,
                                   int x, int y, const uint32_t &color,
                                   const uint32_t *src, int mapWidth, int mapHeight, int srcStride,
                                   const LSpanClipData *clip, bool useGammaCorrection)
{
    if (LColor::isTransparent(color)){
        return;
    }

    const uint32_t c = color;
    std::shared_ptr<LColorTrcLut> colorProfile = nullptr;

    if (useGammaCorrection){
        colorProfile = LGuiApplication::guiInst()->colorProfileForA32Text();
    }

    uint32_t srcColor = color;
    if (colorProfile && LColor::isOpaque(color)){
        srcColor = colorProfile->toLinear(srcColor);
    }

    if (!clip) {
        uint32_t *dst = reinterpret_cast<uint32_t*>(rasterBuffer->scanLine(y)) + x;
        const int destStride = rasterBuffer->stride<uint32_t>();
        while (mapHeight--) {
            for (int i = 0; i < mapWidth; ++i) {
                const uint32_t coverage = src[i];
                alphargbblendArgb32(dst + i, coverage, srcColor, c, colorProfile);
            }

            dst += destStride;
            src += srcStride;
        }
    } else {
        int bottom = std::min<int>(y + mapHeight, rasterBuffer->height());
        int top = std::max<int>(y, 0);
        src += (top - y) * srcStride;

        const_cast<LSpanClipData *>(clip)->initialize();
        for (int yp = top; yp<bottom; ++yp) {
            const LSpanClipData::ClipLine &line = clip->m_clipLines[yp];

            uint32_t *dst = reinterpret_cast<uint32_t *>(rasterBuffer->scanLine(yp));
            for (int i=0; i<line.count; ++i) {
                const LSpan &clip = line.spans[i];

                int start = std::max<int>(x, clip.x);
                int end = std::min<int>(x + mapWidth, clip.x + clip.len);

                for (int xp=start; xp<end; ++xp) {
                    const uint32_t coverage = src[xp - x];
                    alphargbblendArgb32(dst + xp, coverage, srcColor, c, colorProfile);
                }
            } // for (i -> line.count)
            src += srcStride;
        } // for (yp -> bottom)
    }
}


static void lAlphargbblitGeneric(LRasterBuffer *rasterBuffer,
                                    int x, int y, const uint32_t &color,
                                    const uint32_t *src, int mapWidth, int mapHeight, int srcStride,
                                    const LSpanClipData *clip, bool useGammaCorrection)
{
    if (LColor::isTransparent(color)){
        return;
    }

    const uint32_t c = color;

    std::shared_ptr<LColorTrcLut> colorProfile = nullptr;
    if (useGammaCorrection){
        colorProfile = LGuiApplication::guiInst()->colorProfileForA32Text();
    }

    uint32_t srcColor = color;
    if (colorProfile && LColor::isOpaque(color)){
        srcColor = colorProfile->toLinear(srcColor);
    }

    uint32_t buffer[draw::BufferSize];
    const LDestFetchProc destFetch = destFetchProc[(int)rasterBuffer->format];
    const LDestStoreProc destStore = destStoreProc[(int)rasterBuffer->format];

    if (!clip) {
        for (int ly = 0; ly < mapHeight; ++ly) {
            int i = x;
            int length = mapWidth;
            while (length > 0) {
                int l = std::min<int>(draw::BufferSize, length);
                uint32_t *dest = destFetch(buffer, rasterBuffer, i, y + ly, l);
                for (int j=0; j < l; ++j) {
                    const uint32_t coverage = src[j + (i - x)];
                    alphargbblendArgb32(dest + j, coverage, srcColor, c, colorProfile);
                }
                if (destStore){
                    destStore(rasterBuffer, i, y + ly, dest, l);
                }
                length -= l;
                i += l;
            }
            src += srcStride;
        }
    } else {
        int bottom = std::min<int>(y + mapHeight, rasterBuffer->height());

        int top = std::max<int>(y, 0);
        src += (top - y) * srcStride;

        const_cast<LSpanClipData *>(clip)->initialize();
        for (int yp = top; yp<bottom; ++yp) {
            const LSpanClipData::ClipLine &line = clip->m_clipLines[yp];

            for (int i=0; i<line.count; ++i) {
                const LSpan &clip = line.spans[i];

                int start = std::max<int>(x, clip.x);
                int end = std::min<int>(x + mapWidth, clip.x + clip.len);
                if (end <= start){
                    continue;
                }

                uint32_t *dest = destFetch(buffer, rasterBuffer, start, clip.y, end - start);

                for (int xp=start; xp<end; ++xp) {
                    const uint32_t coverage = src[xp - x];
                    alphargbblendArgb32(dest + xp - start, coverage, srcColor, c, colorProfile);
                }
                if (destStore){
                    destStore(rasterBuffer, start, clip.y, dest, end - start);
                }
            } // for (i -> line.count)
            src += srcStride;
        } // for (yp -> bottom)
    }
}


static void lRectfillArgb32(LRasterBuffer *rasterBuffer,
                               int x, int y, int width, int height,
                               const uint32_t &color)
{
    lRectfill<uint32_t>(reinterpret_cast<uint32_t *>(rasterBuffer->buffer()),
                         color, x, y, width, height, rasterBuffer->bytesPerLine());
}

static void lRectfillNonpremulArgb32(LRasterBuffer *rasterBuffer,
                                         int x, int y, int width, int height,
                                         const uint32_t &color)
{
    lRectfill<uint32_t>(reinterpret_cast<uint32_t *>(rasterBuffer->buffer()),
                        LColor::unpremultiply(color), x, y, width, height, rasterBuffer->bytesPerLine());
}

static void lRectfillArgb16(LRasterBuffer *rasterBuffer,
                                int x, int y, int width, int height,
                                const uint32_t &color)
{
    const draw::LPixelLayout &layout = draw::lPixelLayouts[(int)rasterBuffer->format];
    uint32_t c32 = color;
    uint16_t c16;
    layout.storeFromARGB32PM(reinterpret_cast<uint8_t *>(&c16), &c32, 0, 1, nullptr, nullptr);
    lRectfill<uint16_t>(reinterpret_cast<uint16_t *>(rasterBuffer->buffer()),
                         c16, x, y, width, height, rasterBuffer->bytesPerLine());
}

static void lRectfillArgb24(LRasterBuffer *rasterBuffer,
                                int x, int y, int width, int height,
                                const uint32_t &color)
{
    const draw::LPixelLayout &layout = draw::lPixelLayouts[(int)rasterBuffer->format];
    uint32_t c32 = color;
    uint24_t c24;
    layout.storeFromARGB32PM(reinterpret_cast<uint8_t *>(&c24), &c32, 0, 1, nullptr, nullptr);
    lRectfill<uint24_t>(reinterpret_cast<uint24_t *>(rasterBuffer->buffer()),
                         c24, x, y, width, height, rasterBuffer->bytesPerLine());
}


static void lRectfillAbgr(LRasterBuffer *rasterBuffer,
                             int x, int y, int width, int height,
                             const uint32_t &color)
{
    lRectfill<uint32_t>(reinterpret_cast<uint32_t *>(rasterBuffer->buffer()),
                        LColor::argbToAbgr(color), x, y, width, height, rasterBuffer->bytesPerLine());
}


static void lRectfillNonpremulAbgr(LRasterBuffer *rasterBuffer,
                                       int x, int y, int width, int height,
                                       const uint32_t &color)
{
    lRectfill<uint32_t>(reinterpret_cast<uint32_t *>(rasterBuffer->buffer()),
                       LColor::argbToAbgr(LColor::unpremultiply(color)), x, y, width, height, rasterBuffer->bytesPerLine());
}

template<draw::PixelOrder PixelOrder>
static void lRectfillRgb30(LRasterBuffer *rasterBuffer,
                              int x, int y, int width, int height,
                              const uint32_t &color)
{
    lRectfill<uint32_t>(reinterpret_cast<uint32_t *>(rasterBuffer->buffer()),
                        draw::lConvertRgb32ToRgb30<PixelOrder>(color), x, y, width, height, rasterBuffer->bytesPerLine());
}


static void lRectfillAlpha(LRasterBuffer *rasterBuffer,
                              int x, int y, int width, int height,
                              const uint32_t &color)
{
    lRectfill<uint8_t>(reinterpret_cast<uint8_t *>(rasterBuffer->buffer()),
                       LColor::uintToAlpha(color), x, y, width, height, rasterBuffer->bytesPerLine());
}

static void lRectfillGray(LRasterBuffer *rasterBuffer,
                             int x, int y, int width, int height,
                             const uint32_t &color)
{
    lRectfill<uint8_t>(reinterpret_cast<uint8_t *>(rasterBuffer->buffer()),
                       LColor::toGray(color), x, y, width, height, rasterBuffer->bytesPerLine());
}

static void lRectfillArgb64(LRasterBuffer *rasterBuffer,
                                int x, int y, int width, int height,
                                const uint32_t &color)
{
    const auto store = draw::lStoreFromRGBA64PM[(int)rasterBuffer->format];
    uint64_t c = LColor::argbToArgb64(color);
    uint64_t c64;
    store(reinterpret_cast<uint8_t *>(&c64), &c, 0, 1, nullptr, nullptr);
    lRectfill<uint64_t>(reinterpret_cast<uint64_t *>(rasterBuffer->buffer()),
                         c64, x, y, width, height, rasterBuffer->bytesPerLine());
}

LDrawHelper lDrawHelper[(int)LImage::Format::NImageFormats] =
{
    // Format_Invalid,
    { nullptr, nullptr, nullptr, nullptr, nullptr },
    // Format_Mono,
    {
        blendColorGeneric,
        nullptr, nullptr, nullptr, nullptr
    },
    // Format_MonoLSB,
    {
        blendColorGeneric,
        nullptr, nullptr, nullptr, nullptr
    },
    // Format_Indexed8,
    {
        blendColorGeneric,
        nullptr, nullptr, nullptr, nullptr
    },
    // Format_RGB32,
    {
        blendColorArgb,
        lBitmapblitArgb32,
        lAlphamapblitArgb32,
        lAlphargbblitArgb32,
        lRectfillArgb32
    },
    // Format_ARGB32,
    {
        blendColorGeneric,
        lBitmapblitArgb32,
        lAlphamapblitArgb32,
        lAlphargbblitArgb32,
        lRectfillNonpremulArgb32
    },
    // Format_ARGB32_Premultiplied
    {
        blendColorArgb,
        lBitmapblitArgb32,
        lAlphamapblitArgb32,
        lAlphargbblitArgb32,
        lRectfillArgb32
    },
    // Format_RGB16
    {
        blendColorRgb16,
        lBitmapblitRgb16,
        lAlphamapblitArgb16,
        lAlphargbblitGeneric,
        lRectfillArgb16
    },
    // Format_ARGB8565_Premultiplied
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb24
    },
    // Format_RGB666
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb24
    },
    // Format_ARGB6666_Premultiplied
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb24
    },
    // Format_RGB555
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb16
    },
    // Format_ARGB8555_Premultiplied
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb24
    },
    // Format_RGB888
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb24
    },
    // Format_RGB444
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb16
    },
    // Format_ARGB4444_Premultiplied
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb16
    },
    // Format_RGBX8888
    {
        blendColorGeneric,
        lBitmapblitABGR32,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillAbgr
    },
    // Format_ABGR32
    {
        blendColorGeneric,
        lBitmapblitABGR32,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillNonpremulAbgr
    },
    // Format_RGB8888_Premultiplied
    {
        blendColorGeneric,
        lBitmapblitABGR32,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillAbgr
    },
    // Format_BGR30
    {
        blendColorGeneric,
        lBitmapblitRgb30<draw::PixelOrderBGR>,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillRgb30<draw::PixelOrderBGR>
    },
    // Format_A2BGR30_Premultiplied
    {
        blendColorGeneric,
        lBitmapblitRgb30<draw::PixelOrderBGR>,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillRgb30<draw::PixelOrderBGR>
    },
    // Format_RGB30
    {
        blendColorGeneric,
        lBitmapblitRgb30<draw::PixelOrderRGB>,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillRgb30<draw::PixelOrderRGB>
    },
    // Format_A2RGB30_Premultiplied
    {
        blendColorGeneric,
        lBitmapblitRgb30<draw::PixelOrderRGB>,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillRgb30<draw::PixelOrderRGB>
    },
    // Format_Alpha8
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillAlpha
    },
    // Format_Grayscale8
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillGray
    },
    // Format_RGBX64
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb64
    },
    // Format_RGBA64
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb64
    },
    // Format_RGBA64_Premultiplied
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb64
    },
    // Format_Grayscale16
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb16
    },
    // Format_BGR888
    {
        blendColorGeneric,
        nullptr,
        lAlphamapblitGeneric,
        lAlphargbblitGeneric,
        lRectfillArgb24
    },
};


template <typename T>
void handleSpans(int count, const LSpan *spans, const LSpanData *data, T &handler)
{
    uint32_t const_alpha = 256;
    if (data->type == LSpanData::Texture){
        const_alpha = data->texture->constAlpha;
    }

    int coverage = 0;
    while (count) {
        if (!spans->len) {
            ++spans;
            --count;
            continue;
        }
        int x = spans->x;
        const int y = spans->y;
        int right = x + spans->len;

        // compute length of adjacent spans
        for (int i = 1; i < count && spans[i].y == y && spans[i].x == right; ++i)
            right += spans[i].len;
        int length = right - x;

        while (length) {
            int l = std::min<int>(draw::BufferSize, length);
            length -= l;

            int process_length = l;
            int process_x = x;

            const typename T::BlendType *src = handler.fetch(process_x, y, process_length);
            int offset = 0;
            while (l > 0) {
                if (x == spans->x){ // new span?
                    coverage = (spans->coverage * const_alpha) >> 8;
                }

                int right = spans->x + spans->len;
                int len = std::min<int>(l, right - x);

                handler.process(x, y, len, coverage, src, offset);

                l -= len;
                x += len;
                offset += len;

                if (x == right) { // done with current span?
                    ++spans;
                    --count;
                }
            }
            handler.store(process_x, y, process_length);
        }
    }
}

template<typename T>
struct LBlendBase
{
    typedef T BlendType;
    LBlendBase(LSpanData *d, const LDrawOperator &o)
        : data(d)
        , op(o)
        , dest(nullptr)
    {
    }

    LSpanData *data;
    LDrawOperator op;

    BlendType *dest;

    alignas(8) BlendType buffer[draw::BufferSize];
    alignas(8) BlendType src_buffer[draw::BufferSize];
};

class LBlendSrcGeneric : public LBlendBase<uint32_t>
{
public:
    LBlendSrcGeneric(LSpanData *d, const LDrawOperator &o)
        : LBlendBase<uint32_t>(d, o)
    {
    }

    const uint32_t *fetch(int x, int y, int len)
    {
        dest = op.destFetch(buffer, data->rasterBuffer, x, y, len);
        return op.srcFetch(src_buffer, &op, data, y, x, len);
    }

    void process(int, int, int len, int coverage, const uint32_t *src, int offset)
    {
        op.func(dest + offset, src + offset, len, coverage);
    }

    void store(int x, int y, int len)
    {
        if (op.destStore){
            op.destStore(data->rasterBuffer, x, y, dest, len);
        }
    }
};

static void blendSrcGeneric(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);
    LBlendSrcGeneric blend(data, getDrawOperator(data, spans, count));
    handleSpans(count, spans, data, blend);
}


static void blendUntransformedGeneric(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);

    uint32_t buffer[draw::BufferSize];
    uint32_t src_buffer[draw::BufferSize];
    LDrawOperator op = getDrawOperator(data, spans, count);

    const int image_width = data->texture->width;
    const int image_height = data->texture->height;
    int xoff = std::lround(data->dx);
    int yoff = std::lround(data->dy);

    for (; count--; spans++) {
        if (!spans->len)
            continue;
        int x = spans->x;
        int length = spans->len;
        int sx = xoff + x;
        int sy = yoff + spans->y;
        if (sy >= 0 && sy < image_height && sx < image_width) {
            if (sx < 0) {
                x -= sx;
                length += sx;
                sx = 0;
            }
            if (sx + length > image_width)
                length = image_width - sx;
            if (length > 0) {
                const int coverage = (spans->coverage * data->texture->constAlpha) >> 8;
                while (length) {
                    int l = std::min<int>(draw::BufferSize, length);
                    const uint32_t *src = op.srcFetch(src_buffer, &op, data, sy, sx, l);
                    uint32_t *dest = op.destFetch(buffer, data->rasterBuffer, x, spans->y, l);
                    op.func(dest, src, l, coverage);
                    if (op.destStore)
                        op.destStore(data->rasterBuffer, x, spans->y, dest, l);
                    x += l;
                    sx += l;
                    length -= l;
                }
            }
        }
    }
}

static void blendUntransformedArgb(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);
    if (data->texture->format != LImage::Format::ARGB32Premultiplied
        && data->texture->format != LImage::Format::RGB32) {
        blendUntransformedGeneric(count, spans, userData);
        return;
    }

    LDrawOperator op = getDrawOperator(data, spans, count);

    const int image_width = data->texture->width;
    const int image_height = data->texture->height;
    int xoff = std::lround(data->dx);
    int yoff = std::lround(data->dy);

    for (; count--; spans++) {
        if (!spans->len){
            continue;
        }
        int x = spans->x;
        int length = spans->len;
        int sx = xoff + x;
        int sy = yoff + spans->y;
        if (sy >= 0 && sy < image_height && sx < image_width) {
            if (sx < 0) {
                x -= sx;
                length += sx;
                sx = 0;
            }
            if (sx + length > image_width)
                length = image_width - sx;
            if (length > 0) {
                const int coverage = (spans->coverage * data->texture->constAlpha) >> 8;
                const uint32_t *src = (const uint32_t *)data->texture->scanLine(sy) + sx;
                uint32_t *dest = ((uint32_t *)data->rasterBuffer->scanLine(spans->y)) + x;
                op.func(dest, src, length, coverage);
            }
        }
    }
}


static inline uint16_t interpolatePixelRgb16_255(uint16_t x, uint8_t a,
                                                  uint16_t y, uint8_t b)
{
    uint16_t t = ((((x & 0x07e0) * a) + ((y & 0x07e0) * b)) >> 5) & 0x07e0;
    t |= ((((x & 0xf81f) * a) + ((y & 0xf81f) * b)) >> 5) & 0xf81f;

    return t;
}

static inline uint32_t interpolatePixelRgb16x2_255(uint32_t x, uint8_t a,
                                                    uint32_t y, uint8_t b)
{
    uint16_t t;
    t = ((((x & 0xf81f07e0) >> 5) * a) + (((y & 0xf81f07e0) >> 5) * b)) & 0xf81f07e0;
    t |= ((((x & 0x07e0f81f) * a) + ((y & 0x07e0f81f) * b)) >> 5) & 0x07e0f81f;
    return t;
}

static inline void blendSourceOverRgb16Rgb16(uint16_t * dest,
                                                const uint16_t* src,
                                                int length,
                                                const uint8_t alpha,
                                                const uint8_t ialpha)
{
    const int dstAlign = ((luintptr)dest) & 0x3;
    if (dstAlign) {
        *dest = interpolatePixelRgb16_255(*src, alpha, *dest, ialpha);
        ++dest;
        ++src;
        --length;
    }
    const int srcAlign = ((luintptr)src) & 0x3;
    int length32 = length >> 1;
    if (length32 && srcAlign == 0) {
        while (length32--) {
            const uint32_t *src32 = reinterpret_cast<const uint32_t*>(src);
            uint32_t *dest32 = reinterpret_cast<uint32_t*>(dest);
            *dest32 = interpolatePixelRgb16x2_255(*src32, alpha,
                                                    *dest32, ialpha);
            dest += 2;
            src += 2;
        }
        length &= 0x1;
    }
    while (length--) {
        *dest = interpolatePixelRgb16_255(*src, alpha, *dest, ialpha);
        ++dest;
        ++src;
    }
}


static void blendUntransformedRgb565(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData*>(userData);
    LCompositionMode mode = data->rasterBuffer->compositionMode;

    if (data->texture->format != LImage::Format::RGB16
        || (mode != LCompositionMode::SourceOver
            && mode != LCompositionMode::Source))
    {
        blendUntransformedGeneric(count, spans, userData);
        return;
    }

    const int image_width = data->texture->width;
    const int image_height = data->texture->height;
    int xoff = std::lround(data->dx);
    int yoff = std::lround(data->dy);

    const LSpan *end = spans + count;
    while (spans < end) {
        if (!spans->len) {
            ++spans;
            continue;
        }
        const uint8_t coverage = (data->texture->constAlpha * spans->coverage) >> 8;
        if (coverage == 0) {
            ++spans;
            continue;
        }

        int x = spans->x;
        int length = spans->len;
        int sx = xoff + x;
        int sy = yoff + spans->y;
        if (sy >= 0 && sy < image_height && sx < image_width) {
            if (sx < 0) {
                x -= sx;
                length += sx;
                sx = 0;
            }
            if (sx + length > image_width)
                length = image_width - sx;
            if (length > 0) {
                uint16_t *dest = (uint16_t *)data->rasterBuffer->scanLine(spans->y) + x;
                const uint16_t *src = (const uint16_t *)data->texture->scanLine(sy) + sx;
                if (coverage == 255) {
                    memcpy(dest, src, length * sizeof(uint16_t));
                } else {
                    const uint8_t alpha = (coverage + 1) >> 3;
                    const uint8_t ialpha = 0x20 - alpha;
                    if (alpha > 0){
                        blendSourceOverRgb16Rgb16(dest, src, length, alpha, ialpha);
                    }
                }
            }
        }
        ++spans;
    }
}

static void blendTiledGeneric(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);

    uint32_t buffer[draw::BufferSize];
    uint32_t src_buffer[draw::BufferSize];
    LDrawOperator op = getDrawOperator(data, spans, count);

    const int image_width = data->texture->width;
    const int image_height = data->texture->height;
    int xoff = std::lround(data->dx);
    int yoff = std::lround(data->dy);
    xoff = xoff % image_width;
    yoff = yoff % image_height;

    if (xoff < 0){
        xoff += image_width;
    }
    if (yoff < 0){
        yoff += image_height;
    }

    while (count--) {
        int x = spans->x;
        int length = spans->len;
        int sx = (xoff + spans->x) % image_width;
        int sy = (spans->y + yoff) % image_height;
        if (sx < 0)
            sx += image_width;
        if (sy < 0)
            sy += image_height;

        const int coverage = (spans->coverage * data->texture->constAlpha) >> 8;
        while (length) {
            int l = std::min<int>(image_width - sx, length);
            if (draw::BufferSize < l){
                l = draw::BufferSize;
            }
            const uint32_t *src = op.srcFetch(src_buffer, &op, data, sy, sx, l);
            uint32_t *dest = op.destFetch(buffer, data->rasterBuffer, x, spans->y, l);
            op.func(dest, src, l, coverage);
            if (op.destStore){
                op.destStore(data->rasterBuffer, x, spans->y, dest, l);
            }
            x += l;
            sx += l;
            length -= l;
            if (sx >= image_width){
                sx = 0;
            }
        }
        ++spans;
    }
}

static void blendTiledArgb(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);
    if (data->texture->format != LImage::Format::ARGB32Premultiplied
        && data->texture->format != LImage::Format::RGB32) {
        blendTiledGeneric(count, spans, userData);
        return;
    }

    LDrawOperator op = getDrawOperator(data, spans, count);

    int image_width = data->texture->width;
    int image_height = data->texture->height;
    int xoff = std::lround(data->dx);
    int yoff = std::lround(data->dy);

    xoff = xoff % image_width;
    yoff = yoff % image_height;

    if (xoff < 0){
        xoff += image_width;
    }
    if (yoff < 0){
        yoff += image_height;
    }

    while (count--) {
        int x = spans->x;
        int length = spans->len;
        int sx = (xoff + spans->x) % image_width;
        int sy = (spans->y + yoff) % image_height;
        if (sx < 0){
            sx += image_width;
        }
        if (sy < 0){
            sy += image_height;
        }

        const int coverage = (spans->coverage * data->texture->constAlpha) >> 8;
        while (length) {
            int l = std::min<int>(image_width - sx, length);
            if (draw::BufferSize < l){
                l = draw::BufferSize;
            }
            const uint32_t *src = (const uint32_t *)data->texture->scanLine(sy) + sx;
            uint32_t *dest = ((uint32_t *)data->rasterBuffer->scanLine(spans->y)) + x;
            op.func(dest, src, l, coverage);
            x += l;
            sx += l;
            length -= l;
            if (sx >= image_width)
                sx = 0;
        }
        ++spans;
    }
}


static void blendTiledRgb565(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData*>(userData);
    LCompositionMode mode = data->rasterBuffer->compositionMode;

    if (data->texture->format != LImage::Format::RGB16
        || (mode != LCompositionMode::SourceOver
            && mode != LCompositionMode::Source))
    {
        blendTiledGeneric(count, spans, userData);
        return;
    }

    const int image_width = data->texture->width;
    const int image_height = data->texture->height;

    int xoff = std::lround(data->dx);
    int yoff = std::lround(data->dy);
    xoff = xoff % image_width;
    yoff = yoff % image_height;

    if (xoff < 0){
        xoff += image_width;
    }
    if (yoff < 0){
        yoff += image_height;
    }

    while (count--) {
        const uint8_t coverage = (data->texture->constAlpha * spans->coverage) >> 8;
        if (coverage == 0) {
            ++spans;
            continue;
        }

        int x = spans->x;
        int length = spans->len;
        int sx = (xoff + spans->x) % image_width;
        int sy = (spans->y + yoff) % image_height;
        if (sx < 0){
            sx += image_width;
        }
        if (sy < 0){
            sy += image_height;
        }

        if (coverage == 255) {
            // Copy the first texture block
            length = std::min<int>(image_width,length);
            int tx = x;
            while (length) {
                int l = std::min<int>(image_width - sx, length);
                if (draw::BufferSize < l){
                    l = draw::BufferSize;
                }
                uint16_t *dest = ((uint16_t *)data->rasterBuffer->scanLine(spans->y)) + tx;
                const uint16_t *src = (const uint16_t *)data->texture->scanLine(sy) + sx;
                memcpy(dest, src, l * sizeof(uint16_t));
                length -= l;
                tx += l;
                sx += l;
                if (sx >= image_width){
                    sx = 0;
                }
            }

            // Now use the rasterBuffer as the source of the texture,
            // We can now progressively copy larger blocks
            // - Less cpu time in code figuring out what to copy
            // We are dealing with one block of data
            // - More likely to fit in the cache
            // - can use memcpy
            int copy_image_width = std::min<int>(image_width, int(spans->len));
            length = spans->len - copy_image_width;
            uint16_t *src = ((uint16_t *)data->rasterBuffer->scanLine(spans->y)) + x;
            uint16_t *dest = src + copy_image_width;
            while (copy_image_width < length) {
                memcpy(dest, src, copy_image_width * sizeof(uint16_t));
                dest += copy_image_width;
                length -= copy_image_width;
                copy_image_width *= 2;
            }
            if (length > 0){
                memcpy(dest, src, length * sizeof(uint16_t));
            }
        } else {
            const uint8_t alpha = (coverage + 1) >> 3;
            const uint8_t ialpha = 0x20 - alpha;
            if (alpha > 0) {
                while (length) {
                    int l = std::min<int>(image_width - sx, length);
                    if (draw::BufferSize < l){
                        l = draw::BufferSize;
                    }
                    uint16_t *dest = ((uint16_t *)data->rasterBuffer->scanLine(spans->y)) + x;
                    const uint16_t *src = (const uint16_t *)data->texture->scanLine(sy) + sx;
                    blendSourceOverRgb16Rgb16(dest, src, l, alpha, ialpha);
                    x += l;
                    sx += l;
                    length -= l;
                    if (sx >= image_width){
                        sx = 0;
                    }
                }
            }
        }
        ++spans;
    }
}


/* Image formats here are target formats */
static const ProcessSpans processTextureSpansARGB32PM[NBlendTypes] = {
    blendUntransformedArgb,           // Untransformed
    blendTiledArgb,                   // Tiled
    blendSrcGeneric,                  // Transformed
    blendSrcGeneric,                  // TransformedTiled
    blendSrcGeneric,                  // TransformedBilinear
    blendSrcGeneric                   // TransformedBilinearTiled
};

static const ProcessSpans processTextureSpansRGB16[NBlendTypes] = {
    blendUntransformedRgb565,         // Untransformed
    blendTiledRgb565,                 // Tiled
    blendSrcGeneric,                  // Transformed
    blendSrcGeneric,                  // TransformedTiled
    blendSrcGeneric,                  // TransformedBilinear
    blendSrcGeneric                   // TransformedBilinearTiled
};

static const ProcessSpans processTextureSpansGeneric[NBlendTypes] = {
    blendUntransformedGeneric,        // Untransformed
    blendTiledGeneric,                // Tiled
    blendSrcGeneric,                  // Transformed
    blendSrcGeneric,                  // TransformedTiled
    blendSrcGeneric,                  // TransformedBilinear
    blendSrcGeneric                   // TransformedBilinearTiled
};

static inline bool calculateFixedGradientFactors(int count, const LSpan *spans,
                                                    const LSpanData *data,
                                                    const LinearGradientValues &linear,
                                                    int *pyinc, int *poff)
{

    const int gss = GRADIENT_STOPTABLE_SIZE - 1;
    double ryinc = linear.dy * data->m22 * gss * FIXPT_SIZE;
    double roff = (linear.dy * (data->m22 * double(0.5) + data->dy) + linear.off) * gss * FIXPT_SIZE;
    const int limit = (std::numeric_limits<int>::max)() - FIXPT_SIZE;
    if (count && (std::fabs(ryinc) < limit) && (std::fabs(roff) < limit)
        && (std::fabs(ryinc * spans->y + roff) < limit)
        && (std::fabs(ryinc * (spans + count - 1)->y + roff) < limit)) {
        *pyinc = int(ryinc);
        *poff = int(roff);
        return true;
    }
    return false;
}



static bool blendVerticalGradientArgb(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);

    LinearGradientValues linear;
    getLinearGradientValues(&linear, data);

    CompositionFunctionSolid funcSolid =
        lkFunctionForModeSolidC[(int)data->rasterBuffer->compositionMode];

    int yinc(0), off(0);
    if (!calculateFixedGradientFactors(count, spans, data, linear, &yinc, &off)){
        return false;
    }

    while (count--) {
        int y = spans->y;
        int x = spans->x;

        uint32_t *dst = (uint32_t *)(data->rasterBuffer->scanLine(y)) + x;
        uint32_t color =  GradientBase::fetchSingle(data->gradient, yinc * y + off);
        funcSolid(dst, spans->len, color, spans->coverage);
        ++spans;
    }
    return true;
}


template<ProcessSpans blendColor>
static bool blendVerticalGradient(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);

    LinearGradientValues linear;
    getLinearGradientValues(&linear, data);

    int yinc(0), off(0);
    if (!calculateFixedGradientFactors(count, spans, data, linear, &yinc, &off)){
        return false;
    }

    while (count--) {
        int y = spans->y;
        data->solidColor = GradientBase::fetchSingle(data->gradient, yinc * y + off);
        blendColor(1, spans, userData);
        ++spans;
    }
    return true;
}


void lBlendGradient(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);
    if(data->gradient == nullptr){
        return;
    }

    bool isVerticalGradient =
        data->txop <= LTransform2D::TxScale &&
        data->type == LSpanData::Gradient &&
        data->gradient->m_type == LGradient::Type::Linear &&
        data->gradient->m_linear.end.x == data->gradient->m_linear.origin.x;

    switch (data->rasterBuffer->format) {
    case LImage::Format::Invalid:
        break;
    case LImage::Format::RGB32:
    case LImage::Format::ARGB32Premultiplied:{
        if (isVerticalGradient && blendVerticalGradientArgb(count, spans, userData)){
            return;
        }
        return blendSrcGeneric(count, spans, userData);
    }
    default:
        if (isVerticalGradient && blendVerticalGradient<blendColorGeneric>(count, spans, userData)){
            return;
        }
        return blendSrcGeneric(count, spans, userData);
    }
}


void lBlendTexture(int count, const LSpan *spans, void *userData)
{
    LSpanData *data = reinterpret_cast<LSpanData *>(userData);
    TextureBlendType blendType = getBlendType(data);
    ProcessSpans proc;
    switch (data->rasterBuffer->format) {
    case LImage::Format::ARGB32Premultiplied:{
        proc = processTextureSpansARGB32PM[blendType];
        break;
    }
    case LImage::Format::RGB16:{
        proc = processTextureSpansRGB16[blendType];
        break;
    }
    case LImage::Format::Invalid:{
        return;
    }
    default:{
        proc = processTextureSpansGeneric[blendType];
        break;
    }
    }
    proc(count, spans, userData);
}



LK_NAMESPACE_END
