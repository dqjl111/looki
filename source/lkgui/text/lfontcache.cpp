#include "lfontcache.h"
LK_NAMESPACE_BEGIN

std::shared_ptr<LFontCache> LFontCache::inst = nullptr;

LFontCache::LFontCache()
{
}

LFontCache::~LFontCache()
{
    clear();
}

std::shared_ptr<LFontCache> LFontCache::instance()
{
    if(inst == nullptr){
        inst = std::make_shared<LFontCache>();
    }
    return inst;
}

void LFontCache::cleanup()
{
    inst = nullptr;
}

void LFontCache::clear()
{
    engineCache.clear();
}

std::shared_ptr<LFontEngine> LFontCache::findEngine(const LFont::Def &key)
{
    EngineCache::iterator it = engineCache.find(key);
    if (it == engineCache.end()){
        return nullptr;
    }
    return it->second;
}


void LFontCache::insertEngine(const LFont::Def &key, std::shared_ptr<LFontEngine> engine)
{
    if(engine != nullptr){
        engineCache[key] = engine;
    }
}



LK_NAMESPACE_END
