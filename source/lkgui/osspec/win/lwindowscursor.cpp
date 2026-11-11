#include "lwindowscursor.h"
#include "lkgui/osspec/win/lwindowsscreen.h"
#include "lkgui/osspec/win/lwindowswin.h"
#include "lkgui/lplatformscreen.h"
#include "lkgui/image/lpixmap.h"
#include "lkgui/lguiapplication.h"
LK_NAMESPACE_BEGIN


HCURSOR LWindowsCursor::m_overriddenCursor = nullptr;
HCURSOR LWindowsCursor::m_overrideCursor = nullptr;


LWindowsCursor::LWindowsCursor(const std::shared_ptr<LWindowsScreen> screen): m_screen(screen)
{

}

void LWindowsCursor::enforceOverrideCursor()
{
    if (hasOverrideCursor() && m_overrideCursor != GetCursor()){
        SetCursor(m_overrideCursor);
    }
}

struct LWindowsStandardCursorMapping {
    LCursorShape shape;
    LPCSTR resource;
};

// Create a cursor from image and mask of the format LImage::Format::Mono.
static HCURSOR createBitmapCursor(const std::shared_ptr<LImage> bbits,
                                  const std::shared_ptr<LImage>  &mbits,
                                  LPoint<int> hotSpot = LPoint<int>(-1, -1),
                                  bool invb = false, bool invm = false)
{
    const int width = bbits->width();
    const int height = bbits->height();
    if (hotSpot.m_x < 0){
        hotSpot.m_x = (width / 2);
    }
    if (hotSpot.m_y < 0){
        hotSpot.m_y = (height / 2);
    }
    const int n = std::max<int>(1, width / 8);
    std::vector<uint8_t> xBits(height * n);
    std::vector<uint8_t> xMask(height * n);
    int x = 0;
    for (int i = 0; i < height; ++i) {
        const uint8_t *bits = bbits->scanLine(i);
        const uint8_t *mask = mbits->scanLine(i);
        for (int j = 0; j < n; ++j) {
            uint8_t b = bits[j];
            uint8_t m = mask[j];
            if (invb){
                b ^= 0xff;
            }
            if (invm){
                m ^= 0xff;
            }
            xBits[x] = ~m;
            xMask[x] = b ^ m;
            ++x;
        }
    }
    return CreateCursor(GetModuleHandle(nullptr),
                        hotSpot.m_x, hotSpot.m_y,
                        width, height,
                        xBits.data(), xMask.data());
}


HCURSOR LWindowsCursor::createCursorFromShape(LCursorShape cursorShape,
                                              const std::shared_ptr<LPlatformScreen> screen)
{
    static const LWindowsStandardCursorMapping standardCursors[] = {
        { LCursorShape::Arrow, IDC_ARROW},
        { LCursorShape::UpArrow, IDC_UPARROW },
        { LCursorShape::Cross, IDC_CROSS },
        { LCursorShape::Wait, IDC_WAIT },
        { LCursorShape::IBeam, IDC_IBEAM },
        { LCursorShape::SizeVer, IDC_SIZENS },
        { LCursorShape::SizeHor, IDC_SIZEWE },
        { LCursorShape::SizeBDiag, IDC_SIZENESW },
        { LCursorShape::SizeFDiag, IDC_SIZENWSE },
        { LCursorShape::SizeAll, IDC_SIZEALL },
        { LCursorShape::Forbidden, IDC_NO },
        { LCursorShape::WhatsThis, IDC_HELP },
        { LCursorShape::Busy, IDC_APPSTARTING },
        { LCursorShape::PointingHand, IDC_HAND }
    };

    switch (cursorShape) {
    case LCursorShape::Blank: {
        std::shared_ptr<LImage> blank =
            std::make_shared<LImage>(LGuiApplication::platformIntegration()->systemCursorSize(),
                                     LImage::Format::Mono);
        blank->fill(0); // ignore color table
        return createBitmapCursor(blank, blank);
    }
    case LCursorShape::OpenHand:
    case LCursorShape::ClosedHand:
    case LCursorShape::DragCopy:
    case LCursorShape::DragMove:
    case LCursorShape::DragLink:{
        return LWindowsCursor::createPixmapCursor(customCursor(cursorShape, screen));
    }
    default:
        break;
    }

    // Load available standard cursors from resources
    for (const LWindowsStandardCursorMapping &s : standardCursors) {
        if (s.shape == cursorShape) {
            return LoadCursor(NULL, s.resource);
        }
    }
    return nullptr;

}

LPoint<int> LWindowsCursor::mousePosition()
{
    POINT p;
    GetCursorPos(&p);
    return LPoint<int>(p.x, p.y);
}

void LWindowsCursor::changeCursor(const LCursor& cursorIn, std::shared_ptr<LWindow> window)
{
    std::shared_ptr<LWindowsWin> platformWindow = LWindowsWin::windowsWindowOf(window);
    if (platformWindow == nullptr){ // Desktop/Foreign window.
        return;
    }

    const std::shared_ptr<LCursorHandle> wcursor = cursorHandle(cursorIn);
    if (wcursor->handle()) {
        platformWindow->setCursor(wcursor);
    }
}

void LWindowsCursor::setOverrideCursor(const LCursor &cursor)
{
    const std::shared_ptr<LCursorHandle> wcursor = cursorHandle(cursor);
    if (const auto overrideCursor = wcursor->handle()) {
        m_overrideCursor = overrideCursor;
        const HCURSOR previousCursor = SetCursor(overrideCursor);
        if (m_overriddenCursor == nullptr){
            m_overriddenCursor = previousCursor;
        }
    }
}

void LWindowsCursor::clearOverrideCursor()
{
    if (m_overriddenCursor) {
        SetCursor(m_overriddenCursor);
        m_overriddenCursor = m_overrideCursor = nullptr;
    }
}

std::shared_ptr<LCursorHandle> LWindowsCursor::standardWindowCursor(LCursorShape s)
{
    std::map<LCursorShape, std::shared_ptr<LCursorHandle> >::iterator it = m_standardCursorCache.find(s);
    if (it != m_standardCursorCache.end()) {
        return it->second;
    }else{
        std::shared_ptr<LWindowsScreen> scr = m_screen.lock();
        if(scr != nullptr){
            if (const HCURSOR hc = LWindowsCursor::createCursorFromShape(s, scr)){
                std::shared_ptr<LCursorHandle> h = std::make_shared<LCursorHandle>(hc);
                m_standardCursorCache[s] = h;
                return h;
            }
        }
        return std::make_shared<LCursorHandle>();
    }
}

HCURSOR LWindowsCursor::createPixmapCursor(std::shared_ptr<LPixmap> pixmap, double scaleFactor)
{
    HCURSOR cur = nullptr;
    const double pixmapScaleFactor = scaleFactor / pixmap->devicePixelRatioF();
    if (!lFuzzyCompare(pixmapScaleFactor, 1)) {
        pixmap = pixmap->scaled(pixmapScaleFactor * pixmap->size(),
                               LAspectRatioMode::KeepAspectRatio,
                               LTransMode::Smooth);
    }

    std::shared_ptr<LImage> mask = pixmap->mask(false);
    if(mask == nullptr){
        mask = std::make_shared<LImage>(pixmap->width(),
                                        pixmap->height(),
                                        LImage::Format::Mono);
        mask->fill(LColorID::Black);
    }

    std::shared_ptr<LImage> tm = pixmap->toImage();

    ICONINFO ii;
    ii.fIcon     = 0;
    ii.xHotspot  = DWORD(std::lround(pixmap->m_startPos.m_x * scaleFactor));
    ii.yHotspot  = DWORD(std::lround(pixmap->m_startPos.m_y * scaleFactor));
    ii.hbmMask   = CreateBitmap(mask->width(), mask->height(), 1, 1, mask->bits());
    ii.hbmColor  = LWindowsWin::imageToWinHBITMAP(tm, HBitmapFormat::Alpha);

    cur = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
    return cur;
}

std::shared_ptr<LCursorHandle> LWindowsCursor::cursorHandle(const LCursor& c)
{
    return standardWindowCursor(c.shape());
}


//               ? pixmapWindowCursor(c)
LK_NAMESPACE_END
