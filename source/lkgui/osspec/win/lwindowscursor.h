#ifndef LWINDOWSCURSOR_H
#define LWINDOWSCURSOR_H

#include <map>
#include "lkgui/lplatformcursor.h"
#include "lwindows.h"
LK_NAMESPACE_BEGIN

class LCursorHandle
{
    L_DISABLE_COPY_MOVE(LCursorHandle)
public:
    explicit LCursorHandle(HCURSOR hcursor = nullptr) : m_hcursor(hcursor) {}
    ~LCursorHandle()
    {
        if (m_hcursor){
            DestroyCursor(m_hcursor);
        }
    }

    bool isNull() const { return !m_hcursor; }
    HCURSOR handle() const { return m_hcursor; }
private:
    const HCURSOR m_hcursor;
};



class LWindowsScreen;
class LPlatformScreen;
class LPixmap;
class LWindowsCursor : public LPlatformCursor
{
public:
    LWindowsCursor(const std::shared_ptr<LWindowsScreen> screen);

    static void enforceOverrideCursor();
    static bool hasOverrideCursor() { return m_overriddenCursor != nullptr; }
    static HCURSOR createCursorFromShape(LCursorShape cursorShape,
                                         const std::shared_ptr<LPlatformScreen> screen = nullptr);

    static LPoint<int> mousePosition();

    virtual void changeCursor(const LCursor& cursorIn, std::shared_ptr<LWindow> window) override;
    virtual void setOverrideCursor(const LCursor& cursorIn) override;
    virtual void clearOverrideCursor();

    std::shared_ptr<LCursorHandle> standardWindowCursor(LCursorShape s = LCursorShape::Arrow);

    static HCURSOR createPixmapCursor(std::shared_ptr<LPixmap> pixmap, double scaleFactor = 1);
protected:
    const std::weak_ptr<LWindowsScreen> m_screen;

    static HCURSOR m_overriddenCursor;
    static HCURSOR m_overrideCursor;

    std::map<LCursorShape, std::shared_ptr<LCursorHandle>> m_standardCursorCache;
    std::shared_ptr<LCursorHandle>  cursorHandle(const LCursor& c);
};


LK_NAMESPACE_END
#endif // LWINDOWSCURSOR_H
