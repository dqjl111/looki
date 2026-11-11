#ifndef LDYNAMICLIBRARY_H
#define LDYNAMICLIBRARY_H

#include "lkcore/plugin/lplugin.h"
#include <map>

LK_NAMESPACE_BEGIN
LKCOREDEF std::string lAppFileName();
LKCOREDEF std::string lSystemDirectory();
#ifdef _WIN32
#elif __APPLE__
#else
LKCOREDEF std::string lReadlink(const std::string& path);
#endif

class LKCOREDEF LDynamicLibrary
{
public:
    ~LDynamicLibrary();

    LDynamicLibrary& operator=(const LDynamicLibrary&) = delete;
    LDynamicLibrary(const LDynamicLibrary&)			   = delete;

    enum LoadPathFlag{
        AppPath = 0x01,
        LookiPath = 0x02,
        SystemPath = 0x04,
        EnvPath = 0x08,
        AllPath = AppPath|LookiPath|SystemPath|EnvPath
    };

    LDynamicLibrary(const std::string& name, uint8_t loadPathFlags = AppPath);
    inline bool isValid() const{
        return m_handle != nullptr;
    }

    const std::string& libraryName() const{
        return m_libraryName;
    }

    void* handle() const{
        return m_handle;
    }

    LFunctionPointer resolve(const std::string& symbol);
    std::shared_ptr<LPlugin> createPlugin();

    template<class T>
    std::shared_ptr<T> createPluginT(){
        return std::dynamic_pointer_cast<T>(createPlugin());
    }

    static LFunctionPointer resolve(const std::string &libraryName, const std::string& symbol, uint8_t loadPathFlags = SystemPath);
    static std::shared_ptr<LDynamicLibrary> load(const std::string& libraryName, uint8_t loadPathFlags = AllPath);
    static void unload(const std::string& libraryName);
    static std::shared_ptr<LPlugin> loadPlugin(const std::string &libraryName, uint8_t loadPathFlags = AppPath|LookiPath);

    template<class T>
    static std::shared_ptr<T> loadPluginT(const std::string &libraryName, uint8_t loadPathFlags = AppPath|LookiPath){
        return std::dynamic_pointer_cast<T>(loadPlugin(libraryName, loadPathFlags));
    }

private:
    static std::map<std::string, std::shared_ptr<LDynamicLibrary> > m_libraries;
    void* m_handle = nullptr;
    std::string m_libraryName;
};


LK_NAMESPACE_END

#endif // LDYNAMICLIBRARY_H
