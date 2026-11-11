#include "lkgui/lplatformcursor.h"
#include "lkgui/lguiapplication.h"
#include "lkcore/ldebug.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lkgui/image/limagereader.h"
#include "lkgui/lplatformscreen.h"
#include "lkgui/lscreen.h"

LK_NAMESPACE_BEGIN


LPlatformCursor::LPlatformCursor()
{

}

LPoint<int> LPlatformCursor::pos() const
{
    return LGuiApplication::m_lastCursorPosition.toPoint();
}

void LPlatformCursor::setPos(const LPoint<int> &pos)
{
    static bool firstCall = true;
    if (firstCall) {
        firstCall = false;
        lWarn() << "Not support set cursor pos; Emulating movement within the application.";
    }
    LWindowSystemInterface::handleMouseEvent(nullptr,
                                             pos.toPointF(), pos.toPointF(),
                                             LMouseButton::No, LMouseButton::No, LEvent::MouseMove);
}

LSize<int> LPlatformCursor::size() const
{
    return LSize<int>(16, 16);
}

void LPlatformCursor::setOverrideCursor(const LCursor &)
{

}

void LPlatformCursor::clearOverrideCursor()
{

}

LSize<int> LPlatformCursor::screenCursorSize(const std::shared_ptr<LPlatformScreen> screen)
{
    const LSize<int> primaryScreenCursorSize = LGuiApplication::platformIntegration()->systemCursorSize();
    if (screen != nullptr) {
        // Correct the size if the DPI value of the screen differs from
        // that of the primary screen.
        if (const std::shared_ptr<LScreen> primaryLScreen = LGuiApplication::primaryScreen()) {
            const std::shared_ptr<LPlatformScreen> primaryScreen = primaryLScreen->handle();
            if (screen != primaryScreen) {
                const double logicalDpi = screen->logicalDpi().first;
                const double primaryScreenLogicalDpi = primaryScreen->logicalDpi().first;
                if (!lFuzzyCompare(logicalDpi, primaryScreenLogicalDpi)){
                    return (LSize<int>(primaryScreenCursorSize) * logicalDpi / primaryScreenLogicalDpi).toSize();
                }
            }
        }
    }
    return primaryScreenCursorSize;
}


std::shared_ptr<LPixmap> LPlatformCursor::customCursor(LCursorShape cursorShape,
                                                       const std::shared_ptr<LPlatformScreen> screen)
{
    static const uint8_t openhandBits[] = {
                                           0x80,0x01,0x58,0x0e,0x64,0x12,0x64,0x52,0x48,0xb2,0x48,0x92,
                                           0x16,0x90,0x19,0x80,0x11,0x40,0x02,0x40,0x04,0x40,0x04,0x20,
                                           0x08,0x20,0x10,0x10,0x20,0x10,0x00,0x00};
    static const uint8_t openhandmBits[] = {
                                            0x80,0x01,0xd8,0x0f,0xfc,0x1f,0xfc,0x5f,0xf8,0xff,0xf8,0xff,
                                            0xf6,0xff,0xff,0xff,0xff,0x7f,0xfe,0x7f,0xfc,0x7f,0xfc,0x3f,
                                            0xf8,0x3f,0xf0,0x1f,0xe0,0x1f,0x00,0x00};
    static const uint8_t closedhandBits[] = {
                                             0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x0d,0x48,0x32,0x08,0x50,
                                             0x10,0x40,0x18,0x40,0x04,0x40,0x04,0x20,0x08,0x20,0x10,0x10,
                                             0x20,0x10,0x20,0x10,0x00,0x00,0x00,0x00};
    static const uint8_t closedhandmBits[] = {
                                              0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x0d,0xf8,0x3f,0xf8,0x7f,
                                              0xf0,0x7f,0xf8,0x7f,0xfc,0x7f,0xfc,0x3f,0xf8,0x3f,0xf0,0x1f,
                                              0xe0,0x1f,0xe0,0x1f,0x00,0x00,0x00,0x00};

    static const char * const moveDragCursorXpmC[] = {
                                                     "11 20 3 1",
                                                     ".        c None",
                                                     "a        c #FFFFFF",
                                                     "X        c #000000", // X11 cursor is traditionally black
                                                     "aa.........",
                                                     "aXa........",
                                                     "aXXa.......",
                                                     "aXXXa......",
                                                     "aXXXXa.....",
                                                     "aXXXXXa....",
                                                     "aXXXXXXa...",
                                                     "aXXXXXXXa..",
                                                     "aXXXXXXXXa.",
                                                     "aXXXXXXXXXa",
                                                     "aXXXXXXaaaa",
                                                     "aXXXaXXa...",
                                                     "aXXaaXXa...",
                                                     "aXa..aXXa..",
                                                     "aa...aXXa..",
                                                     "a.....aXXa.",
                                                     "......aXXa.",
                                                     ".......aXXa",
                                                     ".......aXXa",
                                                     "........aa."};

    static const char * copyDragCursorXpmC[] = {
                                                     "24 30 3 1",
                                                     ".        c None",
                                                     "a        c #000000",
                                                     "X        c #FFFFFF",
                                                     "XX......................",
                                                     "XaX.....................",
                                                     "XaaX....................",
                                                     "XaaaX...................",
                                                     "XaaaaX..................",
                                                     "XaaaaaX.................",
                                                     "XaaaaaaX................",
                                                     "XaaaaaaaX...............",
                                                     "XaaaaaaaaX..............",
                                                     "XaaaaaaaaaX.............",
                                                     "XaaaaaaXXXX.............",
                                                     "XaaaXaaX................",
                                                     "XaaXXaaX................",
                                                     "XaX..XaaX...............",
                                                     "XX...XaaX...............",
                                                     "X.....XaaX..............",
                                                     "......XaaX..............",
                                                     ".......XaaX.............",
                                                     ".......XaaX.............",
                                                     "........XX...aaaaaaaaaaa",
                                                     ".............aXXXXXXXXXa",
                                                     ".............aXXXXXXXXXa",
                                                     ".............aXXXXaXXXXa",
                                                     ".............aXXXXaXXXXa",
                                                     ".............aXXaaaaaXXa",
                                                     ".............aXXXXaXXXXa",
                                                     ".............aXXXXaXXXXa",
                                                     ".............aXXXXXXXXXa",
                                                     ".............aXXXXXXXXXa",
                                                     ".............aaaaaaaaaaa"};

    static const char * linkDragCursorXpmC[] = {
                                                     "24 30 3 1",
                                                     ".        c None",
                                                     "a        c #000000",
                                                     "X        c #FFFFFF",
                                                     "XX......................",
                                                     "XaX.....................",
                                                     "XaaX....................",
                                                     "XaaaX...................",
                                                     "XaaaaX..................",
                                                     "XaaaaaX.................",
                                                     "XaaaaaaX................",
                                                     "XaaaaaaaX...............",
                                                     "XaaaaaaaaX..............",
                                                     "XaaaaaaaaaX.............",
                                                     "XaaaaaaXXXX.............",
                                                     "XaaaXaaX................",
                                                     "XaaXXaaX................",
                                                     "XaX..XaaX...............",
                                                     "XX...XaaX...............",
                                                     "X.....XaaX..............",
                                                     "......XaaX..............",
                                                     ".......XaaX.............",
                                                     ".......XaaX.............",
                                                     "........XX...aaaaaaaaaaa",
                                                     ".............aXXXXXXXXXa",
                                                     ".............aXXXaaaaXXa",
                                                     ".............aXXXXaaaXXa",
                                                     ".............aXXXaaaaXXa",
                                                     ".............aXXaaaXaXXa",
                                                     ".............aXXaaXXXXXa",
                                                     ".............aXXaXXXXXXa",
                                                     ".............aXXXaXXXXXa",
                                                     ".............aXXXXXXXXXa",
                                                     ".............aaaaaaaaaaa"};

    switch (cursorShape) {
    case LCursorShape::OpenHand:{
        return createPixmapCursorFromData(screenCursorSize(screen),
                                          LSize<int>(16, 16),
                                          LSize<int>(7, 7),
                                          openhandBits,
                                          openhandmBits);
    }
    case LCursorShape::ClosedHand:{
        return createPixmapCursorFromData(screenCursorSize(screen),
                                          LSize<int>(16, 16),
                                          LSize<int>(7, 7),
                                          closedhandBits,
                                          closedhandmBits);

    }
    case LCursorShape::DragCopy:{
        return LPixmap::fromImage(LImageReader::loadXpmFromArray(copyDragCursorXpmC));
    }
    case LCursorShape::DragMove:{
        return LPixmap::fromImage(LImageReader::loadXpmFromArray(moveDragCursorXpmC));
    }
    case LCursorShape::DragLink:{
        return LPixmap::fromImage(LImageReader::loadXpmFromArray(linkDragCursorXpmC));
    }
    default:{
        break;
    }
    }

    return std::make_shared<LPixmap>();
}

std::shared_ptr<LPixmap> LPlatformCursor::createPixmapCursorFromData(const LSize<int> &screenCursorSize,
                                                                     const LSize<int> &bitmapSize,
                                                                     const LSize<int>& hotp,
                                                                     const uint8_t *bits,
                                                                     const uint8_t *maskBits)
{
    std::shared_ptr<LImage> cursorImage = std::make_shared<LImage>(bitmapSize.m_x, bitmapSize.m_y, LImage::Format::Indexed8);
    if (!bits || !maskBits || (cursorImage == nullptr)){
        return nullptr;
    }

    cursorImage->setColorCount(3);
    cursorImage->setColor(0, LColorID::White);
    cursorImage->setColor(1, LColorID::Black);
    cursorImage->setColor(2, LColorID::Transparent);

    int bytesPerLine = (bitmapSize.m_x + 7) / 8;
    int p = 0;
    int d, m;

    int x = -1;

    uint8_t *cursorData = cursorImage->bits();
    lsizetype bpl = cursorImage->bytesPerLine();
    for (int i = 0; i < bitmapSize.m_y; i++)
    {
        for (int j = 0; j < bytesPerLine; j++, bits++, maskBits++)
        {
            for (int b = 0; b < 8 && j*8+b < bitmapSize.m_x; b++)
            {
                d = *bits & (1 << b);
                m = *maskBits & (1 << b);
                if (d && m){
                    p = 0;
                }
                else if (!d && m){
                    p = 1;
                }
                else{
                    p = 2;
                }
                cursorData[j*8+b] = p;

                // calc region
                if (x < 0 && m){
                    x = j*8+b;
                }
                else if (x >= 0 && !m) {
                    x = -1;
                }
            }
        }
        if (x >= 0) {
            x = -1;
        }
        cursorData += bpl;
    }
    std::shared_ptr<LPixmap> rawImage = LPixmap::fromImage(cursorImage);
    const double factor = double(screenCursorSize.width()) / double(bitmapSize.width());
    // Scale images if the cursor size is significantly different, starting with 150% where the system cursor
    // size is 48.
    if (std::abs(factor - 1.0) > 0.4) {
        const LTransform2D transform = LTransform2D::fromScale(factor, factor);
        rawImage = rawImage->transformed(transform, LTransMode::Smooth);
    }

    rawImage->m_startPos.m_x = hotp.m_x;
    rawImage->m_startPos.m_y = hotp.m_y;
    return rawImage;
}

LK_NAMESPACE_END
