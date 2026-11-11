#ifndef LTEXTEDIT_H
#define LTEXTEDIT_H

#include "lkwidgets/widget/labstractscrollarea.h"
#include "lkwidgets/widget/ledititemview.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LTextEdit : public LAbstractScrollArea
{
public:
     _LIVarTagEx(LTextEdit, LWidget)

    LTextEdit(std::shared_ptr<LWidget> parent = nullptr);

    std::shared_ptr<LIEditor> editor() const;
    void setEditor(std::shared_ptr<LIEditor> e);

    std::shared_ptr<LEditItemView> editView();

    void setText(std::wstring& text);
    std::wstring text(std::wstring lineSeperator = L"\n") const;
    std::wstring selectText(std::wstring lineSeperator = L"\n");

    void selectAll();
    void copy();
    void cut();
    void paste();

    virtual LVariant inputMethodQuery(LInputMethodQuery, const LVariant& args = LVariant()) const override;
protected:
    virtual void initViewport() override;
    virtual bool handleEvent(std::shared_ptr<LEvent> e) override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> e) override;
};

LK_NAMESPACE_END
#endif // LTEXTEDIT_H
