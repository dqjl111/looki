#ifndef LCOLORTRANSFERFUNCTION_H
#define LCOLORTRANSFERFUNCTION_H
#include "lcolor.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LColorTransferFunction
{
public:
    LColorTransferFunction()
        : m_a(1.0f), m_b(0.0f), m_c(1.0f), m_d(0.0f), m_e(0.0f), m_f(0.0f), m_g(1.0f), m_flags(0)
    { }
    LColorTransferFunction(float a, float b, float c, float d, float e, float f, float g)
        : m_a(a), m_b(b), m_c(c), m_d(d), m_e(e), m_f(f), m_g(g), m_flags(0)
    { }

    bool isGamma() const
    {
        updateHints();
        return m_flags & uint32_t(Hints::IsGamma);
    }
    bool isLinear() const
    {
        updateHints();
        return m_flags & uint32_t(Hints::IsLinear);
    }
    bool isSRgb() const
    {
        updateHints();
        return m_flags & uint32_t(Hints::IsSRgb);
    }

    float apply(float x) const
    {
        if (x < m_d)
            return m_c * x + m_f;
        else
            return std::pow(m_a * x + m_b, m_g) + m_e;
    }

    LColorTransferFunction inverted() const
    {
        float a, b, c, d, e, f, g;

        d = m_c * m_d + m_f;

        if (!lFuzzyIsZero(m_c)) {
            c = 1.0f / m_c;
            f = -m_f / m_c;
        } else {
            c = 0.0f;
            f = 0.0f;
        }

        if (!lFuzzyIsZero(m_a) && !lFuzzyIsZero(m_g)) {
            a = std::pow(1.0f / m_a, m_g);
            b = -a * m_e;
            e = -m_b / m_a;
            g = 1.0f / m_g;
        } else {
            a = 0.0f;
            b = 0.0f;
            e = 1.0f;
            g = 1.0f;
        }

        return LColorTransferFunction(a, b, c, d, e, f, g);
    }

    // A few predefined curves:
    static LColorTransferFunction fromGamma(float gamma)
    {
        return LColorTransferFunction(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gamma);
    }
    static LColorTransferFunction fromSRgb()
    {
        return LColorTransferFunction(1.0f / 1.055f, 0.055f / 1.055f, 1.0f / 12.92f, 0.04045f, 0.0f, 0.0f, 2.4f);
    }
    static LColorTransferFunction fromProPhotoRgb()
    {
        return LColorTransferFunction(1.0f, 0.0f, 1.0f / 16.0f, 16.0f / 512.0f, 0.0f, 0.0f, 1.8f);
    }
    bool matches(const LColorTransferFunction &o) const
    {
        return paramCompare(m_a, o.m_a) && paramCompare(m_b, o.m_b)
               && paramCompare(m_c, o.m_c) && paramCompare(m_d, o.m_d)
               && paramCompare(m_e, o.m_e) && paramCompare(m_f, o.m_f)
               && paramCompare(m_g, o.m_g);
    }
    friend inline bool operator==(const LColorTransferFunction &f1, const LColorTransferFunction &f2);
    friend inline bool operator!=(const LColorTransferFunction &f1, const LColorTransferFunction &f2);

    float m_a;
    float m_b;
    float m_c;
    float m_d;
    float m_e;
    float m_f;
    float m_g;
private:
    static inline bool paramCompare(float p1, float p2)
    {
        // Much fuzzier than fuzzy compare.
        // It tries match parameters that has been passed through a 8.8
        // fixed point form.
        return (std::abs(p1 - p2) <= (1.0f / 512.0f));
    }

    void updateHints() const
    {
        if (m_flags & uint32_t(Hints::Calculated))
            return;
        // We do not consider the case with m_d = 1.0f linear or simple,
        // since it wouldn't be linear for applyExtended().
        bool simple = paramCompare(m_a, 1.0f) && paramCompare(m_b, 0.0f)
                      && paramCompare(m_d, 0.0f)
                      && paramCompare(m_e, 0.0f);
        if (simple) {
            m_flags |= uint32_t(Hints::IsGamma);
            if (lFuzzyCompare(m_g, 1.0f))
                m_flags |= uint32_t(Hints::IsLinear);
        } else {
            if (*this == fromSRgb())
                m_flags |= uint32_t(Hints::IsSRgb);
        }
        m_flags |= uint32_t(Hints::Calculated);
    }
    enum class Hints : uint32_t {
        Calculated = 1,
        IsGamma = 2,
        IsLinear = 4,
        IsSRgb = 8
    };
    mutable uint32_t m_flags;
};

inline bool operator==(const LColorTransferFunction &f1, const LColorTransferFunction &f2)
{
    return f1.matches(f2);
}
inline bool operator!=(const LColorTransferFunction &f1, const LColorTransferFunction &f2)
{
    return !f1.matches(f2);
}


LK_NAMESPACE_END
#endif // LCOLORTRANSFERFUNCTION_H
