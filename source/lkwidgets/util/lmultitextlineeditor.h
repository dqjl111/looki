#ifndef LMULTITEXTLINEEDITOR_H
#define LMULTITEXTLINEEDITOR_H


#include "lkcore/lobject.h"
#include "lkgui/lguievent.h"
#include "lkwidgets/util/ledittext.h"
#include "lkwidgets/util/lieditor.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LMultiTextLineEditor : public LTEditor<LEditText>
{
public:
    LMultiTextLineEditor();

    virtual uint32_t job() const override;
    virtual std::list<std::shared_ptr<LEditOp>> doInput(const std::wstring& text) override;
    virtual bool checkUpdate() override;

    virtual const LMargins<double>& margins() const override{
        return m_margin;
    }

    virtual void setMargins(const LMargins<double>& v) override;
    virtual LRect<double> cursorRect() const override;
    virtual LRect<double> anchorRect() const override;
    virtual int contentRowNum() const override;

    virtual uint32_t contentLength() const override;
protected:
    virtual void updateMaxLength() override;

    int m_maxLength = -1;
    LMargins<double> m_margin;
};


LK_NAMESPACE_END

#endif // LMULTITEXTLINEEDITOR_H
