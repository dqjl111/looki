#include "ldynamiclibrary.h"
#include "lkcore/ldebug.h"
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else //UNIX
#include <dlfcn.h>
#endif

LK_NAMESPACE_BEGIN
#ifdef _WIN32
#elif __APPLE__
#else
#ifndef PATH_MAX
// suitably large value that won't consume too much memory
#define PATH_MAX  1024*1024
#endif
std::string lReadlink(const std::string& path){
    std::vector<char> space(256);
    ssize_t retLen = ::readlink(path.data(), space.data(), space.size());
    if (retLen > space.size()) {
        space.resize(retLen);
        retLen = ::readlink(path.data(), space.data(), space.size());
    }else{
        while(retLen == space.size()){
            if (space.size() >= PATH_MAX) {
                return "";
            }

            space.resize(space.size() * 2);
            retLen = ::readlink(path.data(), space.data(), space.size());
        }
    }
    if(retLen <0){
        return "";
    }
    return std::string(space.data(), retLen);
}
#endif


std::string lAppFileName()
{
#ifdef _WIN32
    std::vector<char> space(MAX_PATH);
    UINT retLen = ::GetModuleFileNameA(NULL, space.data(), space.size());
    if (retLen > MAX_PATH) {
        space.resize(retLen);
        retLen= ::GetModuleFileNameA(NULL, space.data(), space.size());
    }
    if(retLen <0){
        return "";
    }
    return std::string(space.data(), retLen);
#elif __APPLE__
    static QString appFileName;
    if (appFileName.isEmpty()) {
        QCFType<CFURLRef> bundleURL(CFBundleCopyExecutableURL(CFBundleGetMainBundle()));
        if (bundleURL) {
            QCFString cfPath(CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle));
            if (cfPath){
                appFileName = cfPath;
            }
        }
    }
    return appFileName;
#else
    return lReadlink("/proc/self/exe");
#endif
}

std::string lSystemDirectory(){
#ifdef _WIN32
    std::vector<char> fullPath(MAX_PATH);
    UINT retLen = ::GetSystemDirectoryA(fullPath.data(), MAX_PATH);
    if (retLen > MAX_PATH) {
        fullPath.resize(retLen);
        retLen = ::GetSystemDirectoryA(fullPath.data(), retLen);
    }
    return std::string(fullPath.data(), retLen);
#elif __APPLE__
    return "";
#else
    return "";
#endif
}

inline std::string adaptLibraryName(const std::string& libraryName)
{
#ifdef _WIN32
    return std::string(libraryName + ".dll");
#elif __APPLE__
    return std::string(libraryName + ".dylib");
#else \
    //On unix-ish the library name probably needs to start with `lib`
    return std::string(libraryName + ".so");
#endif
}

[[nodiscard]] inline void* loadDynamicLibrary(const std::string& libraryName)
{
#ifdef _WIN32
    return reinterpret_cast<void*>(LoadLibraryA(libraryName.c_str()));
#else
    return dlopen(libraryName.c_str(), RTLD_NOW);
#endif
}

inline void unloadDynamicLibrary(void* libHandle)
{
#ifdef _WIN32
    FreeLibrary(HMODULE(libHandle));
#else
    dlclose(libHandle);
#endif
}

[[nodiscard]] inline auto getProcAddress(void* libHandle, const std::string& functionName)
{
#ifdef _WIN32
    return GetProcAddress(HMODULE(libHandle), LPCSTR(functionName.c_str()));
#else
    return dlsym(libHandle, functionName.c_str());
#endif
}

std::map<std::string, std::shared_ptr<LDynamicLibrary> > LDynamicLibrary::m_libraries;
LDynamicLibrary::~LDynamicLibrary()
{
    if(m_handle != nullptr){
        unloadDynamicLibrary(m_handle);
    }
    m_handle = nullptr;
}


LDynamicLibrary::LDynamicLibrary(const std::string &name, uint8_t loadPathFlags)
{
    m_libraryName = name;
    std::string pathName = adaptLibraryName(name);
    if(!(loadPathFlags & AllPath)){
        m_handle = loadDynamicLibrary(pathName);
    }else{
        std::list<std::string> searchOrder;

        if (loadPathFlags & AppPath){
            std::filesystem::path path = lAppFileName();
            searchOrder.push_back(path.parent_path().string());
        }
        if (loadPathFlags & LookiPath){
            std::string lookiPath = lGetEnv("LOOKI_PATH");
            if(!lookiPath.empty()){
                searchOrder.push_back(lookiPath);
            }
        }
        if (loadPathFlags & SystemPath){
            searchOrder.push_back(lSystemDirectory());
        }
        if (loadPathFlags & EnvPath) {
            std::string envPath = lGetEnv("PATH");
            searchOrder.splice(searchOrder.end(), LStringUtil::split(envPath, ";"));
        }
        // Start looking in the order specified
        for (std::string& fullPathAttempt: searchOrder) {
            if (!fullPathAttempt.ends_with(lchar::SpecialChar::PathSeparator)) {
                fullPathAttempt.push_back(lchar::SpecialChar::PathSeparator);
            }
            fullPathAttempt += pathName;
            m_handle = loadDynamicLibrary(fullPathAttempt);
            if (m_handle != 0){
                break;
            }
        }
    }
}

LFunctionPointer LDynamicLibrary::resolve(const std::string &symbol)
{
    if(m_handle == nullptr){
        return nullptr;
    }

    return LFunctionPointer(getProcAddress(m_handle, symbol));
}

LFunctionPointer LDynamicLibrary::resolve(const std::string &libraryName, const std::string &symbol, uint8_t loadPathFlags)
{
    std::shared_ptr<LDynamicLibrary> lib = load(libraryName, loadPathFlags);
    if(lib != nullptr){
        return lib->resolve(symbol);
    }
    return nullptr;
}


std::shared_ptr<LPlugin> LDynamicLibrary::createPlugin()
{
    if(m_handle == nullptr){
        return nullptr;
    }

    using EntryFunc = LPlugin* (*)();
    EntryFunc fun = reinterpret_cast<EntryFunc>(getProcAddress(m_handle, _LPLUGIN_ENTRY_FUN_NAME_STR));
    if(fun != nullptr){
        return std::shared_ptr<LPlugin>(fun());
    }
    return nullptr;
}

std::shared_ptr<LDynamicLibrary> LDynamicLibrary::load(const std::string &libraryName, uint8_t loadPathFlags)
{
    auto libraryIterator = m_libraries.find(libraryName);
    if(libraryIterator == m_libraries.end())
    {
        std::shared_ptr<LDynamicLibrary> lib = std::make_shared<LDynamicLibrary>(libraryName, loadPathFlags);
        if(lib->isValid()){
            m_libraries[libraryName] = lib;
            return lib;
        }else{
            return nullptr;
        }
    }else{
        return libraryIterator->second;
    }
}

void LDynamicLibrary::unload(const std::string &libraryName)
{
    m_libraries.erase(libraryName);
}

std::shared_ptr<LPlugin> LDynamicLibrary::loadPlugin(const std::string &libraryName, uint8_t loadPathFlags)
{
    std::shared_ptr<LDynamicLibrary> lib = load(libraryName, loadPathFlags);
    if(lib != nullptr){
        return lib->createPlugin();
    }
    return nullptr;
}


LK_NAMESPACE_END
