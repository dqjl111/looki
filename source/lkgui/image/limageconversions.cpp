#include "limageconversions.h"
#include "lkcore/io/lioman.h"

LK_NAMESPACE_BEGIN
// first index source, second dest
ImageConverter lImageConverterMap[(int)LImage::Format::NImageFormats][(int)LImage::Format::NImageFormats] = {};
//InPlaceImageConverter lImageInplaceConverterMap[(int)LImage::Format::NImageFormats][(int)LImage::Format::NImageFormats] = {};


static void storeRGB32FromARGB32PM(uint8_t *dest,
                                   const uint32_t *src,
                                   int index,
                                   int count,
                                   const std::vector<uint32_t> *clut,
                                   LPoint<int> *dither)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = 0xff000000 | LColor::unpremultiply(src[i]);
    }
}

static void storeRGB32FromARGB32(uint8_t *dest,
                                 const uint32_t *src,
                                 int index,
                                 int count,
                                 const std::vector<uint32_t> *clut,
                                 LPoint<int> *dither)
{
    uint32_t *d = reinterpret_cast<uint32_t *>(dest) + index;
    for (int i = 0; i < count; ++i){
        d[i] = 0xff000000 | src[i];
    }
}

static const uint32_t* fetchRGB32ToARGB32PM(uint32_t *buffer,
                                            const uint8_t *src,
                                            int index,
                                            int count,
                                            const std::vector<uint32_t> *,
                                            LPoint<int> *)
{
    const uint32_t *s = reinterpret_cast<const uint32_t *>(src) + index;
    for (int i = 0; i < count; ++i){
        buffer[i] = 0xff000000 | s[i];
    }
    return buffer;
}


static inline uint32_t byteMul(uint32_t x, uint32_t a) {
    uint32_t t = (x & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080);
    x &= 0xff00ff00;
    x |= t;
    return x;
}

inline uint32_t lRgbSwapRgb30(uint32_t c)
{
    const uint32_t ag = c & 0xc00ffc00;
    const uint32_t rb = c & 0x3ff003ff;
    return ag | (rb << 20) | (rb >> 20);
}


struct LRgbMap {
    inline LRgbMap() : used(0) { }
    uint8_t  pix;
    uint8_t used;
    uint32_t  rgb;
};

static const uint8_t bitflip[256] = {
    /*
    open OUT, "| fmt";
    for $i (0..255) {
        print OUT (($i >> 7) & 0x01) | (($i >> 5) & 0x02) |
                  (($i >> 3) & 0x04) | (($i >> 1) & 0x08) |
                  (($i << 7) & 0x80) | (($i << 5) & 0x40) |
                  (($i << 3) & 0x20) | (($i << 1) & 0x10), ", ";
    }
    close OUT;
*/
    0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
    8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
    4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
    12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
    2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
    10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
    6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
    14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
    1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
    9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
    5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
    13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
    3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
    11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
    7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
    15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255
};


LImageConversions LImageConversions::inst;

LImageConversions::LImageConversions()
{
    initImageConversions();
}

const uint8_t *LImageConversions::lGetBitFlipArray()
{
    return bitflip;
}

static std::vector<uint32_t> fixColorTable(const std::vector<uint32_t> &ctbl, LImage::Format format)
{
    std::vector<uint32_t> colorTable = ctbl;
    if (format == LImage::Format::RGB32) {
        // check if the color table has alpha
        for (uint32_t i = 0; i < colorTable.size(); ++i){
            if (LColor::uintToAlpha(colorTable.at(i)) != 0xff){
                colorTable[i] = colorTable.at(i) | 0xff000000;
            }
        }
    } else if (format == LImage::Format::ARGB32Premultiplied) {
        // check if the color table has alpha
        for (uint32_t i = 0; i < colorTable.size(); ++i){
            colorTable[i] = LColor::premultiply(colorTable.at(i));
        }
    }
    return colorTable;
}

void LImageConversions::initImageConversions()
{
    // Some conversions can not be generic, other are just hard to make as fast in the generic converter.

    // All conversions to and from indexed formats can not be generic and needs to go over RGB32 or ARGB32
    lImageConverterMap[(int)LImage::Format::Mono][(int)LImage::Format::MonoLSB] = swapBitOrder;
    lImageConverterMap[(int)LImage::Format::Mono][(int)LImage::Format::Indexed8] = convertMonoToIndexed8;
    lImageConverterMap[(int)LImage::Format::Mono][(int)LImage::Format::RGB32] = convertMonoToX32;
    lImageConverterMap[(int)LImage::Format::Mono][(int)LImage::Format::ARGB32] = convertMonoToX32;
    lImageConverterMap[(int)LImage::Format::Mono][(int)LImage::Format::ARGB32Premultiplied] = convertMonoToX32;

    lImageConverterMap[(int)LImage::Format::MonoLSB][(int)LImage::Format::Mono] = swapBitOrder;
    lImageConverterMap[(int)LImage::Format::MonoLSB][(int)LImage::Format::Indexed8] = convertMonoToIndexed8;
    lImageConverterMap[(int)LImage::Format::MonoLSB][(int)LImage::Format::RGB32] = convertMonoToX32;
    lImageConverterMap[(int)LImage::Format::MonoLSB][(int)LImage::Format::ARGB32] = convertMonoToX32;
    lImageConverterMap[(int)LImage::Format::MonoLSB][(int)LImage::Format::ARGB32Premultiplied] = convertMonoToX32;

    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::Mono] = convertXToMono;
    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::MonoLSB] = convertXToMono;
    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::RGB32] = convertIndexed8ToX32;
    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::ARGB32] = convertIndexed8ToX32;
    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::ARGB32Premultiplied] = convertIndexed8ToX32;
    // Indexed8, Alpha8 and Grayscale8 have a special relationship that can be short-cut.
    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::Grayscale8] = convertIndexed8ToGrayscale8;
    lImageConverterMap[(int)LImage::Format::Indexed8][(int)LImage::Format::Alpha8] = convertIndexed8ToAlpha8;

    lImageConverterMap[(int)LImage::Format::RGB32][(int)LImage::Format::Mono] = convertXToMono;
    lImageConverterMap[(int)LImage::Format::RGB32][(int)LImage::Format::MonoLSB] = convertXToMono;
    lImageConverterMap[(int)LImage::Format::RGB32][(int)LImage::Format::Indexed8] = convertRGBToIndexed8;
    lImageConverterMap[(int)LImage::Format::RGB32][(int)LImage::Format::ARGB32] = maskAlphaConverter;
    lImageConverterMap[(int)LImage::Format::RGB32][(int)LImage::Format::ARGB32Premultiplied] = maskAlphaConverter;

    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::Mono] = convertXToMono;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::MonoLSB] = convertXToMono;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::Indexed8] = convertARGBToIndexed8;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::RGB32] = maskAlphaConverter;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::XBGR32] = convertARGBToXBGR;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::ABGR32] = convertARGBToABGR;
    // ARGB32 has higher precision than ARGB32PM and needs explicit conversions to other higher color-precision formats with alpha
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::A2BGR30Premultiplied] = convertARGBToA2RGB30<draw::PixelOrderBGR, false>;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::A2RGB30Premultiplied] = convertARGBToA2RGB30<draw::PixelOrderRGB, false>;
    lImageConverterMap[(int)LImage::Format::ARGB32][(int)LImage::Format::RGBA64] = convertARGB32ToRGBA64<false>;

    lImageConverterMap[(int)LImage::Format::ARGB32Premultiplied][(int)LImage::Format::Mono] = convertARGBPMToMono;
    lImageConverterMap[(int)LImage::Format::ARGB32Premultiplied][(int)LImage::Format::MonoLSB] = convertARGBPMToMono;
    lImageConverterMap[(int)LImage::Format::ARGB32Premultiplied][(int)LImage::Format::Indexed8] = convertARGBPMToIndexed8;
    lImageConverterMap[(int)LImage::Format::ARGB32Premultiplied][(int)LImage::Format::ABGR32Premultiplied] = convertARGBToABGR;

    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::RGB32] = convertRGB888ToRGB<false>;
    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::ARGB32] = convertRGB888ToRGB<false>;
    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::ARGB32Premultiplied] = convertRGB888ToRGB<false>;
    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::XBGR32] = convertRGB888ToRGB<true>;
    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::ABGR32] = convertRGB888ToRGB<true>;
    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::ABGR32Premultiplied] = convertRGB888ToRGB<true>;
    lImageConverterMap[(int)LImage::Format::RGB888][(int)LImage::Format::BGR888] = convertRGBSwapGeneric;

    lImageConverterMap[(int)LImage::Format::XBGR32][(int)LImage::Format::RGB32] = convertRGBAToRGB;
    lImageConverterMap[(int)LImage::Format::XBGR32][(int)LImage::Format::ARGB32] = convertRGBAToRGB;
    lImageConverterMap[(int)LImage::Format::XBGR32][(int)LImage::Format::ARGB32Premultiplied] = convertRGBAToARGB;
    lImageConverterMap[(int)LImage::Format::XBGR32][(int)LImage::Format::ABGR32] = convertPassthrough;
    lImageConverterMap[(int)LImage::Format::XBGR32][(int)LImage::Format::ABGR32Premultiplied] = convertPassthrough;

    lImageConverterMap[(int)LImage::Format::ABGR32][(int)LImage::Format::RGB32] = convertRGBAToRGB;
    lImageConverterMap[(int)LImage::Format::ABGR32][(int)LImage::Format::ARGB32] = convertRGBAToARGB;
    lImageConverterMap[(int)LImage::Format::ABGR32][(int)LImage::Format::XBGR32] = maskAlphaConverterRGBx;
    lImageConverterMap[(int)LImage::Format::ABGR32][(int)LImage::Format::A2BGR30Premultiplied] = convertARGBToA2RGB30<draw::PixelOrderBGR, true>;
    lImageConverterMap[(int)LImage::Format::ABGR32][(int)LImage::Format::A2RGB30Premultiplied] = convertARGBToA2RGB30<draw::PixelOrderRGB, true>;
    lImageConverterMap[(int)LImage::Format::ABGR32][(int)LImage::Format::RGBA64] = convertARGB32ToRGBA64<true>;

    lImageConverterMap[(int)LImage::Format::ABGR32Premultiplied][(int)LImage::Format::ARGB32Premultiplied] = convertRGBAToARGB;

    lImageConverterMap[(int)LImage::Format::BGR30][(int)LImage::Format::A2BGR30Premultiplied] = convertPassthrough;
    lImageConverterMap[(int)LImage::Format::BGR30][(int)LImage::Format::RGB30] = convertRGBSwapGeneric;
    lImageConverterMap[(int)LImage::Format::BGR30][(int)LImage::Format::A2RGB30Premultiplied] = convertRGBSwapGeneric;

    lImageConverterMap[(int)LImage::Format::A2BGR30Premultiplied][(int)LImage::Format::ARGB32] = convertA2RGB30PMToARGB<draw::PixelOrderBGR, false>;
    lImageConverterMap[(int)LImage::Format::A2BGR30Premultiplied][(int)LImage::Format::ABGR32] = convertA2RGB30PMToARGB<draw::PixelOrderBGR, true>;
    lImageConverterMap[(int)LImage::Format::A2BGR30Premultiplied][(int)LImage::Format::BGR30] = convertA2RGB30PMToRGB30<false>;
    lImageConverterMap[(int)LImage::Format::A2BGR30Premultiplied][(int)LImage::Format::RGB30] = convertA2RGB30PMToRGB30<true>;
    lImageConverterMap[(int)LImage::Format::A2BGR30Premultiplied][(int)LImage::Format::A2RGB30Premultiplied] = convertRGBSwapGeneric;

    lImageConverterMap[(int)LImage::Format::RGB30][(int)LImage::Format::BGR30] = convertRGBSwapGeneric;
    lImageConverterMap[(int)LImage::Format::RGB30][(int)LImage::Format::A2BGR30Premultiplied] = convertRGBSwapGeneric;
    lImageConverterMap[(int)LImage::Format::RGB30][(int)LImage::Format::A2RGB30Premultiplied] = convertPassthrough;

    lImageConverterMap[(int)LImage::Format::A2RGB30Premultiplied][(int)LImage::Format::ARGB32] = convertA2RGB30PMToARGB<draw::PixelOrderRGB, false>;
    lImageConverterMap[(int)LImage::Format::A2RGB30Premultiplied][(int)LImage::Format::ABGR32] = convertA2RGB30PMToARGB<draw::PixelOrderRGB, true>;
    lImageConverterMap[(int)LImage::Format::A2RGB30Premultiplied][(int)LImage::Format::BGR30] = convertA2RGB30PMToRGB30<true>;
    lImageConverterMap[(int)LImage::Format::A2RGB30Premultiplied][(int)LImage::Format::A2BGR30Premultiplied] = convertRGBSwapGeneric;
    lImageConverterMap[(int)LImage::Format::A2RGB30Premultiplied][(int)LImage::Format::RGB30] = convertA2RGB30PMToRGB30<false>;

    lImageConverterMap[(int)LImage::Format::Grayscale8][(int)LImage::Format::Indexed8] = convertGrayScale8ToIndexed8;
    lImageConverterMap[(int)LImage::Format::Alpha8][(int)LImage::Format::Indexed8] = convertAlpha8ToIndexed8;

    lImageConverterMap[(int)LImage::Format::RGBX64][(int)LImage::Format::RGBA64] = convertPassthrough;
    lImageConverterMap[(int)LImage::Format::RGBX64][(int)LImage::Format::RGBA64Premultiplied] = convertPassthrough;
    lImageConverterMap[(int)LImage::Format::RGBX64][(int)LImage::Format::Grayscale16] = convertRGBA64ToGray16;

    lImageConverterMap[(int)LImage::Format::RGBA64][(int)LImage::Format::ARGB32] = convertRGBA64ToARGB32<false>;
    lImageConverterMap[(int)LImage::Format::RGBA64][(int)LImage::Format::ABGR32] = convertRGBA64ToARGB32<true>;
    lImageConverterMap[(int)LImage::Format::RGBA64][(int)LImage::Format::RGBX64] = convertRGBA64ToRGBx64;
    lImageConverterMap[(int)LImage::Format::RGBA64][(int)LImage::Format::RGBA64Premultiplied] =  convertRGBA64ToRGBA64PM;

    lImageConverterMap[(int)LImage::Format::RGBA64Premultiplied][(int)LImage::Format::RGBX64] = convertRGBA64PMToRGBA64<true>;
    lImageConverterMap[(int)LImage::Format::RGBA64Premultiplied][(int)LImage::Format::RGBA64] = convertRGBA64PMToRGBA64<false>;
    lImageConverterMap[(int)LImage::Format::RGBA64Premultiplied][(int)LImage::Format::Grayscale16] = convertRGBA64ToGray16;

    lImageConverterMap[(int)LImage::Format::Grayscale16][(int)LImage::Format::RGBX64] = convertGray16ToRGBA64;
    lImageConverterMap[(int)LImage::Format::Grayscale16][(int)LImage::Format::RGBA64] = convertGray16ToRGBA64;
    lImageConverterMap[(int)LImage::Format::Grayscale16][(int)LImage::Format::RGBA64Premultiplied] = convertGray16ToRGBA64;

    lImageConverterMap[(int)LImage::Format::BGR888][(int)LImage::Format::RGB888] = convertRGBSwapGeneric;
    lImageConverterMap[(int)LImage::Format::BGR888][(int)LImage::Format::XBGR32] = convertRGB888ToRGB<false>;
    lImageConverterMap[(int)LImage::Format::BGR888][(int)LImage::Format::ABGR32] = convertRGB888ToRGB<false>;
    lImageConverterMap[(int)LImage::Format::BGR888][(int)LImage::Format::ABGR32Premultiplied] = convertRGB888ToRGB<false>;
}

void LImageConversions::convertGeneric(std::shared_ptr<LImage> dst,
                            const std::shared_ptr<LImage> src,
                            LFlags<LImageConvertFlag> flags)
{
    const draw::LPixelLayout *srcLayout = &draw::lPixelLayouts[(int)src->m_format];
    const draw::LPixelLayout *destLayout = &draw::lPixelLayouts[(int)dst->m_format];

    draw::FetchAndConvertPixelsFunc fetch = srcLayout->fetchToARGB32PM;
    draw::ConvertAndStorePixelsFunc store = destLayout->storeFromARGB32PM;

    if (!srcLayout->hasAlphaChannel && destLayout->storeFromRGB32) {
        // If the source doesn't have an alpha channel, we can use the faster storeFromRGB32 method.
        store = destLayout->storeFromRGB32;
    } else {
        // The drawhelpers do not mask the alpha value in RGB32, we want to here.
        if (src->m_format == LImage::Format::RGB32){
            fetch = fetchRGB32ToARGB32PM;
        }

        if (dst->m_format == LImage::Format::RGB32) {
            store = storeRGB32FromARGB32PM;
        }
    }

    if (srcLayout->hasAlphaChannel && !srcLayout->premultiplied &&
        !destLayout->hasAlphaChannel && destLayout->storeFromRGB32) {
        // Avoid unnecessary premultiply and unpremultiply when converting from unpremultiplied src format.
        fetch = draw::lPixelLayouts[(int)src->m_format + 1].fetchToARGB32PM;
        if (dst->m_format == LImage::Format::RGB32)
            store = storeRGB32FromARGB32;
        else
            store = destLayout->storeFromRGB32;
    }

    if(store != nullptr && fetch != nullptr){
        auto convertSegment = [=](int yStart, int yEnd) {
            uint32_t buf[2048];
            uint32_t *buffer = buf;
            const uint8_t *srcData = src->m_data + src->m_bytesPerLine * yStart;
            uint8_t *destData = dst->m_data + dst->m_bytesPerLine * yStart;

            LPoint<int> dither;
            LPoint<int> *ditherPtr = nullptr;
            if ((flags & LImageConvertFlag::PreferDither)
                && (flags & LImageConvertFlag::DitherMask) != LImageConvertFlag::ThresholdDither){
                ditherPtr = &dither;
            }

            for (int y = yStart; y < yEnd; ++y) {
                dither.m_y = y;
                uint32_t x = 0;
                while (x < src->m_width) {
                    dither.m_x = x;
                    int l = src->m_width - x;
                    if (destLayout->bpp == draw::BPP32){
                        buffer = reinterpret_cast<uint32_t *>(destData) + x;
                    }else{
                        l = std::min(l, 2048);
                    }
                    const uint32_t *ptr = fetch(buffer, srcData, x, l, nullptr, nullptr);
                    store(destData, ptr, x, l, nullptr, nullptr);
                    x += l;
                }
                srcData += src->m_bytesPerLine;
                destData += dst->m_bytesPerLine;
            }
        };
        convertSegment(0, src->m_height);
    }
}

void LImageConversions::convertGenericToRgb64(std::shared_ptr<LImage> dst,
                                   const std::shared_ptr<LImage> src,
                                   LFlags<LImageConvertFlag> flags)
{
    const draw::LPixelLayout *srcLayout = &draw::lPixelLayouts[(int)src->m_format];
    const draw::LPixelLayout *destLayout = &draw::lPixelLayouts[(int)dst->m_format];

    const draw::FetchAndConvertPixelsFunc64 fetch = srcLayout->fetchToRGBA64PM;
    const draw::ConvertAndStorePixelsFunc64 store = draw::lStoreFromRGBA64PM[(int)dst->m_format];

    auto convertSegment = [=](uint32_t yStart, uint32_t yEnd) {
        uint64_t buf[draw::BufferSize];
        uint64_t *buffer = buf;
        const uint8_t *srcData = src->m_data+ yStart * src->m_bytesPerLine;
        uint8_t *destData = dst->m_data + yStart * dst->m_bytesPerLine;
        for (uint32_t y = yStart; y < yEnd; ++y) {
            uint32_t x = 0;
            while (x < src->m_width) {
                int l = src->m_width - x;
                if (destLayout->bpp == draw::BPP64){
                    buffer = reinterpret_cast<uint64_t *>(destData) + x;
                }else{
                    l = std::min(l, draw::BufferSize);
                }
                const uint64_t *ptr = fetch(buffer, srcData, x, l, nullptr, nullptr);
                store(destData, ptr, x, l, nullptr, nullptr);
                x += l;
            }
            srcData += src->m_bytesPerLine;
            destData += dst->m_bytesPerLine;
        }
    };

    convertSegment(0, src->m_height);
}



void LImageConversions::ditherToMono(std::shared_ptr<LImage> dst,
                                     const std::shared_ptr<LImage> src,
                                     LFlags<LImageConvertFlag> flags,
                                     bool fromalpha)
{
    dst->m_colorTable.clear();
    dst->m_colorTable.push_back(0xffffffff);
    dst->m_colorTable.push_back(0xff000000);

    enum { Threshold, Ordered, Diffuse } dithermode;

    if (fromalpha) {
        if ((flags & LImageConvertFlag::AlphaDitherMask) == LImageConvertFlag::DiffuseAlphaDither){
            dithermode = Diffuse;
        }
        else if ((flags & LImageConvertFlag::AlphaDitherMask) == LImageConvertFlag::OrderedAlphaDither){
            dithermode = Ordered;
        }
        else{
            dithermode = Threshold;
        }
    } else {
        if ((flags & LImageConvertFlag::DitherMask) == LImageConvertFlag::ThresholdDither){
            dithermode = Threshold;
        }
        else if ((flags & LImageConvertFlag::DitherMask) == LImageConvertFlag::OrderedDither){
            dithermode = Ordered;
        }
        else{
            dithermode = Diffuse;
        }
    }

    int w = src->m_width;
    int h = src->m_height;
    int d = src->m_depth;
    uint8_t gray[256];                                // gray map for 8 bit images
    bool  use_gray = (d == 8);
    if (use_gray) {                                // make gray map
        if (fromalpha) {
            // Alpha 0x00 -> 0 pixels (white)
            // Alpha 0xFF -> 1 pixels (black)
            for (uint32_t i = 0; i < src->m_colorTable.size(); i++){
                gray[i] = (255 - (src->m_colorTable.at(i) >> 24));
            }
        } else {
            // Pixel 0x00 -> 1 pixels (black)
            // Pixel 0xFF -> 0 pixels (white)
            for (uint32_t i = 0; i < src->m_colorTable.size(); i++){
                gray[i] = LColor::toGray(src->m_colorTable.at(i));
            }
        }
    }

    uint8_t *dst_data = dst->m_data;
    lsizetype dst_bpl = dst->m_bytesPerLine;
    const uint8_t *src_data = src->m_data;
    lsizetype src_bpl = src->m_bytesPerLine;

    switch (dithermode) {
    case Diffuse: {
        std::vector<int> lineBuffer(w*2);

        int *line1 = lineBuffer.data();
        int *line2 = lineBuffer.data() + w;
        int bmwidth = (w+7)/8;

        int *b1, *b2;
        int wbytes = w * (d/8);
        const uint8_t *p = src->m_data;
        const uint8_t *end = p + wbytes;
        b2 = line2;
        if (use_gray) {                        // 8 bit image
            while (p < end){
                *b2++ = gray[*p++];
            }
        } else {                                // 32 bit image
            if (fromalpha) {
                while (p < end) {
                    *b2++ = 255 - (*(const uint32_t*)p >> 24);
                    p += 4;
                }
            } else {
                while (p < end) {
                    *b2++ = LColor::toGray(*(const uint32_t*)p);
                    p += 4;
                }
            }
        }
        for (int y=0; y<h; y++) {                        // for each scan line...
            int *tmp = line1; line1 = line2; line2 = tmp;
            bool not_last_line = y < h - 1;
            if (not_last_line) {                // calc. grayvals for next line
                p = src->m_data + (y+1)*src->m_bytesPerLine;
                end = p + wbytes;
                b2 = line2;
                if (use_gray) {                // 8 bit image
                    while (p < end){
                        *b2++ = gray[*p++];
                    }
                } else {                        // 24 bit image
                    if (fromalpha) {
                        while (p < end) {
                            *b2++ = 255 - (*(const uint32_t*)p >> 24);
                            p += 4;
                        }
                    } else {
                        while (p < end) {
                            *b2++ = LColor::toGray(*(const uint32_t*)p);
                            p += 4;
                        }
                    }
                }
            }

            int err;
            uint8_t *p = dst->m_data + y*dst->m_bytesPerLine;
            memset(p, 0, bmwidth);
            b1 = line1;
            b2 = line2;
            int bit = 7;
            for (int x=1; x<=w; x++) {
                if (*b1 < 128) {                // black pixel
                    err = *b1++;
                    *p |= 1 << bit;
                } else {                        // white pixel
                    err = *b1++ - 255;
                }
                if (bit == 0) {
                    p++;
                    bit = 7;
                } else {
                    bit--;
                }
                const int e7 = ((err * 7) + 8) >> 4;
                const int e5 = ((err * 5) + 8) >> 4;
                const int e3 = ((err * 3) + 8) >> 4;
                const int e1 = err - (e7 + e5 + e3);
                if (x < w){
                    *b1 += e7;                  // spread error to right pixel
                }
                if (not_last_line) {
                    b2[0] += e5;                // pixel below
                    if (x > 1){
                        b2[-1] += e3;           // pixel below left
                    }
                    if (x < w){
                        b2[1] += e1;            // pixel below right
                    }
                }
                b2++;
            }
        }
    } break;
    case Ordered: {

        memset(dst->m_data, 0, dst->m_nbytes);
        if (d == 32) {
            for (int i=0; i<h; i++) {
                const uint32_t *p = (const uint32_t *)src_data;
                const uint32_t *end = p + w;
                uint8_t *m = dst_data;
                int bit = 7;
                int j = 0;
                if (fromalpha) {
                    while (p < end) {
                        if ((*p++ >> 24) >= draw::lBayerMatrix[j++&15][i&15])
                            *m |= 1 << bit;
                        if (bit == 0) {
                            m++;
                            bit = 7;
                        } else {
                            bit--;
                        }
                    }
                } else {
                    while (p < end) {
                        if ((uint32_t)LColor::toGray(*p++) < draw::lBayerMatrix[j++&15][i&15])
                            *m |= 1 << bit;
                        if (bit == 0) {
                            m++;
                            bit = 7;
                        } else {
                            bit--;
                        }
                    }
                }
                dst_data += dst_bpl;
                src_data += src_bpl;
            }
        } else if (d == 8) {
            for (int i=0; i<h; i++) {
                const uint8_t *p = src_data;
                const uint8_t *end = p + w;
                uint8_t *m = dst_data;
                int bit = 7;
                int j = 0;
                while (p < end) {
                    if ((uint32_t)gray[*p++] < draw::lBayerMatrix[j++&15][i&15]){
                        *m |= 1 << bit;
                    }
                    if (bit == 0) {
                        m++;
                        bit = 7;
                    } else {
                        bit--;
                    }
                }
                dst_data += dst_bpl;
                src_data += src_bpl;
            }
        }
    } break;
    default: { // Threshold:
        memset(dst->m_data, 0, dst->m_nbytes);
        if (d == 32) {
            for (int i=0; i<h; i++) {
                const uint32_t *p = (const uint32_t *)src_data;
                const uint32_t *end = p + w;
                uint8_t *m = dst_data;
                int bit = 7;
                if (fromalpha) {
                    while (p < end) {
                        if ((*p++ >> 24) >= 128){
                            *m |= 1 << bit;        // Set mask "on"
                        }
                        if (bit == 0) {
                            m++;
                            bit = 7;
                        } else {
                            bit--;
                        }
                    }
                } else {
                    while (p < end) {
                        if (LColor::toGray(*p++) < 128){
                            *m |= 1 << bit;        // Set pixel "black"
                        }
                        if (bit == 0) {
                            m++;
                            bit = 7;
                        } else {
                            bit--;
                        }
                    }
                }
                dst_data += dst_bpl;
                src_data += src_bpl;
            }
        } else
            if (d == 8) {
                for (int i=0; i<h; i++) {
                    const uint8_t *p = src_data;
                    const uint8_t *end = p + w;
                    uint8_t *m = dst_data;
                    int bit = 7;
                    while (p < end) {
                        if (gray[*p++] < 128)
                            *m |= 1 << bit;                // Set mask "on"/ pixel "black"
                        if (bit == 0) {
                            m++;
                            bit = 7;
                        } else {
                            bit--;
                        }
                    }
                    dst_data += dst_bpl;
                    src_data += src_bpl;
                }
            }
    }
    }

    if (dst->m_format == LImage::Format::MonoLSB) {
        // need to swap bit order
        uint8_t *sl = dst->m_data;
        int bpl = (dst->m_width + 7) * dst->m_depth / 8;
        int pad = dst->m_bytesPerLine - bpl;
        for (uint32_t y=0; y<dst->m_height; ++y) {
            for (int x=0; x<bpl; ++x) {
                *sl = bitflip[*sl];
                ++sl;
            }
            sl += pad;
        }
    }

}

void LImageConversions::copy8bitPixels(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src)
{
    if (src->m_bytesPerLine == dest->m_bytesPerLine) {
        memcpy(dest->m_data, src->m_data, src->m_bytesPerLine * src->m_height);
    } else {
        const uint8_t *sdata = src->m_data;
        uint8_t *ddata = dest->m_data;
        for (uint32_t y = 0; y < src->m_height; ++y) {
            memcpy(ddata, sdata, src->m_width);
            sdata += src->m_bytesPerLine;
            ddata += dest->m_bytesPerLine;
        }
    }
}


void LImageConversions::swapBitOrder(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    dest->m_colorTable = src->m_colorTable;

    const uint8_t *src_data = src->m_data;
    const uint8_t *end = src->m_data + src->m_nbytes;
    uint8_t *dest_data = dest->m_data;
    while (src_data < end) {
        *dest_data = bitflip[*src_data];
        ++src_data;
        ++dest_data;
    }
}

void LImageConversions::convertMonoToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    std::vector<uint32_t> ctbl = src->m_colorTable;
    if (ctbl.size() > 2) {
        ctbl.resize(2);
    } else if (ctbl.size() < 2) {
        if (ctbl.size() == 0){
            ctbl.push_back(0xff000000);
        }
        ctbl.push_back(0xffffffff);
    }
    dest->m_colorTable = ctbl;
    dest->m_hasAlphaClut = src->m_hasAlphaClut;

    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;
    if (src->m_format == LImage::Format::Mono) {
        for (uint32_t y = 0; y < dest->m_height; y++) {
            uint8_t *p = dest_data;
            for (uint32_t x = 0; x < dest->m_width; x++){
                *p++ = (src_data[x>>3] >> (7 - (x & 7))) & 1;
            }
            src_data += src->m_bytesPerLine;
            dest_data += dest->m_bytesPerLine;
        }
    } else {
        for (uint32_t y = 0; y < dest->m_height; y++) {
            uint8_t *p = dest_data;
            for (uint32_t x = 0; x < dest->m_width; x++){
                *p++ = (src_data[x>>3] >> (x & 7)) & 1;
            }
            src_data += src->m_bytesPerLine;
            dest_data += dest->m_bytesPerLine;
        }
    }
}

void LImageConversions::convertMonoToX32(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    std::vector<uint32_t> colorTable = fixColorTable(src->m_colorTable, dest->m_format);

    // Default to black / white colors
    if (colorTable.size() < 2) {
        if (colorTable.size() == 0){
            colorTable.push_back(0xff000000);
        }
        colorTable.push_back(0xffffffff);
    }

    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;
    if (src->m_format == LImage::Format::Mono) {
        for (uint32_t y = 0; y < dest->m_height; y++) {
            uint32_t *p = (uint32_t *)dest_data;
            for (uint32_t x = 0; x < dest->m_width; x++){
                *p++ = colorTable.at((src_data[x>>3] >> (7 - (x & 7))) & 1);
            }

            src_data += src->m_bytesPerLine;
            dest_data += dest->m_bytesPerLine;
        }
    } else {
        for (uint32_t y = 0; y < dest->m_height; y++) {
            uint32_t *p = (uint32_t *)dest_data;
            for (uint32_t x = 0; x < dest->m_width; x++){
                *p++ = colorTable.at((src_data[x>>3] >> (x & 7)) & 1);
            }

            src_data += src->m_bytesPerLine;
            dest_data += dest->m_bytesPerLine;
        }
    }
}

void LImageConversions::convertXToMono(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag> flags)
{
    ditherToMono(dest, src, flags, false);
}

void LImageConversions::convertIndexed8ToX32(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    std::vector<uint32_t> colorTable = src->m_hasAlphaClut ? fixColorTable(src->m_colorTable, dest->m_format) : src->m_colorTable;
    if (colorTable.size() == 0) {
        colorTable.resize(256);
        for (int i=0; i<256; ++i){
            colorTable[i] = LColor::argbToUInt(i, i, i);
        }
    }
    if (colorTable.size() < 256) {
        int tableSize = colorTable.size();
        colorTable.resize(256);
        uint32_t fallbackColor = (dest->m_format == LImage::Format::RGB32) ? 0xff000000 : 0;
        for (int i=tableSize; i<256; ++i){
            colorTable[i] = fallbackColor;
        }
    }

    int w = src->m_width;
    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;
    const uint32_t *colorTablePtr = colorTable.data();
    for (uint32_t y = 0; y < src->m_height; y++) {
        uint32_t *p = reinterpret_cast<uint32_t *>(dest_data);
        const uint8_t *b = src_data;
        uint32_t *end = p + w;

        while (p < end){
            *p++ = colorTablePtr[*b++];
        }

        src_data += src->m_bytesPerLine;
        dest_data += dest->m_bytesPerLine;
    }
}

void LImageConversions::convertIndexed8ToGrayscale8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    uint8_t translate[256];
    const std::vector<uint32_t> &colors = src->m_colorTable;
    bool simpleCase = (colors.size() == 256);
    for (uint32_t i = 0; i < colors.size(); ++i) {
        uint8_t gray = LColor::toGray(colors[i]);
        translate[i] = gray;
        simpleCase = simpleCase && (gray == i);
    }

    if (simpleCase){
        copy8bitPixels(dest, src);
    }
    else {
        const uint8_t *sdata = src->m_data;
        uint8_t *ddata = dest->m_data;
        for (uint32_t y = 0; y < src->m_height; ++y) {
            for (uint32_t x = 0; x < src->m_width; ++x){
                ddata[x] = translate[sdata[x]];
            }
            sdata += src->m_bytesPerLine;
            ddata += dest->m_bytesPerLine;
        }
    }
}

void LImageConversions::convertIndexed8ToAlpha8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    uint8_t translate[256];
    const std::vector<uint32_t> &colors = src->m_colorTable;
    bool simpleCase = (colors.size() == 256);
    for (uint32_t i = 0; i < colors.size(); ++i) {
        uint8_t alpha = LColor::uintToAlpha(colors[i]);
        translate[i] = alpha;
        simpleCase = simpleCase && (alpha == i);
    }

    if (simpleCase)
        copy8bitPixels(dest, src);
    else {
        const uint8_t *sdata = src->m_data;
        uint8_t *ddata = dest->m_data;
        for (uint32_t y = 0; y < src->m_height; ++y) {
            for (uint32_t x = 0; x < src->m_width; ++x){
                ddata[x] = translate[sdata[x]];
            }
            sdata += src->m_bytesPerLine;
            ddata += dest->m_bytesPerLine;
        }
    }
}

void LImageConversions::convertRGBToIndexed8(std::shared_ptr<LImage> dst, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag> flags)
{
    bool    do_quant = (flags & LImageConvertFlag::DitherModeMask) == LImageConvertFlag::PreferDither
                    || src->m_format == LImage::Format::ARGB32;
    uint32_t alpha_mask = src->m_format == LImage::Format::RGB32 ? 0xff000000 : 0;

    const int tablesize = 997; // prime
    LRgbMap table[tablesize];
    int   pix=0;

    if (!dst->m_colorTable.empty()) {
        std::vector<uint32_t> ctbl = dst->m_colorTable;
        dst->m_colorTable.resize(256);
        // Preload palette into table.
        // Almost same code as pixel insertion below
        for (uint32_t i = 0; i < dst->m_colorTable.size(); ++i) {
            // Find in table...
            uint32_t p = ctbl.at(i) | alpha_mask;
            int hash = p % tablesize;
            for (;;) {
                if (table[hash].used) {
                    if (table[hash].rgb == p) {
                        // Found previous insertion - use it
                        break;
                    } else {
                        // Keep searching...
                        if (++hash == tablesize) hash = 0;
                    }
                } else {
                    // Insert into table at this unused position
                    dst->m_colorTable[pix] = p;
                    table[hash].pix = pix++;
                    table[hash].rgb = p;
                    table[hash].used = 1;
                    break;
                }
            }
        }
    }

    if ((flags & LImageConvertFlag::DitherModeMask) != LImageConvertFlag::PreferDither) {
        dst->m_colorTable.resize(256);
        const uint8_t *src_data = src->m_data;
        uint8_t *dest_data = dst->m_data;
        for (uint32_t y = 0; y < src->m_height; y++) {        // check if <= 256 colors
            const uint32_t *s = (const uint32_t *)src_data;
            uint8_t *b = dest_data;
            for (uint32_t x = 0; x < src->m_width; ++x) {
                uint32_t p = s[x] | alpha_mask;
                int hash = p % tablesize;
                for (;;) {
                    if (table[hash].used) {
                        if (table[hash].rgb == (p)) {
                            // Found previous insertion - use it
                            break;
                        } else {
                        // Keep searching...
                            if (++hash == tablesize){
                                hash = 0;
                            }
                        }
                    } else {
                        // Cannot be in table
                        if (pix == 256) {        // too many colors
                            do_quant = true;
                            // Break right out
                            x = src->m_width;
                            y = src->m_height;
                        } else {
                            // Insert into table at this unused position
                            dst->m_colorTable[pix] = p;
                            table[hash].pix = pix++;
                            table[hash].rgb = p;
                            table[hash].used = 1;
                        }
                        break;
                    }
                }
                *b++ = table[hash].pix;                // May occur once incorrectly
            }
            src_data += src->m_bytesPerLine;
            dest_data += dst->m_bytesPerLine;
        }
    }
    int numColors = do_quant ? 256 : pix;

    dst->m_colorTable.resize(numColors);

    if (do_quant) {                                // quantization needed

#define MAX_R 5
#define MAX_G 5
#define MAX_B 5
#define INDEXOF(r,g,b) (((r)*(MAX_G+1)+(g))*(MAX_B+1)+(b))

        for (int rc=0; rc<=MAX_R; rc++){                // build 6x6x6 color cube
            for (int gc=0; gc<=MAX_G; gc++){
                for (int bc=0; bc<=MAX_B; bc++){
                    dst->m_colorTable[INDEXOF(rc,gc,bc)] = LColor::argbToUInt(rc*255/MAX_R, gc*255/MAX_G, bc*255/MAX_B);
                }
            }
        }

        const uint8_t *src_data = src->m_data;
        uint8_t *dest_data = dst->m_data;
        if ((flags & LImageConvertFlag::DitherMask) == LImageConvertFlag::ThresholdDither) {
            for (uint32_t y = 0; y < src->m_height; y++) {
                const uint32_t *p = (const uint32_t *)src_data;
                const uint32_t *end = p + src->m_width;
                uint8_t *b = dest_data;

                while (p < end) {
#define DITHER(p,m) ((uint8_t) ((p * (m) + 127) / 255))
                        *b++ =
                            INDEXOF(
                                DITHER(LColor::uintToRed(*p), MAX_R),
                                DITHER(LColor::uintToGreen(*p), MAX_G),
                                DITHER(LColor::uintToBlue(*p), MAX_B)
                                );
#undef DITHER
                        p++;
                }
                src_data += src->m_bytesPerLine;
                dest_data += dst->m_bytesPerLine;
            }
        } else if ((flags & LImageConvertFlag::DitherMask) == LImageConvertFlag::DiffuseDither) {
            int* line1[3];
            int* line2[3];
            int* pv[3];
            std::vector<int> lineBuffer(src->m_width * 9);
            line1[0] = lineBuffer.data();
            line2[0] = lineBuffer.data() + src->m_width;
            line1[1] = lineBuffer.data() + src->m_width * 2;
            line2[1] = lineBuffer.data() + src->m_width * 3;
            line1[2] = lineBuffer.data() + src->m_width * 4;
            line2[2] = lineBuffer.data() + src->m_width * 5;
            pv[0] = lineBuffer.data() + src->m_width * 6;
            pv[1] = lineBuffer.data() + src->m_width * 7;
            pv[2] = lineBuffer.data() + src->m_width * 8;

            int endian = LIOMan::isSysLittleEndian()?0:1;
            for (uint32_t y = 0; y < src->m_height; y++) {
                const uint8_t* q = src_data;
                const uint8_t* q2 = y < src->m_height - 1 ? q + src->m_bytesPerLine : src->m_data;
                uint8_t *b = dest_data;
                for (int chan = 0; chan < 3; chan++) {
                    int *l1 = (y&1) ? line2[chan] : line1[chan];
                    int *l2 = (y&1) ? line1[chan] : line2[chan];
                    if (y == 0) {
                        for (uint32_t i = 0; i < src->m_width; i++)
                        l1[i] = q[i*4+chan+endian];
                    }
                    if (y+1 < src->m_height) {
                        for (uint32_t i = 0; i < src->m_width; i++)
                        l2[i] = q2[i*4+chan+endian];
                    }
                    // Bi-directional error diffusion
                    if (y&1) {
                        for (uint32_t x = 0; x < src->m_width; x++) {
                            int pix = std::max(std::min(5, (l1[x] * 5 + 128)/ 255), 0);
                            int err = l1[x] - pix * 255 / 5;
                            pv[chan][x] = pix;

                            // Spread the error around...
                            if (x + 1< src->m_width) {
                                l1[x+1] += (err*7)>>4;
                                l2[x+1] += err>>4;
                            }
                            l2[x]+=(err*5)>>4;
                            if (x>1){
                                l2[x-1]+=(err*3)>>4;
                            }
                        }
                    }
                    else{
                        for (uint32_t x = src->m_width; x-- > 0;) {
                            int pix = std::max(std::min(5, (l1[x] * 5 + 128)/ 255), 0);
                            int err = l1[x] - pix * 255 / 5;
                            pv[chan][x] = pix;

                            // Spread the error around...
                            if (x > 0) {
                                l1[x-1] += (err*7)>>4;
                                l2[x-1] += err>>4;
                            }
                            l2[x]+=(err*5)>>4;
                            if (x + 1 < src->m_width){
                                l2[x+1]+=(err*3)>>4;
                            }
                        }
                    }
                }
                if (endian) {
                    for (uint32_t x = 0; x < src->m_width; x++) {
                        *b++ = INDEXOF(pv[0][x],pv[1][x],pv[2][x]);
                    }
                } else {
                    for (uint32_t x = 0; x < src->m_width; x++) {
                        *b++ = INDEXOF(pv[2][x],pv[1][x],pv[0][x]);
                    }
                }
                src_data += src->m_bytesPerLine;
                dest_data += dst->m_bytesPerLine;
            }
        } else { // OrderedDither
            for (uint32_t y = 0; y < src->m_height; y++) {
                const uint32_t *p = (const uint32_t *)src_data;
                const uint32_t *end = p + src->m_width;
                uint8_t *b = dest_data;

                int x = 0;
                while (p < end) {
                    uint32_t d = draw::lBayerMatrix[y & 15][x & 15] << 8;

#define DITHER(p, d, m) ((uint8_t) ((((256 * (m) + (m) + 1)) * (p) + (d)) >> 16))
                        *b++ =
                            INDEXOF(
                                DITHER(LColor::uintToRed(*p), d, MAX_R),
                                DITHER(LColor::uintToGreen(*p), d, MAX_G),
                                DITHER(LColor::uintToBlue(*p), d, MAX_B)
                                );
#undef DITHER
                        p++;
                        x++;
                }
                src_data += src->m_bytesPerLine;
                dest_data += dst->m_bytesPerLine;
            }
        }

        if (src->m_format != LImage::Format::RGB32
            && src->m_format != LImage::Format::RGB16) {
            const int trans = 216;
            dst->m_colorTable[trans] = 0;
            std::shared_ptr<LImage> mask =
                std::make_shared<LImage>(src->m_width, src->m_height, LImage::Format::Mono);

            ditherToMono(mask, src, flags, true);
            uint8_t *dst_data = dst->m_data;
            const uint8_t *mask_data = mask->m_data;
            for (uint32_t y = 0; y < src->m_height; y++) {
                for (uint32_t x = 0; x < src->m_width ; x++) {
                    if (!(mask_data[x>>3] & (0x80 >> (x & 7)))){
                        dst_data[x] = trans;
                    }
                }
                mask_data += mask->m_bytesPerLine;
                dst_data += dst->m_bytesPerLine;
            }
            dst->m_hasAlphaClut = true;
        }

#undef MAX_R
#undef MAX_G
#undef MAX_B
#undef INDEXOF

    }
}

void LImageConversions::maskAlphaConverter(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (const uint32_t *)(src->m_data);
    uint32_t *dest_data = (uint32_t *)(dest->m_data);

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = *src_data | 0xff000000;
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertARGBToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag> flags)
{
    convertRGBToIndexed8(dest, src, flags);
}

void LImageConversions::convertARGBToXBGR(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (uint32_t *) (src->m_data);
    uint32_t *dest_data = (uint32_t *) (dest->m_data);

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = LColor::argbToAbgr(0xff000000 | *src_data);
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertARGBToABGR(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (uint32_t *) (src->m_data);
    uint32_t *dest_data = (uint32_t *) (dest->m_data);

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = LColor::argbToAbgr(*src_data);
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertARGBPMToMono(std::shared_ptr<LImage> dst,
                                            const std::shared_ptr<LImage> src,
                                            LFlags<LImageConvertFlag> flags)
{
    std::shared_ptr<LImage> tmp = std::make_shared<LImage>(src->m_width, src->m_height, LImage::Format::ARGB32);
    convertGeneric(tmp, src, LImageConvertFlag::AutoColor);
    ditherToMono(dst, tmp, flags, false);
}

void LImageConversions::convertARGBPMToIndexed8(std::shared_ptr<LImage> dst, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag> flags)
{
    std::shared_ptr<LImage> tmp = std::make_shared<LImage>(src->m_width, src->m_height, LImage::Format::ARGB32);
    convertGeneric(tmp, src, LImageConvertFlag::AutoColor);
    convertRGBToIndexed8(dst, tmp, flags);
}

template<draw::PixelOrder order, bool ABGR>
void LImageConversions::convertARGBToA2RGB30(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (uint32_t *) src->m_data;
    uint32_t *dest_data = (uint32_t *) dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            uint32_t c = *src_data;
            if (ABGR){
                c = LColor::abgrToArgb(c);
            }
            const uint32_t alpha = (LColor::uintToAlpha(c) >> 6) * 85;
            c = byteMul(c, alpha);
            if(order == draw::PixelOrderRGB){
                *dest_data = (LColor::argbToRgb30(c) & 0x3fffffff) | (alpha << 30);
            }else{
                *dest_data = (LColor::argbToBgr30(c) & 0x3fffffff) | (alpha << 30);
            }

            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

template<bool ABGR>
void LImageConversions::convertARGB32ToRGBA64(std::shared_ptr<LImage> dest,
                                              const std::shared_ptr<LImage> src,
                                              LFlags<LImageConvertFlag>)
{
    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;
    const draw::FetchAndConvertPixelsFunc64 fetch = draw::lPixelLayouts[(int)src->m_format + 1].fetchToRGBA64PM;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        fetch(reinterpret_cast<uint64_t *>(dest_data), src_data, 0, src->m_width, nullptr, nullptr);
        src_data += src->m_bytesPerLine;;
        dest_data += dest->m_bytesPerLine;
    }
}


void lConvertRgb888ToRgb32(uint32_t *dest_data, const uint8_t *src_data, int len)
{
    int pixel = 0;
    // prolog: align input to 32bit
    while ((luintptr(src_data) & 0x3) && pixel < len) {
        *dest_data = 0xff000000 | (src_data[0] << 16) | (src_data[1] << 8) | (src_data[2]);
        src_data += 3;
        ++dest_data;
        ++pixel;
    }

    // epilog: handle left over pixels
    for (; pixel < len; ++pixel) {
        *dest_data = 0xff000000 | (src_data[0] << 16) | (src_data[1] << 8) | (src_data[2]);
        src_data += 3;
        ++dest_data;
    }
}

void lConvertRgb888ToAbgr32(uint32_t *dest_data, const uint8_t *src_data, int len)
{
    int pixel = 0;
    // prolog: align input to 32bit
    while ((luintptr(src_data) & 0x3) && pixel < len) {
        *dest_data = LColor::argbToAbgr(0xff000000 | (src_data[0] << 16) | (src_data[1] << 8) | (src_data[2]));
        src_data += 3;
        ++dest_data;
        ++pixel;
    }

    // epilog: handle left over pixels
    for (; pixel < len; ++pixel) {
        *dest_data = LColor::argbToAbgr(0xff000000 | (src_data[0] << 16) | (src_data[1] << 8) | (src_data[2]));
        src_data += 3;
        ++dest_data;
    }
}


typedef void (*Rgb888ToRgbConverter)(uint32_t *dst, const uint8_t *src, int len);

template<bool rgbx>
void LImageConversions::convertRGB888ToRGB(std::shared_ptr<LImage> dest,
                                           const std::shared_ptr<LImage> src,
                                           LFlags<LImageConvertFlag>)
{
    const uint8_t *src_data = (uint8_t *) src->m_data;
    uint32_t *dest_data = (uint32_t *) dest->m_data;

    Rgb888ToRgbConverter line_converter;
    if(rgbx){
        line_converter = lConvertRgb888ToAbgr32;
    }else{
        line_converter = lConvertRgb888ToRgb32;
    }

    for (uint32_t i = 0; i < src->m_height; ++i) {
        line_converter(dest_data, src_data, src->m_width);
        src_data += src->m_bytesPerLine;
        dest_data = (uint32_t *)((uint8_t*)dest_data + dest->m_bytesPerLine);
    }
}

void LImageConversions::convertRGBSwapGeneric(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const draw::RbSwapFunc func = draw::lPixelLayouts[(int)src->m_format].rbSwap;

    const lsizetype sbpl = src->m_bytesPerLine;
    const lsizetype dbpl = dest->m_bytesPerLine;
    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        func(dest_data, src_data, src->m_width);

        src_data += sbpl;
        dest_data += dbpl;
    }
}

void LImageConversions::convertRGBAToRGB(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (const uint32_t *)src->m_data;
    uint32_t *dest_data = (uint32_t *)dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = LColor::abgrToArgb(*src_data) | 0xff000000;
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertRGBAToARGB(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (const uint32_t *)src->m_data;
    uint32_t *dest_data = (uint32_t *)dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = LColor::abgrToArgb(*src_data);
            //std::cout << *src_data << "," <<  *dest_data << " ";
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertPassthrough(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{

    const int src_bpl = src->m_bytesPerLine;
    const int dest_bpl = dest->m_bytesPerLine;
    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        memcpy(dest_data, src_data, src_bpl);
        src_data += src_bpl;
        dest_data += dest_bpl;
    }
}

void LImageConversions::maskAlphaConverterRGBx(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag> flags)
{
    //For little endian
    return maskAlphaConverter(dest, src, flags);
    //Todo consider bigger endian
}

void LImageConversions::convertGrayScale8ToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    copy8bitPixels(dest, src);
    dest->m_colorTable = defaultGrayTable();
}

void LImageConversions::convertAlpha8ToIndexed8(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    copy8bitPixels(dest, src);
    dest->m_colorTable = defaultAlphaTable();
}

void LImageConversions::convertRGBA64ToGray16(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const lsizetype sbpl = src->m_bytesPerLine;
    const lsizetype dbpl = dest->m_bytesPerLine;
    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint64_t *src_line = reinterpret_cast<const uint64_t *>(src_data);
        uint16_t *dest_line = reinterpret_cast<uint16_t *>(dest_data);
        for (uint32_t j = 0; j < src->m_width; ++j) {
            uint64_t s = LColor::unpremultiply(src_line[j]);
            dest_line[j] = LColor::toGray(s);
        }
        src_data += sbpl;
        dest_data += dbpl;
    }
}

std::vector<uint32_t> LImageConversions::defaultGrayTable()
{
    std::vector<uint32_t> ret;
    for (int i = 0; i < 256; ++i) {
        ret[i] = LColor::argbToUInt(i, i, i);
    }
    return ret;
}

std::vector<uint32_t> LImageConversions::defaultAlphaTable()
{
    std::vector<uint32_t> ret;
    for (int i = 0; i < 256; ++i) {
        ret[i] = LColor::argbToUInt(0, 0, 0, i);
    }
    return ret;
}


template<draw::PixelOrder order, bool ABGR>
void LImageConversions::convertA2RGB30PMToARGB(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (uint32_t *) src->m_data;
    uint32_t *dest_data = (uint32_t *) dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = draw::lConvertA2rgb30ToArgb32<order>(LColor::unpremultiplyRgb30(*src_data));
            if (ABGR){
                *dest_data = LColor::argbToAbgr(*dest_data);
            }
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

template<bool rgbswap>
void LImageConversions::convertA2RGB30PMToRGB30(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 2) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 2) - dest->m_width;
    const uint32_t *src_data = (uint32_t *) src->m_data;
    uint32_t *dest_data = (uint32_t *) dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint32_t *end = src_data + src->m_width;
        while (src_data < end) {
            const uint32_t p = 0xc0000000 | LColor::unpremultiplyRgb30(*src_data);
            *dest_data = (rgbswap) ? lRgbSwapRgb30(p) : p;
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }

}

template<bool ABGR>
void LImageConversions::convertRGBA64ToARGB32(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const uint8_t *srcData = src->m_data;
    uint8_t *destData = dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        uint32_t *d = reinterpret_cast<uint32_t *>(destData);
        const uint64_t *s = reinterpret_cast<const uint64_t *>(srcData);
        draw::lConvertRGBA64ToARGB32<ABGR>(d, s, src->m_width);
        srcData += src->m_bytesPerLine;
        destData += dest->m_bytesPerLine;
    }
}

void LImageConversions::convertRGBA64ToRGBx64(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 3) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 3) - dest->m_width;
    const uint64_t *src_data = reinterpret_cast<const uint64_t *>(src->m_data);
    uint64_t *dest_data = reinterpret_cast<uint64_t *>(dest->m_data);

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint64_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = *src_data;
            *dest_data = LColor::setAlpha16(*dest_data, 65535);
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertRGBA64ToRGBA64PM(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 3) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 3) - dest->m_width;
    const uint64_t *src_data = reinterpret_cast<const uint64_t *>(src->m_data);
    uint64_t *dest_data = reinterpret_cast<uint64_t *>(dest->m_data);

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint64_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = LColor::premultiply(*src_data);
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

template<bool MaskAlpha>
void LImageConversions::convertRGBA64PMToRGBA64(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const int src_pad = (src->m_bytesPerLine >> 3) - src->m_width;
    const int dest_pad = (dest->m_bytesPerLine >> 3) - dest->m_width;
    const uint64_t *src_data = reinterpret_cast<const uint64_t *>(src->m_data);
    uint64_t *dest_data = reinterpret_cast<uint64_t *>(dest->m_data);

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint64_t *end = src_data + src->m_width;
        while (src_data < end) {
            *dest_data = LColor::unpremultiply(*src_data);
            if (MaskAlpha){
                LColor::setAlpha16(*dest_data, 65535);
            }
            ++src_data;
            ++dest_data;
        }
        src_data += src_pad;
        dest_data += dest_pad;
    }
}

void LImageConversions::convertGray16ToRGBA64(std::shared_ptr<LImage> dest, const std::shared_ptr<LImage> src, LFlags<LImageConvertFlag>)
{
    const lsizetype sbpl = src->m_bytesPerLine;
    const lsizetype dbpl = dest->m_bytesPerLine;
    const uint8_t *src_data = src->m_data;
    uint8_t *dest_data = dest->m_data;

    for (uint32_t i = 0; i < src->m_height; ++i) {
        const uint16_t *src_line = reinterpret_cast<const uint16_t *>(src_data);
        uint64_t *dest_line = reinterpret_cast<uint64_t *>(dest_data);
        for (uint32_t j = 0; j < src->m_width; ++j) {
            uint16_t s = src_line[j];
            dest_line[j] = LColor::argb16ToUInt64(s, s, s, 0xFFFF);
        }
        src_data += sbpl;
        dest_data += dbpl;
    }
}

LK_NAMESPACE_END
