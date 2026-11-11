#ifndef LMATH_H
#define LMATH_H

#include "lkcore/lglobal.h"
#include <cmath>
LK_NAMESPACE_BEGIN

template <typename R, typename F> // For qfloat16 to specialize
struct LHypotType { using type = decltype(std::hypot(R(1), F(1))); };

// Implements hypot() without limiting number of arguments:
template <typename T>
class LHypotHelper
{
    T scale, total;
    template <typename F> friend class LHypotHelper;
    LHypotHelper(T first, T prior) : scale(first), total(prior) {}
public:
    LHypotHelper(T first) : scale(std::abs(first)), total(1) {}
    T result() const
    { return std::isfinite(scale) ? scale > 0 ? scale * T(std::sqrt(total)) : T(0) : scale; }

    template<typename F, typename ...Fs>
    auto add(F first, Fs... rest) const
    { return add(first).add(rest...); }

    template<typename F, typename R = typename LHypotType<T, F>::type>
    LHypotHelper<R> add(F next) const
    {
        if (std::isinf(scale) || (std::isnan(scale) && !std::isinf(next)))
            return LHypotHelper<R>(scale, R(1));
        if (std::isnan(next))
            return LHypotHelper<R>(next, R(1));
        const R val = std::abs(next);
        if (!(scale > 0) || std::isinf(next))
            return LHypotHelper<R>(val, R(1));
        if (!(val > 0))
            return LHypotHelper<R>(scale, total);
        if (val > scale) {
            const R ratio = scale / next;
            return LHypotHelper<R>(val, total * ratio * ratio + R(1));
        }
        const R ratio = next / scale;
        return LHypotHelper<R>(scale, total + ratio * ratio);
    }
};

template<typename F, typename ...Fs>
auto lHypot(F first, Fs... rest)
{
    return LHypotHelper<F>(first).add(rest...).result();
}

template <typename T, typename S>
[[nodiscard]] constexpr bool fuzzyCompare(const T &lhs, const S &rhs) noexcept
{
    return lIsZero(lhs) || lIsZero(rhs) ? lFuzzyIsZero(lhs - rhs) : lFuzzyCompare(lhs, rhs);
}

LK_NAMESPACE_END
#endif // LMATH_H
