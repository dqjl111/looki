#ifndef LMULTILINEEDITOR_H
#define LMULTILINEEDITOR_H
#include "lkcore/lobject.h"

#include "lkwidgets/util/leditrows.h"
#include "lkwidgets/util/leditop.h"
#include "lkwidgets/util/lieditor.h"

LK_NAMESPACE_BEGIN
class LKWIDGETSDEF LMultiLineEditor : public LTEditor<LEditRows>
{
public:
    LMultiLineEditor();

    virtual uint32_t job() const override;
    virtual std::list<std::shared_ptr<LEditOp>> doInput(const std::wstring& text) override;
    virtual std::list<std::shared_ptr<LEditOp>> doEnterNewItem() override;
    std::shared_ptr<LEditRow> activeRow(bool create = false);
    std::shared_ptr<LEditText> activeText();
    std::shared_ptr<LEditBase> activeBlock(bool create = false);

    virtual bool doMouseClick(const LPoint<double>& p) override;
    virtual bool checkUpdate() override;
    virtual int contentRowNum() const override;
protected:
    std::list<std::shared_ptr<LEditOp>> doInput(std::shared_ptr<LEditBase> item);

    friend class LMultiEditOp;
    friend class LEditDoc;
    friend class LRowInputDelete;
};

LK_NAMESPACE_END

#endif // LMULTILINEEDITOR_H
