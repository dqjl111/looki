#ifndef LMAINWINDOW_H
#define LMAINWINDOW_H
#include "lkwidgets/core/lwidget.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LMainWindow : public LWidget
{
public:
    _LIVarTagEx(LMainWindow, LWidget)

    LMainWindow(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
};


LK_NAMESPACE_END

#endif // LMAINWINDOW_H
