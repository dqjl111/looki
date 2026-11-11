#ifndef LCOLORTRCLUT_H
#define LCOLORTRCLUT_H

#include "lcolor.h"
#include <array>

LK_NAMESPACE_BEGIN

class LColorTransferFunction;
class LColorTransferTable;

class LKGUIDEF LColorTrcLut : public std::enable_shared_from_this<LColorTrcLut>
{
public:
    LColorTrcLut();

    static std::shared_ptr<LColorTrcLut> fromGamma(double gamma);
    static std::shared_ptr<LColorTrcLut> fromTransferFunction(const LColorTransferFunction &transfn);
    static std::shared_ptr<LColorTrcLut> fromTransferTable(const LColorTransferTable &transTable);


    uint32_t toLinear(uint32_t rgb32) const
    {
        return convertWithTable(rgb32, m_toLinear);
    }


    double u8ToLinearF32(int c) const
    {
        uint16_t v = m_toLinear[c << 4];
        return v * (1.0 / (255*256));
    }

    double u16ToLinearF32(int c) const
    {
        c -= (c >> 8);
        uint16_t v = m_toLinear[c >> 4];
        return v * (1.0 / (255*256));
    }

    double toLinear(double f) const
    {
        uint16_t v = m_toLinear[(int)(f * (255 * 16) + 0.5)];
        return v * (1.0 / (255*256));
    }

    uint32_t fromLinear(uint32_t rgb32) const
    {
        return convertWithTable(rgb32, m_fromLinear);
    }

    int u8FromLinearF32(double f) const
    {
        uint16_t v = m_fromLinear[(int)(f * (255 * 16) + 0.5)];
        return (v + 0x80) >> 8;
    }
    int u16FromLinearF32(double f) const
    {
        uint16_t v = m_fromLinear[(int)(f * (255 * 16) + 0.5)];
        return v + (v >> 8);
    }
    double fromLinear(double f) const
    {
        uint16_t v = m_fromLinear[(int)(f * (255 * 16) + 0.5)];
        return v * (1.0 / (255*256));
    }

    const std::array<uint16_t, 4081>& toLinear() const{
        return m_toLinear;
    }
    const std::array<uint16_t, 4081>& fromLinear() const{
        return m_fromLinear;
    }
private:   
    inline static uint32_t convertWithTable(uint32_t rgb32, const std::array<uint16_t, 4081>& table)
    {
        const int r = (table[LColor::uintToRed(rgb32) << 4] + 0x80) >> 8;
        const int g = (table[LColor::uintToGreen(rgb32) << 4] + 0x80) >> 8;
        const int b = (table[LColor::uintToBlue(rgb32) << 4] + 0x80) >> 8;
        return (rgb32 & 0xff000000) | (r << 16) | (g << 8) | b;
    }

    // We translate to 0-65280 (255*256) instead to 0-65535 to make simple
    // shifting an accurate conversion.
    // We translate from 0-4080 (255*16) for the same speed up, and to keep
    // the tables small enough to fit in most inner caches.
    std::array<uint16_t, 4081> m_toLinear; // [0-4080] -> [0-65280]
    std::array<uint16_t, 4081> m_fromLinear; // [0-4080] -> [0-65280]

};


LK_NAMESPACE_END

#endif // LCOLORTRCLUT_H
