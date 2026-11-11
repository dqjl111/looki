#ifndef LIMAGE_H
#define LIMAGE_H
#include "lkgui/paint/lpaintdevice.h"
#include "lkgui/paint/lcolor.h"
#include "lkgui/paint/ltransform2d.h"
#include "lkgui/paint/lregion.h"
#include <vector>

LK_NAMESPACE_BEGIN

#define _PixModelWidth 4
#define _Pix1stWidth 6
#define _Pix2ndWidth 6
#define _Pix3rdWidth 6
#define _Pix4thWidth 6
#define _Pix5thWidth 6
#define _PixAlphaWidth 6
#define _PixAlphaUsageFieldWidth 1
#define _PixAlphaPositionFieldWidth 1
#define _PixPremulFieldWidth 1
#define _PixTypeInterpretationFieldWidth 4
#define _PixByteOrderFieldWidth 2
#define _PixSubEnumFieldWidth 6
#define _PixUnusedFieldWidth 9

namespace pixfmt{
enum ColorModel {
    RGB,
    BGR,
    Indexed,
    Grayscale,
    CMYK,
    HSL,
    HSV,
    YUV,
    Alpha
};

enum AlphaUsage {
    UsesAlpha,
    IgnoresAlpha
};

enum AlphaPosition {
    AtBeginning,
    AtEnd
};

enum AlphaPremultiplied {
    NotPremultiplied,
    Premultiplied
};

enum TypeInterpretation {
    UnsignedInteger,
    UnsignedShort,
    UnsignedByte,
    FloatingPoint
};

enum YUVLayout {
    YUV444,
    YUV422,
    YUV411,
    YUV420P,
    YUV420SP,
    YV12,
    UYVY,
    YUYV,
    NV12,
    NV21,
    IMC1,
    IMC2,
    IMC3,
    IMC4,
    Y8,
    Y16
};

enum ByteOrder {
    LittleEndian,
    BigEndian,
    CurrentSystemEndian
};



struct BitmapDIV5Header {
    uint32_t  bV5Size;
    int   bV5Width;
    int   bV5Height;
    uint16_t   bV5Planes;
    uint16_t   bV5BitCount;
    uint32_t  bV5Compression;
    uint32_t  bV5SizeImage;
    int   bV5XPelsPerMeter;
    int   bV5YPelsPerMeter;
    uint32_t  bV5ClrUsed;
    uint32_t  bV5ClrImportant;
    uint32_t  bV5RedMask;
    uint32_t  bV5GreenMask;
    uint32_t  bV5BlueMask;
    uint32_t  bV5AlphaMask;
    uint32_t  bV5CSType;
    uint32_t bV5Endpoints[9];
    uint32_t  bV5GammaRed;
    uint32_t  bV5GammaGreen;
    uint32_t  bV5GammaBlue;
    uint32_t  bV5Intent;
    uint32_t  bV5ProfileData;
    uint32_t  bV5ProfileSize;
    uint32_t  bV5Reserved;
};

}


union LPixelFormat
{
    uint64_t m_data;
    struct{
        uint64_t m_model : _PixModelWidth;
        uint64_t m_1st : _Pix1stWidth;
        uint64_t m_2nd : _Pix2ndWidth;
        uint64_t m_3rd : _Pix3rdWidth;
        uint64_t m_4th : _Pix4thWidth;
        uint64_t m_5th : _Pix5thWidth;
        uint64_t m_alpha : _PixAlphaWidth;
        uint64_t m_alphaUsage : _PixAlphaUsageFieldWidth;
        uint64_t m_alphaPos : _PixAlphaPositionFieldWidth;
        uint64_t m_premul :_PixPremulFieldWidth;
        uint64_t m_typeInterpretation : _PixTypeInterpretationFieldWidth;
        uint64_t m_byteorder : _PixByteOrderFieldWidth;
        uint64_t m_subEnum : _PixSubEnumFieldWidth;
        uint64_t m_unused : _PixUnusedFieldWidth;
    };

    LPixelFormat():m_data(0){

    }

    LPixelFormat(pixfmt::ColorModel colorModel,
                 uint8_t firstSize,
                 uint8_t secondSize,
                 uint8_t thirdSize,
                 uint8_t fourthSize,
                 uint8_t fifthSize,
                 uint8_t alphaSize,
                 pixfmt::AlphaUsage alphaUsage,
                 pixfmt::AlphaPosition alphaPosition,
                 pixfmt::AlphaPremultiplied premultiplied,
                 pixfmt::TypeInterpretation typeInterpretation,
                 pixfmt::ByteOrder byteOrder = pixfmt::CurrentSystemEndian,
                 uint8_t subEnum = 0)
        :m_model(colorModel), m_1st(firstSize), m_2nd(secondSize), m_3rd(thirdSize)
        ,m_4th(fourthSize), m_5th(fifthSize), m_alpha(alphaSize), m_alphaUsage(alphaUsage)
        ,m_alphaPos(alphaPosition), m_premul(premultiplied), m_typeInterpretation(typeInterpretation)
        ,m_byteorder(byteOrder), m_subEnum(subEnum)
    {

    }
};

class LImageConversions;
class LKGUIDEF LImage: public LPaintDevice
{
public:
    enum class Format {
        Invalid,
        Mono,
        MonoLSB,
        Indexed8,
        RGB32,
        ARGB32,
        ARGB32Premultiplied,
        RGB16,
        ARGB8565Premultiplied,
        RGB666,
        ARGB6666Premultiplied,
        RGB555,
        ARGB8555Premultiplied,
        RGB888,
        RGB444,
        ARGB4444Premultiplied,
        XBGR32,
        ABGR32, //字节顺序与大小端无关，按字节顺序是 0xRR,0xGG,0xBB,0xAA，转成小端Int32时AA在高位
        ABGR32Premultiplied,
        BGR30,
        A2BGR30Premultiplied,
        RGB30,
        A2RGB30Premultiplied,
        Alpha8,
        Grayscale8,
        RGBX64,
        RGBA64,
        RGBA64Premultiplied,
        Grayscale16,
        BGR888,
        NImageFormats
    };


    LImage();
    LImage(const LSize<int> &size, Format format);
    LImage(int width, int height, Format format);
    LImage(const std::vector<uint8_t>& data, int width, int height, Format format);
    LImage(const uint8_t* data, int width, int height, Format format, bool ownData =false);
    LImage(uint8_t* data, int width, int height, Format format, bool ownData =false);

    LImage(const uint8_t *data, int width, int height, int bytesPerLine, Format format, bool ownData =false);
    LImage(uint8_t *data, int width, int height, int bytesPerLine, Format format, bool ownData =false);
    ~LImage();

    enum InvertMode { InvertRgb, InvertRgba };

    inline virtual int devType() const override{
        return PDtImage;
    }

    inline bool isNull(){
        return m_data == nullptr;
    }

    bool valid(uint32_t x, uint32_t y) const;
    int pixelIndex(uint32_t x, uint32_t y) const; //有颜色表时返回有效，效则为-1

    uint32_t pixel(uint32_t x, uint32_t y) const;
    void setPixel(uint32_t x, uint32_t y, uint32_t value);

    inline LColor pixelColor(uint32_t x, uint32_t y) const{
        return LColor::uintToColor(pixel(x, y));
    }
    inline void setPixelColor(uint32_t x, uint32_t y, const LColor &c){
        return setPixel(x, y, c.toUInt());
    }

    double devicePixelRatio() const;
    void setDevicePixelRatio(double scaleFactor);


    void fill(uint32_t value);
    void fill(const LColor &color);

    virtual std::shared_ptr<LPaintEngine> paintEngine() const override;

    bool save(const std::string &fileName) const;
    inline LSize<int> size() const{
        return LSize<int>(m_width, m_height);
    }

    inline LRect<int> rect() const{
        return LRect<int>(0, 0, m_width, m_height, false);
    }

    inline LSize<double> sizeF() const{
        return LSize<double>(m_width, m_height);
    }

    inline LRect<double> rectF() const{
        return LRect<double>(0, 0, m_width, m_height, false);
    }

    inline Format format() const{
        return m_format;
    }

    inline int width() const{
        return m_width;
    }
    inline int height() const{
        return m_height;
    }

    inline int depth() const{
        return m_depth;
    }

    inline uint32_t bytesPerLine() const{
        return m_bytesPerLine;
    }

    inline int dotsPerMeterX() const{
        return m_dpmx;
    }

    inline int dotsPerMeterY() const{
        return m_dpmy;
    }

    inline void setDotsPerMeterX(int v){
        m_dpmx = v;
    }

    inline void setDotsPerMeterY(int v){
        m_dpmy = v;
    }

    LColor color(uint32_t i) const;
    void setColor(uint32_t i, const LColor& c);
    void setColorCount(uint32_t);
    int colorCount() const;

    inline const uint8_t* bits() const{
        return m_data;
    }

    inline uint8_t* bits(){
        return m_data;
    }

    inline uint32_t sizeInBytes() const{
        return m_nbytes;
    }

    std::shared_ptr<LImage> copy(const LRect<int>& r) const;
    LImage &operator=(const LImage &src);

    inline std::shared_ptr<LImage> scaled(int w, int h,
                                          LAspectRatioMode aspectMode = LAspectRatioMode::IgnoreAspectRatio,
                                          LTransMode mode = LTransMode::Fast) const
    { return scaled(LSize<int>(w, h), aspectMode, mode); }

    std::shared_ptr<LImage> scaled(const LSize<int> &s,
                                   LAspectRatioMode aspectMode = LAspectRatioMode::IgnoreAspectRatio,
                                   LTransMode mode = LTransMode::Fast) const;
    std::shared_ptr<LImage> scaledToWidth(int w, LTransMode mode = LTransMode::Fast) const;
    std::shared_ptr<LImage> scaledToHeight(int h, LTransMode mode = LTransMode::Fast) const;

    std::shared_ptr<LImage> transformed(const LTransform2D &matrix,
                                        LTransMode mode = LTransMode::Fast) const;

    std::shared_ptr<LImage> mirrored(bool horizontal=true, bool vertical=true, bool canRetThis = false) const;
    std::shared_ptr<LImage> smoothScaled(int w, int h) const;

    bool hasAlphaChannel() const;
    uint8_t *scanLine(int);
    const uint8_t *scanLine(int) const;
    void toOwnData();

    const std::vector<uint32_t>& colorTable(){
        return m_colorTable;
    }
    std::shared_ptr<LImage> convertToFormat(Format f,
                                            LFlags<LImageConvertFlag> flags = LImageConvertFlag::AutoColor,
                                            bool canRetThis = false) const;

    std::shared_ptr<LRegion> toRegion();

    const inline LPoint<int>& offset() const{
        return m_offset;
    }
    void setOffset(const LPoint<int>& off);
    static LTransform2D trueMatrix(const LTransform2D &, int w, int h);
    static std::shared_ptr<LImage> rotated90(const std::shared_ptr<LImage> src);
    static std::shared_ptr<LImage> rotated180(const std::shared_ptr<LImage> src);
    static std::shared_ptr<LImage> rotated270(const std::shared_ptr<LImage> src);

    static bool hasPremultiplied(LImage::Format format);
    static LPixelFormat toPixelFormat(LImage::Format format);
    static LImage::Format toImageFormat(LPixelFormat format);

    static Format maybeAlphaVersionWithSameDepth(Format format);
    static int depthForFormat(Format format);


protected:
    virtual int metric(Metric metric) const override;

    void create(const LSize<int> &size, Format format);
    void create(uint8_t* data,
                int width,
                int height,
                int bpl,
                Format format,
                bool readOnly,
                int dataLen = -1,
                bool ownData = true);

    static void copyPhysicalMetadata(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src);
    static void copyMetadata(std::shared_ptr<LImage> dst, std::shared_ptr<LImage> src);
    static void doMirror(std::shared_ptr<LImage> dst,
                         std::shared_ptr<LImage> src,
                         bool horizontal,
                         bool vertical);

    static void doFlip(std::shared_ptr<LImage> dst,
                       std::shared_ptr<LImage> src,
                       int w, int h, int depth);

    struct SizeParams {
        uint32_t bytesPerLine;
        uint32_t totalSize;
        bool isValid() const { return bytesPerLine > 0 && totalSize > 0; }
    };

    static SizeParams calculateImageParams(uint32_t width, uint32_t height, uint32_t depth);

    static std::shared_ptr<LImage> lSmoothScaleImage(const std::shared_ptr<LImage> img, int w, int h);

    void createMonoColorTable();

    uint32_t m_width= 0;
    uint32_t m_height= 0;
    uint32_t m_depth= 0;

    double m_devicePixelRatio= 1;
    std::vector<uint32_t> m_colorTable;
    uint32_t m_nbytes= 0;               // number of bytes data
    uint8_t* m_data = nullptr;
    Format m_format = Format::ARGB32;
    uint32_t m_bytesPerLine = 0;
    int m_serNo = 0;               // serial number
    int m_detachNo = 0;
    LPoint<int>  m_offset;           // offset in pixels
    double  m_dpmx = 0;                // dots per meter X (or 0)
    double  m_dpmy = 0;                // dots per meter Y (or 0)

    std::map<std::string, std::string> m_text;

    union{
        uint8_t m_imageFlags = 0;
        struct{
            uint8_t m_ownData:1;
            uint8_t m_roData:1;
            uint8_t m_hasAlphaClut:1;
            uint8_t m_isCached:1;
            uint8_t m_isLocked:1;
        };
    };

    std::shared_ptr<LPaintEngine>  m_paintEngine;
    friend class LImageConversions;
};



typedef void (*ImageConverter)(std::shared_ptr<LImage> dest,
                               const std::shared_ptr<LImage> src,
                               LFlags<LImageConvertFlag>);
//typedef bool (*InPlaceImageConverter)(std::shared_ptr<LImage> data,
//                                      LFlags<LImageConvertFlag>);

extern ImageConverter lImageConverterMap[(int)LImage::Format::NImageFormats][(int)LImage::Format::NImageFormats];
//extern InPlaceImageConverter lImageInplaceConverterMap[(int)LImage::Format::NImageFormats][(int)LImage::Format::NImageFormats];

LK_NAMESPACE_END
#endif // LIMAGE_H
