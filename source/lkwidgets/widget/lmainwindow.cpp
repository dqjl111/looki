#include "lmainwindow.h"
#include "lkgui/paint/lpainter.h"
LK_NAMESPACE_BEGIN

LMainWindow::LMainWindow(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
  : LWidget(parent, f | LWindowType::Window)
{

}

void LMainWindow::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    // 主窗口需要先铺窗口底色，否则布局边距等未被控件覆盖的区域会显示为黑色
    if (paintDevice() == nullptr) {
        return;
    }
    LPainter p(paintDevice());
    p.fillRect(rect().toRectF(), palette()->brush(backgroundRole()));
}

LK_NAMESPACE_END
