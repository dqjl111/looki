#ifndef LCLIPBOARD_H
#define LCLIPBOARD_H
#include "lkcore/lobject.h"
#include "lkgui/image/limage.h"
#include "lkgui/image/lpixmap.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LClipboard : public LObject
{
public:
    LClipboard();

    std::wstring text() const;
    void setText(const std::wstring &);

    LCbor cbor(const std::string typeName = "");
    void setCbor(const LCbor &c, const std::string typeName = "");

    std::shared_ptr<LImage> image() const;
    void setImage(const std::shared_ptr<LImage>);

    std::shared_ptr<LPixmap> pixmap() const;
    void setPixmap(const std::shared_ptr<LPixmap>);

    void clear();
};


LK_NAMESPACE_END
#endif // LCLIPBOARD_H
