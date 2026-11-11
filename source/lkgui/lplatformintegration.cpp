#include "lplatformintegration.h"
#include "lplatformtheme.h"
#include "lguiapplication.h"
#include "lplatformcursor.h"

LK_NAMESPACE_BEGIN

LPlatformIntegration::LPlatformIntegration()
{

}

void LPlatformIntegration::initialize()
{
}

void LPlatformIntegration::destroy()
{

}

std::shared_ptr<LPlatformPixmap> LPlatformIntegration::createPlatformPixmap() const
{
    return nullptr;
}

LVariant LPlatformIntegration::styleHint(LPlatformTheme::Hint hint) const {
    return LPlatformTheme::defaultThemeHint(hint);
}

LWindowState LPlatformIntegration::defaultWindowState(LFlags<LWindowType> flags) const {
    if (uint32_t(flags) & uint32_t(LWindowType::Popup) & ~uint32_t(LWindowType::Window)) {
        return LWindowState::NoState;
    }

    if (styleHint(LPlatformTheme::Hint::ShowIsFullScreen).getBool()) {
        return LWindowState::FullScreen;
    }
    else if (styleHint(LPlatformTheme::Hint::ShowIsMaximized).getBool()) {
        return LWindowState::Maximized;
    }
    return LWindowState::NoState;
}

bool LPlatformIntegration::hasCapability(LPlatformCapability cap) const
{
    return cap == LPlatformCapability::NonFullScreenWindows
           || cap == LPlatformCapability::NativeWidgets
           || cap == LPlatformCapability::WindowManagement
           || cap == LPlatformCapability::TopStackedNativeChildWindows
           || cap == LPlatformCapability::WindowActivation;
}

std::shared_ptr<LPaintEngine> LPlatformIntegration::createImagePaintEngine(std::shared_ptr<LPaintDevice> paintDevice) const
{
    return nullptr;
}

std::shared_ptr<LPlatformFontDatabase> LPlatformIntegration::fontDatabase() const
{
    return nullptr;
}

std::shared_ptr<LPlatformClipboard> LPlatformIntegration::clipboard() const
{
    return nullptr;
}

std::shared_ptr<LPlatformDialogHelper> LPlatformIntegration::createPlatformDialogHelper(LPlatformDialogHelper::Type type) const
{
    return nullptr;
}

void LPlatformIntegration::applicationStateChanged(LApplicationState state)
{

}

LSize<int> LPlatformIntegration::systemCursorSize() const
{
    return LPlatformCursor::standardCursorSize();
}

std::list<uint32_t> LPlatformIntegration::possibleKeys(const std::shared_ptr<LKeyEvent> e) const
{
    return std::list<uint32_t>();
}

LK_NAMESPACE_END
