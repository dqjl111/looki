#ifndef LLINEEDITOR_H
#define LLINEEDITOR_H

#include "lkcore/lobject.h"
#include "lkgui/lguievent.h"
#include "lkwidgets/util/leditrow.h"
#include "lkwidgets/util/leditop.h"
#include "lkwidgets/util/lieditor.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LLineEditor  : public LTEditor<LEditRow>
{
public:
    LLineEditor();

    virtual void moveToStart() override;
    virtual void moveToEnd() override;

    virtual std::wstring selectText() const;
    virtual uint32_t job() const override;
    virtual std::list<std::shared_ptr<LEditOp>> doInput(const std::wstring& text) override;
    virtual bool checkUpdate() override;

    virtual uint32_t contentLength() const override;

    virtual void setMargins(const LMargins<double>& v) override;
    double cursorToX() const;

    virtual std::wstring content() const override;
    virtual std::wstring selectContent() const override;

protected:
    virtual bool isInputable(const wchar_t c) const override;
    virtual bool isAutoMargin() override{
        return false;
    }

    virtual void updateMaxLength() override;
};


LK_NAMESPACE_END

#endif // LLINEEDITOR_H
