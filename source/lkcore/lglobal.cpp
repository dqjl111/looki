#include "lglobal.h"
#include "ldebug.h"
#include <shared_mutex>
#include <map>
LK_NAMESPACE_BEGIN


static std::shared_mutex environmentMutex;
static std::map<std::string, std::string> userEnv;


std::string lGetEnv(const std::string& varName, bool useUserEnv)
{
    std::shared_lock<std::shared_mutex> lock(environmentMutex);
    if(useUserEnv){
        std::map<std::string, std::string>::const_iterator iter = userEnv.find(varName);
        if(iter != userEnv.cend()){
            return iter->second;
        }
    }
#ifdef WIN32
    char* buf = nullptr;
    size_t s = 0;
    int t = _dupenv_s(&buf, &s, varName.data());
    std::string ret;
    if(buf != nullptr){
        if(t == 0){
            ret.assign(buf, s);
        }
        free(buf);
    }
    return ret;
#else
    return std::string(::getenv(varName.data()));
#endif
}

void lCheckPointer(const char *n, int l) noexcept
{
    lFatal() << "Out of memory in " << n << ", line " << l;
    std::terminate();
}

void lSetUserEnv(const std::string &varName, const std::string &varValue)
{
    if(!varName.empty()){
        std::unique_lock<std::shared_mutex> lock(environmentMutex);
        userEnv[varName] = varValue;
    }
}

LK_NAMESPACE_END
