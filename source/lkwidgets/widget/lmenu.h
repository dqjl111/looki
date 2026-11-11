#ifndef LMENU_H
#define LMENU_H

#include "lkwidgets/widget/labstractmenu.h"

LK_NAMESPACE_BEGIN

class LMenuButton;
class LKWIDGETSDEF LMenu : public LAbstractMenu
{
public:
    _LIVarTagEx(LMenu, LWidget)

    LMenu(std::shared_ptr<LWidget> parent = nullptr);
    // virtual ~LMenu();

    static std::shared_ptr<LActionBase> exec(const std::list<std::shared_ptr<LActionBase>> &actions,
                                         const LPoint<int>  &pos,
                                         std::shared_ptr<LActionBase> at = nullptr,
                                         std::shared_ptr<LWidget> parent = nullptr);
protected:
    virtual void updateActionRects(const LRect<int> &screenRect) override;
    virtual void popupAction(std::shared_ptr<LActionBase> a, bool activateFirst) override;
    virtual void drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt& opt, std::shared_ptr<LStyle> pStyle) override;


    mutable uint16_t m_maxIconWidth = 0;
    mutable uint16_t m_tabWidth = 0;
};


LK_NAMESPACE_END

#endif // LMENU_H
