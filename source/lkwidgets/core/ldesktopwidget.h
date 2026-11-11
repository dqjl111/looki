#ifndef LDESKTOPWIDGET_H
#define LDESKTOPWIDGET_H

#include "ldesktopscreenwidget.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LDesktopWidget : public LWidget
{
public:
    LDesktopWidget();

    static int screenNumber(const std::shared_ptr<LWidget> widget = nullptr);
    static const LRect<int> screenGeometry(const std::shared_ptr<LWidget> widget);
    static const LRect<int> screenGeometry(int screen = -1);

    static const LRect<int> availableGeometry(const std::shared_ptr<LWidget> widget);
    static const LRect<int> availableGeometry(int screen = -1);

    static bool useFullScreenForPopup();
    static const LRect<int> popupGeometry(const std::shared_ptr<LWidget> widget);
    static const LRect<int> popupGeometry(int screen);

protected:
    void resizeEvent(std::shared_ptr<LResizeEvent> e) override;

    friend class LDesktopScreenWidget;

    std::list<std::shared_ptr<LDesktopScreenWidget> > m_screens;
};

LK_NAMESPACE_END
#endif // LDESKTOPWIDGET_H
