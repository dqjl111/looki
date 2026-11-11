#include "lstringutil.h"
#include "lkcore/text/lchar.h"
#include "lkcore/text/lcharconst.h"
#include <sstream>
#include <regex>
#include <string>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include "lkcore/ldebug.h"
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <iconv.h>
#endif

LK_NAMESPACE_BEGIN


#define FLAG(x) (1 << (x))

static const unsigned short specialCaseMap[] = {
    0x0, // placeholder
    0x1, 0x2c65,
    0x1, 0x2c66,
    0x1, 0x2c7e,
    0x1, 0x2c7f,
    0x1, 0x2c6f,
    0x1, 0x2c6d,
    0x1, 0x2c70,
    0x1, 0xa7ab,
    0x1, 0xa7ac,
    0x1, 0xa78d,
    0x1, 0xa7aa,
    0x1, 0xa7ae,
    0x1, 0x2c62,
    0x1, 0xa7ad,
    0x1, 0x2c6e,
    0x1, 0x2c64,
    0x1, 0xa7c5,
    0x1, 0xa7b1,
    0x1, 0xa7b2,
    0x1, 0xa7b0,
    0x1, 0xab70,
    0x1, 0xab71,
    0x1, 0xab72,
    0x1, 0xab73,
    0x1, 0xab74,
    0x1, 0xab75,
    0x1, 0xab76,
    0x1, 0xab77,
    0x1, 0xab78,
    0x1, 0xab79,
    0x1, 0xab7a,
    0x1, 0xab7b,
    0x1, 0xab7c,
    0x1, 0xab7d,
    0x1, 0xab7e,
    0x1, 0xab7f,
    0x1, 0xab80,
    0x1, 0xab81,
    0x1, 0xab82,
    0x1, 0xab83,
    0x1, 0xab84,
    0x1, 0xab85,
    0x1, 0xab86,
    0x1, 0xab87,
    0x1, 0xab88,
    0x1, 0xab89,
    0x1, 0xab8a,
    0x1, 0xab8b,
    0x1, 0xab8c,
    0x1, 0xab8d,
    0x1, 0xab8e,
    0x1, 0xab8f,
    0x1, 0xab90,
    0x1, 0xab91,
    0x1, 0xab92,
    0x1, 0xab93,
    0x1, 0xab94,
    0x1, 0xab95,
    0x1, 0xab96,
    0x1, 0xab97,
    0x1, 0xab98,
    0x1, 0xab99,
    0x1, 0xab9a,
    0x1, 0xab9b,
    0x1, 0xab9c,
    0x1, 0xab9d,
    0x1, 0xab9e,
    0x1, 0xab9f,
    0x1, 0xaba0,
    0x1, 0xaba1,
    0x1, 0xaba2,
    0x1, 0xaba3,
    0x1, 0xaba4,
    0x1, 0xaba5,
    0x1, 0xaba6,
    0x1, 0xaba7,
    0x1, 0xaba8,
    0x1, 0xaba9,
    0x1, 0xabaa,
    0x1, 0xabab,
    0x1, 0xabac,
    0x1, 0xabad,
    0x1, 0xabae,
    0x1, 0xabaf,
    0x1, 0xabb0,
    0x1, 0xabb1,
    0x1, 0xabb2,
    0x1, 0xabb3,
    0x1, 0xabb4,
    0x1, 0xabb5,
    0x1, 0xabb6,
    0x1, 0xabb7,
    0x1, 0xabb8,
    0x1, 0xabb9,
    0x1, 0xabba,
    0x1, 0xabbb,
    0x1, 0xabbc,
    0x1, 0xabbd,
    0x1, 0xabbe,
    0x1, 0xabbf,
    0x1, 0xa64a,
    0x1, 0xa77d,
    0x1, 0xa7c6,
    0x1, 0x6b,
    0x1, 0xe5,
    0x1, 0x26b,
    0x1, 0x27d,
    0x1, 0x23a,
    0x1, 0x23e,
    0x1, 0x251,
    0x1, 0x271,
    0x1, 0x250,
    0x1, 0x252,
    0x1, 0x23f,
    0x1, 0x240,
    0x1, 0x1d79,
    0x1, 0x265,
    0x1, 0x266,
    0x1, 0x25c,
    0x1, 0x261,
    0x1, 0x26c,
    0x1, 0x26a,
    0x1, 0x29e,
    0x1, 0x287,
    0x1, 0x29d,
    0x1, 0x282,
    0x1, 0x1d8e,
    0x1, 0x13a0,
    0x1, 0x13a1,
    0x1, 0x13a2,
    0x1, 0x13a3,
    0x1, 0x13a4,
    0x1, 0x13a5,
    0x1, 0x13a6,
    0x1, 0x13a7,
    0x1, 0x13a8,
    0x1, 0x13a9,
    0x1, 0x13aa,
    0x1, 0x13ab,
    0x1, 0x13ac,
    0x1, 0x13ad,
    0x1, 0x13ae,
    0x1, 0x13af,
    0x1, 0x13b0,
    0x1, 0x13b1,
    0x1, 0x13b2,
    0x1, 0x13b3,
    0x1, 0x13b4,
    0x1, 0x13b5,
    0x1, 0x13b6,
    0x1, 0x13b7,
    0x1, 0x13b8,
    0x1, 0x13b9,
    0x1, 0x13ba,
    0x1, 0x13bb,
    0x1, 0x13bc,
    0x1, 0x13bd,
    0x1, 0x13be,
    0x1, 0x13bf,
    0x1, 0x13c0,
    0x1, 0x13c1,
    0x1, 0x13c2,
    0x1, 0x13c3,
    0x1, 0x13c4,
    0x1, 0x13c5,
    0x1, 0x13c6,
    0x1, 0x13c7,
    0x1, 0x13c8,
    0x1, 0x13c9,
    0x1, 0x13ca,
    0x1, 0x13cb,
    0x1, 0x13cc,
    0x1, 0x13cd,
    0x1, 0x13ce,
    0x1, 0x13cf,
    0x1, 0x13d0,
    0x1, 0x13d1,
    0x1, 0x13d2,
    0x1, 0x13d3,
    0x1, 0x13d4,
    0x1, 0x13d5,
    0x1, 0x13d6,
    0x1, 0x13d7,
    0x1, 0x13d8,
    0x1, 0x13d9,
    0x1, 0x13da,
    0x1, 0x13db,
    0x1, 0x13dc,
    0x1, 0x13dd,
    0x1, 0x13de,
    0x1, 0x13df,
    0x1, 0x13e0,
    0x1, 0x13e1,
    0x1, 0x13e2,
    0x1, 0x13e3,
    0x1, 0x13e4,
    0x1, 0x13e5,
    0x1, 0x13e6,
    0x1, 0x13e7,
    0x1, 0x13e8,
    0x1, 0x13e9,
    0x1, 0x13ea,
    0x1, 0x13eb,
    0x1, 0x13ec,
    0x1, 0x13ed,
    0x1, 0x13ee,
    0x1, 0x13ef,
    0x2, 0x53, 0x73,
    0x2, 0x53, 0x53,
    0x2, 0x69, 0x307,
    0x2, 0x46, 0x66,
    0x2, 0x46, 0x46,
    0x2, 0x46, 0x69,
    0x2, 0x46, 0x49,
    0x2, 0x46, 0x6c,
    0x2, 0x46, 0x4c,
    0x3, 0x46, 0x66, 0x69,
    0x3, 0x46, 0x46, 0x49,
    0x3, 0x46, 0x66, 0x6c,
    0x3, 0x46, 0x46, 0x4c,
    0x2, 0x53, 0x74,
    0x2, 0x53, 0x54,
    0x2, 0x535, 0x582,
    0x2, 0x535, 0x552,
    0x2, 0x544, 0x576,
    0x2, 0x544, 0x546,
    0x2, 0x544, 0x565,
    0x2, 0x544, 0x535,
    0x2, 0x544, 0x56b,
    0x2, 0x544, 0x53b,
    0x2, 0x54e, 0x576,
    0x2, 0x54e, 0x546,
    0x2, 0x544, 0x56d,
    0x2, 0x544, 0x53d,
    0x2, 0x2bc, 0x4e,
    0x3, 0x399, 0x308, 0x301,
    0x3, 0x3a5, 0x308, 0x301,
    0x2, 0x4a, 0x30c,
    0x2, 0x48, 0x331,
    0x2, 0x54, 0x308,
    0x2, 0x57, 0x30a,
    0x2, 0x59, 0x30a,
    0x2, 0x41, 0x2be,
    0x2, 0x3a5, 0x313,
    0x3, 0x3a5, 0x313, 0x300,
    0x3, 0x3a5, 0x313, 0x301,
    0x3, 0x3a5, 0x313, 0x342,
    0x2, 0x391, 0x342,
    0x2, 0x397, 0x342,
    0x3, 0x399, 0x308, 0x300,
    0x2, 0x399, 0x342,
    0x3, 0x399, 0x308, 0x342,
    0x3, 0x3a5, 0x308, 0x300,
    0x2, 0x3a1, 0x313,
    0x2, 0x3a5, 0x342,
    0x3, 0x3a5, 0x308, 0x342,
    0x2, 0x3a9, 0x342,
    0x2, 0x1f08, 0x399,
    0x2, 0x1f09, 0x399,
    0x2, 0x1f0a, 0x399,
    0x2, 0x1f0b, 0x399,
    0x2, 0x1f0c, 0x399,
    0x2, 0x1f0d, 0x399,
    0x2, 0x1f0e, 0x399,
    0x2, 0x1f0f, 0x399,
    0x2, 0x1f28, 0x399,
    0x2, 0x1f29, 0x399,
    0x2, 0x1f2a, 0x399,
    0x2, 0x1f2b, 0x399,
    0x2, 0x1f2c, 0x399,
    0x2, 0x1f2d, 0x399,
    0x2, 0x1f2e, 0x399,
    0x2, 0x1f2f, 0x399,
    0x2, 0x1f68, 0x399,
    0x2, 0x1f69, 0x399,
    0x2, 0x1f6a, 0x399,
    0x2, 0x1f6b, 0x399,
    0x2, 0x1f6c, 0x399,
    0x2, 0x1f6d, 0x399,
    0x2, 0x1f6e, 0x399,
    0x2, 0x1f6f, 0x399,
    0x2, 0x391, 0x399,
    0x2, 0x397, 0x399,
    0x2, 0x3a9, 0x399,
    0x2, 0x1fba, 0x345,
    0x2, 0x1fba, 0x399,
    0x2, 0x386, 0x345,
    0x2, 0x386, 0x399,
    0x2, 0x1fca, 0x345,
    0x2, 0x1fca, 0x399,
    0x2, 0x389, 0x345,
    0x2, 0x389, 0x399,
    0x2, 0x1ffa, 0x345,
    0x2, 0x1ffa, 0x399,
    0x2, 0x38f, 0x345,
    0x2, 0x38f, 0x399,
    0x3, 0x391, 0x342, 0x345,
    0x3, 0x391, 0x342, 0x399,
    0x3, 0x397, 0x342, 0x345,
    0x3, 0x397, 0x342, 0x399,
    0x3, 0x3a9, 0x342, 0x345,
    0x3, 0x3a9, 0x342, 0x399,
    0x1, 0xa64b
};



template <typename T>
static inline T convertCaseHelper(T uc, lchar::Case which)
{
    lchar::Properties::CaseItem fold = lchar::Util::properties(uc)->cases[which];
    if (L_UNLIKELY(fold.special)) {
        const uint16_t *specialCase = specialCaseMap + fold.diff;
        // so far, there are no special cases beyond BMP (guaranteed by the qunicodetables generator)
        return *specialCase == 1 ? specialCase[1] : uc;
    }
    return uc + fold.diff;
}

LStringUtil::LStringUtil()
{

}

std::wstring LStringUtil::utf8ToWide(const std::string &utf8)
{
    // Special case of empty input string
    if (utf8.empty())
    {
        // Empty input --> return empty output string
        return std::wstring{};
    }
#ifdef WIN32
    // Safely fail if an invalid UTF-8 character sequence is encountered
    constexpr DWORD kFlags = MB_ERR_INVALID_CHARS;

    const int utf8Length = (int)utf8.length();

    // Get the size of the destination UTF-16 string
    const int wideLength = ::MultiByteToWideChar(
        CP_UTF8,       // source string is in UTF-8
        kFlags,        // conversion flags
        utf8.data(),   // source UTF-8 string pointer
        utf8Length,    // length of the source UTF-8 string, in chars
        nullptr,       // unused - no conversion done in this step
        0              // request size of destination buffer, in wchar_ts
        );
    if (wideLength == 0)
    {
        lWarn() << "Can't get result UTF-16 string length (MultiByteToWideChar failed):" << ::GetLastError();
        return std::wstring();
    }

    // Make room in the destination string for the converted bits
    std::wstring wideStr(wideLength, L' ');
    wchar_t* utf16Buffer = wideStr.data();
    _ASSERTE(utf16Buffer != nullptr);

    // Do the actual conversion from UTF-8 to UTF-16
    int result = ::MultiByteToWideChar(
        CP_UTF8,       // source string is in UTF-8
        kFlags,        // conversion flags
        utf8.data(),   // source UTF-8 string pointer
        utf8Length,    // length of source UTF-8 string, in chars
        utf16Buffer,   // pointer to destination buffer
        wideLength    // size of destination buffer, in wchar_ts
        );
    if (result == 0)
    {
        lWarn() << "Can't convert from UTF-8 to wstring (MultiByteToWideChar failed):" << ::GetLastError();
        return std::wstring();
    }

    return wideStr;
#else
    iconv_t cd = iconv_open("WCHAR_T", "UTF-8"); // "WCHAR_T" may vary based on your platform. For example, it could be "WCHAR_T//UCS4" on some systems.
    if (cd == (iconv_t)-1) {
        lWarn() << "iconv_open failed";
        return std::wstring();
    }

    size_t inBytes = utf8.size();
    size_t outBytes = inBytes * 2; // Rough estimation for worst case scenario.
    std::vector<wchar_t> wstr(outBytes);
    char* inbuf = const_cast<char*>(utf8.c_str()); // iconv expects a non-const char* pointer.
    char* outbuf = reinterpret_cast<char*>(wstr.data()); // iconv expects a char* pointer.
    size_t inleft = inBytes;
    size_t outleft = outBytes * sizeof(wchar_t);

    if (iconv(cd, &inbuf, &inleft, &outbuf, &outleft) == (size_t)-1) {
        iconv_close(cd);
        lWarn() << "iconv failed";
        return std::wstring();
    }
    iconv_close(cd);
    return std::wstring(wstr.data(), (outBytes - outleft / sizeof(wchar_t))); // Adjust size based on remaining output buffer size.
#endif
}

std::string LStringUtil::wideToUtf8(const std::wstring &wideStr)
{
    // Special case of empty input string
    if (wideStr.empty())
    {
        // Empty input --> return empty output string
        return std::string{};
    }

#ifdef WIN32
    // Safely fail if an invalid UTF-16 character sequence is encountered
    constexpr DWORD kFlags = WC_ERR_INVALID_CHARS;

    const int wideLength = (int)wideStr.length();
    // Get the length, in chars, of the resulting UTF-8 string
    const int utf8Length = ::WideCharToMultiByte(
        CP_UTF8,            // convert to UTF-8
        kFlags,             // conversion flags
        wideStr.data(),       // source UTF-16 string
        wideLength,        // length of source UTF-16 string, in wchar_ts
        nullptr,            // unused - no conversion required in this step
        0,                  // request size of destination buffer, in chars
        nullptr, nullptr    // unused
        );
    if (utf8Length == 0)
    {
        // Conversion error: capture error code and throw
        lWarn() << "Can't get result UTF-8 string length (WideCharToMultiByte failed):" << ::GetLastError();
        return std::string();
    }

    // Make room in the destination string for the converted bits
    std::string utf8(utf8Length, ' ');
    char* utf8Buffer = utf8.data();
    _ASSERTE(utf8Buffer != nullptr);

    // Do the actual conversion from UTF-16 to UTF-8
    int result = ::WideCharToMultiByte(
        CP_UTF8,            // convert to UTF-8
        kFlags,             // conversion flags
        wideStr.data(),       // source UTF-16 string
        wideLength,        // length of source UTF-16 string, in wchar_ts
        utf8Buffer,         // pointer to destination buffer
        utf8Length,         // size of destination buffer, in chars
        nullptr, nullptr    // unused
        );
    if (result == 0)
    {
        lWarn() << "Can't convert from wstring to UTF-8 string (WideCharToMultiByte failed):" << ::GetLastError();
        return std::string();
    }
    return utf8;
#else
    iconv_t cd = iconv_open("UTF-8", setlocale(LC_CTYPE, NULL)); // 获取当前locale对应的编码名
    if (cd == (iconv_t)-1) {
        lWarn() << "iconv_open failed";
        return std::string();
    }

    size_t inbytes = wstr.length() * sizeof(wchar_t);
    size_t outbytes = inbytes * 4; // 假设最多4个字节表示一个UTF-8字符
    std::vector<char> buf(outbytes);

    char* outbuf = buf.data();
    wchar_t* inbuf = (wchar_t*)wideStr.c_str();
    if (iconv(cd, (char**)&inbuf, &inbytes, &outbuf, &outbytes) == (size_t)-1) {
        iconv_close(cd);
        lWarn() << "iconv failed";
        return std::string();
    }
    iconv_close(cd);
    std::string result(outbuf, outbytes - outbytes % 4); // 减去可能剩余的不完整字符的字节数
    return result;
#endif

}

#if _WIN32
std::string LStringUtil::convertEncode(const std::string &in, const std::string &fromCharset, const std::string &toCharset)
{
    unsigned inCodePage = (fromCharset.empty() || fromCharset=="utf-8") ? CP_UTF8 : CP_ACP;
    unsigned outCodePage= (toCharset.empty() || toCharset=="utf-8") ? CP_UTF8 : CP_ACP;
    if(inCodePage == outCodePage){
        return in;
    }

    unsigned inlen = MultiByteToWideChar(inCodePage, 0, in.data(), -1, nullptr, 0);
    if (inlen == 0)
    {
        lWarn() << "Can't get result UTF-16 string length (MultiByteToWideChar failed):" << ::GetLastError();
        return std::string();
    }


    std::wstring unicodeStr(inlen, wchar_t('\0'));
    MultiByteToWideChar(inCodePage, 0, in.data(), -1, unicodeStr.data(), inlen);

    unsigned outlen = WideCharToMultiByte(outCodePage, 0, unicodeStr.data(), -1, nullptr, 0, nullptr, nullptr);
    if (outlen == 0)
    {
        // Conversion error: capture error code and throw
        lWarn() << "Can't get result encode string length (WideCharToMultiByte failed):" << ::GetLastError();
        return std::string();
    }

    std::string result(outlen, char('\0'));
    WideCharToMultiByte(outCodePage, 0, unicodeStr.data(), -1, result.data(), outlen, nullptr, nullptr);
    return result;
}
#else
inline std::string LStringUtil::convertEncode(const std::string &in, const std::string &fromCharset, const std::string &toCharset){
    if(fromCharset == toCharset){
        return in;
    }

    iconv_t cd = iconv_open(toCharset.data(), fromCharset.data());
    if (cd == (iconv_t)(-1)) {
        return in;
    }

    size_t inlen = in.length();
    size_t outlen = inlen * 2;

    std::string result;
    result.resize(outlen);
    char* outbuf = result.data();
    char* inbuf = const_cast<char*>(in.data());

    char** pin = &inbuf;
    char** pout = &outbuf;

    if (iconv(cd, pin, &inlen, pout, &outlen) == -1) {
        int old_errno = errno;
        iconv_close(cd);
        errno = old_errno;
        return "convert error!";
    }
    iconv_close(cd);

    result.resize(result.length() - outlen);
    return result;
}
#endif


uint32_t LStringUtil::toLower(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return ucs4;
    }
    return convertCaseHelper(ucs4, lchar::LowerCase);
}

uint32_t LStringUtil::toUpper(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return ucs4;
    }
    return convertCaseHelper(ucs4, lchar::UpperCase);
}

bool LStringUtil::isPrint(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return false;
    }
    const int test = FLAG(lchar::Other_Control) |
                     FLAG(lchar::Other_Format) |
                     FLAG(lchar::Other_Surrogate) |
                     FLAG(lchar::Other_PrivateUse) |
                     FLAG(lchar::Other_NotAssigned);
    return !(FLAG(lchar::Util::properties(ucs4)->category) & test);
}

bool LStringUtil::isRightToLeft(const std::wstring &str)
{
    return isRightToLeft(str.data(), str.length());
}

bool LStringUtil::isRightToLeft(const wchar_t *s, int size)
{
    const uint16_t *p = (uint16_t *)s;
    const uint16_t * const end = p + size;
    int isolateLevel = 0;
    while (p < end) {
        uint32_t ucs4 = *p;
        if (isHighSurrogate(ucs4) && p < end - 1) {
            uint16_t low = p[1];
            if (isLowSurrogate(low)) {
                ucs4 = surrogateToUcs4(ucs4, low);
                ++p;
            }
        }
        switch (ucs4)
        {
        case (uint32_t)lchar::DirRLI:
        case (uint32_t)lchar::DirLRI:
        case (uint32_t)lchar::DirFSI:{
            ++isolateLevel;
            break;
        }
        case (uint32_t)lchar::DirPDI:{
            if (isolateLevel){
                --isolateLevel;
            }
            break;
        }
        case (uint32_t)lchar::DirL:{
            if (isolateLevel){
                break;
            }
            return false;
        }
        case (uint32_t)lchar::DirR:
        case (uint32_t)lchar::DirAL:{
            if (isolateLevel){
                break;
            }
            return true;
        }
        default:{
            break;
        }
        }
        ++p;
    }
    return false;

}

enum {
    Hangul_SBase = 0xac00,
    Hangul_LBase = 0x1100,
    Hangul_VBase = 0x1161,
    Hangul_TBase = 0x11a7,
    Hangul_LCount = 19,
    Hangul_VCount = 21,
    Hangul_TCount = 28,
    Hangul_NCount = Hangul_VCount * Hangul_TCount,
    Hangul_SCount = Hangul_LCount * Hangul_NCount
};


lchar::Decomposition LStringUtil::decompositionTag(uint32_t ucs4)
{
    if (ucs4 >= Hangul_SBase && ucs4 < Hangul_SBase + Hangul_SCount){
        return lchar::Canonical;
    }
    uint16_t index = 0xffff;
    if(ucs4 < 0x3400){
        index = lchar::uc_decomposition_trie[lchar::uc_decomposition_trie[ucs4>>4] + (ucs4 & 0xf)];
    }else if(ucs4 < 0x30000){
        index = lchar::uc_decomposition_trie[lchar::uc_decomposition_trie[((ucs4 - 0x3400)>>8) + 0x340] + (ucs4 & 0xff)];
    }

    if (index == 0xffff){
        return lchar::NoDecomposition;
    }
    return (lchar::Decomposition)(lchar::uc_decomposition_map[index] & 0xff);
}

bool LStringUtil::isSpaceHelper(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return false;
    }
    const int test = FLAG(lchar::Separator_Space) |
                     FLAG(lchar::Separator_Line) |
                     FLAG(lchar::Separator_Paragraph);
    return FLAG(lchar::Util::properties(ucs4)->category) & test;
}

bool LStringUtil::isLetterHelper(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return false;
    }
    const int test = FLAG(lchar::Letter_Uppercase) |
                     FLAG(lchar::Letter_Lowercase) |
                     FLAG(lchar::Letter_Titlecase) |
                     FLAG(lchar::Letter_Modifier) |
                     FLAG(lchar::Letter_Other);
    return FLAG(lchar::Util::properties(ucs4)->category) & test;
}

bool LStringUtil::isNumberHelper(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return false;
    }
    const int test = FLAG(lchar::Number_DecimalDigit) |
                     FLAG(lchar::Number_Letter) |
                     FLAG(lchar::Number_Other);
    return FLAG(lchar::Util::properties(ucs4)->category) & test;
}

bool LStringUtil::isLetterOrNumberHelper(uint32_t ucs4)
{
    if (ucs4 > lchar::LastValidCodePoint){
        return false;
    }
    const int test = FLAG(lchar::Letter_Uppercase) |
                     FLAG(lchar::Letter_Lowercase) |
                     FLAG(lchar::Letter_Titlecase) |
                     FLAG(lchar::Letter_Modifier) |
                     FLAG(lchar::Letter_Other) |
                     FLAG(lchar::Number_DecimalDigit) |
                     FLAG(lchar::Number_Letter) |
                     FLAG(lchar::Number_Other);
    return FLAG(lchar::Util::properties(ucs4)->category) & test;
}

bool LStringUtil::stob(const std::string& str, bool* isOk)
{
    std::string t= trim(str);
    std::transform(t.begin(), t.end(), t.begin(), tolower);
    if (t == "true") {
        if(isOk != nullptr){
            *isOk = true;
        }
        return true;
    } else if (t == "false") {
        if(isOk != nullptr){
            *isOk = true;
        }
        return false;
    } else {
        if(isOk != nullptr){
            *isOk = false;
        }
        return false;
    }
}


template<typename T, class S = std::string>
inline T stdStoInt(const S &str, bool *isOk, int base){
    T ret = 0;
    if(str.empty()){
        if(isOk != nullptr){
            *isOk = false;
        }
        return ret;
    }

    try
    {
        if constexpr (std::is_same<T, int>::value) {
            ret = std::stoi(str, nullptr, base);
        } else if constexpr (std::is_same<T, long>::value) {
            ret = std::stol(str, nullptr, base);
        } else if constexpr (std::is_same<T, unsigned long>::value) {
            ret = std::stoul(str, nullptr, base);
        }else if constexpr (std::is_same<T, long long>::value) {
            ret = std::stoll(str, nullptr, base);
        }else if constexpr (std::is_same<T, unsigned long long>::value) {
            ret = std::stoull(str, nullptr, base);
        }else{
            ret = (T)std::stoll(str, nullptr, base);
        }

        if(isOk != nullptr){
            *isOk = true;
        }
    }
    catch (std::invalid_argument const& ex)
    {
        if(isOk != nullptr){
            *isOk = false;
        }
    }
    catch (std::out_of_range const& ex)
    {
        if(isOk != nullptr){
            *isOk = false;
        }
    }
    return ret;
}

template<typename T, class S = std::string>
inline T stdStoFloat(const S &str, bool *isOk){
    T ret = 0;
    if(str.empty()){
        if(isOk != nullptr){
            *isOk = false;
        }
        return ret;
    }

    try
    {
        if constexpr (std::is_same<T, float>::value) {
            ret = std::stof(str, nullptr);
        } else if constexpr (std::is_same<T, double>::value) {
            ret = std::stod(str, nullptr);
        } else if constexpr (std::is_same<T, long double>::value) {
            ret = std::stold(str, nullptr);
        }else{
            ret = (T)std::stold(str, nullptr);
        }

        if(isOk != nullptr){
            *isOk = true;
        }
    }
    catch (std::invalid_argument const& ex)
    {
        if(isOk != nullptr){
            *isOk = false;
        }
    }
    catch (std::out_of_range const& ex)
    {
        if(isOk != nullptr){
            *isOk = false;
        }
    }
    return ret;
}

int LStringUtil::stoi(const std::string &str, bool *isOk, int base)
{
    return stdStoInt<int>(str, isOk, base);
}

long LStringUtil::stol(const std::string &str, bool *isOk, int base)
{
    return stdStoInt<long>(str, isOk, base);
}

unsigned long LStringUtil::stoul(const std::string &str, bool *isOk, int base)
{
    return stdStoInt<unsigned long>(str, isOk, base);
}

long long LStringUtil::stoll(const std::string &str, bool *isOk, int base)
{
    return stdStoInt<long long>(str, isOk, base);
}

unsigned long long LStringUtil::stoull(const std::string &str, bool *isOk, int base)
{
    return stdStoInt<unsigned long long>(str, isOk, base);
}

float LStringUtil::stof(const std::string &str, bool *isOk)
{
    return stdStoFloat<float>(str, isOk);
}

double LStringUtil::stod(const std::string &str, bool *isOk)
{
    return stdStoFloat<double>(str, isOk);
}

long double LStringUtil::stold(const std::string &str, bool *isOk)
{
    return stdStoFloat<long double>(str, isOk);
}

bool LStringUtil::stob(const std::wstring &str, bool *isOk)
{
    std::string t = wideToUtf8(str);
    return stob(t, isOk);
}


int LStringUtil::stoi(const std::wstring &str, bool *isOk, int base)
{
    return stdStoInt<int>(str, isOk, base);
}

long LStringUtil::stol(const std::wstring &str, bool *isOk, int base)
{
    return stdStoInt<long>(str, isOk, base);
}

unsigned long LStringUtil::stoul(const std::wstring &str, bool *isOk, int base)
{
    return stdStoInt<unsigned long>(str, isOk, base);
}

long long LStringUtil::stoll(const std::wstring &str, bool *isOk, int base)
{
    return stdStoInt<long long>(str, isOk, base);
}

unsigned long long LStringUtil::stoull(const std::wstring &str, bool *isOk, int base)
{
    return stdStoInt<unsigned long long>(str, isOk, base);
}

float LStringUtil::stof(const std::wstring &str, bool *isOk)
{
    return stdStoFloat<float>(str, isOk);
}

double LStringUtil::stod(const std::wstring &str, bool *isOk)
{
    return stdStoFloat<double>(str, isOk);
}

long double LStringUtil::stold(const std::wstring &str, bool *isOk)
{
    return stdStoFloat<long double>(str, isOk);
}


std::string LStringUtil::replace(const std::string& orgStr, const std::string& newStr, std::string str)
{
    int len = orgStr.length();
    int replace_len = newStr.length();
    int pos = str.find(orgStr);

    while(pos != std::string::npos)
    {
        str.replace(pos, len, newStr);
        pos = str.find(orgStr, pos + replace_len);
    }
    return str;
}


std::wstring LStringUtil::replace(const std::wstring& orgStr, const std::wstring& newStr, std::wstring str)
{
    int len = orgStr.length();
    int replace_len = newStr.length();
    int pos = str.find(orgStr);

    while(pos != std::wstring::npos)
    {
        str.replace(pos, len, newStr);
        pos = str.find(orgStr, pos + replace_len);
    }
    return str;
}

std::string_view LStringUtil::trim(std::string_view str, const std::string &whitespace)
{
    return trimLeft(trimRight(str, whitespace), whitespace);
}

std::string_view LStringUtil::trimLeft(std::string_view str, const std::string &whitespace)
{
    while (!str.empty() && whitespace.contains(str.front())) {
        str.remove_prefix(1);
    }
    return str;
}

std::string_view LStringUtil::trimRight(std::string_view str, const std::string &whitespace)
{
    while (!str.empty() && whitespace.contains(str.back())) {
        str.remove_suffix(1);
    }
    return str;
}

std::wstring_view LStringUtil::trim(std::wstring_view str, const std::wstring &whitespace)
{
    return trimLeft(trimRight(str, whitespace), whitespace);
}

std::wstring_view LStringUtil::trimLeft(std::wstring_view str, const std::wstring &whitespace)
{
    while (!str.empty() && whitespace.contains(str.front())) {
        str.remove_prefix(1);
    }
    return str;
}

std::wstring_view LStringUtil::trimRight(std::wstring_view str, const std::wstring &whitespace)
{
    while (!str.empty() && whitespace.contains(str.back())) {
        str.remove_suffix(1);
    }
    return str;
}


std::string LStringUtil::indent(int lvl, const std::string &base)
{
    std::string ind = "";
    for (int i = 0; i < lvl; i++) {
        ind = ind + base;
    }
    return ind;
}

std::vector<std::wstring> LStringUtil::splitToVec(const std::wstring &str, const std::wstring &delimStr, bool skipEmpty)
{
    std::wregex delim{delimStr};
    std::vector<std::wstring> results;
    std::wsregex_token_iterator end;
    std::wsregex_token_iterator iter(str.begin(), str.end(), delim, -1);
    for ( ; iter != end; ++iter) {
        std::wstring  t(*iter);
        if ((t.empty() == false) || (skipEmpty == false)){
            results.push_back(t);
        }
    }
    return results;
}

std::vector<std::string> LStringUtil::splitToVec(const std::string &str, const std::string &delimStr, bool skipEmpty)
{
    std::regex delim{delimStr};
    std::vector<std::string> results;
    std::sregex_token_iterator end;
    std::sregex_token_iterator iter(str.begin(), str.end(), delim, -1);
    for ( ; iter != end; ++iter) {
        std::string  t(*iter);
        if ((t.empty() == false) || (skipEmpty == false)){
            results.push_back(t);
        }
    }
    return results;
}

void LStringUtil::split(const std::string &s, char delimiter, std::vector<std::string_view> &elems, bool skipEmpty)
{
    std::size_t prev = 0, pos;
    while ((pos = s.find(delimiter, prev)) != std::string::npos) {
        if((pos >prev) || (skipEmpty == false)){
            elems.push_back(std::string_view(&s[prev], pos - prev));
            prev = pos + 1;
        }
    }
    // 添加最后一个元素（不包括分隔符）
    if(prev < s.length()){
        elems.push_back(std::string_view(&s[prev], s.length() - prev));
    }
}


std::string LStringUtil::simplified(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    const auto isSpace = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' ||
               c == '\v' || c == '\f' || c == '\r';
    };

    auto it = str.begin();
    auto end = str.end();

    // 1. 跳过前导空白
    while (it != end && isSpace(*it)) ++it;

    // 2. 处理中间部分：连续空白压缩为单个空格
    bool inSpace = false;
    for (; it != end; ++it) {
        if (isSpace(*it)) {
            inSpace = true;
        } else {
            if (inSpace) {
                result.push_back(' ');
                inSpace = false;
            }
            result.push_back(*it);
        }
    }
    // 3. 末尾无需再处理：因为只有遇到非空白字符才会 push 空格，
    //    所以末尾的空白自然被丢弃

    return result;
}


namespace base64
{
std::array<char, 64> constexpr encodeTable{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
                                            'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                                            'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5',
                                            '6', '7', '8', '9', '+', '/'};

std::array<std::uint8_t, 256> constexpr decodeTable{0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x3E, 0x64, 0x64,
                                                     0x64, 0x3F, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                                                     0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x1A, 0x1B, 0x1C,
                                                     0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
                                                     0x2F, 0x30, 0x31, 0x32, 0x33, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64,
                                                     0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64, 0x64};

std::array<char, 4> encodeTripplet(std::uint8_t a, std::uint8_t b, std::uint8_t c)
{
    std::uint32_t const concat_bits = (a << 16) | (b << 8) | c;

    auto const b64_char1 = encodeTable[(concat_bits >> 18) & 0b0011'1111];
    auto const b64_char2 = encodeTable[(concat_bits >> 12) & 0b0011'1111];
    auto const b64_char3 = encodeTable[(concat_bits >> 6) & 0b0011'1111];
    auto const b64_char4 = encodeTable[concat_bits & 0b0011'1111];
    return {b64_char1, b64_char2, b64_char3, b64_char4};
}

inline bool isValidBase64Char(char c)
{
    auto const decode_byte = decodeTable[c];
    return decode_byte != 0x64;
}

inline bool isValidBase64Str(std::string_view const encoded_str)
{
    if ((encoded_str.size() % 4) == 1)
    {
        return false;
    }

    if (!std::all_of(begin(encoded_str), end(encoded_str) - 2, [](char c) { return isValidBase64Char(c); }))
    {
        return false;
    }

    auto const last = rbegin(encoded_str);
    if (!isValidBase64Char(*next(last)))
    {
        return (*next(last) == '=') && (*last == '=');
    }

    return isValidBase64Char(*last) || (*last == '=');
}

std::array<std::uint8_t, 3> decodeQuad(char a, char b, char c, char d)
{
    std::uint32_t const concat_bytes =
        (decodeTable[a] << 18) | (decodeTable[b] << 12) | (decodeTable[c] << 6) | decodeTable[d];

    std::uint8_t const byte1 = (concat_bytes >> 16) & 0b1111'1111;
    std::uint8_t const byte2 = (concat_bytes >> 8) & 0b1111'1111;
    std::uint8_t const byte3 = concat_bytes & 0b1111'1111;
    return {byte1, byte2, byte3};
}
} // namespace


std::string LStringUtil::encodeToBase64(const std::span<uint8_t> input)
{
    auto const size          = input.size();
    auto const fullTripples = size / 3;

    std::string output;
    output.reserve((fullTripples + 2) * 4);

    for (std::size_t i = 0; i < fullTripples; ++i)
    {
        auto const tripplet     = input.subspan(i * 3, 3);
        auto const base64Chars = base64::encodeTripplet(tripplet[0], tripplet[1], tripplet[2]);
        std::copy(begin(base64Chars), end(base64Chars), back_inserter(output));
    }

    if (auto const remainingChars = size - fullTripples * 3; remainingChars == 2)
    {
        auto const lastTwo     = input.last(2);
        auto const base64Chars = base64::encodeTripplet(lastTwo[0], lastTwo[1], 0x00);

        output.push_back(base64Chars[0]);
        output.push_back(base64Chars[1]);
        output.push_back(base64Chars[2]);
        output.push_back('=');
    }
    else if (remainingChars == 1)
    {
        auto const base64Chars = base64::encodeTripplet(input.back(), 0x00, 0x00);

        output.push_back(base64Chars[0]);
        output.push_back(base64Chars[1]);
        output.push_back('=');
        output.push_back('=');
    }

    return output;
}

std::optional<std::vector<uint8_t> > LStringUtil::decodeFromBase64(const std::string_view encodedStr)
{
    if (encodedStr.size() == 0)
    {
        return std::vector<std::uint8_t>{};
    }

    if (!base64::isValidBase64Str(encodedStr))
    {
        return std::nullopt;
    }

    auto const unpaddedEncodedStr = encodedStr.substr(0, encodedStr.find_first_of('='));
    auto const full_quadruples      = unpaddedEncodedStr.size() / 4;

    std::vector<std::uint8_t> decoded_bytes;
    decoded_bytes.reserve(((full_quadruples + 2) * 3) / 4);

    for (std::size_t i = 0; i < full_quadruples; ++i)
    {
        auto const quad  = unpaddedEncodedStr.substr(i * 4, 4);
        auto const bytes = base64::decodeQuad(quad[0], quad[1], quad[2], quad[3]);
        std::copy(begin(bytes), end(bytes), back_inserter(decoded_bytes));
    }

    if (auto const last_quad = unpaddedEncodedStr.substr(full_quadruples * 4); last_quad.size() == 0)
    {
        return decoded_bytes;
    }
    else if ((last_quad.size() == 2) || (last_quad[2] == '='))
    {
        auto const bytes = base64::decodeQuad(last_quad[0], last_quad[1], 'A', 'A');
        decoded_bytes.push_back(bytes[0]);
    }
    else
    {
        auto const bytes = base64::decodeQuad(last_quad[0], last_quad[1], last_quad[2], 'A');
        std::copy_n(begin(bytes), 2, back_inserter(decoded_bytes));
    }

    return decoded_bytes;
}

LK_NAMESPACE_END
