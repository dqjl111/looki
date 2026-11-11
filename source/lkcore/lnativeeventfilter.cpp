#include "lnativeeventfilter.h"
#include "leventdispatcher.h"

LK_NAMESPACE_BEGIN

LNativeEventFilter::LNativeEventFilter()
{

}

LNativeEventFilter::~LNativeEventFilter()
{
    std::shared_ptr<LEventDispatcher> eventDispatcher = LEventDispatcher::instance();
    if (eventDispatcher){
        eventDispatcher->removeNativeEventFilter(this);
    }
}


LK_NAMESPACE_END
