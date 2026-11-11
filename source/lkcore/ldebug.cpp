#include "ldebug.h"
#include "lloging.h"

LK_NAMESPACE_BEGIN
LDebug::~LDebug()
{
    if (stream) {
        if(stream->ts.tellp()>0){
            LLoger::doLog(stream->type,
                          stream->context,
                          stream->ts.str());
        }
        delete stream;
    }
}

LK_NAMESPACE_END


