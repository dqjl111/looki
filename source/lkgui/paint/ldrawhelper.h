#ifndef LDRAWHELPER_H
#define LDRAWHELPER_H
#include "lkgui/paint/lcolor.h"
#include <emmintrin.h>
#include <vector>
#include "lkgui/lpoint.h"
#include "lkgui/image/limage.h"
#include "lkgui/paint/lgradient.h"
#include "lkgui/paint/lpainter.h"

LK_NAMESPACE_BEGIN
namespace draw {
static const int BufferSize = 2048;
static const uint32_t lBayerMatrix[16][16] = {
    { 0x1, 0xc0, 0x30, 0xf0, 0xc, 0xcc, 0x3c, 0xfc,
     0x3, 0xc3, 0x33, 0xf3, 0xf, 0xcf, 0x3f, 0xff},
    { 0x80, 0x40, 0xb0, 0x70, 0x8c, 0x4c, 0xbc, 0x7c,
     0x83, 0x43, 0xb3, 0x73, 0x8f, 0x4f, 0xbf, 0x7f},
    { 0x20, 0xe0, 0x10, 0xd0, 0x2c, 0xec, 0x1c, 0xdc,
     0x23, 0xe3, 0x13, 0xd3, 0x2f, 0xef, 0x1f, 0xdf},
    { 0xa0, 0x60, 0x90, 0x50, 0xac, 0x6c, 0x9c, 0x5c,
     0xa3, 0x63, 0x93, 0x53, 0xaf, 0x6f, 0x9f, 0x5f},
    { 0x8, 0xc8, 0x38, 0xf8, 0x4, 0xc4, 0x34, 0xf4,
     0xb, 0xcb, 0x3b, 0xfb, 0x7, 0xc7, 0x37, 0xf7},
    { 0x88, 0x48, 0xb8, 0x78, 0x84, 0x44, 0xb4, 0x74,
     0x8b, 0x4b, 0xbb, 0x7b, 0x87, 0x47, 0xb7, 0x77},
    { 0x28, 0xe8, 0x18, 0xd8, 0x24, 0xe4, 0x14, 0xd4,
     0x2b, 0xeb, 0x1b, 0xdb, 0x27, 0xe7, 0x17, 0xd7},
    { 0xa8, 0x68, 0x98, 0x58, 0xa4, 0x64, 0x94, 0x54,
     0xab, 0x6b, 0x9b, 0x5b, 0xa7, 0x67, 0x97, 0x57},
    { 0x2, 0xc2, 0x32, 0xf2, 0xe, 0xce, 0x3e, 0xfe,
     0x1, 0xc1, 0x31, 0xf1, 0xd, 0xcd, 0x3d, 0xfd},
    { 0x82, 0x42, 0xb2, 0x72, 0x8e, 0x4e, 0xbe, 0x7e,
     0x81, 0x41, 0xb1, 0x71, 0x8d, 0x4d, 0xbd, 0x7d},
    { 0x22, 0xe2, 0x12, 0xd2, 0x2e, 0xee, 0x1e, 0xde,
     0x21, 0xe1, 0x11, 0xd1, 0x2d, 0xed, 0x1d, 0xdd},
    { 0xa2, 0x62, 0x92, 0x52, 0xae, 0x6e, 0x9e, 0x5e,
     0xa1, 0x61, 0x91, 0x51, 0xad, 0x6d, 0x9d, 0x5d},
    { 0xa, 0xca, 0x3a, 0xfa, 0x6, 0xc6, 0x36, 0xf6,
     0x9, 0xc9, 0x39, 0xf9, 0x5, 0xc5, 0x35, 0xf5},
    { 0x8a, 0x4a, 0xba, 0x7a, 0x86, 0x46, 0xb6, 0x76,
     0x89, 0x49, 0xb9, 0x79, 0x85, 0x45, 0xb5, 0x75},
    { 0x2a, 0xea, 0x1a, 0xda, 0x26, 0xe6, 0x16, 0xd6,
     0x29, 0xe9, 0x19, 0xd9, 0x25, 0xe5, 0x15, 0xd5},
    { 0xaa, 0x6a, 0x9a, 0x5a, 0xa6, 0x66, 0x96, 0x56,
     0xa9, 0x69, 0x99, 0x59, 0xa5, 0x65, 0x95, 0x55}
};

// Bits per pixel
enum BPP {
    BPPNone,
    BPP1MSB,
    BPP1LSB,
    BPP8,
    BPP16,
    BPP24,
    BPP32,
    BPP64,
    BPPCount
};


enum PixelOrder {
    PixelOrderRGB,
    PixelOrderBGR
};

typedef const uint32_t*(*FetchAndConvertPixelsFunc)(uint32_t *buffer,
                                                     const uint8_t *src,
                                                     int index,
                                                     int count,
                                                     const std::vector<uint32_t> *clut,
                                                     LPoint<int> *dither);
typedef void (*ConvertAndStorePixelsFunc)(uint8_t *dest,
                                          const uint32_t *src,
                                          int index,
                                          int count,
                                          const std::vector<uint32_t> *clut,
                                          LPoint<int> *dither);

typedef const uint64_t *(*FetchAndConvertPixelsFunc64)(uint64_t *buffer,
                                                       const uint8_t *src,
                                                       int index,
                                                       int count,
                                                       const std::vector<uint32_t> *clut,
                                                       LPoint<int> *dither);

typedef void (*ConvertAndStorePixelsFunc64)(uint8_t *dest,
                                            const uint64_t *src,
                                            int index,
                                            int count,
                                            const std::vector<uint32_t> *clut,
                                            LPoint<int> *dither);

typedef void (*ConvertFunc)(uint32_t *buffer, int count, const std::vector<uint32_t> *clut);
typedef void (*Convert64Func)(uint64_t *buffer, int count, const std::vector<uint32_t> *clut);
typedef const uint64_t *(*ConvertTo64Func)(uint64_t *buffer,
                                           const uint32_t *src,
                                           int count,
                                           const std::vector<uint32_t> *clut,
                                           LPoint<int> *dither);
typedef void (*RbSwapFunc)(uint8_t *dst, const uint8_t *src, int count);

struct LPixelLayout
{
    bool hasAlphaChannel;
    bool premultiplied;
    draw::BPP bpp;
    RbSwapFunc rbSwap;
    ConvertFunc convertToARGB32PM;
    ConvertTo64Func convertToRGBA64PM;
    FetchAndConvertPixelsFunc fetchToARGB32PM;
    FetchAndConvertPixelsFunc64 fetchToRGBA64PM;
    ConvertAndStorePixelsFunc storeFromARGB32PM;
    ConvertAndStorePixelsFunc storeFromRGB32;
};
extern LPixelLayout lPixelLayouts[(int)LImage::Format::NImageFormats];
extern ConvertAndStorePixelsFunc64 lStoreFromRGBA64PM[(int)LImage::Format::NImageFormats];

static uint32_t interpolatePixel256(uint32_t x, uint32_t a, uint32_t y, uint32_t b) {
    uint32_t t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t >>= 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x &= 0xff00ff00;
    x |= t;
    return x;
}


static uint32_t interpolate4Pixels(uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br, uint32_t distx, uint32_t disty)
{
    uint32_t idistx = 256 - distx;
    uint32_t idisty = 256 - disty;
    uint32_t xtop = interpolatePixel256(tl, idistx, tr, distx);
    uint32_t xbot = interpolatePixel256(bl, idistx, br, distx);
    return interpolatePixel256(xtop, idisty, xbot, disty);
}

static uint32_t interpolate4Pixels(const uint32_t t[], const uint32_t b[], uint32_t distx, uint32_t disty)
{
    return interpolate4Pixels(t[0], t[1], b[0], b[1], distx, disty);
}


template<enum PixelOrder>
inline uint32_t lConvertArgb32ToA2rgb30(uint32_t);

template<>
inline uint32_t lConvertArgb32ToA2rgb30<PixelOrderBGR>(uint32_t c)
{
    c = LColor::repremultiply<6>(c);
    return (c & 0xc0000000)
           | (((c << 22) & 0x3fc00000) | ((c << 14) & 0x00300000))
           | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
           | (((c >> 14) & 0x000003fc) | ((c >> 22) & 0x00000003));
}

template<>
inline uint32_t lConvertArgb32ToA2rgb30<PixelOrderRGB>(uint32_t c)
{
    c = LColor::repremultiply<6>(c);
    return (c & 0xc0000000)
           | (((c << 6) & 0x3fc00000) | ((c >> 2) & 0x00300000))
           | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
           | (((c << 2) & 0x000003fc) | ((c >> 6) & 0x00000003));
}


template<enum PixelOrder>
inline uint32_t lConvertRgb32ToRgb30(uint32_t);


template<>
inline uint32_t lConvertRgb32ToRgb30<PixelOrderBGR>(uint32_t c)
{
    return 0xc0000000
           | (((c << 22) & 0x3fc00000) | ((c << 14) & 0x00300000))
           | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
           | (((c >> 14) & 0x000003fc) | ((c >> 22) & 0x00000003));
}

template<>
inline uint32_t lConvertRgb32ToRgb30<PixelOrderRGB>(uint32_t c)
{
    return 0xc0000000
           | (((c << 6) & 0x3fc00000) | ((c >> 2) & 0x00300000))
           | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
           | (((c << 2) & 0x000003fc) | ((c >> 6) & 0x00000003));
}


template<enum PixelOrder>
inline uint32_t lConvertA2rgb30ToArgb32(uint32_t c);

template<>
inline uint32_t lConvertA2rgb30ToArgb32<PixelOrderBGR>(uint32_t c)
{
    uint32_t a = c >> 30;
    a |= a << 2;
    a |= a << 4;
    return (a << 24)
           | ((c << 14) & 0x00ff0000)
           | ((c >> 4) & 0x0000ff00)
           | ((c >> 22) & 0x000000ff);
}

template<>
inline uint32_t lConvertA2rgb30ToArgb32<PixelOrderRGB>(uint32_t c)
{
    uint32_t a = c >> 30;
    a |= a << 2;
    a |= a << 4;
    return (a << 24)
           | ((c >> 6) & 0x00ff0000)
           | ((c >> 4) & 0x0000ff00)
           | ((c >> 2) & 0x000000ff);
}


template<enum PixelOrder>
inline uint64_t lConvertA2rgb30ToRgb64(uint32_t rgb);

template<>
inline uint64_t lConvertA2rgb30ToRgb64<PixelOrderBGR>(uint32_t rgb)
{
    uint16_t alpha = rgb >> 30;
    uint16_t blue  = (rgb >> 20) & 0x3ff;
    uint16_t green = (rgb >> 10) & 0x3ff;
    uint16_t red   = rgb & 0x3ff;
    // Expand the range.
    alpha |= (alpha << 2);
    alpha |= (alpha << 4);
    alpha |= (alpha << 8);
    red   = (red   << 6) | (red   >> 4);
    green = (green << 6) | (green >> 4);
    blue  = (blue  << 6) | (blue  >> 4);
    return LColor::argb16ToUInt64(red, green, blue, alpha);
}

template<>
inline uint64_t lConvertA2rgb30ToRgb64<PixelOrderRGB>(uint32_t rgb)
{
    uint16_t alpha = rgb >> 30;
    uint16_t red   = (rgb >> 20) & 0x3ff;
    uint16_t green = (rgb >> 10) & 0x3ff;
    uint16_t blue  = rgb & 0x3ff;
    // Expand the range.
    alpha |= (alpha << 2);
    alpha |= (alpha << 4);
    alpha |= (alpha << 8);
    red   = (red   << 6) | (red   >> 4);
    green = (green << 6) | (green >> 4);
    blue  = (blue  << 6) | (blue  >> 4);
    return LColor::argb16ToUInt64(red, green, blue, alpha);
}

template<enum PixelOrder>
inline uint32_t lConvertRgb64ToRgb30(uint64_t);

template<>
inline uint32_t lConvertRgb64ToRgb30<PixelOrderBGR>(uint64_t c)
{
    c = LColor::repremultiply<14>(c);
    const uint32_t a = LColor::alpha16(c) >> 14;
    const uint32_t r = LColor::red16(c) >> 6;
    const uint32_t g = LColor::green16(c) >> 6;
    const uint32_t b = LColor::blue16(c) >> 6;
    return (a << 30) | (b << 20) | (g << 10) | r;
}

template<>
inline unsigned int lConvertRgb64ToRgb30<PixelOrderRGB>(uint64_t c)
{
    c = LColor::repremultiply<14>(c);
    const uint32_t a = LColor::alpha16(c) >> 14;
    const uint32_t r = LColor::red16(c) >> 6;
    const uint32_t g = LColor::blue16(c) >> 6;
    const uint32_t b = LColor::blue16(c) >> 6;
    return (a << 30) | (r << 20) | (g << 10) | b;
}

template<bool ABGR>
void lConvertRGBA64ToARGB32(uint32_t *dst, const uint64_t *src, int count){
    for (int i=0; i < count; i++) {
        uint32_t s = LColor::argb64ToArgb(*src++);
        if (ABGR){
            s = LColor::argbToAbgr(s);
        }
        *dst++ = s;
    }
}


static inline uint32_t lByteMul(uint32_t x, uint32_t a) {
    uint32_t t = (x & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080);
    x &= 0xff00ff00;
    x |= t;
    return x;
}

static inline uint32_t lByteMulRgb16(uint32_t x, uint32_t a) {
    a += 1;
    uint32_t t = (((x & 0x07e0)*a) >> 8) & 0x07e0;
    t |= (((x & 0xf81f)*(a>>2)) >> 6) & 0xf81f;
    return t;
}

static inline uint32_t lByteMulRgb16_32(uint32_t x, uint32_t a) {
    uint32_t t = (((x & 0xf81f07e0) >> 5)*a) & 0xf81f07e0;
    t |= (((x & 0x07e0f81f)*a) >> 5) & 0x07e0f81f;
    return t;
}
}

struct LTextureData
{
    const uint8_t *imageData = nullptr;
    const uint8_t *scanLine(int y) const { return imageData + y*bytesPerLine; }

    int width = 0;
    int height = 0;
    // clip rect
    int x1= 0;
    int y1 = 0;
    int x2 = 0;
    int y2 = 0;
    lsizetype bytesPerLine= 0;
    LImage::Format format = LImage::Format::Invalid;
    const std::vector<uint32_t> *colorTable = nullptr;
    bool hasAlpha = false;
    enum Type : uint8_t{
        Plain,
        Tiled
    } type = Plain;
    int constAlpha = 0;
};


struct  LSpan
{
    int x = 0;
    int len = 0;
    int y = 0;
    unsigned char coverage = 0; //像素覆盖率，通过透明度处理锯齿
};

typedef void
    (*ProcessSpans)(int count,
                    const LSpan*  spans,
                    void* worker);

class LRasterBuffer;
class LSpanClipData;
typedef void (*BitmapBlitFunc)(LRasterBuffer *rasterBuffer,
                               int x, int y, const uint32_t &color,
                               const uint8_t *bitmap,
                               int mapWidth, int mapHeight, int mapStride);

typedef void (*AlphamapBlitFunc)(LRasterBuffer *rasterBuffer,
                                 int x, int y, const uint32_t &color,
                                 const uint8_t *bitmap,
                                 int mapWidth, int mapHeight, int mapStride,
                                 const LSpanClipData *clip, bool useGammaCorrection);

typedef void (*AlphaRGBBlitFunc)(LRasterBuffer *rasterBuffer,
                                 int x, int y, const uint32_t &color,
                                 const uint32_t *rgbmask,
                                 int mapWidth, int mapHeight, int mapStride,
                                 const LSpanClipData *clip, bool useGammaCorrection);

typedef void (*RectFillFunc)(LRasterBuffer *rasterBuffer,
                             int x, int y, int width, int height,
                             const uint32_t &color);


struct LDrawHelper {
    ProcessSpans blendColor;
    BitmapBlitFunc bitmapBlit;
    AlphamapBlitFunc alphamapBlit;
    AlphaRGBBlitFunc alphaRGBBlit;
    RectFillFunc fillRect;
};


extern LDrawHelper lDrawHelper[(int)LImage::Format::NImageFormats];


typedef void (*CompositionFunction)(uint32_t * dest, const uint32_t * src, int length, uint32_t alpha);
typedef void (*CompositionFunctionSolid)(uint32_t *dest, int length, uint32_t color, uint32_t alpha);


extern CompositionFunctionSolid lkFunctionForModeSolidC[];
extern CompositionFunction lkFunctionForModeC[];

template <class T>
inline void lMemFillTemplate(T *dest, T color, lsizetype count)
{
    if (!count){
        return;
    }

    lsizetype n = (count + 7) / 8;
    switch (count & 0x07)
    {
    case 0: do { *dest++ = color;
        case 7:      *dest++ = color;
        case 6:      *dest++ = color;
        case 5:      *dest++ = color;
        case 4:      *dest++ = color;
        case 3:      *dest++ = color;
        case 2:      *dest++ = color;
        case 1:      *dest++ = color;
        } while (--n > 0);
    }
}


template <class T> static
inline void lRectfill(T *dest, T value,
                int x, int y, int width, int height, lsizetype stride)
{
    char *d = reinterpret_cast<char*>(dest + x) + y * stride;
    if (uint32_t(stride) == (width * sizeof(T))) {
        lMemFillTemplate(reinterpret_cast<T*>(d), value, lsizetype(width) * height);
    } else {
        for (int j = 0; j < height; ++j) {
            dest = reinterpret_cast<T*>(d);
            lMemFillTemplate(dest, value, width);
            d += stride;
        }
    }
}

struct LDrawOperator;
struct LSpanData;

typedef uint32_t* (*LDestFetchProc)(uint32_t *buffer, LRasterBuffer *rasterBuffer, int x, int y, int length);
typedef void (*LDestStoreProc)(LRasterBuffer *rasterBuffer, int x, int y, const uint32_t *buffer, int length);
typedef const uint32_t* (*LSourceFetchProc)(uint32_t *buffer, const LDrawOperator *o, const LSpanData *data, int y, int x, int length);


extern void lBlendGradient(int count, const LSpan *spans, void *userData);
extern void lBlendTexture(int count, const LSpan *spans, void *userData);

LK_NAMESPACE_END
#endif // LDRAWHELPER_H
