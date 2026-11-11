#include "ldesktopwidget.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN

LDesktopWidget::LDesktopWidget() : LWidget(nullptr, LWindowType::Desktop)
{

}

int LDesktopWidget::screenNumber(const std::shared_ptr<LWidget> w)
{
    if (w == nullptr){
        return 0;
    }

    std::list<std::shared_ptr<LScreen> > allScreens = LGuiApplication::screens();
    std::list<std::shared_ptr<LScreen> > screens = allScreens;
    if (screens.empty()){
        return 0;
    }
    // Get the screen number from window position using screen geometry
    // and proper screens.
    LRect<int> frame = w->frameGeometry();
    if (!w->isWindow()){
        frame.moveTopLeft(w->mapToGlobal(LPoint<int>(0, 0)));
    }

    std::shared_ptr<LScreen> widgetScreen = nullptr;
    int largestArea = 0;
    for(const std::shared_ptr<LScreen>& screen: screens) {
        const LRect<int> deviceIndependentScreenGeometry =
            LHighDpi::fromNativePixels(screen->handle()->geometry(), screen.get());
        const LRect<int> intersected = deviceIndependentScreenGeometry.intersected(frame);
        int area = intersected.width() * intersected.height();
        if (largestArea < area) {
            widgetScreen = screen;
            largestArea = area;
        }
    }
    return lItemIndex(allScreens, widgetScreen);
}

const LRect<int> LDesktopWidget::screenGeometry(const std::shared_ptr<LWidget> widget)
{
    return screenGeometry(screenNumber(widget));
}

const LRect<int> LDesktopWidget::screenGeometry(int screenNo)
{
    const std::shared_ptr<LScreen> s = LGuiApplication::screenAt(screenNo);
    return s!= nullptr ? s->m_geometry : LRect<int>();
}

const LRect<int> LDesktopWidget::availableGeometry(const std::shared_ptr<LWidget> widget)
{
    return availableGeometry(screenNumber(widget));
}

const LRect<int> LDesktopWidget::availableGeometry(int screenNo)
{
    const std::shared_ptr<LScreen> s = LGuiApplication::screenAt(screenNo);
    return s!= nullptr ? s->m_availableGeometry : LRect<int>();
}

bool LDesktopWidget::useFullScreenForPopup()
{
    return LGuiApplication::platformIntegration()
        ->styleHint(LPlatformTheme::Hint::UseFullScreenForPopup).toBool();
}

const LRect<int> LDesktopWidget::popupGeometry(const std::shared_ptr<LWidget> widget)
{
    return useFullScreenForPopup()
               ? screenGeometry(widget)
               : availableGeometry(widget);
}

const LRect<int> LDesktopWidget::popupGeometry(int screen)
{
    return useFullScreenForPopup()
               ? screenGeometry(screen)
               : availableGeometry(screen);
}

void LDesktopWidget::resizeEvent(std::shared_ptr<LResizeEvent> e)
{

}



LK_NAMESPACE_END
