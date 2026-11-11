#ifndef LMENUBAR_H
#define LMENUBAR_H
#include "lkwidgets/widget/labstractmenu.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LMenuBar : public LAbstractMenu
{
public:
    _LIVarTagEx(LMenuBar, LWidget)

    LMenuBar(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

protected:
    virtual void updateActionRects(const LRect<int> &containerRect) override;
    virtual void popupAction(std::shared_ptr<LActionBase> a, bool activateFirst) override;
    virtual void drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt& opt, std::shared_ptr<LStyle> pStyle) override;

    LPoint<int> adjustedSubPosition(std::shared_ptr<LActionBase> causeAction);
};

LK_NAMESPACE_END

#endif // LMENUBAR_H
