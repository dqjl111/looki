#include "lplatformfontdatabase.h"
#include "lkcore/lstringutil.h"
LK_NAMESPACE_BEGIN

LPlatformFontDatabase::LPlatformFontDatabase()
{

}

void LPlatformFontDatabase::populateFontDatabase()
{

}

std::shared_ptr<LFont> LPlatformFontDatabase::defaultFont() const
{
    return std::make_shared<LFont>();
}

std::shared_ptr<LFontEngine> LPlatformFontDatabase::fontEngine(const LFont::Def &fontDef)
{
    return nullptr;
}


LFont::Weight LPlatformFontDatabase::weightFromInteger(int weight)
{
    if (weight < 150){
        return LFont::Thin;
    }
    if (weight < 250){
        return LFont::ExtraLight;
    }
    if (weight < 350){
        return LFont::Light;
    }
    if (weight < 450){
        return LFont::Normal;
    }
    if (weight < 550){
        return LFont::Medium;
    }
    if (weight < 650){
        return LFont::DemiBold;
    }
    if (weight < 750){
        return LFont::Bold;
    }
    if (weight < 850){
        return LFont::ExtraBold;
    }
    return LFont::Black;
}

LK_NAMESPACE_END
