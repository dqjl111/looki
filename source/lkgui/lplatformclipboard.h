#ifndef LPLATFORMCLIPBOARD_H
#define LPLATFORMCLIPBOARD_H

#include "lkcore/lglobal.h"

#include "lkgui/image/limage.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LPlatformClipboard
{
public:
    LPlatformClipboard();

    virtual std::wstring text() const = 0;
    virtual void setText(const std::wstring &) = 0;

    virtual LCbor cbor(const std::string &typeName = "") const = 0;
    virtual void setCbor(const LCbor &c, const std::string &typeName = "") = 0;

    virtual std::shared_ptr<LImage> image() const = 0;
    virtual void setImage(const std::shared_ptr<LImage>) = 0;
    virtual void clear() = 0;
};

LK_NAMESPACE_END

#endif // LPLATFORMCLIPBOARD_H
