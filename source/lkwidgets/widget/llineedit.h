#ifndef LLINEEDIT_H
#define LLINEEDIT_H

#include "labstractlineedit.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LLineEdit : public LAbstractLineEdit
{
public:
    _LIVarTagEx(LLineEdit, LWidget)

    LLineEdit(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0, bool isRow = false);
    LLineEdit(std::shared_ptr<LTextLineEditor> editor,
              std::shared_ptr<LWidget> parent = nullptr,
              LFlags<LWindowType> f = 0);

    LLineEdit(std::shared_ptr<LLineEditor> editor,
              std::shared_ptr<LWidget> parent = nullptr,
              LFlags<LWindowType> f = 0);

    virtual LSize<int> sizeHint() const override;
    virtual LSize<int> minimumSizeHint() const override;
    std::wstring text() const;
    void setText(const std::wstring &s);
    std::wstring selectText() const;
protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> ev) override;
    virtual LRect<int> editRect() const override;

    /** 聚焦时描边宽度；派生类可重写以得到更细的边框 */
    virtual int focusFrameWidth() const{ return 2; }
};


LK_NAMESPACE_END
#endif // LLINEEDIT_H
