#ifndef LFONTDATABASE_H
#define LFONTDATABASE_H
#include "lfont.h"
#include "lfontengine.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LFontDatabase
{
public:
    LFontDatabase();

    static void registerFontFamily(const std::wstring &familyName);
    static std::shared_ptr<LFontEngine> load(const LFont* font);

    static const std::list<std::wstring>& families();
    static std::shared_ptr<LFont> systemFont();
private:
    static std::shared_ptr<LFontEngine> findFont(const LFont::Def &request);

    static std::list<std::wstring> m_families;
//    static std::mutex m_fontMutex;

    friend class LFont;
    friend class LFontDef;
};


LK_NAMESPACE_END
#endif // LFONTDATABASE_H
