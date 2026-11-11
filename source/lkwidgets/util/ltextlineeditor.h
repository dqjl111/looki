#ifndef LTEXTLINEEDITOR_H
#define LTEXTLINEEDITOR_H

#include "lkcore/lobject.h"
#include "lkgui/lguievent.h"
#include "lkwidgets/util/lmultitextlineeditor.h"

LK_NAMESPACE_BEGIN

class LKWIDGETSDEF LTextLineEditor : public LMultiTextLineEditor
{
public:
    LTextLineEditor();

protected:
    virtual bool isInputable(const wchar_t c) const override;
};

LK_NAMESPACE_END

#endif // LTEXTLINEEDITOR_H
