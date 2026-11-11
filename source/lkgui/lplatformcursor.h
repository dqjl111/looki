#ifndef LPLATFORMCURSOR_H
#define LPLATFORMCURSOR_H
#include "lkgui/lpoint.h"
#include "lkgui/lguievent.h"

LK_NAMESPACE_BEGIN
class LCursor;
class LWindow;
class LPixmap;
class LPlatformScreen;
class LKGUIDEF LPlatformCursor
{
public:
    LPlatformCursor();

    virtual LPoint<int> pos() const;
    virtual void setPos(const LPoint<int> &pos);
    virtual LSize<int> size() const;
    virtual void pointerEvent(const std::shared_ptr<LMouseEvent> event) {
    }
    virtual void changeCursor(const LCursor& cursorIn, std::shared_ptr<LWindow> window) = 0;
    virtual void setOverrideCursor(const LCursor&);
    virtual void clearOverrideCursor();

    static inline LSize<int> standardCursorSize() { return LSize<int>(32, 32); }
    static LSize<int> screenCursorSize(const std::shared_ptr<LPlatformScreen> screen = nullptr);
    static std::shared_ptr<LPixmap> customCursor(LCursorShape cursorShape,
                                                 const std::shared_ptr<LPlatformScreen> screen = nullptr);
protected:
    static std::shared_ptr<LPixmap> createPixmapCursorFromData(const LSize<int> &screenCursorSize,
                                                               // The cursor size the bitmap is targeted for
                                                               const LSize<int> &bitmapSize,
                                                               // The actual size of the bitmap data
                                                               const LSize<int>& hotp,
                                                               const uint8_t *bits,
                                                               const uint8_t *maskBits);


};


LK_NAMESPACE_END

#endif // LPLATFORMCURSOR_H


