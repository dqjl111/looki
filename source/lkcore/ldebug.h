#ifndef LDEBUG_H
#define LDEBUG_H
#include "lloging.h"
#include <sstream>
#include <utility>
#include "lstringutil.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LDebug
{
    friend class LLoger;
    struct Stream {
        Stream(LLogType t):ref(1),type(t){
        }

        int ref;
        LLogType type;
        std::ostringstream ts;
        LLogContext context;
    }*stream;
public:
    inline LDebug(LLogType t) :  stream(new Stream(t)){}
    inline LDebug(const LDebug &o):stream(o.stream) { ++stream->ref; }
    LDebug(LDebug &&other) noexcept : stream{std::exchange(other.stream, nullptr)} {}

    inline void swap(LDebug &other) noexcept { std::swap(stream, other.stream); }
    inline LDebug &operator=(const LDebug &other)
    {
        LDebug{other}.swap(*this);
        return *this;
    }

    LDebug &operator=(LDebug &&other) noexcept
    {
        LDebug{std::move(other)}.swap(*this);
        return *this;
    }

    ~LDebug();

    inline LDebug &separate() {
        if(stream->ts.tellp()>0){
            stream->ts << ' ';
        }
        return *this;
    }

    inline LDebug &operator<<(bool t) {
        separate();
        stream->ts << (t ? "true" : "false");
        return *this;
    }

    inline LDebug &operator<<(char t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(signed short t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(unsigned short t) {
        separate();
        stream->ts << t;
        return *this;
    }

    inline LDebug &operator<<(signed int t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(uint32_t t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(signed long t) {
        separate();
        stream->ts << t;
        return *this;
    }

    inline LDebug &operator<<(unsigned long t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(signed long long t) {
        separate();
        stream->ts << t;
        return *this;
    }

    inline LDebug &operator<<(unsigned long long t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(float t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(double t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(const char* t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(const wchar_t* t) {
        separate();
        stream->ts << LStringUtil::wideToUtf8(t);;
        return *this;
    }
    inline LDebug &operator<<(const std::string& t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(const std::string_view t) {
        separate();
        stream->ts << t;
        return *this;
    }
    inline LDebug &operator<<(const std::wstring& t) {
        separate();
        stream->ts << LStringUtil::wideToUtf8(t);
        return *this;
    }
    inline LDebug &operator<<(const void * t) {
        separate();
        stream->ts << t;
        return *this;
    }

    inline LDebug &operator<<(std::nullptr_t) {
        separate();
        stream->ts << "(nullptr)";
        return *this;
    }
};


LK_NAMESPACE_END

#endif // LDEBUG_H
