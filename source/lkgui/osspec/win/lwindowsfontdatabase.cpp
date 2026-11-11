#include "lwindowsfontdatabase.h"
#include "lwindowsfontengine.h"
#include "lkgui/text/lfontdatabase.h"

LK_NAMESPACE_BEGIN

bool isLocalizedName(const std::wstring &name)
{
    for(const wchar_t t: name){
        if(t>=0x100){
            return true;
        }
    }
    return false;
}


static int sDefaultVerticalDPI = 96; // Native Pixels

static int L_WIN_CALLBACK populateFontFamilies(const LOGFONTW *logFont, const TEXTMETRICW *textmetric, DWORD, LPARAM)
{
    // the "@family" fonts are just the same as "family". Ignore them.
    const ENUMLOGFONTEXW *f = reinterpret_cast<const ENUMLOGFONTEXW *>(logFont);
    const wchar_t *faceNameW = f->elfLogFont.lfFaceName;
    if (faceNameW[0] && faceNameW[0] != L'@' && wcsncmp(faceNameW, L"WST_", 4)) {
        const std::wstring faceName = faceNameW;
        LFontDatabase::registerFontFamily(faceName);
        // Register current font's english name as alias
//        const bool ttf = (textmetric->tmPitchAndFamily & TMPF_TRUETYPE);
//        if (ttf && isLocalizedName(faceName)) {
//            const std::string englishName = getEnglishName(faceName);
//            if (!englishName.empty()){
//                LPlatformFontDatabase::registerAliasToFontFamily(faceName, englishName);
//            }
//        }
    }
    return 1; // continue
}


LWindowsFontDatabase::LWindowsFontDatabase()
{

}


void LWindowsFontDatabase::removeApplicationFonts()
{
    for (const WinApplicationFont &font : m_applicationFonts) {
        if (font.handle) {
            RemoveFontMemResourceEx(font.handle);
        } else {
            RemoveFontResourceExW(reinterpret_cast<LPCWSTR>(font.fileName.data()),
                                  FR_PRIVATE, nullptr);
        }
    }
    m_applicationFonts.clear();
    m_eudcFonts.clear();
}

std::shared_ptr<LFont> LWindowsFontDatabase::systemDefaultFont()
{
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = FIELD_OFFSET(NONCLIENTMETRICSW, lfMessageFont) + sizeof(LOGFONTW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize , &ncm, 0);
    return LWindowsFontDatabase::logFontToLFont(ncm.lfMessageFont);
}


void LWindowsFontDatabase::populateFontDatabase()
{
    removeApplicationFonts();
    HDC dummy = GetDC(0);
    LOGFONTW lf;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfFaceName[0] = 0;
    lf.lfPitchAndFamily = 0;
    EnumFontFamiliesExW(dummy, &lf, populateFontFamilies, 0, 0);
    ReleaseDC(0, dummy);
//    // Work around EnumFontFamiliesEx() not listing the system font.
//    std::wstring systemDefaultFamily = LWindowsFontDatabase::systemDefaultFont().family();
//    if (LWindowsFontDatabase::resolveFontFamilyAlias(systemDefaultFamily) == systemDefaultFamily){
//        LWindowsFontDatabase::registerFontFamily(systemDefaultFamily);
//    }
    //    addDefaultEUDCFont();
}

std::shared_ptr<LFontEngine> LWindowsFontDatabase::fontEngine(const LFont::Def &fontDef)
{
    return createEngine(fontDef, defaultVerticalDPI());
}

int LWindowsFontDatabase::defaultVerticalDPI()
{
    return sDefaultVerticalDPI;
}

void LWindowsFontDatabase::setDefaultVerticalDPI(int d)
{
    sDefaultVerticalDPI = d;
}

HFONT LWindowsFontDatabase::systemFont()
{
    static const auto stockSysfont =
        reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    return stockSysfont;
}

double LWindowsFontDatabase::fontSmoothingGamma()
{
    int winSmooth;
    double result = 1;
    if (SystemParametersInfo(0x200C /* SPI_GETFONTSMOOTHINGCONTRAST */, 0, &winSmooth, 0)){
        result = double(winSmooth) / double(1000.0);
    }

    // Safeguard ourselves against corrupt registry values...
    if (result > 5 || result < 1){
        result = double(1.4);
    }
    return result;
}

std::shared_ptr<LFont> LWindowsFontDatabase::logFontToLFont(const LOGFONTW &logFont, int verticalDPI)
{
    if (verticalDPI <= 0){
        verticalDPI = defaultVerticalDPI();
    }
    std::shared_ptr<LFont> qFont = std::make_shared<LFont>(logFont.lfFaceName);
    qFont->setItalic(logFont.lfItalic);
    if (logFont.lfWeight != FW_DONTCARE){
        qFont->setWeight(LPlatformFontDatabase::weightFromInteger(logFont.lfWeight));
    }
    const double logFontHeight = std::abs(logFont.lfHeight);
    qFont->setPointSizeF(logFontHeight * 72.0 / double(verticalDPI));
    qFont->setUnderline(logFont.lfUnderline);
    qFont->setOverline(false);
    qFont->setStrikeOut(logFont.lfStrikeOut);
    return qFont;
}

LOGFONTW LWindowsFontDatabase::fontDefToLOGFONT(const LFont::Def &request)
{
    LOGFONTW lf;
    memset(&lf, 0, sizeof(LOGFONTW));

    lf.lfHeight = -std::lround(request.pixelSize);
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    if (request.weight == 50){
        lf.lfWeight = FW_DONTCARE;
    }else{
        lf.lfWeight = (request.weight*900)/99;
    }
    lf.lfItalic = request.style != LFont::StyleNormal;
    lf.lfCharSet = DEFAULT_CHARSET;

    int strat = OUT_DEFAULT_PRECIS;
    if (request.styleStrategy & LFont::PreferBitmap) {
        strat = OUT_RASTER_PRECIS;
    } else if (request.styleStrategy & LFont::PreferDevice) {
        strat = OUT_DEVICE_PRECIS;
    } else if (request.styleStrategy & LFont::PreferOutline) {
        strat = OUT_OUTLINE_PRECIS;
    } else if (request.styleStrategy & LFont::ForceOutline) {
        strat = OUT_TT_ONLY_PRECIS;
    }

    lf.lfOutPrecision   = strat;

    int qual = DEFAULT_QUALITY;

    if (request.styleStrategy & LFont::PreferMatch){
        qual = DRAFT_QUALITY;
    }else if (request.styleStrategy & LFont::PreferQuality){
        qual = PROOF_QUALITY;
    }

    if (request.styleStrategy & LFont::PreferAntialias) {
        qual = (request.styleStrategy & LFont::NoSubpixelAntialias) == 0
                   ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY;
    } else if (request.styleStrategy & LFont::NoAntialias) {
        qual = NONANTIALIASED_QUALITY;
    } else if ((request.styleStrategy & LFont::NoSubpixelAntialias)
               && LWindowsFontEngine::sharedData()->m_clearTypeEnabled) {
        qual = ANTIALIASED_QUALITY;
    }

    lf.lfQuality = qual;

    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;

    int hint = FF_DONTCARE;
    switch (request.styleHint) {
    case LFont::Helvetica:{
        hint = FF_SWISS;
        break;
    }
    case LFont::Times:{
        hint = FF_ROMAN;
        break;
    }
    case LFont::Courier:{
        hint = FF_MODERN;
        break;
    }
    case LFont::OldEnglish:{
        hint = FF_DECORATIVE;
        break;
    }
    case LFont::System:{
        hint = FF_MODERN;
        break;
    }
    default:{
        break;
    }
    }

    lf.lfPitchAndFamily = DEFAULT_PITCH | hint;
    std::wstring fam = request.family;
    if (fam.empty()){
        if (request.style == LFont::StyleItalic || (-lf.lfHeight > 18 && -lf.lfHeight != 24)) {
            fam = L"Arial";
        }else{
            fam = L"MS Sans Serif";
        }
    }
    memcpy(lf.lfFaceName, fam.data(), fam.size() * sizeof(wchar_t));
    return lf;

}

std::shared_ptr<LFontEngine> LWindowsFontDatabase::createEngine(const LFont::Def &request, int dpi)
{
    LOGFONTW lf = fontDefToLOGFONT(request);
    std::shared_ptr<LWindowsFontEngine> fe = std::make_shared<LWindowsFontEngine>(request.family, lf);
    std::shared_ptr<LWindowsFontEngineData> data = LWindowsFontEngine::sharedData();

    const bool preferClearTypeAA = lf.lfQuality == CLEARTYPE_QUALITY;

    if (request.stretch != 100) {
        HFONT hfont = CreateFontIndirectW(&lf);
        if (!hfont) {
            lWarn() << "CreateFontIndirect failed";
            hfont = LWindowsFontDatabase::systemFont();
        }

        HGDIOBJ oldObj = SelectObject(data->m_hdc, hfont);
        TEXTMETRICW tm;
        if (!GetTextMetricsW(data->m_hdc, &tm)){
            lWarn() << "GetTextMetrics failed";
        }else{
            lf.lfWidth = tm.tmAveCharWidth * request.stretch / 100;
        }
        SelectObject(data->m_hdc, oldObj);
        DeleteObject(hfont);
    }

    if (preferClearTypeAA){
        fe->m_glyphFormat = LGlyphFormat::A32;
    }
    fe->initFontInfo(request, dpi);
    return fe;
}


LK_NAMESPACE_END
