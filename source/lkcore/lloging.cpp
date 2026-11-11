#include "lloging.h"
#include "ldebug.h"
#include <atomic>
#include <iostream>
#include "lstringutil.h"

LK_NAMESPACE_BEGIN



LLogContext &LLogContext::copyContextFrom(const LLogContext &logContext) noexcept
{
    this->file = logContext.file;
    this->line = logContext.line;
    return *this;
}

std::string LLoger::defaultLogEncode;

void LLoger::defaultLogCout(LLogType type, const LLogContext &context, const std::string &message)
{
    std::ostream& os = type == LFatalMsg ? std::cerr : std::cout;
#ifdef _DEBUG
    if(defaultLogEncode.empty() || defaultLogEncode == "utf-8"){
        os << context.file << '['<<  context.line << "]:" << message << std::endl;
    }else{
        os << context.file << '['<<  context.line << "]:" << LStringUtil::convertEncode(message, "", defaultLogEncode).data() << std::endl;
    }
#else
    os << message << std::endl;
#endif
}


static std::atomic<LLogHandler> llogHandler = LLoger::defaultLogCout;

LDebug LLoger::debug() const
{
    LDebug dbg = LDebug(LDebugMsg);
    dbg.stream->context.copyContextFrom(context);
    return dbg;
}

LDebug LLoger::info() const
{
    LDebug dbg = LDebug(LInfoMsg);
    dbg.stream->context.copyContextFrom(context);
    return dbg;
}

LDebug LLoger::warn() const
{
    LDebug dbg = LDebug(LWarningMsg);
    dbg.stream->context.copyContextFrom(context);
    return dbg;
}

LDebug LLoger::sys() const
{
    LDebug dbg = LDebug(LSystemMsg);
    dbg.stream->context.copyContextFrom(context);
    return dbg;
}

LDebug LLoger::fatal() const
{
    LDebug dbg = LDebug(LFatalMsg);
    dbg.stream->context.copyContextFrom(context);
    return dbg;
}

LLogHandler LLoger::installLogHandler(LLogHandler h)
{
    LLogHandler old = llogHandler.exchange(h);
    return old;
}

void LLoger::doLog(LLogType type, const LLogContext &context, const std::string &message)
{
    LLogHandler handler = llogHandler.load(std::memory_order_relaxed);
    if (handler != nullptr) {
        handler(type, context, message);
    }
}

LK_NAMESPACE_END
