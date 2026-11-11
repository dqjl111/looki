#include "ldrawhelper.h"

LK_NAMESPACE_BEGIN

static inline uint32_t interpolatePixel255(uint32_t x, uint32_t a, uint32_t y, uint32_t b) {
    uint32_t t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x = (x + ((x >> 8) & 0xff00ff) + 0x800080);
    x &= 0xff00ff00;
    x |= t;
    return x;
}


static const uint32_t AMASK = 0xff000000;
static const uint32_t RMASK = 0x00ff0000;
static const uint32_t GMASK = 0x0000ff00;
static const uint32_t BMASK = 0x000000ff;

#define AMIX(mask) uint32_t(std::min<uint64_t>(((uint64_t(s)&mask) + (uint64_t(d)&mask)), uint64_t(mask)))
#define MIX(mask) (std::min<uint32_t>(((uint32_t(s)&mask) + (uint32_t(d)&mask)), uint32_t(mask)))

inline uint32_t compFuncPlusOnePixel(uint32_t d, const uint32_t s)
{
    const uint32_t result = uint32_t(AMIX(AMASK) | MIX(RMASK) | MIX(GMASK) | MIX(BMASK));
    return result;
}

#undef AMIX
#undef MIX

struct Argb32OperationsC
{
    typedef uint32_t Type;
    typedef uint32_t Scalar;
    typedef uint32_t OptimalType;
    typedef uint32_t OptimalScalar;

    static const Type clear;
    static bool isOpaque(Type val)
    { return LColor::isOpaque(val); }
    static bool isTransparent(Type val)
    { return LColor::isTransparent(val) == 0; }
    static Scalar scalarFrom8bit(uint32_t a)
    { return a; }
    static void memfill(Type *ptr, Type value, lsizetype len)
    {
        lMemFillTemplate(ptr, value, len);
    }
    static void memcpy(Type * dest, const Type * src, lsizetype len)
    { ::memcpy((char*)dest, src, len * sizeof(Type)); }

    static OptimalType load(const Type *ptr)
    { return *ptr; }
    static OptimalType convert(const Type &val)
    { return val; }
    static void store(Type *ptr, OptimalType value)
    { *ptr = value; }
    static OptimalType add(OptimalType a, OptimalType b)
    { return a + b; }
//    static OptimalScalar add(OptimalScalar a, OptimalScalar b)
//    { return a + b; }
    static OptimalType plus(OptimalType a, OptimalType b)
    { return compFuncPlusOnePixel(a, b); }
    static OptimalScalar alpha(OptimalType val)
    { return LColor::uintToAlpha(val); }
    static OptimalScalar invAlphaByte(OptimalScalar c)
    { return 255 - c; }
    static OptimalScalar invAlpha(OptimalType val)
    { return alpha(~val); }
    static OptimalScalar scalar(Scalar v)
    { return v; }
    static OptimalType multiplyAlpha8bit(OptimalType val, uint32_t a)
    { return draw::lByteMul(val, a); }
    static OptimalType interpolate8bit(OptimalType x, uint32_t a1, OptimalType y, uint32_t a2)
    { return interpolatePixel255(x, a1, y, a2); }
    static OptimalType multiplyAlpha(OptimalType val, OptimalScalar a)
    { return draw::lByteMul(val, a); }
//    static OptimalScalar multiplyAlpha8bit(OptimalScalar val, uint32_t a)
//    { return LColor::div255(val * a); }
    static OptimalType interpolate(OptimalType x, OptimalScalar a1, OptimalType y, OptimalScalar a2)
    { return interpolatePixel255(x, a1, y, a2); }
};

const Argb32OperationsC::Type Argb32OperationsC::clear = 0;
typedef Argb32OperationsC Argb32Operations;

/*
  result = 0
  d = d * cia
*/
template<class Ops>
inline static void compFuncClearTemplate(typename Ops::Type *dest, int length, uint32_t const_alpha)
{
    if (const_alpha == 255)
        Ops::memfill(dest, Ops::clear, length);
    else {
        uint32_t ialpha = 255 - const_alpha;
        for (int i = 0; i < length; ++i) {
            Ops::store(&dest[i], Ops::multiplyAlpha8bit(Ops::load(&dest[i]), ialpha));
        }
    }
}

void  compFuncSolidClear(uint32_t *dest, int length, uint32_t, uint32_t const_alpha)
{
    compFuncClearTemplate<Argb32Operations>(dest, length, const_alpha);
}

void  compFuncClear(uint32_t *dest, const uint32_t *, int length, uint32_t const_alpha)
{
    compFuncClearTemplate<Argb32Operations>(dest, length, const_alpha);
}

/*
  result = s
  dest = s * ca + d * cia
*/
template<class Ops>
inline static void compFuncSolidSourceTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    if (const_alpha == 255)
        Ops::memfill(dest, color, length);
    else {
        const uint32_t ialpha = 255 - const_alpha;
        auto s = Ops::multiplyAlpha8bit(Ops::convert(color), const_alpha);
        for (int i = 0; i < length; ++i) {
            auto d = Ops::multiplyAlpha8bit(Ops::load(&dest[i]), ialpha);
            Ops::store(&dest[i], Ops::add(s, d));
        }
    }
}

template<class Ops>
inline static void compFuncSourceTemplate(typename Ops::Type * dest,
                                             const typename Ops::Type * src,
                                             int length, uint32_t const_alpha)
{
    if (const_alpha == 255)
        Ops::memcpy(dest, src, length);
    else {
        const uint32_t ialpha = 255 - const_alpha;
        for (int i = 0; i < length; ++i) {
            auto s = Ops::load(src + i);
            auto d = Ops::load(dest + i);
            Ops::store(&dest[i], Ops::interpolate8bit(s, const_alpha, d, ialpha));
        }
    }
}

void  compFuncSolidSource(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidSourceTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncSource(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncSourceTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

void  compFuncSolidDestination(uint32_t *, int, uint32_t, uint32_t)
{
}

void  compFuncDestination(uint32_t *, const uint32_t *, int, uint32_t)
{
}

/*
  result = s + d * sia
  dest = (s + d * sia) * ca + d * cia
       = s * ca + d * (sia * ca + cia)
       = s * ca + d * (1 - sa*ca)
*/
template<class Ops>
inline static void compFuncSolidSourceOverTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    if (const_alpha == 255 && Ops::isOpaque(color))
        Ops::memfill(dest, color, length);
    else {
        auto c = Ops::convert(color);
        if (const_alpha != 255)
            c = Ops::multiplyAlpha8bit(c, const_alpha);
        auto cAlpha = Ops::invAlpha(c);
        for (int i = 0; i < length; ++i) {
            auto d = Ops::multiplyAlpha(Ops::load(&dest[i]), cAlpha);
            Ops::store(&dest[i], Ops::add(c, d));
        }
    }
}

template<class Ops>
inline static void compFuncSourceOverTemplate(typename Ops::Type * dest,
                                                 const typename Ops::Type * src,
                                                 int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto c = src[i];
            if (Ops::isOpaque(c))
                Ops::store(&dest[i], Ops::convert(c));
            else if (!Ops::isTransparent(c)) {
                auto s = Ops::convert(c);
                auto d = Ops::multiplyAlpha(Ops::load(&dest[i]), Ops::invAlpha(s));
                Ops::store(&dest[i], Ops::add(s, d));
            }
        }
    } else {
        for (int i = 0; i < length; ++i) {
            auto s = Ops::multiplyAlpha8bit(Ops::load(&src[i]), const_alpha);
            auto d = Ops::multiplyAlpha(Ops::load(&dest[i]), Ops::invAlpha(s));
            Ops::store(&dest[i], Ops::add(s, d));
        }
    }
}

void  compFuncSolidSourceOver(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidSourceOverTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncSourceOver(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncSourceOverTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = d + s * dia
  dest = (d + s * dia) * ca + d * cia
       = d + s * dia * ca
*/
template<class Ops>
inline static void compFuncSolidDestinationOverTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto c = Ops::convert(color);
    if (const_alpha != 255)
        c = Ops::multiplyAlpha8bit(c, const_alpha);
    for (int i = 0; i < length; ++i) {
        auto d = Ops::load(&dest[i]);
        auto s = Ops::multiplyAlpha(c, Ops::invAlpha(d));
        Ops::store(&dest[i], Ops::add(s, d));
    }
}

template<class Ops>
inline static void compFuncDestinationOverTemplate(typename Ops::Type * dest,
                                                      const typename Ops::Type * src,
                                                      int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::multiplyAlpha(Ops::load(&src[i]), Ops::invAlpha(d));
            Ops::store(&dest[i], Ops::add(s, d));
        }
    } else {
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::multiplyAlpha8bit(Ops::load(&src[i]), const_alpha);
            s = Ops::multiplyAlpha(s, Ops::invAlpha(d));
            Ops::store(&dest[i], Ops::add(s, d));
        }
    }
}

void  compFuncSolidDestinationOver(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidDestinationOverTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncDestinationOver(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncDestinationOverTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = s * da
  dest = s * da * ca + d * cia
*/
template<class Ops>
inline static void compFuncSolidSourceInTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        auto c = Ops::convert(color);
        for (int i = 0; i < length; ++i) {
            Ops::store(&dest[i], Ops::multiplyAlpha(c, Ops::alpha(Ops::load(&dest[i]))));
        }
    } else {
        auto c = Ops::multiplyAlpha8bit(Ops::convert(color), const_alpha);
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::interpolate(c, Ops::alpha(d), d, cia));
        }
    }
}

template<class Ops>
inline static void compFuncSourceInTemplate(typename Ops::Type * dest,
                                               const typename Ops::Type * src,
                                               int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto s = Ops::load(&src[i]);
            Ops::store(&dest[i], Ops::multiplyAlpha(s, Ops::alpha(Ops::load(&dest[i]))));
        }
    } else {
        auto ca = Ops::scalarFrom8bit(const_alpha);
        auto cia = Ops::invAlpha(ca);
        auto cav = Ops::scalar(ca);
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::multiplyAlpha(Ops::load(&src[i]), cav);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::alpha(d), d, cia));
        }
    }
}

void  compFuncSolidSourceIn(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidSourceInTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncSourceIn(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncSourceInTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = d * sa
  dest = d * sa * ca + d * cia
       = d * (sa * ca + cia)
*/
template<class Ops>
inline static void compFuncSolidDestinationInTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto sa = Ops::alpha(Ops::convert(color));
    if (const_alpha != 255) {
        sa = Ops::multiplyAlpha8bit(sa, const_alpha);
        sa = Ops::add(sa, Ops::invAlpha(Ops::scalarFrom8bit(const_alpha)));
    }

    for (int i = 0; i < length; ++i) {
        Ops::store(&dest[i], Ops::multiplyAlpha(Ops::load(&dest[i]), sa));
    }
}

template<class Ops>
inline static void compFuncDestinationInTemplate(typename Ops::Type * dest,
                                                    const typename Ops::Type * src,
                                                    int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto a = Ops::alpha(Ops::load(&src[i]));
            Ops::store(&dest[i], Ops::multiplyAlpha(Ops::load(&dest[i]), a));
        }
    } else {
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        for (int i = 0; i < length; ++i) {
            auto sa = Ops::multiplyAlpha8bit(Ops::alpha(Ops::load(&src[i])), const_alpha);
            sa = Ops::add(sa, cia);
            Ops::store(&dest[i], Ops::multiplyAlpha(Ops::load(&dest[i]), sa));
        }
    }
}

void  compFuncSolidDestinationIn(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidDestinationInTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncDestinationIn(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncDestinationInTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = s * dia
  dest = s * dia * ca + d * cia
*/
template<class Ops>
inline static void compFuncSolidSourceOutTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto c = Ops::convert(color);
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i)
            Ops::store(&dest[i], Ops::multiplyAlpha(c, Ops::invAlpha(Ops::load(&dest[i]))));
    } else {
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        c = Ops::multiplyAlpha8bit(c, const_alpha);
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::interpolate(c, Ops::invAlpha(d), d, cia));
        }
    }
}

template<class Ops>
inline static void compFuncSourceOutTemplate(typename Ops::Type * dest,
                                                const typename Ops::Type * src,
                                                int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto s = Ops::load(&src[i]);
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::multiplyAlpha(s, Ops::invAlpha(d)));
        }
    } else {
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        for (int i = 0; i < length; ++i) {
            auto s = Ops::multiplyAlpha8bit(Ops::load(&src[i]), const_alpha);
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::invAlpha(d), d, cia));
        }
    }
}

void  compFuncSolidSourceOut(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidSourceOutTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncSourceOut(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncSourceOutTemplate<Argb32Operations>(dest, src, length, const_alpha);
}


/*
  result = d * sia
  dest = d * sia * ca + d * cia
       = d * (sia * ca + cia)
*/
template<class Ops>
inline static void compFuncSolidDestinationOutTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto sai = Ops::invAlpha(Ops::convert(color));
    if (const_alpha != 255) {
        sai = Ops::multiplyAlpha8bit(sai, const_alpha);
        sai = Ops::add(sai, Ops::invAlpha(Ops::scalarFrom8bit(const_alpha)));
    }

    for (int i = 0; i < length; ++i) {
        Ops::store(&dest[i], Ops::multiplyAlpha(Ops::load(&dest[i]), sai));
    }
}

template<class Ops>
inline static void compFuncDestinationOutTemplate(typename Ops::Type * dest,
                                                     const typename Ops::Type * src,
                                                     int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto sia = Ops::invAlpha(Ops::load(&src[i]));
            Ops::store(&dest[i], Ops::multiplyAlpha(Ops::load(&dest[i]), sia));
        }
    } else {
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        for (int i = 0; i < length; ++i) {
            auto sia = Ops::multiplyAlpha8bit(Ops::invAlpha(Ops::load(&src[i])), const_alpha);
            sia = Ops::add(sia, cia);
            Ops::store(&dest[i], Ops::multiplyAlpha(Ops::load(&dest[i]), sia));
        }
    }
}

void  compFuncSolidDestinationOut(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidDestinationOutTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncDestinationOut(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncDestinationOutTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = s*da + d*sia
  dest = s*da*ca + d*sia*ca + d *cia
       = s*ca * da + d * (sia*ca + cia)
       = s*ca * da + d * (1 - sa*ca)
*/
template<class Ops>
inline static void compFuncSolidSourceAtopTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto c = Ops::convert(color);
    if (const_alpha != 255)
        c = Ops::multiplyAlpha8bit(c, const_alpha);
    auto sia = Ops::invAlpha(c);
    for (int i = 0; i < length; ++i) {
        auto d = Ops::load(&dest[i]);
        Ops::store(&dest[i], Ops::interpolate(c, Ops::alpha(d), d, sia));
    }
}

template<class Ops>
inline static void compFuncSourceAtopTemplate(typename Ops::Type * dest,
                                                 const typename Ops::Type * src,
                                                 int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto s = Ops::load(&src[i]);
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::alpha(d), d, Ops::invAlpha(s)));
        }
    } else {
        for (int i = 0; i < length; ++i) {
            auto s = Ops::multiplyAlpha8bit(Ops::load(&src[i]), const_alpha);
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::alpha(d), d, Ops::invAlpha(s)));
        }
    }
}

void  compFuncSolidSourceAtop(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidSourceAtopTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncSourceAtop(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncSourceAtopTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = d*sa + s*dia
  dest = d*sa*ca + s*dia*ca + d *cia
       = s*ca * dia + d * (sa*ca + cia)
*/
template<class Ops>
inline static void compFuncSolidDestinationAtopTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto c = Ops::convert(color);
    auto sa = Ops::alpha(c);
    if (const_alpha != 255) {
        c = Ops::multiplyAlpha8bit(c, const_alpha);
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        sa = Ops::add(Ops::alpha(c), cia);
    }

    for (int i = 0; i < length; ++i) {
        auto d = Ops::load(&dest[i]);
        Ops::store(&dest[i], Ops::interpolate(c, Ops::invAlpha(d), d, sa));
    }
}

template<class Ops>
inline static void compFuncDestinationAtopTemplate(typename Ops::Type * dest,
                                                      const typename Ops::Type * src,
                                                      int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto s = Ops::load(&src[i]);
            auto d = Ops::load(&dest[i]);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::invAlpha(d), d, Ops::alpha(s)));
        }
    } else {
        auto cia = Ops::invAlpha(Ops::scalarFrom8bit(const_alpha));
        for (int i = 0; i < length; ++i) {
            auto s = Ops::multiplyAlpha8bit(Ops::load(&src[i]), const_alpha);
            auto d = Ops::load(&dest[i]);
            auto sa = Ops::add(Ops::alpha(s), cia);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::invAlpha(d), d, sa));
        }
    }
}

void  compFuncSolidDestinationAtop(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidDestinationAtopTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncDestinationAtop(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncDestinationAtopTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
  result = d*sia + s*dia
  dest = d*sia*ca + s*dia*ca + d *cia
       = s*ca * dia + d * (sia*ca + cia)
       = s*ca * dia + d * (1 - sa*ca)
*/
template<class Ops>
inline static void compFuncSolidXORTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto c = Ops::convert(color);
    if (const_alpha != 255)
        c = Ops::multiplyAlpha8bit(c, const_alpha);

    auto sia = Ops::invAlpha(c);
    for (int i = 0; i < length; ++i) {
        auto d = Ops::load(&dest[i]);
        Ops::store(&dest[i], Ops::interpolate(c, Ops::invAlpha(d), d, sia));
    }
}

template<class Ops>
inline static void compFuncXORTemplate(typename Ops::Type * dest,
                                          const typename Ops::Type * src,
                                          int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::load(&src[i]);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::invAlpha(d), d, Ops::invAlpha(s)));
        }
    } else {
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::multiplyAlpha8bit(Ops::load(&src[i]), const_alpha);
            Ops::store(&dest[i], Ops::interpolate(s, Ops::invAlpha(d), d, Ops::invAlpha(s)));
        }
    }
}

void  compFuncSolidXOR(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidXORTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncXOR(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncXORTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

struct LFullCoverage {
    inline void store(uint32_t *dest, const uint32_t src) const
    {
        *dest = src;
    }
};

struct LPartialCoverage {
    inline LPartialCoverage(uint32_t const_alpha)
        : ca(const_alpha)
        , ica(255 - const_alpha)
    {
    }

    inline void store(uint32_t *dest, const uint32_t src) const
    {
        *dest = interpolatePixel255(src, ca, *dest, ica);
    }

private:
    const uint32_t ca;
    const uint32_t ica;
};

static inline int mixAlpha(int da, int sa)
{
    return 255 - ((255 - sa) * (255 - da) >> 8);
}

/*
    Dca' = Sca.Da + Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa)
         = Sca + Dca
*/
template<class Ops>
inline static void compFuncSolidPlusTemplate(typename Ops::Type *dest, int length, typename Ops::Type color, uint32_t const_alpha)
{
    auto c = Ops::convert(color);
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            d = Ops::plus(d, c);
            Ops::store(&dest[i], d);
        }
    } else {
        uint32_t ia = 255 - const_alpha;
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            d = Ops::interpolate8bit(Ops::plus(d, c), const_alpha, d, ia);
            Ops::store(&dest[i], d);
        }
    }
}

template<class Ops>
inline static void compFuncPlusTemplate(typename Ops::Type * dest,
                                           const typename Ops::Type * src,
                                           int length, uint32_t const_alpha)
{
    if (const_alpha == 255) {
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::load(&src[i]);
            d = Ops::plus(d, s);
            Ops::store(&dest[i], d);
        }
    } else {
        uint32_t ia = 255 - const_alpha;
        for (int i = 0; i < length; ++i) {
            auto d = Ops::load(&dest[i]);
            auto s = Ops::load(&src[i]);
            d = Ops::interpolate8bit(Ops::plus(d, s), const_alpha, d, ia);
            Ops::store(&dest[i], d);
        }
    }
}

void  compFuncSolidPlus(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    compFuncSolidPlusTemplate<Argb32Operations>(dest, length, color, const_alpha);
}

void  compFuncPlus(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    compFuncPlusTemplate<Argb32Operations>(dest, src, length, const_alpha);
}

/*
    Dca' = Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
*/
static inline int multiplyOp(int dst, int src, int da, int sa)
{
    return LColor::div255(src * dst + src * (255 - da) + dst * (255 - sa));
}

template <typename T>
static inline void compFuncSolidMultiplyImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) multiplyOp(a, b, da, sa)
        int r = OP(LColor::uintToRed(d), sr);
        int b = OP(LColor::uintToBlue(d), sb);
        int g = OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidMultiply(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255)
        compFuncSolidMultiplyImpl(dest, length, color, LFullCoverage());
    else
        compFuncSolidMultiplyImpl(dest, length, color, LPartialCoverage(const_alpha));
}

template <typename T>
static inline void compFuncMultiplyImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) multiplyOp(a, b, da, sa)
        int r = OP(LColor::uintToRed(d),LColor::uintToRed(s));
        int b = OP(LColor::uintToBlue(d),LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d),LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncMultiply(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255)
        compFuncMultiplyImpl(dest, src, length, LFullCoverage());
    else
        compFuncMultiplyImpl(dest, src, length, LPartialCoverage(const_alpha));
}

/*
    Dca' = (Sca.Da + Dca.Sa - Sca.Dca) + Sca.(1 - Da) + Dca.(1 - Sa)
         = Sca + Dca - Sca.Dca
*/
template <typename T>
static inline void compFuncSolidScreenImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) 255 - LColor::div255((255-a) * (255-b))
        int r = OP(LColor::uintToRed(d), sr);
        int b = OP( LColor::uintToBlue(d), sb);
        int g = OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidScreen(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidScreenImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidScreenImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncScreenImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) 255 - (((255-a) * (255-b)) >> 8)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncScreen(uint32_t *dest, const uint32_t *src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncScreenImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncScreenImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
    if 2.Dca < Da
        Dca' = 2.Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
    otherwise
        Dca' = Sa.Da - 2.(Da - Dca).(Sa - Sca) + Sca.(1 - Da) + Dca.(1 - Sa)
*/
static inline int overlayOp(int dst, int src, int da, int sa)
{
    const int temp = src * (255 - da) + dst * (255 - sa);
    if (2 * dst < da){
        return LColor::div255(2 * src * dst + temp);
    }else{
        return LColor::div255(sa * da - 2 * (da - dst) * (sa - src) + temp);
    }
}

template <typename T>
static inline void compFuncSolidOverlayImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) overlayOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d), sr);
        int b = OP( LColor::uintToBlue(d), sb);
        int g = OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidOverlay(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidOverlayImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidOverlayImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}


template <typename T>
static inline void compFuncOverlayImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) overlayOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncOverlay(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncOverlayImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncOverlayImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
    Dca' = min(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
    Da'  = Sa + Da - Sa.Da
*/
static inline int darkenOp(int dst, int src, int da, int sa)
{
    return LColor::div255(std::min<int>(src * da, dst * sa) + src * (255 - da) + dst * (255 - sa));
}

template <typename T>
static inline void compFuncSolidDarkenImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) darkenOp(a, b, da, sa)
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidDarken(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidDarkenImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidDarkenImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncDarkenImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) darkenOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncDarken(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncDarkenImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncDarkenImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}


/*
   Dca' = max(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
   Da'  = Sa + Da - Sa.Da
*/
static inline int lightenOp(int dst, int src, int da, int sa)
{
    return LColor::div255(std::max<int>(src * da, dst * sa) + src * (255 - da) + dst * (255 - sa));
}

template <typename T>
static inline void compFuncSolidLightenImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) lightenOp(a, b, da, sa)
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidLighten(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidLightenImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidLightenImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}


template <typename T>
static inline void compFuncLightenImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) lightenOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncLighten(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncLightenImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncLightenImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
   if Sca.Da + Dca.Sa > Sa.Da
       Dca' = Sa.Da + Sca.(1 - Da) + Dca.(1 - Sa)
   else if Sca == Sa
       Dca' = Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa)
   otherwise
       Dca' = Dca.Sa/(1-Sca/Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
*/
static inline int colorDodgeOp(int dst, int src, int da, int sa)
{
    const int sa_da = sa * da;
    const int dst_sa = dst * sa;
    const int src_da = src * da;

    const int temp = src * (255 - da) + dst * (255 - sa);
    if (src_da + dst_sa > sa_da){
        return LColor::div255(sa_da + temp);
    }
    else if (src == sa || sa == 0){
        return LColor::div255(temp);
    }
    else{
        return LColor::div255(255 * dst_sa / (255 - 255 * src / sa) + temp);
    }
}

template <typename T>
static inline void compFuncSolidColorDodgeImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a,b) colorDodgeOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d), sr);
        int b = OP( LColor::uintToBlue(d), sb);
        int g = OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidColorDodge(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidColorDodgeImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidColorDodgeImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncColorDodgeImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) colorDodgeOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncColorDodge(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncColorDodgeImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncColorDodgeImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
   if Sca.Da + Dca.Sa < Sa.Da
       Dca' = Sca.(1 - Da) + Dca.(1 - Sa)
   else if Sca == 0
       Dca' = Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa)
   otherwise
       Dca' = Sa.(Sca.Da + Dca.Sa - Sa.Da)/Sca + Sca.(1 - Da) + Dca.(1 - Sa)
*/
static inline int colorBurnOp(int dst, int src, int da, int sa)
{
    const int src_da = src * da;
    const int dst_sa = dst * sa;
    const int sa_da = sa * da;

    const int temp = src * (255 - da) + dst * (255 - sa);

    if (src_da + dst_sa < sa_da){
        return LColor::div255(temp);
    }else if (src == 0){
        return LColor::div255(dst_sa + temp);
    }
    return LColor::div255(sa * (src_da + dst_sa - sa_da) / src + temp);
}

template <typename T>
static inline void compFuncSolidColorBurnImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) colorBurnOp(a, b, da, sa)
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidColorBurn(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidColorBurnImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidColorBurnImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncColorBurnImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) colorBurnOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncColorBurn(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncColorBurnImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncColorBurnImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
    if 2.Sca < Sa
        Dca' = 2.Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
    otherwise
        Dca' = Sa.Da - 2.(Da - Dca).(Sa - Sca) + Sca.(1 - Da) + Dca.(1 - Sa)
*/
static inline uint32_t hardLightOp(int dst, int src, int da, int sa)
{
    const uint32_t temp = src * (255 - da) + dst * (255 - sa);

    if (2 * src < sa){
        return LColor::div255(2 * src * dst + temp);
    }else{
        return LColor::div255(sa * da - 2 * (da - dst) * (sa - src) + temp);
    }
}

template <typename T>
static inline void compFuncSolidHardLightImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) hardLightOp(a, b, da, sa)
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidHardLight(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidHardLightImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidHardLightImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncHardLightImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) hardLightOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncHardLight(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncHardLightImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncHardLightImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
    if 2.Sca <= Sa
        Dca' = Dca.(Sa + (2.Sca - Sa).(1 - Dca/Da)) + Sca.(1 - Da) + Dca.(1 - Sa)
    otherwise if 2.Sca > Sa and 4.Dca <= Da
        Dca' = Dca.Sa + Da.(2.Sca - Sa).(4.Dca/Da.(4.Dca/Da + 1).(Dca/Da - 1) + 7.Dca/Da) + Sca.(1 - Da) + Dca.(1 - Sa)
    otherwise if 2.Sca > Sa and 4.Dca > Da
        Dca' = Dca.Sa + Da.(2.Sca - Sa).((Dca/Da)^0.5 - Dca/Da) + Sca.(1 - Da) + Dca.(1 - Sa)
*/
static inline int softLightOp(int dst, int src, int da, int sa)
{
    const int src2 = src << 1;
    const int dst_np = da != 0 ? (255 * dst) / da : 0;
    const int temp = (src * (255 - da) + dst * (255 - sa)) * 255;

    if (src2 < sa){
        return (dst * (sa * 255 + (src2 - sa) * (255 - dst_np)) + temp) / 65025;
    }else if (4 * dst <= da){
        return (dst * sa * 255 + da * (src2 - sa) * ((((16 * dst_np - 12 * 255) * dst_np + 3 * 65025) * dst_np) / 65025) + temp) / 65025;
    }else {
        return (dst * sa * 255 + da * (src2 - sa) * (int(std::sqrt(double(dst_np * 255))) - dst_np) + temp) / 65025;
    }
}

template <typename T>
static inline void compFuncSolidSoftLightImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) softLightOp(a, b, da, sa)
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}


void  compFuncSolidSoftLight(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidSoftLightImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidSoftLightImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncSoftLightImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) softLightOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSoftLight(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSoftLightImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncSoftLightImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
   Dca' = abs(Dca.Sa - Sca.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
        = Sca + Dca - 2.min(Sca.Da, Dca.Sa)
*/
static inline int differenceOp(int dst, int src, int da, int sa)
{
    return src + dst - LColor::div255(2 * std::min<int>(src * da, dst * sa));
}

template <typename T>
static inline void compFuncSolidDifferenceImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) differenceOp(a, b, da, sa)
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidDifference(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidDifferenceImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidDifferenceImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncDifferenceImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) differenceOp(a, b, da, sa)
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncDifference(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncDifferenceImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncDifferenceImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

/*
    Dca' = (Sca.Da + Dca.Sa - 2.Sca.Dca) + Sca.(1 - Da) + Dca.(1 - Sa)
*/
template <typename T>
static inline void  compFuncSolidExclusionImpl(uint32_t *dest, int length, uint32_t color, const T &coverage)
{
    int sa = LColor::uintToAlpha(color);
    int sr = LColor::uintToRed(color);
    int sg = LColor::uintToGreen(color);
    int sb = LColor::uintToBlue(color);

    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        int da = LColor::uintToAlpha(d);

#define OP(a, b) (a + b - LColor::div255(2*(a*b)))
        int r =  OP(  LColor::uintToRed(d), sr);
        int b =  OP( LColor::uintToBlue(d), sb);
        int g =  OP(LColor::uintToGreen(d), sg);
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncSolidExclusion(uint32_t *dest, int length, uint32_t color, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncSolidExclusionImpl(dest, length, color, LFullCoverage());
    }else{
        compFuncSolidExclusionImpl(dest, length, color, LPartialCoverage(const_alpha));
    }
}

template <typename T>
static inline void compFuncExclusionImpl(uint32_t * dest, const uint32_t * src, int length, const T &coverage)
{
    for (int i = 0; i < length; ++i) {
        uint32_t d = dest[i];
        uint32_t s = src[i];

        int da = LColor::uintToAlpha(d);
        int sa = LColor::uintToAlpha(s);

#define OP(a, b) (a + b - ((a*b) >> 7))
        int r = OP(  LColor::uintToRed(d),   LColor::uintToRed(s));
        int b = OP( LColor::uintToBlue(d),  LColor::uintToBlue(s));
        int g = OP(LColor::uintToGreen(d), LColor::uintToGreen(s));
        int a = mixAlpha(da, sa);
#undef OP

        coverage.store(&dest[i], LColor::argbToUInt(r, g, b, a));
    }
}

void  compFuncExclusion(uint32_t * dest, const uint32_t * src, int length, uint32_t const_alpha)
{
    if (const_alpha == 255){
        compFuncExclusionImpl(dest, src, length, LFullCoverage());
    }else{
        compFuncExclusionImpl(dest, src, length, LPartialCoverage(const_alpha));
    }
}

void  rasteropSolidSourceOrDestination(uint32_t *dest,
                                                    int length,
                                                    uint32_t color,
                                                    uint32_t const_alpha)
{
    while (length--){
        *dest++ |= color;
    }
}

void  rasteropSourceOrDestination(uint32_t * dest,
                                              const uint32_t * src,
                                              int length,
                                              uint32_t const_alpha)
{
    while (length--){
        *dest++ |= *src++;
    }
}

void  rasteropSolidSourceAndDestination(uint32_t *dest,
                                                     int length,
                                                     uint32_t color,
                                                     uint32_t const_alpha)
{
    color |= 0xff000000;
    while (length--){
        *dest++ &= color;
    }
}

void  rasteropSourceAndDestination(uint32_t * dest,
                                               const uint32_t * src,
                                               int length,
                                               uint32_t const_alpha)
{
    while (length--) {
        *dest = (*src & *dest) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidSourceXorDestination(uint32_t *dest,
                                                     int length,
                                                     uint32_t color,
                                                     uint32_t const_alpha)
{
    color &= 0x00ffffff;
    while (length--){
        *dest++ ^= color;
    }
}

void  rasteropSourceXorDestination(uint32_t * dest,
                                               const uint32_t * src,
                                               int length,
                                               uint32_t const_alpha)
{
    while (length--) {
        *dest = (*src ^ *dest) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidNotSourceAndNotDestination(uint32_t *dest,
                                                           int length,
                                                           uint32_t color,
                                                           uint32_t const_alpha)
{
    color = ~color;
    while (length--) {
        *dest = (color & ~(*dest)) | 0xff000000;
        ++dest;
    }
}

void  rasteropNotSourceAndNotDestination(uint32_t * dest,
                                                     const uint32_t * src,
                                                     int length,
                                                     uint32_t const_alpha)
{
    while (length--) {
        *dest = (~(*src) & ~(*dest)) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidNotSourceOrNotDestination(uint32_t *dest,
                                                          int length,
                                                          uint32_t color,
                                                          uint32_t const_alpha)
{
    color = ~color | 0xff000000;
    while (length--) {
        *dest = color | ~(*dest);
        ++dest;
    }
}

void  rasteropNotSourceOrNotDestination(uint32_t * dest,
                                                    const uint32_t * src,
                                                    int length,
                                                    uint32_t const_alpha)
{
    while (length--) {
        *dest = ~(*src) | ~(*dest) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidNotSourceXorDestination(uint32_t *dest,
                                                        int length,
                                                        uint32_t color,
                                                        uint32_t const_alpha)
{
    color = ~color & 0x00ffffff;
    while (length--) {
        *dest = color ^ (*dest);
        ++dest;
    }
}

void  rasteropNotSourceXorDestination(uint32_t * dest,
                                                  const uint32_t * src,
                                                  int length,
                                                  uint32_t const_alpha)
{
    while (length--) {
        *dest = ((~(*src)) ^ (*dest)) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidNotSource(uint32_t *dest, int length,
                                          uint32_t color, uint32_t const_alpha)
{
    lMemFillTemplate(dest, color, length);
}

void  rasteropNotSource(uint32_t * dest, const uint32_t * src,
                                    int length, uint32_t const_alpha)
{
    while (length--){
        *dest++ = ~(*src++) | 0xff000000;
    }
}

void  rasteropSolidNotSourceAndDestination(uint32_t *dest,
                                                        int length,
                                                        uint32_t color,
                                                        uint32_t const_alpha)
{
    color = ~color | 0xff000000;
    while (length--) {
        *dest = color & *dest;
        ++dest;
    }
}

void  rasteropNotSourceAndDestination(uint32_t * dest,
                                                  const uint32_t * src,
                                                  int length,
                                                  uint32_t const_alpha)
{
    while (length--) {
        *dest = (~(*src) & *dest) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidSourceAndNotDestination(uint32_t *dest,
                                                        int length,
                                                        uint32_t color,
                                                        uint32_t const_alpha)
{
    while (length--) {
        *dest = (color & ~(*dest)) | 0xff000000;
        ++dest;
    }
}

void  rasteropSourceAndNotDestination(uint32_t * dest,
                                                  const uint32_t * src,
                                                  int length,
                                                  uint32_t const_alpha)
{
    while (length--) {
        *dest = (*src & ~(*dest)) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropNotSourceOrDestination(uint32_t * dest,
                                                 const uint32_t * src,
                                                 int length,
                                                 uint32_t const_alpha)
{
    while (length--) {
        *dest = (~(*src) | *dest) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidNotSourceOrDestination(uint32_t * dest,
                                                       int length,
                                                       uint32_t color,
                                                       uint32_t const_alpha)
{
    color = ~color | 0xff000000;
    while (length--){
        *dest++ |= color;
    }
}

void  rasteropSourceOrNotDestination(uint32_t * dest,
                                                 const uint32_t * src,
                                                 int length,
                                                 uint32_t const_alpha)
{
    while (length--) {
        *dest = (*src | ~(*dest)) | 0xff000000;
        ++dest; ++src;
    }
}

void  rasteropSolidSourceOrNotDestination(uint32_t * dest,
                                                       int length,
                                                       uint32_t color,
                                                       uint32_t const_alpha)
{
    while (length--) {
        *dest = (color | ~(*dest)) | 0xff000000;
        ++dest;
    }
}

void  rasteropClearDestination(uint32_t * dest,
                                           const uint32_t * src,
                                           int length,
                                           uint32_t const_alpha)
{
    compFuncSolidSourceOver (dest, length, 0xff000000, const_alpha);
}

void  rasteropSolidClearDestination(uint32_t * dest,
                                                 int length,
                                                 uint32_t color,
                                                 uint32_t const_alpha)
{
    compFuncSolidSourceOver (dest, length, 0xff000000, const_alpha);
}

void  rasteropSetDestination(uint32_t * dest,
                                         const uint32_t * src,
                                         int length,
                                         uint32_t const_alpha)
{
    compFuncSolidSourceOver (dest, length, 0xffffffff, const_alpha);
}

void  rasteropSolidSetDestination(uint32_t * dest,
                                               int length,
                                               uint32_t color,
                                               uint32_t const_alpha)
{
    compFuncSolidSourceOver (dest, length, 0xffffffff, const_alpha);
}

void  rasteropNotDestination(uint32_t * dest,
                                         const uint32_t * src,
                                         int length,
                                         uint32_t const_alpha)
{
    rasteropSolidSourceXorDestination (dest, length, 0x00ffffff, const_alpha);
}

void  rasteropSolidNotDestination(uint32_t * dest,
                                               int length,
                                               uint32_t color,
                                               uint32_t const_alpha)
{
    rasteropSolidSourceXorDestination (dest, length, 0x00ffffff, const_alpha);
}

CompositionFunctionSolid lkFunctionForModeSolidC[] = {
    compFuncSolidSourceOver,
    compFuncSolidDestinationOver,
    compFuncSolidClear,
    compFuncSolidSource,
    compFuncSolidDestination,
    compFuncSolidSourceIn,
    compFuncSolidDestinationIn,
    compFuncSolidSourceOut,
    compFuncSolidDestinationOut,
    compFuncSolidSourceAtop,
    compFuncSolidDestinationAtop,
    compFuncSolidXOR,
    compFuncSolidPlus,
    compFuncSolidMultiply,
    compFuncSolidScreen,
    compFuncSolidOverlay,
    compFuncSolidDarken,
    compFuncSolidLighten,
    compFuncSolidColorDodge,
    compFuncSolidColorBurn,
    compFuncSolidHardLight,
    compFuncSolidSoftLight,
    compFuncSolidDifference,
    compFuncSolidExclusion,
    rasteropSolidSourceOrDestination,
    rasteropSolidSourceAndDestination,
    rasteropSolidSourceXorDestination,
    rasteropSolidNotSourceAndNotDestination,
    rasteropSolidNotSourceOrNotDestination,
    rasteropSolidNotSourceXorDestination,
    rasteropSolidNotSource,
    rasteropSolidNotSourceAndDestination,
    rasteropSolidSourceAndNotDestination,
    rasteropSolidNotSourceOrDestination,
    rasteropSolidSourceOrNotDestination,
    rasteropSolidClearDestination,
    rasteropSolidSetDestination,
    rasteropSolidNotDestination
};


CompositionFunction lkFunctionForModeC[] = {
    compFuncSourceOver,
    compFuncDestinationOver,
    compFuncClear,
    compFuncSource,
    compFuncDestination,
    compFuncSourceIn,
    compFuncDestinationIn,
    compFuncSourceOut,
    compFuncDestinationOut,
    compFuncSourceAtop,
    compFuncDestinationAtop,
    compFuncXOR,
    compFuncPlus,
    compFuncMultiply,
    compFuncScreen,
    compFuncOverlay,
    compFuncDarken,
    compFuncLighten,
    compFuncColorDodge,
    compFuncColorBurn,
    compFuncHardLight,
    compFuncSoftLight,
    compFuncDifference,
    compFuncExclusion,
    rasteropSourceOrDestination,
    rasteropSourceAndDestination,
    rasteropSourceXorDestination,
    rasteropNotSourceAndNotDestination,
    rasteropNotSourceOrNotDestination,
    rasteropNotSourceXorDestination,
    rasteropNotSource,
    rasteropNotSourceAndDestination,
    rasteropSourceAndNotDestination,
    rasteropNotSourceOrDestination,
    rasteropSourceOrNotDestination,
    rasteropClearDestination,
    rasteropSetDestination,
    rasteropNotDestination
};

LK_NAMESPACE_END
