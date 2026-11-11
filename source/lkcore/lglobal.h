#ifndef LGLOBAL_H
#define LGLOBAL_H
#include "lkcore/lookiconfig.h"

#include <cmath>
#include <mutex>
#include <list>
#include <chrono>
#include <numbers>
#include "lprocessordetection.h"
#include "lkcore/3rd/xxhash.h"


LK_NAMESPACE_BEGIN

//defines the type for the WNDPROC on windows
//the alignment needs to be forced for sse2 to not crash with mingw
#if defined(_WIN32)
#  if defined(__MINGW32__) && !defined(__WIN64)
#    define L_ENSURE_STACK_ALIGNED_FOR_SSE __attribute__ ((force_align_arg_pointer))
#  else
#    define L_ENSURE_STACK_ALIGNED_FOR_SSE
#  endif
#  define L_WIN_CALLBACK CALLBACK L_ENSURE_STACK_ALIGNED_FOR_SSE
#endif

#ifdef __GNUC__
#  define L_LIKELY(expr)    __builtin_expect(!!(expr), true)
#  define L_UNLIKELY(expr)  __builtin_expect(!!(expr), false)
#else
#  define L_LIKELY(expr)    (expr)
#  define L_UNLIKELY(expr)  (expr)
#endif

#ifndef L_PROCESSOR_WORDSIZE
#define L_PROCESSOR_WORDSIZE 4
#endif


#define L_UNUSED(x) (void)x;
#define L_STR(var) #var

#define L_DISABLE_COPY(Class) \
    Class(const Class &) = delete;\
    Class &operator=(const Class &) = delete;

#define L_DISABLE_MOVE(Class) \
    Class(Class &&) = delete; \
    Class &operator=(Class &&) = delete;

#define L_DISABLE_COPY_MOVE(Class) \
    L_DISABLE_COPY(Class) \
    L_DISABLE_MOVE(Class)

#define L_RET_CONST_STRING(str) {static const std::wstring t(str); return t;}
#define LOFFSET(T, s, v) (T)&(((s*)0)->v)

LKCOREDEF void lCheckPointer(const char *, int) noexcept;
#define L_CHECK_PTR(p) do {if (!(p)) lCheckPointer(__FILE__,__LINE__);} while (false)

template <typename T>
inline T *l_check_ptr(T *p) { L_CHECK_PTR(p); return p; }


template <int> struct LIntegerForSize;
template <>    struct LIntegerForSize<1> { typedef uint8_t Unsigned; typedef int8_t Signed; };
template <>    struct LIntegerForSize<2> { typedef uint16_t Unsigned; typedef int16_t Signed; };
template <>    struct LIntegerForSize<4> { typedef uint32_t Unsigned; typedef int Signed; };
template <>    struct LIntegerForSize<8> { typedef uint64_t Unsigned; typedef int64_t Signed; };
#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
template <>    struct LIntegerForSize<16> { __extension__ typedef unsigned __int128 Unsigned; __extension__ typedef __int128 Signed; };
#endif
template <class T> struct LIntegerForSizeof: LIntegerForSize<sizeof(T)> { };
typedef LIntegerForSize<L_PROCESSOR_WORDSIZE>::Signed qregisterint;
typedef LIntegerForSize<L_PROCESSOR_WORDSIZE>::Unsigned qregisteruint;
typedef LIntegerForSizeof<void*>::Unsigned luintptr;
typedef LIntegerForSizeof<void*>::Signed lptrdiff;
typedef lptrdiff lintptr;
using lsizetype = LIntegerForSizeof<std::size_t>::Signed;

//typedef luintptr WId;
typedef uint64_t WId;

struct LKCOREDEF uint24_t {
    uint24_t() = default;
    uint24_t(uint32_t value)
    {
        data[0] = uint8_t(value >> 16);
        data[1] = uint8_t(value >> 8);
        data[2] = uint8_t(value);
    }
    operator uint32_t() const
    {
        return data[2] | (data[1] << 8) | (data[0] << 16);
    }
    uint8_t data[3];
};

typedef void (*LFunctionPointer)();

extern inline bool lFuzzyCompare(double p1, double p2)
{
    return std::abs(p1 - p2) * 1000000000000. <= std::min<double>(std::abs(p1), std::abs(p2));
}

extern inline bool lFuzzyCompare(float p1, float p2)
{
    return std::abs(p1 - p2) * 100000.f <= std::min<float>(std::abs(p1), std::abs(p2));
}

extern inline bool lFuzzyIsZero(double d)
{
    return std::abs(d) <= 0.000000000001;
}

extern inline bool lFuzzyIsZero(float f)
{
    return std::abs(f) <= 0.00001f;
}

extern inline bool lFuzzyIsZero(int f) noexcept
{
    return f == 0;
}

extern inline  bool lIsZero(double d) noexcept
{
    return d == 0.0;
}

extern inline  bool lIsZero(float f) noexcept
{
    return f == 0.0f;
}

extern inline  bool lIsZero(int f) noexcept
{
    return f == 0;
}
LKCOREDEF std::string lGetEnv(const std::string& varName, bool useUserEnv = true);
LKCOREDEF void lSetUserEnv(const std::string& varName, const std::string& varValue);

typedef std::chrono::steady_clock LClock;
typedef LClock::time_point LTimePoint;
typedef LClock::duration LDuration;

typedef std::chrono::system_clock LSysClock;
typedef LSysClock::time_point LSysTimePoint;
typedef LSysClock::duration LSysDuration;

typedef std::chrono::utc_clock LUtcClock;
typedef LUtcClock::time_point LUtcTimePoint;
typedef LUtcClock::duration LUtcDuration;

extern inline int64_t lDurationToNs(const LDuration& p){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(p).count();
}

extern inline int64_t lSysDurationToNs(const LSysDuration& p){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(p).count();
}

extern inline int64_t lDurationToMs(const LDuration& p){
    return std::chrono::duration_cast<std::chrono::milliseconds>(p).count();
}

extern inline int64_t lSysDurationToMs(const LSysDuration& p){
    return std::chrono::duration_cast<std::chrono::milliseconds>(p).count();
}

extern inline int64_t lDurationToSecond(const LDuration& p){
    return std::chrono::duration_cast<std::chrono::seconds>(p).count();
}

extern inline int64_t lSysDurationToSecond(const LSysDuration& p){
    return std::chrono::duration_cast<std::chrono::seconds>(p).count();
}


template <typename T>
constexpr inline const T &lBound(const T &min, const T &val, const T &max)
{ return std::max<T>(min, std::min<T>(max, val)); }


extern inline float lDegreesToRadians(float degrees)
{
    return degrees * float(std::numbers::pi/180);
}

extern inline double lDegreesToRadians(double degrees)
{
    return degrees * (std::numbers::pi / 180);
}

extern inline float lRadiansToDegrees(float radians)
{
    return radians * float(180/std::numbers::pi);
}

extern inline double lRadiansToDegrees(double radians)
{
    return radians * (180 / std::numbers::pi);
}

extern inline double lDpiScaled(double value, double dpi)
{
    return value * dpi / 96;
}

template<class T>
uint32_t lHashCombine(uint32_t seed, const T& t){
    return seed ^ ((uint32_t)std::hash<T>{}(t) + (uint32_t)0x9e3779b9 + (seed << 6) + (seed >> 2)) ;
}

template<typename VT, typename CT>
constexpr inline const uint64_t lMemberAddr(VT CT::* v){
    return (uint64_t)(&(((CT*)0)->*v));
}

template<class T>
int lItemIndex(const std::list<T>& ll, const T& v){
    int i =0;
    for(const T& t : ll){
        if(t == v){
            return i;
        }
        ++i;
    }
    return -1;
}

extern inline constexpr uint32_t hashCase(const std::string_view& s) {
    uint32_t h = 2166136261u ; for ( unsigned char c : s) { h ^= c; h *= 16777619u ; } return h;
}

LK_NAMESPACE_END
#endif // LGLOBAL_H
