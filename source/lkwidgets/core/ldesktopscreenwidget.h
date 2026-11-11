#ifndef LDESKTOPSCREENWIDGET_H
#define LDESKTOPSCREENWIDGET_H
#include "lwidget.h"
#include "lkgui/lscreen.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LDesktopScreenWidget : public LWidget
{
public:
    LDesktopScreenWidget(std::shared_ptr<LScreen> screen, const LRect<int> &geometry);

    int screenNumber() const;
    void setScreenGeometry(const LRect<int> &geometry);

    std::shared_ptr<LScreen> assignedScreen() const { return m_screen; }
    LRect<int> screenGeometry() const { return m_geometry; }

protected:
    std::shared_ptr<LScreen> m_screen;
    LRect<int> m_geometry;

};

LK_NAMESPACE_END

#endif // LDESKTOPSCREENWIDGET_H
