#ifndef LLOGING_H
#define LLOGING_H
#include <functional>
#include "lglobal.h"
LK_NAMESPACE_BEGIN

class LDebug;
class LLoger;
enum LLogType { LDebugMsg, LWarningMsg, LSystemMsg, LFatalMsg, LInfoMsg};

class LKCOREDEF LLogContext
{
public:
    constexpr LLogContext() noexcept = default;
    constexpr LLogContext(const char *fileName, int lineNumber) noexcept
        : line(lineNumber)
        , file(fileName)
    {

    }

    int line = 0;
    const char *file = nullptr;
    //const char *function = nullptr;
private:
    LLogContext &copyContextFrom(const LLogContext &logContext) noexcept;

    friend class LDebug;
    friend class LLoger;
};


typedef void (*LLogHandler)(LLogType, const LLogContext &, const std::string &);
class LKCOREDEF LLoger
{
public:
    constexpr LLoger() : context() {}
    constexpr LLoger(const char *file, int line)
        : context(file, line) {}

    LDebug debug() const;
    LDebug info() const;
    LDebug warn() const;
    LDebug sys() const;
    LDebug fatal() const;

    static LLogHandler installLogHandler(LLogHandler);
    static void doLog(LLogType type, const LLogContext &context, const std::string &message);

    static void defaultLogCout(LLogType type, const LLogContext &context, const std::string &message);
    static std::string defaultLogEncode;
private:
    LLogContext context;
};

#ifndef NDEBUG
#define _LLOG_FILE static_cast<const char *>(__FILE__)
#define _LLOG_LINE __LINE__
#else
#define _LLOG_FILE nullptr
#define _LLOG_LINE 0
#endif

#define lDebug LLoger(_LLOG_FILE, _LLOG_LINE).debug
#define lInfo LLoger(_LLOG_FILE, _LLOG_LINE).info
#define lWarn LLoger(_LLOG_FILE, _LLOG_LINE).warn
#define lSys LLoger(_LLOG_FILE, _LLOG_LINE).sys
#define lFatal LLoger(_LLOG_FILE, _LLOG_LINE).fatal


LK_NAMESPACE_END
#endif // LLOGING_H
