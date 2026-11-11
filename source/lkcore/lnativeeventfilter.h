#ifndef LNATIVEEVENTFILTER_H
#define LNATIVEEVENTFILTER_H

#include "lkcore/lglobal.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LNativeEventFilter
{
public:
    LNativeEventFilter();
    virtual ~LNativeEventFilter();
    virtual bool nativeEventFilter(const std::string &eventType, void *message, lintptr *result) = 0;
};

LK_NAMESPACE_END

#endif // LNATIVEEVENTFILTER_H
