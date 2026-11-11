#ifndef LCOLOR_H
#define LCOLOR_H

#include "lkcore/io/lnamedvar.h"
#include "lkgui/lguienums.h"

LK_NAMESPACE_BEGIN

struct LHsv{
    uint8_t m_alpha;
    uint16_t m_hue;
    uint8_t m_saturation;
    uint8_t m_value;
};


class LKGUIDEF LColor
{
    static std::vector<LNamedVar<uint32_t, LIDynNamedVar>> m_innerColors;
    static void initInnerColors();
public:
    LColor();
    LColor(const LColor& c);
    LColor(LColorID id);
    LColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);


    uint8_t m_alpha = 255;
    uint8_t m_red = 0;
    uint8_t m_green = 0;
    uint8_t m_blue = 0;

    static void colorIdToRgba(LColorID id, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);
    static const std::string& colorIdName(LColorID id);
    static const std::vector<LNamedVar<uint32_t, LIDynNamedVar>>& innerColors();
    static LColor fromColorName(const std::string& name, bool* isValid = nullptr);


    LColor &operator=(const LColor &s) = default;

    LColor& toLighter(int f = 150);
    LColor& toDarker(int f = 200);

    LColor lighter(int f = 150) const{
        LColor ret = *this;
        return ret.toLighter(f);
    }
    LColor darker(int f = 200) const{
        LColor ret = *this;
        return ret.toDarker(f);
    }

    void toHsv(LHsv& hsv) const;
    void fromHsv(LHsv& hsv);

    inline bool isOpaque() const
    {
        return m_alpha == 255;
    }

    inline bool isTransparent() const
    {
        return m_alpha == 0;
    }

    inline int rgbAvg(){
        return (m_red * 5 + m_green * 6 + m_blue * 5) / 16;
    }

    const LColor& multiAlpha(uint8_t alpha){
        double factor = alpha/255.0;
        m_red = (uint8_t)std::lround(factor*m_red);
        m_green = (uint8_t)std::lround(factor*m_green);
        m_blue = (uint8_t)std::lround(factor*m_blue);
        m_alpha = (uint8_t)std::lround(factor*m_alpha);
        return *this;
    }

    enum NameFormat { HexRgb, HexArgb };
    std::string name(NameFormat format = HexArgb);
    static LColor mixColors(LColor a, LColor b, double ratioA = 0.5);

    bool operator==(const LColor &) const;
    inline bool operator!=(const LColor &c) const{ return !(operator==(c)); }
    static uint32_t premultiply(uint32_t x);
    static uint32_t unpremultiply(uint32_t x);

    static uint64_t premultiply(uint64_t x);
    static uint64_t unpremultiply(uint64_t x);

    static uint32_t unpremultiplyRgb30(uint32_t rgb30);

    static inline uint32_t argbToUInt(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF)
    {
        return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }

    inline uint32_t toUInt() const{
        return argbToUInt(m_red, m_green, m_blue, m_alpha);
    }

    static inline uint64_t argbToUInt64(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF)
    {
        uint64_t ret = uint64_t(r) | (uint64_t(g) << 16) | (uint64_t(b) << 32) | (uint64_t(a) << 48) ;
        ret |= ret << 8;
        return ret;
    }

    static inline uint64_t argb16ToUInt64(uint16_t r, uint16_t g, uint16_t b, uint16_t a=0xFFFF)
    {
        return  uint64_t(r) | (uint64_t(g) << 16) | (uint64_t(b) << 32) | (uint64_t(a) << 48) ;
    }

    inline uint64_t toUInt64() const{
        return argbToUInt64(m_red, m_green, m_blue, m_alpha);
    }

    static inline uint32_t uintToRed(uint32_t rgb) // get red part of RGB
    { return ((rgb >> 16) & 0xff); }

    static inline uint32_t uintToGreen(uint32_t rgb) // get green part of RGB
    { return ((rgb >> 8) & 0xff); }

    static inline uint32_t uintToBlue(uint32_t rgb) // get blue part of RGB
    { return (rgb & 0xff); }

    static inline uint32_t uintToAlpha(uint32_t rgb) // get alpha part of RGBA
    { return rgb >> 24; }

    static inline LColor uintToColor(uint32_t value){
        return LColor(uint8_t((value >> 16) & 0xffu),
                      uint8_t((value >> 8) & 0xffu),
                      uint8_t(value & 0xffu),
                      uint8_t(value >> 24));
    }

    inline void fromUInt(uint32_t value){
        m_alpha = (value >> 24);
        m_red = (value >> 16) & 0xffu;
        m_green = (value >> 8) & 0xffu;
        m_blue = value & 0xffu;
    }

    inline void fromAbgr(uint32_t value){
        m_alpha = (value >> 24);
        m_blue = (value >> 16) & 0xffu;
        m_green = (value >> 8) & 0xffu;
        m_red = value & 0xffu;
    }

    //大端rgba转大端argb
    static inline uint32_t rgbaToArgb(uint32_t x) {
        uint32_t rgb = x >> 8;
        uint32_t a = x << 24;
        return a | rgb;
    }

    //大端argb转大端rgba
    static inline uint32_t argbToRgba(uint32_t x) {
        uint32_t rgb = x << 8;
        uint32_t a = x >> 24;
        return a | rgb;
    }

    //小端rgba转大端argb
    static inline uint32_t abgrToArgb(uint32_t x) {
        uint32_t ag = x & 0xff00ff00;
        uint32_t rb = x & 0x00ff00ff;
        return ag | (rb  << 16) | (rb >> 16);
    }

    //大端argb转小端rgba 与上同
    static inline uint32_t argbToAbgr(uint32_t x) {
        uint32_t ag = x & 0xff00ff00;
        uint32_t rb = x & 0x00ff00ff;
        return ag | (rb  << 16) | (rb >> 16);
    }

    static inline uint32_t bgr30ToArgb(uint32_t c)
    {
        uint32_t a = c >> 30;
        a |= a << 2;
        a |= a << 4;
        return (a << 24)
               | ((c << 14) & 0x00ff0000)
               | ((c >> 4) & 0x0000ff00)
               | ((c >> 22) & 0x000000ff);
    }

    static inline uint32_t rgb30ToArgb(uint32_t c)
    {
        uint32_t a = c >> 30;
        a |= a << 2;
        a |= a << 4;
        return (a << 24)
               | ((c >> 6) & 0x00ff0000)
               | ((c >> 4) & 0x0000ff00)
               | ((c >> 2) & 0x000000ff);
    }

    static inline uint32_t rgb16ToArgb(uint16_t c)
    {
        return 0xff000000
               | (((uint32_t(c) << 3) & 0xf8) | ((uint32_t(c) >> 2) & 0x7))
               | (((uint32_t(c) << 5) & 0xfc00) | ((uint32_t(c) >> 1) & 0x300))
               | (((uint32_t(c) << 8) & 0xf80000) | ((uint32_t(c) << 3) & 0x70000));
    }

    static inline uint16_t argbToRgb16(uint32_t c)
    {
        return (((c) >> 3) & 0x001f)
               | (((c) >> 5) & 0x07e0)
               | (((c) >> 8) & 0xf800);
    }

    static inline uint32_t argbToRgb30(uint32_t c)
    {
        return 0xc0000000
               | (((c << 6) & 0x3fc00000) | ((c >> 2) & 0x00300000))
               | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
               | (((c << 2) & 0x000003fc) | ((c >> 6) & 0x00000003));
    }

    static inline uint32_t argbToBgr30(uint32_t c)
    {
        return 0xc0000000
               | (((c << 22) & 0x3fc00000) | ((c << 14) & 0x00300000))
               | (((c << 4) & 0x000ff000) | ((c >> 4) & 0x00000c00))
               | (((c >> 14) & 0x000003fc) | ((c >> 22) & 0x00000003));
    }

    static uint16_t red16(uint64_t rgba)  { return uint16_t(rgba >> 0);   }
    static uint16_t green16(uint64_t rgba) { return uint16_t(rgba >> 16); }
    static uint16_t blue16(uint64_t rgba)  { return uint16_t(rgba >> 32);  }
    static uint16_t alpha16(uint64_t rgba) { return uint16_t(rgba >> 48); }

    static uint64_t setRed16(uint64_t rgba, uint16_t _red)     {
        rgba = (rgba & ~(uint64_t(0xffff)))  | (uint64_t(_red));
        return rgba;
    }
    static uint64_t setGreen16(uint64_t rgba, uint16_t _green) {
        rgba = (rgba & ~(uint64_t(0xffff) << 16)) | (uint64_t(_green) << 16);
        return rgba;
    }
    static uint64_t setBlue16(uint64_t rgba, uint16_t _blue)   {
        rgba = (rgba & ~(uint64_t(0xffff) << 32))  | (uint64_t(_blue) << 32);
        return rgba;
    }
    static uint64_t setAlpha16(uint64_t rgba, uint16_t _alpha) {
        rgba = (rgba & ~(uint64_t(0xffff) << 48)) | (uint64_t(_alpha) << 48);
        return rgba;
    }


    template<unsigned int Shift>
    static inline uint32_t repremultiply(uint32_t p)
    {
        const uint32_t alpha = p >>24;
        if (alpha == 255 || alpha == 0){
            return p;
        }
        p = unpremultiply(p);
        uint32_t mult = 255 / (255 >> Shift);
        const uint32_t newAlpha = mult * (alpha >> Shift);
        p = (p & ~0xff000000) | (newAlpha<<24);
        return premultiply(p);
    }

    template<unsigned int Shift>
    static inline uint64_t repremultiply(uint64_t p)
    {
        const uint32_t alpha = alpha16(p);
        if (alpha == 65535 || alpha == 0){
            return p;
        }
        p = unpremultiply(p);
        const uint32_t mult = 65535 / (65535 >> Shift);
        p = setAlpha16(p, mult * (alpha >> Shift));
        return premultiply(p);
    }

    static inline uint32_t argbToA2rgb30(uint32_t c)
    {
        c = repremultiply<6>(c);
        return argbToRgb30(c);
    }


    static inline uint32_t argbToA2bgr30(uint32_t c)
    {
        c = repremultiply<6>(c);
        return argbToBgr30(c);
    }

    static inline uint32_t rgbSwapRgb30(uint32_t c)
    {
        const uint32_t ag = c & 0xc00ffc00;
        const uint32_t rb = c & 0x3ff003ff;
        return ag | (rb << 20) | (rb >> 20);
    }

    static uint32_t argb64ToArgb(uint64_t c);
    static uint64_t argbToArgb64(uint32_t c);

    static inline uint32_t toGray(uint32_t r, uint32_t g, uint32_t b)// convert R,G,B to gray 0..255
    { return (r*11+g*16+b*5)/32; }

    static inline uint8_t toGray(uint32_t c)
    { return (uint8_t)toGray(LColor::uintToRed(c), LColor::uintToGreen(c), LColor::uintToBlue(c)); }

    static inline uint16_t toGray(uint64_t c)
    { return (uint16_t)toGray(LColor::red16(c), LColor::green16(c), LColor::blue16(c)); }

    static inline uint8_t toGray(const LColor& c)                // convert RGB to gray 0..255
    { return (uint8_t)toGray(c.m_red, c.m_green, c.m_blue); }

    static inline bool isGray(const LColor& c)
    { return c.m_red == c.m_green && c.m_red == c.m_blue; }

    static inline uint8_t div257Floor(uint32_t x) { return uint8_t((x - (x >> 8)) >> 8); }
    static inline uint8_t div257(uint16_t x) { return div257Floor(x + 128U); }
    static inline int div255(int x) { return (x + (x>>8) + 0x80) >> 8; }

    static inline bool isOpaque(uint32_t val)
    { return LColor::uintToAlpha(val) == 255; }

    static bool isTransparent(uint32_t val)
    { return LColor::uintToAlpha(val) == 0; }

    static inline int rgbAvg(uint32_t rgb)
    {
        return (uintToRed(rgb) * 5 + uintToGreen(rgb) * 6 + uintToBlue(rgb) * 5) / 16;
    }


};

#define lRed(x) LColor::uintToRed(x)
#define lGreen(x) LColor::uintToGreen(x)
#define lBlue(x) LColor::uintToBlue(x)
#define lAlpha(x) LColor::uintToAlpha(x)

LK_NAMESPACE_END
#endif // LCOLOR_H
