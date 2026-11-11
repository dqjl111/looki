#ifndef LPLATFORMFONTDATABASE_H
#define LPLATFORMFONTDATABASE_H

#include "lkcore/lookiconfig.h"
#include <string>
#include "lkgui/text/lfont.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LPlatformFontDatabase
{
public:
    LPlatformFontDatabase();
    virtual void populateFontDatabase();
    virtual std::shared_ptr<LFont> defaultFont() const;

    virtual std::shared_ptr<LFontEngine> fontEngine(const LFont::Def &fontDef);
    static LFont::Weight weightFromInteger(int weight);

protected:

};

LK_NAMESPACE_END
#endif // LPLATFORMFONTDATABASE_H
