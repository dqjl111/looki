#include "lcursor.h"
#include "lkgui/lscreen.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN
LCursor::LCursor()
{
}

LCursor::LCursor(LCursorShape s) : m_cshape(s), m_isValid(true)
{
}

LCursor::LCursor(const LCursor &c) : m_flags(c.m_flags)
{

}

LPoint<int> LCursor::pos()
{
    return LCursor::pos(LGuiApplication::primaryScreen());
}

LPoint<int> LCursor::pos(const std::shared_ptr<LScreen> screen)
{
    if (screen != nullptr) {
        const std::shared_ptr<LPlatformCursor> cursor = screen->handle()->cursor();
        if (cursor != nullptr) {
            std::shared_ptr<LPlatformScreen> ps = screen->handle();
            LPoint<int> nativePos = cursor->pos();
            ps = ps->screenForPosition(nativePos);
            return LHighDpi::fromNativePixels(nativePos, ps->screen().get());
        }
    }
    return LGuiApplication::m_lastCursorPosition.toPoint();
}

void LCursor::setPos(int x, int y)
{
    LCursor::setPos(LGuiApplication::primaryScreen(), x, y);
}

void LCursor::setPos(std::shared_ptr<LScreen> screen, int x, int y)
{
    if (screen != nullptr) {
        const std::shared_ptr<LPlatformCursor> cursor = screen->handle()->cursor();
        if (cursor != nullptr) {
            const LPoint<int> devicePos = LHighDpi::toNativePixels(LPoint<int>(x, y), screen.get());
            // Need to check, since some X servers generate null mouse move
            // events, causing looping in applications which call setPos() on
            // every mouse move event.
            if (devicePos != cursor->pos()){
                cursor->setPos(devicePos);
            }
        }
    }
}

LCursorShape LCursor::shape() const
{
    if(m_isValid){
        return m_cshape;
    }
    return LCursorShape::Arrow;
}

void LCursor::setShape(LCursorShape newShape)
{
    m_cshape = newShape;
    m_isValid = true;
}


LK_NAMESPACE_END
