#include "ldesktopscreenwidget.h"
#include "ldesktopwidget.h"
#include "lapplication.h"

LK_NAMESPACE_BEGIN

LDesktopScreenWidget::LDesktopScreenWidget(std::shared_ptr<LScreen> screen, const LRect<int> &geometry)
//: LWidget(nullptr, LWindowType::Desktop), m_screen(screen)
: m_screen(screen)
{

}

int LDesktopScreenWidget::screenNumber() const
{
    const std::shared_ptr<LDesktopWidget> desktopWidgetP = LApplication::desktop();
    if(desktopWidgetP != nullptr){
        int i=0;
        for(std::shared_ptr<LDesktopScreenWidget> w: desktopWidgetP->m_screens){
            if(w.get() ==  this){
                return i;
            }
            ++i;
        }
    }
    return -1;
}

void LDesktopScreenWidget::setScreenGeometry(const LRect<int> &geometry)
{
    m_geometry = geometry;
    setGeometry(geometry);
}


LK_NAMESPACE_END
