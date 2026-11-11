#ifndef LCOLORTRANSFERTABLE_H
#define LCOLORTRANSFERTABLE_H
#include "lcolortransferfunction.h"
#include <vector>

LK_NAMESPACE_BEGIN

class LKGUIDEF LColorTransferTable
{
public:
    LColorTransferTable() noexcept
        : m_tableSize(0)
    { }
    LColorTransferTable(uint32_t size, const std::vector<uint8_t> &table) noexcept
        : m_tableSize(std::min<uint32_t>(size, table.size()))
        , m_table8(table)
    {
    }

    LColorTransferTable(uint32_t size, const std::vector<uint16_t> &table) noexcept
        : m_tableSize(std::min<uint32_t>(size, table.size()))
        , m_table16(table)
    {
    }

    bool isEmpty() const
    {
        return m_tableSize == 0;
    }

    bool checkValidity() const
    {
        if (isEmpty()){
            return true;
        }
        // Only one table can be set
        if (!m_table8.empty() && !m_table16.empty()){
            return false;
        }
        // At least 2 elements
        if (m_tableSize < 2){
            return false;
        }
        // The table must describe an injective curve:
        if (!m_table8.empty()) {
            uint8_t val = 0;
            for (uint32_t i = 0; i < m_tableSize; ++i) {
                if (m_table8[i] < val){
                    return false;
                }
                val = m_table8[i];
            }
        }
        if (!m_table16.empty()) {
            uint16_t val = 0;
            for (uint32_t i = 0; i < m_tableSize; ++i) {
                if (m_table16[i] < val){
                    return false;
                }
                val = m_table16[i];
            }
        }
        return true;
    }

    double apply(double x) const
    {
        x = std::min(std::max(x, 0.0), 1.0);
        x *= m_tableSize - 1;
        uint32_t lo = std::lround(std::floor(x));
        uint32_t hi = std::min(lo + 1, m_tableSize - 1);
        double frac = x - lo;
        if (!m_table16.empty()){
            return (m_table16[lo] * (1.0 - frac) + m_table16[hi] * frac) * (1.0/65535.0);
        }
        if (!m_table8.empty()){
            return (m_table8[lo] * (1.0 - frac) + m_table8[hi] * frac) * (1.0/255.0);
        }
        return x;
    }

    // Apply inverse, optimized by giving a previous result a value < x.
    double applyInverse(double x, double resultLargerThan = 0.0f) const
    {
//        std::assert(resultLargerThan >= 0.0f && resultLargerThan <= 1.0f);
        if (x <= 0.0){
            return 0.0;
        }
        if (x >= 1.0){
            return 1.0;
        }
        if (!m_table16.empty()) {
            double v = x * 65535.0;
            uint32_t i = std::floor(resultLargerThan * (m_tableSize - 1)) + 1;
            for ( ; i < m_tableSize; ++i) {
                if (m_table16[i] > v){
                    break;
                }
            }
            if (i >= m_tableSize - 1){
                return 1.0;
            }
            double y1 = m_table16[i - 1];
            double y2 = m_table16[i];

            double fr = (v - y1) / (y2 - y1);
            return (i + fr) * (1.0 / (m_tableSize - 1));

        }
        if (!m_table8.empty()) {
            double v = x * 255.0;
            uint32_t i = std::floor(resultLargerThan * (m_tableSize - 1)) + 1;
            for ( ; i < m_tableSize; ++i) {
                if (m_table8[i] > v){
                    break;
                }
            }
            if (i >= m_tableSize - 1){
                return 1.0;
            }
            double y1 = m_table8[i - 1];
            double y2 = m_table8[i];

            double fr = (v - y1) / (y2 - y1);
            return (i + fr) * (1.0 / (m_tableSize - 1));
        }
        return x;
    }

    bool asColorTransferFunction(LColorTransferFunction* transferFn)
    {
        if (m_tableSize < 2){
            return false;
        }
        if (!m_table8.empty() && (m_table8[0] != 0 || m_table8[m_tableSize - 1] != 255)){
            return false;
        }
        if (!m_table16.empty() && (m_table16[0] != 0 || m_table16[m_tableSize - 1] != 65535)){
            return false;
        }
        if (m_tableSize == 2) {
            *transferFn = LColorTransferFunction(); // Linear
            return true;
        }
        // The following heuristics are based on those from Skia:
        if (m_tableSize == 26 && !m_table16.empty()) {
            // code.facebook.com/posts/411525055626587/under-the-hood-improving-facebook-photos
            if (m_table16[6] != 3062){
                return false;
            }
            if (m_table16[12] != 12824){
                return false;
            }
            if (m_table16[18] != 31237){
                return false;
            }
            *transferFn = LColorTransferFunction::fromSRgb();
            return true;
        }
        if (m_tableSize == 1024 && !m_table16.empty()) {
            // HP and Canon sRGB gamma tables:
            if (m_table16[257] != 3366){
                return false;
            }
            if (m_table16[513] != 14116){
                return false;
            }
            if (m_table16[768] != 34318){
                return false;
            }
            *transferFn = LColorTransferFunction::fromSRgb();
            return true;
        }
        if (m_tableSize == 4096 && !m_table16.empty()) {
            // Nikon, Epson, and lcms2 sRGB gamma tables:
            if (m_table16[515] != 960){
                return false;
            }
            if (m_table16[1025] != 3342){
                return false;
            }
            if (m_table16[2051] != 14079){
                return false;
            }
            *transferFn = LColorTransferFunction::fromSRgb();
            return true;
        }
        return false;
    }
    friend inline bool operator!=(const LColorTransferTable &t1, const LColorTransferTable &t2);
    friend inline bool operator==(const LColorTransferTable &t1, const LColorTransferTable &t2);

    uint32_t m_tableSize;
    std::vector<uint8_t> m_table8;
    std::vector<uint16_t> m_table16;
};

inline bool operator!=(const LColorTransferTable &t1, const LColorTransferTable &t2)
{
    if (t1.m_tableSize != t2.m_tableSize){
        return true;
    }
    if (t1.m_table8.empty() != t2.m_table8.empty()){
        return true;
    }
    if (t1.m_table16.empty() != t2.m_table16.empty()){
        return true;
    }
    if (!t1.m_table8.empty()) {
        for (uint32_t i = 0; i < t1.m_tableSize; ++i) {
            if (t1.m_table8[i] != t2.m_table8[i]){
                return true;
            }
        }
    }
    if (!t1.m_table16.empty()) {
        for (uint32_t i = 0; i < t1.m_tableSize; ++i) {
            if (t1.m_table16[i] != t2.m_table16[i]){
                return true;
            }
        }
    }
    return false;
}

inline bool operator==(const LColorTransferTable &t1, const LColorTransferTable &t2)
{
    return !(t1 != t2);
}

LK_NAMESPACE_END

#endif // LCOLORTRANSFERTABLE_H
