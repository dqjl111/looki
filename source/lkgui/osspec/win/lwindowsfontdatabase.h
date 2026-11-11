#ifndef LWINDOWSFONTDATABASE_H
#define LWINDOWSFONTDATABASE_H
#include "lkgui/text/lplatformfontdatabase.h"
#include "lwindows.h"
#include "lkgui/text/lfont.h"

LK_NAMESPACE_BEGIN

class LWindowsFontDatabase : public LPlatformFontDatabase
{
public:
    LWindowsFontDatabase();

    virtual void populateFontDatabase() override;
    virtual std::shared_ptr<LFont> defaultFont() const override{
        return systemDefaultFont();
    }

    virtual std::shared_ptr<LFontEngine> fontEngine(const LFont::Def &fontDef) override;

    static int defaultVerticalDPI();
    static void setDefaultVerticalDPI(int d);

    static HFONT systemFont();
    static double fontSmoothingGamma();

    static std::shared_ptr<LFont> logFontToLFont(const LOGFONTW& lf, int verticalDPI = 0);
    static LOGFONTW fontDefToLOGFONT(const LFont::Def &request);
    static std::shared_ptr<LFontEngine> createEngine(const LFont::Def &request, int dpi);
private:
    void removeApplicationFonts();
    static std::shared_ptr<LFont> systemDefaultFont();

    struct WinApplicationFont {
        HANDLE handle;
        std::wstring fileName;
    };

    std::list<WinApplicationFont> m_applicationFonts;
    std::list<std::wstring> m_eudcFonts;

};

LK_NAMESPACE_END
#endif // LWINDOWSFONTDATABASE_H
