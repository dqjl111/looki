#ifndef LFONTCACHE_H
#define LFONTCACHE_H
#include "lfont.h"
#include "lfontengine.h"
#include <unordered_map>

LK_NAMESPACE_BEGIN

class LFontCache
{
public:
    LFontCache();
    ~LFontCache();

    static std::shared_ptr<LFontCache> instance();
    static void cleanup();

    void clear();

    typedef std::unordered_map<LFont::Def, std::shared_ptr<LFontEngine> > EngineCache;
    std::shared_ptr<LFontEngine> findEngine(const LFont::Def &key);
    void insertEngine(const LFont::Def &key, std::shared_ptr<LFontEngine> engine);
private:
    EngineCache engineCache;
    static std::shared_ptr<LFontCache> inst;
};

LK_NAMESPACE_END
#endif // LFONTCACHE_H
