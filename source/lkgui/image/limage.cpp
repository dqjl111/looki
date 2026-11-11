#include "limage.h"
#include "lkgui/paint/lcolor.h"
#include <stdint.h>
#include "lkcore/ldebug.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/paint/lrasterpaintengine.h"
#include "lkgui/image/limagewriter.h"
#include "lkgui/image/limagereader.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/paint/ldrawhelper.h"
#include "lkgui/image/limageconversions.h"
#include "lkgui/text/lfont.h"
#include <algorithm>
#include <limits>

LK_NAMESPACE_BEGIN
const static LPixelFormat pixelformats[] = {
    //LImage::Format::Invalid:
    LPixelFormat(),
    //LImage::Format::Mono:
    LPixelFormat(pixfmt::Indexed,
                 /*RED*/            1,
                 /*GREEN*/          0,
                 /*BLUE*/           0,
                 /*FOURTH*/         0,
                 /*FIFTH*/          0,
                 /*ALPHA*/          0,
                 /*ALPHA USAGE*/    pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/ pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/  pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/ pixfmt::UnsignedByte,
                 /*BYTE ORDER*/     pixfmt::CurrentSystemEndian),
    //LImage::Format::MonoLSB:
    LPixelFormat(pixfmt::Indexed,
                 /*RED*/            1,
                 /*GREEN*/          0,
                 /*BLUE*/           0,
                 /*FOURTH*/         0,
                 /*FIFTH*/          0,
                 /*ALPHA*/          0,
                 /*ALPHA USAGE*/    pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/ pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/  pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/ pixfmt::UnsignedByte,
                 /*BYTE ORDER*/     pixfmt::CurrentSystemEndian),
    //LImage::Format::Indexed8:
    LPixelFormat(pixfmt::Indexed,
                 /*RED*/            8,
                 /*GREEN*/          0,
                 /*BLUE*/           0,
                 /*FOURTH*/         0,
                 /*FIFTH*/          0,
                 /*ALPHA*/          0,
                 /*ALPHA USAGE*/    pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/ pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/  pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/ pixfmt::UnsignedByte,
                 /*BYTE ORDER*/     pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB32:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::ARGB32:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::ARGB32Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB16:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                5,
                 /*GREEN*/              6,
                 /*BLUE*/               5,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::ARGB8565_Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                5,
                 /*GREEN*/              6,
                 /*BLUE*/               5,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB666:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                6,
                 /*GREEN*/              6,
                 /*BLUE*/               6,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::ARGB6666_Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                6,
                 /*GREEN*/              6,
                 /*BLUE*/               6,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              6,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB555:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                5,
                 /*GREEN*/              5,
                 /*BLUE*/               5,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::ARGB8555Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                5,
                 /*GREEN*/              5,
                 /*BLUE*/               5,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB888:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB444:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                4,
                 /*GREEN*/              4,
                 /*BLUE*/               4,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::ARGB4444Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                4,
                 /*GREEN*/              4,
                 /*BLUE*/               4,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              4,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGBX8888:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGBA8888:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGBA8888Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::BGR30:
    LPixelFormat(pixfmt::BGR,
                 /*RED*/                10,
                 /*GREEN*/              10,
                 /*BLUE*/               10,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              2,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::A2BGR30Premultiplied:
    LPixelFormat(pixfmt::BGR,
                 /*RED*/                10,
                 /*GREEN*/              10,
                 /*BLUE*/               10,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              2,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGB30:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                10,
                 /*GREEN*/              10,
                 /*BLUE*/               10,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              2,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::A2RGB30Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                10,
                 /*GREEN*/              10,
                 /*BLUE*/               10,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              2,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedInteger,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::Alpha8:
    LPixelFormat(pixfmt::Alpha,
                 /*First*/              0,
                 /*SECOND*/             0,
                 /*THIRD*/              0,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              8,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::Grayscale8:
    LPixelFormat(pixfmt::Grayscale,
                 /*GRAY*/               8,
                 /*SECOND*/             0,
                 /*THIRD*/              0,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGBX64:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                16,
                 /*GREEN*/              16,
                 /*BLUE*/               16,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              16,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGBA64:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                16,
                 /*GREEN*/              16,
                 /*BLUE*/               16,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              16,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::RGBA64Premultiplied:
    LPixelFormat(pixfmt::RGB,
                 /*RED*/                16,
                 /*GREEN*/              16,
                 /*BLUE*/               16,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              16,
                 /*ALPHA USAGE*/       pixfmt::UsesAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtEnd,
                 /*PREMULTIPLIED*/     pixfmt::Premultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::Grayscale16:
    LPixelFormat(pixfmt::Grayscale,
                 /*GRAY*/               16,
                 /*SECOND*/             0,
                 /*THIRD*/              0,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedShort,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
    //LImage::Format::BGR888:
    LPixelFormat(pixfmt::BGR,
                 /*RED*/                8,
                 /*GREEN*/              8,
                 /*BLUE*/               8,
                 /*FOURTH*/             0,
                 /*FIFTH*/              0,
                 /*ALPHA*/              0,
                 /*ALPHA USAGE*/       pixfmt::IgnoresAlpha,
                 /*ALPHA POSITION*/    pixfmt::AtBeginning,
                 /*PREMULTIPLIED*/     pixfmt::NotPremultiplied,
                 /*INTERPRETATION*/    pixfmt::UnsignedByte,
                 /*BYTE ORDER*/        pixfmt::CurrentSystemEndian),
};

inline LImage::Format lAlphaVersion(LImage::Format format)
{
    switch (format) {
    case LImage::Format::RGB16:{
        return LImage::Format::ARGB8565Premultiplied;
    }
    case LImage::Format::RGB555:{
        return LImage::Format::ARGB8555Premultiplied;
    }
    case LImage::Format::RGB666:{
        return LImage::Format::ARGB6666Premultiplied;
    }
    case LImage::Format::RGB444:{
        return LImage::Format::ARGB4444Premultiplied;
    }
    case LImage::Format::XBGR32:{
        return LImage::Format::ABGR32Premultiplied;
    }
    case LImage::Format::BGR30:{
        return LImage::Format::A2BGR30Premultiplied;
    }
    case LImage::Format::RGB30:{
        return LImage::Format::A2RGB30Premultiplied;
    }
    case LImage::Format::RGBX64:{
        return LImage::Format::RGBA64Premultiplied;
    }
    default:
        break;
    }
    return LImage::Format::ARGB32Premultiplied;
}

inline bool lHighColorPrecision(LImage::Format format, bool opaque = false)
{
    // Formats with higher color precision than ARGB32_Premultiplied.
    switch (format) {
    case LImage::Format::ARGB32:
    case LImage::Format::ABGR32:{
        return !opaque;
    }
    case LImage::Format::BGR30:
    case LImage::Format::RGB30:
    case LImage::Format::A2BGR30Premultiplied:
    case LImage::Format::A2RGB30Premultiplied:
    case LImage::Format::RGBX64:
    case LImage::Format::RGBA64:
    case LImage::Format::RGBA64Premultiplied:
    case LImage::Format::Grayscale16:{
        return true;
    }
    default:{
        break;
    }
    }
    return false;
}

#  define LK_XFORM_TYPE_MSBFIRST 0
#  define LK_XFORM_TYPE_LSBFIRST 1


#undef IWX_MSB
#define IWX_MSB(b)        if (trigx < maxws && trigy < maxhs) {                              \
if (*(sptr+sbpl*(trigy>>12)+(trigx>>15)) &                      \
    (1 << (7-((trigx>>12)&7))))                              \
        *dptr |= b;                                              \
}                                                              \
    trigx += m11;                                                      \
    trigy += m12;
// END OF MACRO
#undef IWX_LSB
#define IWX_LSB(b)        if (trigx < maxws && trigy < maxhs) {                              \
if (*(sptr+sbpl*(trigy>>12)+(trigx>>15)) &                      \
    (1 << ((trigx>>12)&7)))                              \
        *dptr |= b;                                              \
}                                                              \
    trigx += m11;                                                      \
    trigy += m12;
// END OF MACRO
#undef IWX_PIX
#define IWX_PIX(b)        if (trigx < maxws && trigy < maxhs) {                              \
if ((*(sptr+sbpl*(trigy>>12)+(trigx>>15)) &              \
     (1 << (7-((trigx>>12)&7)))) == 0)                      \
        *dptr &= ~b;                                              \
}                                                              \
    trigx += m11;                                                      \
    trigy += m12;
// END OF MACRO
bool lXFormHelper(const LTransform2D &trueMat, int xoffset, int type, int depth,
                     uint8_t *dptr, int dbpl, int p_inc, int dHeight,
                     const uint8_t *sptr, int sbpl, int sWidth, int sHeight)
{
    int m11 = int(trueMat.m11()*4096.0);
    int m12 = int(trueMat.m12()*4096.0);
    int m21 = int(trueMat.m21()*4096.0);
    int m22 = int(trueMat.m22()*4096.0);
    int dx  = std::lround(trueMat.dx()*4096.0);
    int dy  = std::lround(trueMat.dy()*4096.0);

    int m21ydx = dx + (xoffset<<16) + (m11 + m21) / 2;
    int m22ydy = dy + (m12 + m22) / 2;
    uint32_t trigx;
    uint32_t trigy;
    uint32_t maxws = sWidth<<12;
    uint32_t maxhs = sHeight<<12;

    for (int y=0; y<dHeight; y++) {                // for each target scanline
        trigx = m21ydx;
        trigy = m22ydy;
        uint8_t *maxp = dptr + dbpl;
        if (depth != 1) {
            switch (depth) {
            case 8:                                // 8 bpp transform
                while (dptr < maxp) {
                    if (trigx < maxws && trigy < maxhs){
                        *dptr = *(sptr+sbpl*(trigy>>12)+(trigx>>12));
                    }
                    trigx += m11;
                    trigy += m12;
                    dptr++;
                }
                break;

            case 16:                        // 16 bpp transform
                while (dptr < maxp) {
                    if (trigx < maxws && trigy < maxhs){
                        *((uint16_t*)dptr) = *((const uint16_t *)(sptr+sbpl*(trigy>>12) +
                                                                 ((trigx>>12)<<1)));
                    }
                    trigx += m11;
                    trigy += m12;
                    dptr++;
                    dptr++;
                }
                break;

            case 24:                        // 24 bpp transform
                while (dptr < maxp) {
                    if (trigx < maxws && trigy < maxhs) {
                        const uint8_t *p2 = sptr+sbpl*(trigy>>12) + ((trigx>>12)*3);
                        dptr[0] = p2[0];
                        dptr[1] = p2[1];
                        dptr[2] = p2[2];
                    }
                    trigx += m11;
                    trigy += m12;
                    dptr += 3;
                }
                break;

            case 32:                        // 32 bpp transform
                while (dptr < maxp) {
                    if (trigx < maxws && trigy < maxhs)
                        *((uint32_t*)dptr) = *((const uint32_t *)(sptr+sbpl*(trigy>>12) +
                                                             ((trigx>>12)<<2)));
                    trigx += m11;
                    trigy += m12;
                    dptr += 4;
                }
                break;

            default: {
                return false;
            }
            }
        } else  {
            switch (type) {
            case LK_XFORM_TYPE_MSBFIRST:{
                while (dptr < maxp) {
                    IWX_MSB(128);
                    IWX_MSB(64);
                    IWX_MSB(32);
                    IWX_MSB(16);
                    IWX_MSB(8);
                    IWX_MSB(4);
                    IWX_MSB(2);
                    IWX_MSB(1);
                    dptr++;
                }
                break;
            }
            case LK_XFORM_TYPE_LSBFIRST:{
                while (dptr < maxp) {
                    IWX_LSB(1);
                    IWX_LSB(2);
                    IWX_LSB(4);
                    IWX_LSB(8);
                    IWX_LSB(16);
                    IWX_LSB(32);
                    IWX_LSB(64);
                    IWX_LSB(128);
                    dptr++;
                }
                break;
            }
            }
        }
        m21ydx += m21;
        m22ydy += m22;
        dptr += p_inc;
    }
    return true;
}
#undef IWX_MSB
#undef IWX_LSB
#undef IWX_PIX


LImage::LImage()
{
    m_dpmx =(LFont::defaultDpiX() * 100 / 2.54);
    m_dpmy =(LFont::defaultDpiY() * 100 / 2.54);
}

LImage::LImage(const LSize<int> &size, Format format)
{
    m_dpmx =(LFont::defaultDpiX() * 100 / 2.54);
    m_dpmy =(LFont::defaultDpiY() * 100 / 2.54);
    create(size, format);
}

LImage::LImage(int width, int height, Format format)
    : LImage(LSize<int>(width, height), format)
{
}

LImage::LImage(const std::vector<uint8_t>& data, int width, int height, Format format)
{
    m_dpmx =(LFont::defaultDpiX() * 100 / 2.54);
    m_dpmy =(LFont::defaultDpiY() * 100 / 2.54);
    create(const_cast<uint8_t*>(data.data()), width, height, 0, format, false, data.size(), true);
}

LImage::LImage(const uint8_t *data, int width, int height, Format format, bool ownData)
{
    m_dpmx =(LFont::defaultDpiX() * 100 / 2.54);
    m_dpmy =(LFont::defaultDpiY() * 100 / 2.54);
    if(data != nullptr){
        create(const_cast<uint8_t*>(data), width, height, 0, format, true, -1, ownData);
    }else{
        create(LSize<int>(width, height), format);
    }
}

LImage::LImage(uint8_t *data, int width, int height, Format format, bool ownData)
{
    m_dpmx =(LFont::defaultDpiX() * 100 / 2.54);
    m_dpmy =(LFont::defaultDpiY() * 100 / 2.54);
    if(data != nullptr){
        create(data, width, height, 0, format, false, -1, ownData);
    }
}

LImage::LImage(const uint8_t *data, int width, int height, int bytesPerLine, Format format, bool ownData)
{
    if(data != nullptr){
        create(const_cast<uint8_t*>(data), width, height, bytesPerLine, format, false, -1, ownData);
    }else{
        create(LSize<int>(width, height), format);
    }
}

LImage::LImage(uint8_t *data, int width, int height, int bytesPerLine, Format format, bool ownData)
{
    if(data != nullptr){
        create(data, width, height, bytesPerLine, format, false, -1, ownData);
    }else{
        create(LSize<int>(width, height), format);
    }
}


LImage::~LImage()
{
    if (m_data && m_ownData){
        delete[] m_data;
    }
    m_data = nullptr;
}

int LImage::depthForFormat(Format format)
{
    int depth = 0;
    switch(format) {
    case Format::Mono:
    case Format::MonoLSB:{
        depth = 1;
        break;
    }
    case Format::Indexed8:
    case Format::Alpha8:
    case Format::Grayscale8:{
        depth = 8;
        break;
    }
    case Format::RGB32:
    case Format::ARGB32:
    case Format::ARGB32Premultiplied:
    case Format::XBGR32:
    case Format::ABGR32:
    case Format::ABGR32Premultiplied:
    case Format::BGR30:
    case Format::A2BGR30Premultiplied:
    case Format::RGB30:
    case Format::A2RGB30Premultiplied:{
        depth = 32;
        break;
    }
    case Format::RGB555:
    case Format::RGB16:
    case Format::RGB444:
    case Format::ARGB4444Premultiplied:
    case Format::Grayscale16:{
        depth = 16;
        break;
    }
    case Format::RGB666:
    case Format::ARGB6666Premultiplied:
    case Format::ARGB8565Premultiplied:
    case Format::ARGB8555Premultiplied:
    case Format::RGB888:
    case Format::BGR888:{
        depth = 24;
        break;
    }
    case Format::RGBX64:
    case Format::RGBA64:
    case Format::RGBA64Premultiplied:{
        depth = 64;
        break;
    }
    default:{
        break;
    }
    }
    return depth;

}

bool LImage::valid(uint32_t x, uint32_t y) const
{
    return x < m_width && y < m_height;
}

int LImage::pixelIndex(uint32_t x, uint32_t y) const
{
    if(m_data == nullptr){
        return -1;
    }

    if ( x >= m_width || y >= m_height) {
        lWarn() << "LImage::pixel: coordinate out of range (" << x << "," << y << ").";
        return -1;
    }

    const uint8_t *s = m_data + y * m_bytesPerLine;

    int index = -1;
    switch (m_format) {
    case Format::Mono:{
        index = (*(s + (x >> 3)) >> (~x & 7)) & 1;
        break;
    }
    case Format::MonoLSB:{
        index = (*(s + (x >> 3)) >> (x & 7)) & 1;
        break;
    }
    case Format::Indexed8:{
        index = s[x];
        break;
    }
    default:{
        index = -1;
        break;
    }
    }
    return index;
}

uint32_t LImage::pixel(uint32_t x, uint32_t y) const
{
    if(m_data == nullptr){
        return 0;
    }

    if(!valid(x, y)){
        return 0;
    }

    int index = pixelIndex(x, y);
    if(index >=0){
        if (index >= 0) {    // Indexed format
            if (index >= (int)m_colorTable.size()) {
                lWarn() << "LImage::pixel: color table index out of range: " << index;
                return 0;
            }
            return m_colorTable[index];
        }
    }

    const uint8_t *s = m_data + y * m_bytesPerLine;

    switch (m_format) {
    case Format::RGB32:{
        return 0xff000000 | reinterpret_cast<const uint32_t *>(s)[x];
    }
    case Format::ARGB32: // Keep old behaviour.
    case Format::ARGB32Premultiplied:
        return reinterpret_cast<const uint32_t *>(s)[x];
    case Format::XBGR32:
    case Format::ABGR32: // Match ARGB32 behavior.
    case Format::ABGR32Premultiplied:{
        return LColor::abgrToArgb(reinterpret_cast<const uint32_t *>(s)[x]);
    }
    case Format::BGR30:
    case Format::A2BGR30Premultiplied:{
        return LColor::rgb30ToArgb(reinterpret_cast<const uint32_t *>(s)[x]);
    }
    case Format::RGB30:
    case Format::A2RGB30Premultiplied:{
        return LColor::rgb30ToArgb(reinterpret_cast<const uint32_t *>(s)[x]);
    }
    case Format::RGB16:{
        return LColor::rgb16ToArgb(reinterpret_cast<const uint16_t *>(s)[x]);
    }
    case Format::RGBX64:
    case Format::RGBA64: // Match ARGB32 behavior.
    case Format::RGBA64Premultiplied:{
        return LColor::argb64ToArgb(reinterpret_cast<const uint64_t *>(s)[x]);
    }
    default:
        break;
    }
    return 0;
}

void LImage::setPixel(uint32_t x, uint32_t y, uint32_t value)
{
    if(m_data == nullptr){
        return;
    }

    if(!valid(x, y)){
        return;
    }

    uint8_t *s = m_data + y * m_bytesPerLine;

    int index = pixelIndex(x, y);
    if(index >=0){
        switch(m_format) {
        case Format::Mono:
        case Format::MonoLSB:{
            if (value > 1) {
                lWarn() << "LImage::setPixel: Index out of range: " <<  value;
            } else if (m_format == Format::MonoLSB) {
                if (value==0){
                    *(s + (x >> 3)) &= ~(1 << (x & 7));
                }
                else{
                    *(s + (x >> 3)) |= (1 << (x & 7));
                }
            } else {
                if (value==0){
                    *(s + (x >> 3)) &= ~(1 << (7-(x & 7)));
                }
                else{
                    *(s + (x >> 3)) |= (1 << (7-(x & 7)));
                }
            }
            return;
        }
        case Format::Indexed8:{
            if (value >= (uint32_t)m_colorTable.size()) {
                return;
            }
            s[x] = value;
            return;
        }
        default:{
            //Never reach
            return;
        }
        }
    }

    switch(m_format) {
    case Format::RGB32:{
        //make sure alpha is 255, we depend on it in qdrawhelper for cases
        // when image is set as a texture pattern on a qbrush
        ((uint32_t *)s)[x] = 0xff000000 | value;
        return;
    }
    case Format::ARGB32:
    case Format::ARGB32Premultiplied:{
        ((uint32_t *)s)[x] = value;
        return;
    }
    case Format::RGB16:{
        ((uint16_t *)s)[x] = LColor::argbToRgb16(LColor::unpremultiply(value));
        return;
    }
    case Format::XBGR32:{
        ((uint32_t *)s)[x] = LColor::argbToAbgr(0xff000000 | value);
        return;
    }
    case Format::ABGR32:
    case Format::ABGR32Premultiplied:{
        ((uint32_t *)s)[x] = LColor::argbToAbgr(value);
        return;
    }
    case Format::BGR30:{
        ((uint32_t *)s)[x] = LColor::argbToBgr30(value);
        return;
    }
    case Format::A2BGR30Premultiplied:{
        ((uint32_t *)s)[x] = LColor::argbToA2bgr30(value);
        return;
    }
    case Format::RGB30:{
        ((uint32_t *)s)[x] = LColor::argbToRgb30(value);
        return;
    }
    case Format::A2RGB30Premultiplied:{
        ((uint32_t *)s)[x] = LColor::argbToA2rgb30(value);
        return;
    }
    default:
        break;
    }
}

double LImage::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

void LImage::setDevicePixelRatio(double scaleFactor)
{
    m_devicePixelRatio = scaleFactor;
}

void LImage::fill(uint32_t pixel)
{
    if(m_data == nullptr){
        return;
    }

    if (m_depth == 1 || m_depth == 8) {
        int w = m_width;
        if (m_depth == 1) {
            if (pixel & 1){
                pixel = 0xffffffff;
            }else{
                pixel = 0;
            }
            w = (w + 7) / 8;
        } else {
            pixel &= 0xff;
        }
        lRectfill<uint8_t>(m_data, pixel, 0, 0, w, m_height, m_bytesPerLine);
        return;
    } else if (m_depth == 16) {
        lRectfill<uint16_t>(reinterpret_cast<uint16_t*>(m_data), pixel,
                             0, 0, m_width, m_height, m_bytesPerLine);
        return;
    } else if (m_depth == 24) {
        lRectfill<uint24_t>(reinterpret_cast<uint24_t*>(m_data), pixel,
                             0, 0, m_width, m_height, m_bytesPerLine);
        return;
    } else if (m_depth == 64) {
        lRectfill<uint64_t>(reinterpret_cast<uint64_t*>(m_data), LColor::argbToArgb64(pixel),
                             0, 0, m_width, m_height, m_bytesPerLine);
        return;
    }

    if (m_format == Format::RGB32){
        pixel |= 0xff000000;
    }

    if (m_format == Format::XBGR32){

#if LOOKI_LITTLE_ENDIAN == 0
        pixel |= 0x000000ff;
#else
        pixel |= 0xff000000;
#endif
    }
    if (m_format == Format::BGR30 || m_format == Format::RGB30){
        pixel |= 0xc0000000;
    }

    lRectfill<uint32_t>(reinterpret_cast<uint32_t*>(m_data), pixel,
                      0, 0, m_width, m_height, m_bytesPerLine);

}

void LImage::fill(const LColor &color)
{
    // In case we run out of memory
    if (m_data== nullptr){
        return;
    }

    switch (m_format) {
    case LImage::Format::RGB32:
    case LImage::Format::ARGB32:
        fill(color.toUInt());
        break;
    case LImage::Format::ARGB32Premultiplied:
        fill(LColor::premultiply(color.toUInt()));
        break;
    case LImage::Format::XBGR32:
        fill(LColor::argbToRgba(color.toUInt() | 0xff000000));
        break;
    case LImage::Format::ABGR32:
        fill(LColor::argbToRgba(color.toUInt()));
        break;
    case LImage::Format::ABGR32Premultiplied:
        fill(LColor::argbToRgba(LColor::premultiply(color.toUInt())));
        break;
    case LImage::Format::BGR30:
    case LImage::Format::A2BGR30Premultiplied:
        fill(LColor::argbToBgr30(color.toUInt()));
        break;
    case LImage::Format::RGB30:
    case LImage::Format::A2RGB30Premultiplied:
        fill(LColor::argbToRgb30(color.toUInt()));
        break;
    case LImage::Format::RGB16:
        fill((uint32_t)LColor::argbToRgb16(color.toUInt()));
        break;
    case LImage::Format::Indexed8: {
        uint32_t pixel = 0;
        for (uint32_t i=0; i<m_colorTable.size(); ++i) {
            if (color.toUInt() == m_colorTable.at(i)) {
                pixel = i;
                break;
            }
        }
        fill(pixel);
        break;
    }
    case LImage::Format::Mono:
    case LImage::Format::MonoLSB:{
        if (color == LColorID::White){
            fill((uint32_t) 1);
        }
        else{
            fill((uint32_t) 0);
        }
        break;
    }
    case LImage::Format::RGBX64: {
        uint64_t c = LColor::setAlpha16(color.toUInt64(), 65535);
        lRectfill<uint64_t>(reinterpret_cast<uint64_t*>(m_data), c,
                             0, 0, m_width, m_height, m_bytesPerLine);
        break;

    }
    case LImage::Format::RGBA64:
    case LImage::Format::RGBA64Premultiplied:{
        lRectfill<uint64_t>(reinterpret_cast<uint64_t*>(m_data), color.toUInt64(),
                             0, 0, m_width, m_height, m_bytesPerLine);
        break;
    }
    default: {
        std::shared_ptr<LImage> tThis = getPtrT<LImage>();
        LPainter p(tThis);
        p.setCompositionMode(LCompositionMode::Source);
        p.fillRect(rect().toRectF(), color);
    }}
}

std::shared_ptr<LPaintEngine> LImage::paintEngine() const
{
    if (m_paintEngine == nullptr) {
        std::shared_ptr<LPaintDevice> paintDevice = getPtrT<LPaintDevice>();
        LPlatformIntegration *platformIntegration = LGuiApplication::platformIntegration();
        if (platformIntegration){
            const_cast<LImage*>(this)->m_paintEngine = platformIntegration->createImagePaintEngine(paintDevice);
        }
        if (m_paintEngine == nullptr){
            const_cast<LImage*>(this)->m_paintEngine = std::make_shared<LRasterPaintEngine>(paintDevice);
        }
    }

    return m_paintEngine;
}

bool LImage::save(const std::string &fileName) const
{
    std::shared_ptr<LImage> image = getPtrT<LImage>();
    return LImageWriter::write(fileName, image);
}

LColor LImage::color(uint32_t i) const
{
    if(i<m_colorTable.size()){
        return LColor::uintToColor(m_colorTable[i]);
    }
    return LColor();
}

void LImage::setColor(uint32_t i, const LColor &c)
{
    if ((m_depth > 8) || (i >= uint32_t(1<<m_depth))) {
        lWarn() << "LImage::setColor: Index out of bound "<<i ;
        return;
    }

    if (i >= m_colorTable.size()){
        setColorCount(i+1);
    }
    m_colorTable[i] = c.toUInt();

    m_hasAlphaClut = 0;
    for(i =0; i<m_colorTable.size(); ++i){
        if(LColor::uintToAlpha(m_colorTable[i]) != 0xFF){
             m_hasAlphaClut = 1;
            break;
        }
    }
//        if(c.m_alpha == 0xFF){
//            m_hasAlphaClut = 0;
//        }else{
//            m_hasAlphaClut = 1;
//        }
}

void LImage::setColorCount(uint32_t colorCount)
{
    if (colorCount == m_colorTable.size()){
        return;
    }
    if (colorCount <= 0) {                        // use no color table
        m_colorTable.clear();
        return;
    }
    int nc = m_colorTable.size();
    m_colorTable.resize(colorCount);
    for (uint32_t i = nc; i < colorCount; ++i){
        m_colorTable[i] = 0;
    }
}

int LImage::colorCount() const
{
    return m_colorTable.size();
}

std::shared_ptr<LImage> LImage::copy(const LRect<int> &r) const
{
    if(m_data == nullptr){
        std::shared_ptr<LImage> image = std::make_shared<LImage>();
        return image;
    }

    std::shared_ptr<LImage> tThis = getPtrT<LImage>();

    if (r.isNull()) {
        std::shared_ptr<LImage> image = std::make_shared<LImage>(m_width, m_height, m_format);
        int dataLen = std::min<int>(image->m_nbytes, m_nbytes);
        if(m_data != nullptr){
            memcpy(image->m_data, m_data, dataLen);
        }
        image->m_colorTable = m_colorTable;
        image->m_offset = m_offset;
        image->m_hasAlphaClut = m_hasAlphaClut;
        copyMetadata(image, tThis);
        return image;
    }

    int x = r.m_x;
    int y = r.m_y;
    int w = r.width();
    int h = r.height();

    int dx = 0;
    int dy = 0;
    if (w <= 0 || h <= 0){
        std::shared_ptr<LImage> image = std::make_shared<LImage>();
        return image;
    }

    std::shared_ptr<LImage> image = std::make_shared<LImage>(w, h, m_format);

    if ((x < 0) || (y < 0) || ((x + w) > int(m_width)) || ((y + h) > int(m_height))) {
        if (x < 0) {
            dx = -x;
            x = 0;
        }
        if (y < 0) {
            dy = -y;
            y = 0;
        }
    }

    image->m_colorTable = m_colorTable;

    int pixels_to_copy = std::max<int>(w - dx, 0);
    if (x > int(m_width)){
        pixels_to_copy = 0;
    }
    else if (pixels_to_copy > int(m_width) - x){
        pixels_to_copy = int(m_width) - x;
    }

    int lines_to_copy = std::max<int>(h - dy, 0);
    if (y > int(m_height)){
        lines_to_copy = 0;
    }
    else if (lines_to_copy > int(m_height) - y){
        lines_to_copy = int(m_height) - y;
    }

    bool byteAligned = true;
    if (m_format == Format::Mono || m_format == Format::MonoLSB){
        byteAligned = !(dx & 7) && !(x & 7) && !(pixels_to_copy & 7);
    }

    if (byteAligned) {
        const uint8_t *src = m_data + ((x * m_depth) >> 3) + y * m_bytesPerLine;
        uint8_t *dest = image->m_data + ((dx * m_depth) >> 3) + dy * image->m_bytesPerLine;
        const int bytes_to_copy = (pixels_to_copy * m_depth) >> 3;
        for (int i = 0; i < lines_to_copy; ++i) {
            memcpy(dest, src, bytes_to_copy);
            src += m_bytesPerLine;
            dest += image->m_bytesPerLine;
        }
    } else if (m_format == Format::Mono) {
        const uint8_t *src = m_data + y * m_bytesPerLine;
        uint8_t *dest = image->m_data+ dy * image->m_bytesPerLine;
        for (int i = 0; i < lines_to_copy; ++i) {
            for (int j = 0; j < pixels_to_copy; ++j) {
                if (src[(x + j) >> 3] & (0x80 >> ((x + j) & 7)))
                    dest[(dx + j) >> 3] |= (0x80 >> ((dx + j) & 7));
                else
                    dest[(dx + j) >> 3] &= ~(0x80 >> ((dx + j) & 7));
            }
            src += m_bytesPerLine;
            dest += image->m_bytesPerLine;
        }
    } else { // Format_MonoLSB        
        const uint8_t *src = m_data + y * m_bytesPerLine;
        uint8_t *dest = image->m_data + dy * image->m_bytesPerLine;
        for (int i = 0; i < lines_to_copy; ++i) {
            for (int j = 0; j < pixels_to_copy; ++j) {
                if (src[(x + j) >> 3] & (0x1 << ((x + j) & 7))){
                    dest[(dx + j) >> 3] |= (0x1 << ((dx + j) & 7));
                }
                else{
                    dest[(dx + j) >> 3] &= ~(0x1 << ((dx + j) & 7));
                }
            }
            src += m_bytesPerLine;
            dest += image->m_bytesPerLine;
        }
    }

    copyMetadata(image, tThis);
    image->m_offset = m_offset;
    image->m_hasAlphaClut = m_hasAlphaClut;
    return image;

}

LImage &LImage::operator=(const LImage &src)
{
    if(this == &src){
        return *this;
    }

    m_width= src.m_width;
    m_height= src.m_height;
    m_depth= src.m_depth;
    m_devicePixelRatio= src.m_devicePixelRatio;
    m_colorTable = src.m_colorTable;
    m_format = src.m_format;
    m_bytesPerLine = src.m_bytesPerLine;
    m_serNo = src.m_serNo;               // serial number
    m_detachNo = src.m_detachNo;

    m_dpmx = src.m_dpmx;                // dots per meter X (or 0)
    m_dpmy = src.m_dpmy;                // dots per meter Y (or 0)
    m_offset = src.m_offset;           // offset in pixels
    m_text = src.m_text;

    if (m_data && m_ownData){
        if(m_nbytes != src.m_nbytes){
            delete[] m_data;
            m_data = nullptr;
        }
    }else{
        m_data = nullptr;
    }

    m_nbytes = src.m_nbytes;
    m_imageFlags = src.m_imageFlags;
    if(m_nbytes >0 ){
        m_ownData = true;
        if(m_data == nullptr){
            m_data = new uint8_t[m_nbytes];
        }
        memcpy(m_data, src.m_data, m_nbytes);
    }else{
        m_ownData = false;
        m_data = nullptr;
    }
    return *this;
}

std::shared_ptr<LImage> LImage::scaled(const LSize<int> &s, LAspectRatioMode aspectMode, LTransMode mode) const
{
    LSize<int> newSize = size();
    if (s.isEmpty() || newSize.isEmpty()){
        std::shared_ptr<LImage> ret = std::make_shared<LImage>();
        return ret;
    }

    newSize.scale(s, aspectMode);
    newSize.m_x = std::max<int>(newSize.width(), 1);
    newSize.m_y = std::max<int>(newSize.height(), 1);
    if (newSize == size()){
        if((uint8_t) mode & uint8_t(LTransMode::FastCanRetThis)){
            return getPtrT<LImage>();
        }else{
            std::shared_ptr<LImage> ret = std::make_shared<LImage>();
            *ret = *this;
            return ret;
        }
    }

    LTransform2D wm = LTransform2D::fromScale((double)newSize.m_x / (double)m_width,
                                              (double)newSize.m_y / (double)m_height);
    return transformed(wm, mode);
}

std::shared_ptr<LImage> LImage::scaledToWidth(int w, LTransMode mode) const
{
    if (w <= 0 || m_width <=0){
        std::shared_ptr<LImage> ret = std::make_shared<LImage>();
        return ret;
    }
    double factor = (double) w / (double)m_width;
    LTransform2D wm = LTransform2D::fromScale(factor, factor);
    return transformed(wm, mode);
}

std::shared_ptr<LImage> LImage::scaledToHeight(int h, LTransMode mode) const
{
    if (h <= 0 || m_height <=0){
        std::shared_ptr<LImage> ret = std::make_shared<LImage>();
        return ret;
    }

    double factor = (double) h / (double)m_height;
    LTransform2D wm = LTransform2D::fromScale(factor, factor);
    return transformed(wm, mode);
}

std::shared_ptr<LImage> LImage::transformed(const LTransform2D &matrix,
                                            LTransMode mode) const
{
    // source image data
    int ws = m_width;
    int hs = m_height;

    // target image data
    int wd;
    int hd;

    std::shared_ptr<LImage> tThis = getPtrT<LImage>();
    bool canRetThis =  (uint8_t) mode & uint8_t(LTransMode::FastCanRetThis);
    mode = (LTransMode)((uint8_t)mode & uint8_t(LTransMode::Smooth));

    // compute size of target image
    LTransform2D mat = trueMatrix(matrix, ws, hs);
    bool complex_xform = false;
    bool scaleXform = false;
    if (mat.type() <= LTransform2D::TxScale) {
        if (mat.type() == LTransform2D::TxNone){ // identity matrix
            if(canRetThis){
                return tThis;
            }else{
                std::shared_ptr<LImage> ret = std::make_shared<LImage>();
                *ret = *this;
                return ret;
            }
        }
        else if (mat.m11() == -1. && mat.m22() == -1.){
            return rotated180(tThis);
        }

        if (mode == LTransMode::Fast) {
            hd = std::lround(std::abs(mat.m22()) * hs);
            wd = std::lround(std::abs(mat.m11()) * ws);
        } else {
            hd = int(std::abs(mat.m22()) * hs + 0.9999);
            wd = int(std::abs(mat.m11()) * ws + 0.9999);
        }
        scaleXform = true;
    } else {
        if (mat.type() <= LTransform2D::TxRotate && mat.m11() == 0 && mat.m22() == 0) {
            if (mat.m12() == 1. && mat.m21() == -1.){
                return rotated90(tThis);
            }
            else if (mat.m12() == -1. && mat.m21() == 1.){
                return rotated270(tThis);
            }
        }

        LRect<int> r = mat.mapRect(LRect<double>(0, 0, ws, hs, false)).toAlignedRect();
        wd = r.width();
        hd = r.height();
        complex_xform = true;
    }

    if (wd == 0 || hd == 0){
        std::shared_ptr<LImage> ret = std::make_shared<LImage>();
        return ret;
    }

    // Make use of the optimized algorithm when we're scaling
    if (scaleXform && mode == LTransMode::Smooth) {
        std::shared_ptr<LImage> ret = smoothScaled(wd, hd);
        if(ret != nullptr){
            if (mat.m11() < 0.0F && mat.m22() < 0.0F) { // horizontal/vertical flip
                ret = ret->mirrored(true, true, canRetThis);
            } else if (mat.m11() < 0.0F) { // horizontal flip
                ret = ret->mirrored(true, false, canRetThis);
            } else if (mat.m22() < 0.0F) { // vertical flip
                ret = ret->mirrored(false, true, canRetThis);
            }
        }
        return ret;
    }

    int bpp = depth();

    int sbpl = m_bytesPerLine;
    const uint8_t *sptr = m_data;

    LImage::Format targetFormat = m_format;

    if (complex_xform || mode == LTransMode::Smooth) {
        if (m_format < LImage::Format::RGB32 || !hasAlphaChannel()) {
            targetFormat = lAlphaVersion(m_format);
        }
    }

    std::shared_ptr<LImage> dImage = std::make_shared<LImage>(wd, hd, targetFormat);

    if (targetFormat == LImage::Format::MonoLSB
        || targetFormat == LImage::Format::Mono
        || targetFormat == LImage::Format::Indexed8) {
        dImage->m_colorTable = m_colorTable;
        dImage->m_hasAlphaClut = m_hasAlphaClut | complex_xform;
    }

    // initizialize the data
    if (targetFormat == LImage::Format::Indexed8) {
        if (dImage->m_colorTable.size() < 256) {///????
            // colors are left in the color table, so pick that one as transparent
            dImage->m_colorTable.push_back(0x0);
            memset(dImage->m_data, dImage->m_colorTable.size() - 1, dImage->m_nbytes);
        }
    }

    if (targetFormat >= LImage::Format::RGB32) {
        std::shared_ptr<LImage> sImage = nullptr;
        if(devicePixelRatio() != 1){
            sImage = std::make_shared<LImage>(m_data, m_width, m_height, m_format);
        }else{
            sImage = tThis;
        }

        LPainter p(dImage);
        if (mode == LTransMode::Smooth) {
            p.setRenderHints(LRenderHint::Antialiasing);
            p.setRenderHints(LRenderHint::SmoothPixmapTransform);
        }
        p.setTransform(mat);
        p.drawImage(LPoint<double>(0, 0), sImage);
    } else {
        bool invertible;
        mat = mat.inverted(&invertible);                // invert matrix
        if (!invertible){        // error, return null image
            std::shared_ptr<LImage> ret = std::make_shared<LImage>();
            return ret;
        }

        int type = format() == LImage::Format::Mono ? LK_XFORM_TYPE_MSBFIRST : LK_XFORM_TYPE_LSBFIRST;
        int dbpl = dImage->m_bytesPerLine;
        lXFormHelper(mat, 0, type, bpp, dImage->bits(), dbpl, 0, hd, sptr, sbpl, ws, hs);
    }
    copyMetadata(dImage, tThis);

    return dImage;
}

std::shared_ptr<LImage> LImage::mirrored(bool horizontal, bool vertical, bool canRetThis) const
{
    if ((m_width <= 1 && m_height <= 1) || (!horizontal && !vertical)){
        if(canRetThis){
            return getPtrT<LImage>();
        }else{
            std::shared_ptr<LImage> ret = std::make_shared<LImage>();
            *ret = *this;
            return ret;
        }
    }

    std::shared_ptr<LImage> tThis = getPtrT<LImage>();
    // Create result image, copy colormap
    std::shared_ptr<LImage> result = std::make_shared<LImage>(m_width, m_height, m_format);
    result->m_colorTable = m_colorTable;
    result->m_hasAlphaClut = m_hasAlphaClut;
    copyMetadata(result, tThis);

    doMirror(result, tThis, horizontal, vertical);
    return result;
}

LTransform2D LImage::trueMatrix(const LTransform2D &matrix, int w, int h)
{
    const LRect<double> rect(0, 0, w, h, false);
    const LRect<int> mapped = matrix.mapRect(rect).toAlignedRect();
    const LPoint<int> delta = mapped.topLeft();
    return matrix * LTransform2D().translate(-delta.m_x, -delta.m_y);
}

std::shared_ptr<LImage> LImage::rotated90(const std::shared_ptr<LImage> image)
{
    std::shared_ptr<LImage> out
        = std::make_shared<LImage>(image->m_height, image->m_width, image->m_format);
    copyMetadata(out, image);

    int w = image->m_width;
    int h = image->m_height;

    if (image->colorCount()){
        out->m_colorTable = image->m_colorTable;

        for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x){
                out->setPixel(h-y-1, x, image->pixelIndex(x, y));
            }
        }
    }
    else{
        for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x){
                out->setPixel(h-y-1, x, image->pixel(x, y));
            }
        }
    }
    return out;
}

std::shared_ptr<LImage> LImage::rotated180(const std::shared_ptr<LImage> src)
{
    return src->mirrored(true, true);;
}

std::shared_ptr<LImage> LImage::rotated270(const std::shared_ptr<LImage> image)
{
    std::shared_ptr<LImage> out
        = std::make_shared<LImage>(image->m_height, image->m_width, image->m_format);
    copyMetadata(out, image);
    int w = image->m_width;
    int h = image->m_height;

    if (image->colorCount() > 0){
        out->m_colorTable = image->m_colorTable;

        for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x){
                out->setPixel(y, w-x-1, image->pixelIndex(x, y));
            }
        }
    }else{
        for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x){
                out->setPixel(y, w-x-1, image->pixel(x, y));
            }
        }
    }
    return out;
}

std::shared_ptr<LImage> LImage::smoothScaled(int w, int h) const
{
    std::shared_ptr<LImage> ret;

    if (hasAlphaChannel()){
        ret = convertToFormat(LImage::Format::ARGB32Premultiplied);
    }
    else{
        ret = convertToFormat(LImage::Format::RGB32);
    }
    ret = lSmoothScaleImage(ret, w, h);
    if (ret != nullptr){
        std::shared_ptr<LImage> tThis = getPtrT<LImage>();
        copyMetadata(ret, tThis);
    }
    return ret;
}


bool LImage::hasAlphaChannel() const
{
    const LPixelFormat pixFormat = pixelformats[(int)m_format];
    if (pixFormat.m_alphaUsage == pixfmt::UsesAlpha){
        return true;
    }
    if (pixFormat.m_model == pixfmt::Indexed){
        return m_hasAlphaClut;
    }
    return false;
}

bool LImage::hasPremultiplied(Format format)
{
    const LPixelFormat pixFormat = pixelformats[(int)format];
    if(pixFormat.m_premul){
        return true;
    }
    return false;
}

LPixelFormat LImage::toPixelFormat(Format format)
{
    return pixelformats[(int)format];
}

LImage::Format LImage::toImageFormat(LPixelFormat format)
{
    for (int i = 0; i < int(Format::NImageFormats); i++) {
        if (format.m_data == pixelformats[i].m_data){
            return Format(i);
        }
    }
    return Format::Invalid;
}

LImage::Format LImage::maybeAlphaVersionWithSameDepth(Format format)
{
    const Format toFormat = lAlphaVersion(format);
    return depthForFormat(format) == depthForFormat(toFormat) ? toFormat : format;
}

uint8_t *LImage::scanLine(int i)
{
    return m_data + i * m_bytesPerLine;
}

const uint8_t *LImage::scanLine(int i) const
{
    return m_data + i * m_bytesPerLine;
}

void LImage::toOwnData()
{
    if(m_ownData == false && (m_data != nullptr) && (m_nbytes>0)){
        m_ownData = true;
        uint8_t* t = m_data;
        m_data = new uint8_t[m_nbytes];
        memcpy(m_data, t, m_nbytes);
    }
}

int LImage::metric(Metric metric) const
{
    switch (metric) {
    case PdmWidth:{
        return m_width;
    }
    case PdmHeight:{
        return m_height;
    }
    case PdmWidthMM:{
        return std::lround(m_width * 1000 / m_dpmx);
    }
    case PdmHeightMM:{
        return std::lround(m_height * 1000 / m_dpmy);
    }
    case PdmNumColors:{
        return m_colorTable.size();
    }
    case PdmDepth:{
        return m_depth;
    }
    case PdmDpiX:{
        return std::lround(m_dpmx * 0.0254);
    }
    case PdmDpiY:{
        return std::lround(m_dpmy * 0.0254);
    }
    case PdmPhysicalDpiX:{
        return std::lround(m_dpmx * 0.0254);
    }
    case PdmPhysicalDpiY:{
        return std::lround(m_dpmy * 0.0254);
    }
    case PdmDevicePixelRatio:{
        return m_devicePixelRatio;
    }
    case PdmDevicePixelRatioScaled:{
        return m_devicePixelRatio * LPaintDevice::devicePixelRatioFScale();
    }
    default:{
        lWarn() << "LImage::metric(): Unhandled metric type: " << metric;
        break;
    }
    }
    return 0;
}


void LImage::create(const LSize<int> &size, Format format)
{
    if (size.isEmpty() || format == Format::Invalid){
        return;
    }


    int width = size.width();
    int height = size.height();
    int depth = depthForFormat(format);
    auto params = calculateImageParams(width, height, depth);
    if (!params.isValid()){
        return;
    }

    m_width = width;
    m_height = height;
    m_depth = depth;
    m_format = format;
    m_hasAlphaClut = false;
    m_isCached = false;

    m_bytesPerLine = params.bytesPerLine;
    m_ownData = true;
    m_nbytes = params.totalSize;
    //m_data = (uint8_t *)malloc(m_nbytes);
    m_data = new uint8_t[m_nbytes];
    memset(m_data, 0, m_nbytes);

    createMonoColorTable();
}

void LImage::create(uint8_t* data,
                    int width,
                    int height,
                    int bpl,
                    Format format,
                    bool readOnly,
                    int dataLen,
                    bool ownData)
{
    if ((width <= 0) ||
        (height <= 0) ||
        (data == nullptr)||
        (format == LImage::Format::Invalid)){
        return;
    }

    const int depth = depthForFormat(format);
    LImage::SizeParams params;
    if (bpl > 0) {
        // can't overflow, because has calculateImageParameters already done this multiplication
        const int min_bytes_per_line = (width * depth + 7)/8;
        if (bpl < min_bytes_per_line){
            return;
        }

        // recalculate the total with this value
        params.bytesPerLine = bpl;
        params.totalSize = bpl*height;
    }else{
        params = calculateImageParams(width, height, depth);
    }

    if (!params.isValid()){
        return;
    }

    if(dataLen >0){
        if(params.totalSize != dataLen){
            return;
        }
    }


    m_ownData = ownData;
    m_roData = readOnly;
    m_nbytes = params.totalSize;
    if(m_ownData){
        m_data = new uint8_t[m_nbytes];
        memcpy(m_data, data, m_nbytes);
    }else{
        m_data = data;
    }
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_format = format;

    m_bytesPerLine = params.bytesPerLine;
    createMonoColorTable();
}

void LImage::copyPhysicalMetadata(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src)
{
    dst->m_dpmx = src->m_dpmx;
    dst->m_dpmy = src->m_dpmy;
    dst->m_devicePixelRatio = src->m_devicePixelRatio;
}

void LImage::copyMetadata(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src)
{
    copyPhysicalMetadata(dst, src);
    dst->m_text = src->m_text;
    //dst->m_colorSpace = src->m_colorSpace; //todo
}

template<class T> inline void doMirrorData(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src,
                           int dstX0, int dstY0,
                           int dstXIncr, int dstYIncr,
                           int w, int h)
{
    if (dst == src) {
        // When mirroring in-place, stop in the middle for one of the directions, since we
        // are swapping the bytes instead of merely copying.
        const int srcXEnd = (dstX0 && !dstY0) ? w / 2 : w;
        const int srcYEnd = dstY0 ? h / 2 : h;
        for (int srcY = 0, dstY = dstY0; srcY < srcYEnd; ++srcY, dstY += dstYIncr) {
            T *srcPtr = (T *) (src->bits() + srcY * src->bytesPerLine());
            T *dstPtr = (T *) (dst->bits() + dstY * dst->bytesPerLine());
            for (int srcX = 0, dstX = dstX0; srcX < srcXEnd; ++srcX, dstX += dstXIncr){
                std::swap(srcPtr[srcX], dstPtr[dstX]);
            }
        }
        // If mirroring both ways, the middle line needs to be mirrored horizontally only.
        if (dstX0 && dstY0 && (h & 1)) {
            int srcY = h / 2;
            int srcXEnd2 = w / 2;
            T *srcPtr = (T *) (src->bits() + srcY * src->bytesPerLine());
            for (int srcX = 0, dstX = dstX0; srcX < srcXEnd2; ++srcX, dstX += dstXIncr){
                std::swap(srcPtr[srcX], srcPtr[dstX]);
            }
        }
    } else {
        for (int srcY = 0, dstY = dstY0; srcY < h; ++srcY, dstY += dstYIncr) {
            T *srcPtr = (T *) (src->bits() + srcY * src->bytesPerLine());
            T *dstPtr = (T *) (dst->bits() + dstY * dst->bytesPerLine());
            for (int srcX = 0, dstX = dstX0; srcX < w; ++srcX, dstX += dstXIncr){
                dstPtr[dstX] = srcPtr[srcX];
            }
        }
    }
}

void LImage::doMirror(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src, bool horizontal, bool vertical)
{
    int w = src->m_width;
    int h = src->m_height;
    int depth = src->m_depth;

    if (src->m_depth == 1) {
        w = (w + 7) / 8; // byte aligned width
        depth = 8;
    }

    if (vertical && !horizontal) {
        // This one is simple and common, so do it a little more optimized
        doFlip(dst, src, w, h, depth);
        return;
    }

    int dstX0 = 0, dstXIncr = 1;
    int dstY0 = 0, dstYIncr = 1;
    if (horizontal) {
        // 0 -> w-1, 1 -> w-2, 2 -> w-3, ...
        dstX0 = w - 1;
        dstXIncr = -1;
    }
    if (vertical) {
        // 0 -> h-1, 1 -> h-2, 2 -> h-3, ...
        dstY0 = h - 1;
        dstYIncr = -1;
    }

    switch (depth) {
    case 64:
        doMirrorData<uint64_t>(dst, src, dstX0, dstY0, dstXIncr, dstYIncr, w, h);
        break;
    case 32:
        doMirrorData<uint32_t>(dst, src, dstX0, dstY0, dstXIncr, dstYIncr, w, h);
        break;
    case 24:
        doMirrorData<uint24_t>(dst, src, dstX0, dstY0, dstXIncr, dstYIncr, w, h);
        break;
    case 16:
        doMirrorData<uint16_t>(dst, src, dstX0, dstY0, dstXIncr, dstYIncr, w, h);
        break;
    case 8:
        doMirrorData<uint8_t>(dst, src, dstX0, dstY0, dstXIncr, dstYIncr, w, h);
        break;
    default:
        break;
    }

    // The bytes are now all in the correct place. In addition, the bits in the individual
    // bytes have to be flipped too when horizontally mirroring a 1 bit-per-pixel image.
    if (horizontal && dst->m_depth == 1) {
        const int shift = 8 - (dst->m_width % 8);
        const uint8_t *bitflip = LImageConversions::lGetBitFlipArray();
        for (int y = 0; y < h; ++y) {
            uint8_t *begin = dst->m_data + y * dst->m_bytesPerLine;
            uint8_t *end = begin + dst->m_bytesPerLine;
            for (uint8_t *p = begin; p < end; ++p) {
                *p = bitflip[*p];
                // When the data is non-byte aligned, an extra bit shift (of the number of
                // unused bits at the end) is needed for the entire scanline.
                if (shift != 8 && p != begin) {
                    if (dst->m_format == LImage::Format::Mono) {
                        for (int i = 0; i < shift; ++i) {
                            p[-1] <<= 1;
                            p[-1] |= (*p & (128 >> i)) >> (7 - i);
                        }
                    } else {
                        for (int i = 0; i < shift; ++i) {
                            p[-1] >>= 1;
                            p[-1] |= (*p & (1 << i)) << (7 - i);
                        }
                    }
                }
            }
            if (shift != 8) {
                if (dst->m_format == LImage::Format::Mono){
                    end[-1] <<= shift;
                }
                else{
                    end[-1] >>= shift;
                }
            }
        }
    }
}

void LImage::doFlip(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src, int w, int h, int depth)
{
    const int data_bytes_per_line = w * (depth / 8);
    if (dst == src) {
        uint32_t *srcPtr = reinterpret_cast<uint32_t *>(src->m_data);
        uint32_t *dstPtr = reinterpret_cast<uint32_t *>(dst->m_data + (h - 1) * dst->m_bytesPerLine);
        h = h / 2;
        const int uint_per_line = (data_bytes_per_line + 3) >> 2; // bytes per line must be a multiple of 4
        for (int y = 0; y < h; ++y) {
            // This is auto-vectorized, no need for SSE2 or NEON versions:
            for (int x = 0; x < uint_per_line; x++) {
                const uint32_t d = dstPtr[x];
                const uint32_t s = srcPtr[x];
                dstPtr[x] = s;
                srcPtr[x] = d;
            }
            srcPtr += src->m_bytesPerLine >> 2;
            dstPtr -= dst->m_bytesPerLine >> 2;
        }

    } else {
        const uint8_t *srcPtr = src->m_data;
        uint8_t *dstPtr = dst->m_data + (h - 1) * dst->m_bytesPerLine;
        for (int y = 0; y < h; ++y) {
            memcpy(dstPtr, srcPtr, data_bytes_per_line);
            srcPtr += src->m_bytesPerLine;
            dstPtr -= dst->m_bytesPerLine;
        }
    }
}

LImage::SizeParams LImage::calculateImageParams(uint32_t width,
                                                uint32_t height,
                                                uint32_t depth)
{
    SizeParams invalid = { 0, 0 };
    if (height <= 0){
        return invalid;
    }

    // calculate the size, taking care of overflows
    uint64_t bytesPerLine = width *depth;
    bytesPerLine += 31;
    // bytes per scanline (must be multiple of 4)
    //一个字节8位，深度转字节数需要除以8，也就是总共要右移3位，但要保证是4的倍数，所以先右移5位，再左移2位
    bytesPerLine = (bytesPerLine>> 5) << 2;
    if(bytesPerLine > UINT_MAX){
        return invalid;
    }

    uint64_t totalSize = bytesPerLine*height;
    if(totalSize > UINT_MAX){
        return invalid;
    }
    return { (uint32_t)bytesPerLine, (uint32_t)totalSize };
}

std::shared_ptr<LImage> LImage::convertToFormat(Format format,
                                                LFlags<LImageConvertFlag> flags,
                                                bool canRetThis) const
{
    if (m_format == format){
        if(canRetThis){
            return getPtrT<LImage>();
        }else{
            std::shared_ptr<LImage> t = std::make_shared<LImage>();
            *t = *this;
            return t;
        }
    }

    if (format == Format::Invalid || m_format == Format::Invalid){
        std::shared_ptr<LImage> t = std::make_shared<LImage>();
        return t;
    }

    std::shared_ptr<LImage> tThis = getPtrT<LImage>();

    const draw::LPixelLayout *destLayout = &draw::lPixelLayouts[(int)format];
    ImageConverter converter =  lImageConverterMap[(int)m_format][(int)format];
    if ( (converter == nullptr)
        && (format > Format::Indexed8)
        && (m_format > Format::Indexed8)) {
        if (lHighColorPrecision(m_format, !destLayout->hasAlphaChannel)
            && lHighColorPrecision(format, !hasAlphaChannel())) {
            converter = LImageConversions::convertGenericToRgb64;
        } else{
            converter = LImageConversions::convertGeneric;
        }
    }


    if (converter != nullptr) {
        std::shared_ptr<LImage> t = std::make_shared<LImage>(m_width, m_height, format);

        t->m_offset = m_offset;
        copyMetadata(t, tThis);
        converter(t, tThis, flags);
        return t;
    }

    if (!hasAlphaChannel()){
        std::shared_ptr<LImage> t= convertToFormat(Format::RGB32, flags);
        if(t!= nullptr){
            return t->convertToFormat(format, flags);
        }
        return nullptr;
    }

    std::shared_ptr<LImage> t= convertToFormat(Format::ARGB32, flags);
    if(t!= nullptr){
        return t->convertToFormat(format, flags);
    }
    return nullptr;
}

std::shared_ptr<LRegion> LImage::toRegion()
{
    std::shared_ptr<LRegion> region = std::make_shared<LRegion>();
    LRect<int> xr;

    const uint8_t zero = 0;
    bool little = m_format == Format::MonoLSB;

    int x, y;
    for (y = 0; y < height(); ++y) {
        const uint8_t *line = scanLine(y);
        int w = width();
        uint8_t all = zero;
        int prev1 = -1;
        for (x = 0; x < w;) {
            uint8_t byte = line[x / 8];
            if (x > w - 8 || byte!=all) {
                if (little) {
                    for (int b = 8; b > 0 && x < w; --b) {
                        if (!(byte & 0x01) == !all) {
                            // More of the same
                        } else {
                            // A change.
                            if (all!=zero) {
                                xr.setCoords(prev1, y, x-1, y);
                                region->unite(xr);
                                all = zero;
                            } else {
                                prev1 = x;
                                all = ~zero;
                            }
                        }
                        byte >>= 1;
                        ++x;
                    }
                } else {
                    for (int b = 8; b > 0 && x < w; --b) {
                        if (!(byte & 0x80) == !all) {
                            // More of the same
                        } else {
                            // A change.
                            if (all != zero) {
                                xr.setCoords(prev1, y, x-1, y);
                                region->unite(xr);
                                all = zero;
                            } else {
                                prev1 = x;
                                all = ~zero;
                            }
                        }
                        byte <<= 1;
                        ++x;
                    }
                }
            } else {
                x += 8;
            }
        }
        if (all != zero) {
            xr.setCoords(prev1, y, x-1, y);
            region->unite(xr);
        }
    }
    return region;
}

void LImage::setOffset(const LPoint<int> &off)
{
    m_offset = off;
}

void LImage::createMonoColorTable()
{
    switch (m_format) {
    case Format::Mono:
    case Format::MonoLSB:{
        m_colorTable.resize(2);
        m_colorTable[0] = LColor(LColorID::Black).toUInt();
        m_colorTable[1] = LColor(LColorID::White).toUInt();
        break;
    }
    default:
        break;
    }
}


LK_NAMESPACE_END
