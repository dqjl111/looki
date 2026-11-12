#ifndef LTABBAR_H
#define LTABBAR_H

#include "lkwidgets/widget/labstractmenu.h"
#include "lkwidgets/widget/lcombobox.h"
#include "lkwidgets/util/laction.h"

LK_NAMESPACE_BEGIN


class LKWIDGETSDEF LTabBar : public LAbstractMenu, public LCombBoxBase
{
public:
    _LIVarTagEx(LTabBar, LWidget)

    LTabBar(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

    virtual std::shared_ptr<LAbstractMenu> menu() const override;
    void setCanCloseAction(bool isClosable);

    LSignal<int> sigCurrentChanged;

protected:
    virtual std::shared_ptr<LCombBoxBase> initItem(std::shared_ptr<LComboBoxItem>) override;
    virtual void updateActionRects(const LRect<int> &containerRect) override;
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt& opt, std::shared_ptr<LStyle> pStyle) override;
    virtual void popupAction(std::shared_ptr<LActionBase> a, bool activateFirst) override;
    virtual void currrentItemUpdate() override;

    virtual LRect<int> actionCloseRect(std::shared_ptr<LActionBase> a,const LRect<int>& actRect) const override;

};


LK_NAMESPACE_END
#endif // LTABBAR_H
