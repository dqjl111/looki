#ifndef LPLUGIN_H
#define LPLUGIN_H

#include "lkcore/lglobal.h"
#include <memory>

#define _LPLUGIN_ENTRY_FUN_NAME _lk_plugin_entry_fun
#define _LPLUGIN_ENTRY_FUN_NAME_STR L_STR(_lk_plugin_entry_fun)
#ifdef _WIN32
#define _LPLUGIN_ENTRY_EXPORT_SYMBOL _declspec(dllexport)
#else
#define _LPLUGIN_ENTRY_EXPORT_SYMBOL
#endif

//插件中定义并实现：
#define _LPLUGIN_ENTRY_DEFINE(plugType)  \
extern "C" _LPLUGIN_ENTRY_EXPORT_SYMBOL _LK::LPlugin* _LPLUGIN_ENTRY_FUN_NAME(){return new plugType;}


LK_NAMESPACE_BEGIN

class LKCOREDEF LPlugin
{
public:
    LPlugin();
    virtual ~LPlugin();
};


LK_NAMESPACE_END

#endif // LPLUGIN_H
