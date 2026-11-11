#ifndef LSTRINGUTIL_H
#define LSTRINGUTIL_H
#include <string>
#include <list>
#include <regex>
#include <vector>
#include <span>
#include <optional>
#include "lkcore/text/lchar.h"

LK_NAMESPACE_BEGIN

struct LKCOREDEF LHashedString {
    LHashedString(const std::string& _str = "") : _value(_str),  _hash(hashCase(_str)){ }
    LHashedString(const char* str) : _value(str), _hash(hashCase(str)) { }

    operator const std::string&() const { return _value; }

    const bool empty() const { return _hash == hashCase(""); }
    const std::string& str() const { return _value; }
    const uint32_t hash() const { return _hash; }

    bool operator ==(const LHashedString& oth) const { return _hash == oth._hash; }
private:
    std::string _value;
    uint32_t _hash;
};


class LKCOREDEF LStringUtil
{
public:
    LStringUtil();
    inline static const std::string empty = "";
    inline static const std::wstring wempty = L"";

    inline static const std::string whiteSpace = "\r\n \t";
    inline static const std::wstring wWhiteSpace = L"\r\n \t";

    static std::wstring utf8ToWide(const std::string& utf8Str);
    static std::string wideToUtf8(const std::wstring& wideStr);

    static std::string convertEncode(const std::string& in, const std::string& fromCharset="utf8", const std::string& toCharset="gbk");

    static std::vector<std::wstring> splitToVec(const std::wstring& str, const std::wstring& delimStr, bool skipEmpty = false);
    static std::vector<std::string> splitToVec(const std::string& str, const std::string& delimStr, bool skipEmpty = false);

    static inline std::list<std::wstring> split(const std::wstring& str, const std::wstring& delimStr, bool skipEmpty = false){
        std::vector<std::wstring> vec =splitToVec(str, delimStr, skipEmpty);
        return std::list<std::wstring>(vec.begin(), vec.end());
    }

    static inline std::list<std::string> split(const std::string& str, const std::string& delimStr, bool skipEmpty = false){
        std::vector<std::string> vec =splitToVec(str, delimStr, skipEmpty);
        return std::list<std::string>(vec.begin(), vec.end());
    }


    static void split(const std::string& s, char delimiter, std::vector<std::string_view>& elems, bool skipEmpty = false);

    static inline bool isNonCharacter(uint32_t ucs4)
    {
        return ucs4 >= 0xfdd0 && (ucs4 <= 0xfdef || (ucs4 & 0xfffe) == 0xfffe);
    }

    static inline bool requiresSurrogates(uint32_t ucs4) noexcept
    {
        return (ucs4 >= 0x10000);
    }

    static inline uint16_t highSurrogate(uint32_t ucs4) noexcept
    {
        return uint16_t((ucs4>>10) + 0xd7c0);
    }
    static inline uint16_t lowSurrogate(uint32_t ucs4) noexcept
    {
        return uint16_t(ucs4%0x400 + 0xdc00);
    }

    static inline bool isHighSurrogate(uint32_t ucs4)
    {
        return ((ucs4 & 0xfffffc00) == 0xd800);
    }
    static inline bool isLowSurrogate(uint32_t ucs4)
    {
        return ((ucs4 & 0xfffffc00) == 0xdc00);
    }

    static inline bool isSurrogate(uint32_t ucs4)
    {
        return (ucs4 - 0xd800u < 2048u);
    }

    static inline uint32_t surrogateToUcs4(uint16_t high, uint16_t low)
    {
        return (uint32_t(high)<<10) + low - 0x35fdc00;
    }


    static inline bool isSpace(uint32_t ucs4)
    {
        // note that [0x09..0x0d] + 0x85 are exceptional Cc-s and must be handled explicitly
        return ucs4 == 0x20 || (ucs4 <= 0x0d && ucs4 >= 0x09)
               || (ucs4 > 127 && (ucs4 == 0x85 || ucs4 == 0xa0 || isSpaceHelper(ucs4)));
    }

    static inline bool isLetter(uint32_t ucs4)
    {
        return (ucs4 >= 'A' && ucs4 <= 'z' && (ucs4 >= 'a' || ucs4 <= 'Z'))
               || (ucs4 > 127 && isLetterHelper(ucs4));
    }

    static uint32_t toLower(uint32_t ucs4);
    static uint32_t toUpper(uint32_t ucs4);

    static inline uint8_t cell(uint32_t ucs)  { return (ucs & 0xff); }
    static inline uint8_t row(uint32_t ucs)  { return ((ucs>>8)&0xff); }


    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](char c) { return tolower(c); });
        return s;
    }

    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](char c) { return toupper(c); });
        return s;
    }

    static std::wstring toLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](wchar_t c) { return towlower(c); });
        return s;
    }

    static std::wstring toUpper(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](wchar_t c) { return towupper(c); });
        return s;
    }

    static lchar::Category category(uint32_t ucs4)
    {
        if (ucs4 > lchar::LastValidCodePoint){
            return lchar::Other_NotAssigned;
        }
        return (lchar::Category)lchar::Util::properties(ucs4)->category;
    }

    static lchar::Direction direction(uint32_t ucs4) noexcept
    {
        if (ucs4 > lchar::LastValidCodePoint){
            return lchar::DirL;
        }
        return (lchar::Direction) lchar::Util::properties(ucs4)->direction;
    }

    static lchar::Script script(uint32_t ucs4) noexcept
    {
        if (ucs4 > lchar::LastValidCodePoint){
            return lchar::Script_Unknown;
        }
        return (lchar::Script) lchar::Util::properties(ucs4)->script;
    }

    static lchar::JoiningType joiningType(uint32_t ucs4){
        if (ucs4 > lchar::LastValidCodePoint){
            return lchar::Joining_None;
        }
        return (lchar::JoiningType) lchar::Util::properties(ucs4)->joining;

    }

    static inline bool isDigit(uint32_t ucs4) noexcept
    { return (ucs4 <= '9' && ucs4 >= '0') || (ucs4 > 127 && category(ucs4) == lchar::Number_DecimalDigit); }
    static inline bool isLower(uint32_t ucs4) noexcept
    { return (ucs4 <= 'z' && ucs4 >= 'a') || (ucs4 > 127 && category(ucs4) == lchar::Letter_Lowercase); }
    static inline bool isUpper(uint32_t ucs4) noexcept
    { return (ucs4 <= 'Z' && ucs4 >= 'A') || (ucs4 > 127 && category(ucs4) == lchar::Letter_Uppercase); }
    static inline bool isHex(uint32_t ucs4) noexcept{
        return isDigit(ucs4) || (ucs4 <= 'f' && ucs4 >= 'a') || (ucs4 <= 'F' && ucs4 >= 'A');
    }
    static inline bool isAlnum(uint32_t ucs4) noexcept{
        // matches [a-zA-Z0-9_]
        return ucs4 - 'a' < 26 || ucs4 - 'A' < 26 || ucs4 - '0' < 10 || ucs4 == '_';
    }

    /// Converts a hex digit to its decimal representation
    static inline uint8_t hexToDec(uint32_t ucs4) {
        if (ucs4 >= '0' && ucs4 <= '9') return ucs4 - '0';
        if (ucs4 >= 'a' && ucs4 <= 'f') return ucs4 - 'a' + 10;
        if (ucs4 >= 'A' && ucs4 <= 'F') return ucs4 - 'A' + 10;
        return 0;
    }

    /// Converts a two digit hex to its decimal representation
    static inline uint8_t hexToDec(uint32_t l, uint32_t r) {
        return 16 * hexToDec(l) + hexToDec(r);
    }


    static bool isPrint(uint32_t ucs4);

    static bool isRightToLeft(const std::wstring& s);
    static bool isRightToLeft(const wchar_t* s, int size);

    static lchar::Decomposition decompositionTag(uint32_t ucs4);

    static bool stob(const std::string& str, bool* isOk = nullptr);
    static int stoi(const std::string& str, bool* isOk = nullptr, int base =10);
    static long stol(const std::string& str, bool* isOk = nullptr, int base =10);
    static unsigned long stoul(const std::string& str, bool* isOk = nullptr, int base =10);
    static long long stoll(const std::string& str, bool* isOk = nullptr, int base =10);
    static unsigned long long stoull(const std::string& str, bool* isOk = nullptr, int base =10);
    static float stof(const std::string& str, bool* isOk = nullptr);
    static double stod(const std::string& str, bool* isOk = nullptr);
    static long double stold(const std::string& str, bool* isOk = nullptr);

    static bool stob(const std::wstring& str, bool* isOk = nullptr);
    static int stoi(const std::wstring& str, bool* isOk = nullptr, int base =10);
    static long stol(const std::wstring& str, bool* isOk = nullptr, int base =10);
    static unsigned long stoul(const std::wstring& str, bool* isOk = nullptr, int base =10);
    static long long stoll(const std::wstring& str, bool* isOk = nullptr, int base =10);
    static unsigned long long stoull(const std::wstring& str, bool* isOk = nullptr, int base =10);
    static float stof(const std::wstring& str, bool* isOk = nullptr);
    static double stod(const std::wstring& str, bool* isOk = nullptr);
    static long double stold(const std::wstring& str, bool* isOk = nullptr);

    template<class T>
    static T sto(const std::string& str, bool* isOk = nullptr){
        T ret;
        if(str.empty()){
            if(isOk != nullptr){
                *isOk = false;
            }
        }else{
            std::istringstream stream(str);
            if(stream >> ret){
                if(isOk != nullptr){
                    *isOk = true;
                }
            }else{
                if(isOk != nullptr){
                    *isOk = false;
                }
            }
        }
        return ret;
    }

    template<class T>
    inline static T sto(std::string_view str, bool* isOk = nullptr){
        std::string s(str);
        return sto<T>(s, isOk);
    }

    template<class T>
    static T sto(const std::wstring& str, bool* isOk = nullptr){
        T ret;
        if(str.empty()){
            if(isOk != nullptr){
                *isOk = false;
            }
        }else{
            std::wistringstream stream(str);
            if(stream >> ret){
                if(isOk != nullptr){
                    *isOk = true;
                }
            }else{
                if(isOk != nullptr){
                    *isOk = false;
                }
            }
        }
        return ret;
    }

    template<class T>
    inline  static T sto(std::wstring_view str, bool* isOk = nullptr){
        std::wstring s(str);
        return sto<T>(s, isOk);
    }

    static std::string replace(const std::string& orgStr, const std::string& newStr, std::string str);
    static std::wstring replace(const std::wstring& orgStr, const std::wstring& newStr, std::wstring str);

    static std::string_view trim(std::string_view str, const std::string& whitespace = whiteSpace);
    static std::string_view trimLeft(std::string_view str, const std::string& whitespace = whiteSpace);
    static std::string_view trimRight(std::string_view str, const std::string& whitespace = whiteSpace);

    static std::wstring_view trim(std::wstring_view str, const std::wstring& whitespace = wWhiteSpace);
    static std::wstring_view trimLeft(std::wstring_view str, const std::wstring& whitespace = wWhiteSpace);
    static std::wstring_view trimRight(std::wstring_view str, const std::wstring& whitespace = wWhiteSpace);

    inline static std::string trim(const std::string& str, const std::string& whitespace = whiteSpace){
        return std::string(trim(std::string_view(str), whitespace));
    }
    inline static std::string trimLeft(const std::string& str, const std::string& whitespace = whiteSpace){
        return std::string(trimLeft(std::string_view(str), whitespace));
    }
    inline static std::string trimRight(const std::string& str, const std::string& whitespace = whiteSpace){
        return std::string(trimRight(std::string_view(str), whitespace));
    }

    inline static std::wstring trim(const std::wstring& str, const std::wstring& whitespace = wWhiteSpace){
        return std::wstring(trim(std::wstring_view(str), whitespace));
    }
    inline static std::wstring trimLeft(const std::wstring& str, const std::wstring& whitespace = wWhiteSpace){
        return std::wstring(trimLeft(std::wstring_view(str), whitespace));
    }
    inline static std::wstring trimRight(const std::wstring& str, const std::wstring& whitespace = wWhiteSpace){
        return std::wstring(trimRight(std::wstring_view(str), whitespace));
    }

    static std::string indent(int lvl, const std::string &base);
    static uint32_t safeAt(const std::string &str, const int pos);

    inline static int compareIgnoreCase(const std::string& a, const std::string& b){
#if defined(_MSC_VER) || defined(__DMC__)
        return _stricmp(a.c_str(), b.c_str());
#else
        return strcasecmp(a.c_str(), b.c_str());
#endif
    }
    inline static int compareIgnoreCase(const std::wstring& a, const std::wstring& b){
#if defined(_MSC_VER) || defined(__DMC__)
        return _wcsicmp(a.c_str(), b.c_str());
#else
        return wcscasecmp(a.c_str(), b.c_str());
#endif
    }

    static std::string simplified(const std::string& str);

    static std::string encodeToBase64(const std::span<uint8_t> input);
    inline static std::string encodeToBase64(std::vector<uint8_t> input){
        return encodeToBase64(std::span<uint8_t>(input.begin(), input.end()));
    }

    inline static std::string encodeToBase64(std::string_view input){
        return encodeToBase64(std::span<uint8_t>((uint8_t*)(input.data()), input.size()));
    }

    static std::optional<std::vector<std::uint8_t>> decodeFromBase64(const std::string_view encodedStr);
protected:
    static bool isSpaceHelper(uint32_t ucs4) ;
    static bool isLetterHelper(uint32_t ucs4) ;
    static bool isNumberHelper(uint32_t ucs4) ;
    static bool isLetterOrNumberHelper(uint32_t ucs4) ;
};

LK_NAMESPACE_END

#endif // LSTRINGUTIL_H
