#include "lfontdatabase.h"
#include "lfontcache.h"
#include "lkcore/lstringutil.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN
#define SMOOTH_SCALABLE 0xffff

std::list<std::wstring> LFontDatabase::m_families;
LFontDatabase::LFontDatabase()
{

}

void LFontDatabase::registerFontFamily(const std::wstring &familyName)
{

    auto it = find(m_families.begin(),
                   m_families.end(),
                   familyName);
    if (it == m_families.end())
    {
        m_families.push_back(familyName);
    }
}


std::shared_ptr<LFontEngine> LFontDatabase::findFont(const LFont::Def &request)
{
    std::shared_ptr<LFontCache> fontCache = LFontCache::instance();
    std::shared_ptr<LFontEngine> engine = fontCache->findEngine(request);
    if (engine != nullptr) {
        return engine;
    }

    std::shared_ptr<LPlatformFontDatabase> pfdb = LGuiApplication::platformIntegration()->fontDatabase();
    engine = pfdb->fontEngine(request);
    if (engine != nullptr) {
        fontCache->insertEngine(request, engine);
    }
    return engine;
}

std::shared_ptr<LFontEngine> LFontDatabase::load(const LFont *font)
{
//    std::unique_lock<std::mutex> lock(m_fontMutex);
    LFont::Def req = font->m_request;

    if (req.pixelSize <0 ) {
        if(req.pointSize <0){
            req.pointSize = 72;
        }
        req.pixelSize = std::floor(((req.pointSize * font->m_dpi) / 72) * 100 + 0.5) / 100;
        req.pixelSize = std::lround(req.pixelSize);
    }
    if (req.pointSize < 0){
        req.pointSize = req.pixelSize*72.0/font->m_dpi;
    }

    if(req.family.empty()){
        const std::wstring& tFam = LGuiApplication::font()->family();
        if(!tFam.empty()){
            req.family = tFam;
        }
    }

    std::shared_ptr<LFontEngine> fe = LFontDatabase::findFont(req);
    if (fe != nullptr) {
        if (font->m_dpi > 0){
            fe->m_fontDef.pointSize = (fe->m_fontDef.pixelSize * 72) / font->m_dpi;
        }
    }
    return fe;
}

const std::list<std::wstring> &LFontDatabase::families()
{
    if(m_families.empty()){
        std::shared_ptr<LPlatformFontDatabase> pfdb = LGuiApplication::platformIntegration()->fontDatabase();
        pfdb->populateFontDatabase();
    }
    return m_families;
}

std::shared_ptr<LFont> LFontDatabase::systemFont()
{
    std::shared_ptr<LPlatformFontDatabase> pfdb = LGuiApplication::platformIntegration()->fontDatabase();
    return pfdb->defaultFont();
}


LK_NAMESPACE_END
