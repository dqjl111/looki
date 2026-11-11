#ifndef LCOMBOEDIT_H
#define LCOMBOEDIT_H

#include "lkwidgets/widget/lcombobox.h"
#include "lkwidgets/widget/labstractlineedit.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LComboEdit : public LAbstractLineEdit, public LCombBoxBase
{
public:
    _LIVarTagEx(LComboEdit, LWidget)

    LComboEdit(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;

    inline const std::wstring editText() const{
        return m_editor->content();
    }

    void setEditText(const std::wstring& s);
    virtual std::shared_ptr<LAbstractMenu> menu() const override;
protected:
    virtual void currrentItemUpdate() override;
    virtual std::shared_ptr<LCombBoxBase> initItem(std::shared_ptr<LComboBoxItem>) override;
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev) override;
    virtual void initSpecific() override;
    virtual bool enterSubItem(const LPoint<int>& p) override;

    virtual LRect<int> editRect() const override;

    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const override;
    void swichMenu();

    LUiSubCtrl m_pressedCtrl;
    LUiSubCtrl m_hoverCtrl;

    std::shared_ptr<LAbstractMenu> m_menu;

};


LK_NAMESPACE_END

#endif // LCOMBOEDIT_H
